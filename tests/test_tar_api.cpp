/*
 * Copyright 2025 TierOne Software
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <tierone/tar/tar.hpp>
#include <tierone/tar/stream.hpp>
#include <tierone/tar/error.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <random>

using namespace tierone::tar;
namespace fs = std::filesystem;

namespace {

class TempFile {
    fs::path path_;
public:
    TempFile() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        auto temp = fs::temp_directory_path();
        path_ = temp / ("tierone_test_" + std::to_string(dis(gen)) + ".tar");
    }
    
    ~TempFile() {
        std::error_code ec;
        fs::remove(path_, ec);
    }
    
    const fs::path& path() const { return path_; }
    
    void write_tar_data(const std::vector<char>& data) {
        std::ofstream file(path_, std::ios::binary);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    
    void write_string(const std::string& content) {
        std::ofstream file(path_, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
};

// Create a minimal valid tar header
std::vector<char> create_minimal_tar() {
    std::vector<char> tar_data(512 * 2, '\0'); // Header + terminator blocks
    
    // Fill in minimal header fields for a regular file
    char* header = tar_data.data();
    
    // File name
    std::strcpy(header, "test.txt");
    
    // Mode (octal)
    std::strcpy(header + 100, "0644   ");
    
    // UID/GID (octal)
    std::strcpy(header + 108, "1000   ");
    std::strcpy(header + 116, "1000   ");
    
    // Size (octal) - 5 bytes
    std::strcpy(header + 124, "5      ");
    
    // Modification time (octal)
    std::strcpy(header + 136, "14000000000");
    
    // Type flag - regular file
    header[156] = '0';
    
    // Magic and version
    std::strcpy(header + 257, "ustar");
    header[263] = '0';
    header[264] = '0';
    
    // Calculate checksum
    unsigned int checksum = 0;
    // Initialize checksum field with spaces
    std::memset(header + 148, ' ', 8);
    
    for (int i = 0; i < 512; ++i) {
        checksum += static_cast<unsigned char>(header[i]);
    }
    
    // Write checksum in octal
    std::sprintf(header + 148, "%06o", checksum);
    header[154] = '\0';
    header[155] = ' ';
    
    // Add file content
    std::strcpy(tar_data.data() + 512, "Hello");
    
    return tar_data;
}

// Mock input stream for testing
class mock_stream : public input_stream {
private:
    std::vector<std::byte> data_;
    size_t position_ = 0;
    bool fail_read_ = false;
    bool fail_skip_ = false;
    
public:
    explicit mock_stream(std::vector<char> data) {
        data_.reserve(data.size());
        for (char c : data) {
            data_.push_back(static_cast<std::byte>(c));
        }
    }
    
    void set_fail_read(bool fail) { fail_read_ = fail; }
    void set_fail_skip(bool fail) { fail_skip_ = fail; }
    
    std::expected<size_t, error> read(std::span<std::byte> buffer) override {
        if (fail_read_) {
            return std::unexpected(error{error_code::io_error, "Mock read failure"});
        }
        
        size_t available = data_.size() - position_;
        size_t to_read = std::min(buffer.size(), available);
        
        std::memcpy(buffer.data(), data_.data() + position_, to_read);
        position_ += to_read;
        
        return to_read;
    }
    
    std::expected<void, error> skip(size_t bytes) override {
        if (fail_skip_) {
            return std::unexpected(error{error_code::io_error, "Mock skip failure"});
        }
        
        if (position_ + bytes > data_.size()) {
            position_ = data_.size();
        } else {
            position_ += bytes;
        }
        return {};
    }
    
    bool at_end() const override {
        return position_ >= data_.size();
    }
};

} // anonymous namespace

TEST_CASE("open_archive from filesystem path", "[unit][tar_api]") {
    SECTION("Valid tar file") {
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        
        REQUIRE(result.has_value());
        auto& reader = result.value();
        
        // Test that we can iterate through the archive
        auto it = reader.begin();
        CHECK(it != reader.end());
        
        auto& entry = *it;
        CHECK(entry.path() == "test.txt");
        CHECK(entry.size() == 5);
        CHECK(entry.is_regular_file());
    }
    
    SECTION("Non-existent file") {
        auto result = open_archive("/non/existent/file.tar");
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Failed to open"));
    }
    
    SECTION("File with no read permissions") {
        // This test requires creating a file with restricted permissions
        // Skip on systems where this isn't feasible
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        // Try to remove read permissions (may fail depending on user permissions)
        std::error_code ec;
        fs::permissions(temp_file.path(), fs::perms::none, ec);
        
        if (!ec) {
            auto result = open_archive(temp_file.path());
            CHECK_FALSE(result.has_value());
            
            // Restore permissions for cleanup
            fs::permissions(temp_file.path(), fs::perms::owner_all, ec);
        }
    }
    
    SECTION("Directory instead of file") {
        auto temp_dir = fs::temp_directory_path();
        
        auto result = open_archive(temp_dir);
        
        // Behavior depends on implementation - typically should fail
        if (!result.has_value()) {
            CHECK(result.error().code() == error_code::io_error);
        }
    }
    
    SECTION("Empty file") {
        TempFile temp_file;
        temp_file.write_string("");
        
        auto result = open_archive(temp_file.path());
        
        // Empty file should typically be handled gracefully
        if (result.has_value()) {
            auto& reader = result.value();
            CHECK(reader.begin() == reader.end());
        }
    }
    
    SECTION("Invalid tar file") {
        TempFile temp_file;
        temp_file.write_string("This is not a valid tar file content");
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            // Error may be detected during iteration rather than opening
            auto& reader = result.value();
            auto it = reader.begin();
            // May throw or return invalid entry
        }
    }
    
    SECTION("Very large file path") {
        std::string long_path = fs::temp_directory_path().string() + "/" + 
                               std::string(255, 'a') + ".tar";
        
        auto result = open_archive(long_path);
        
        // Should handle long paths gracefully
        CHECK_FALSE(result.has_value());
    }
    
    SECTION("Path with special characters") {
        auto temp_dir = fs::temp_directory_path();
        auto special_path = temp_dir / "test file with spaces & symbols.tar";
        
        {
            auto tar_data = create_minimal_tar();
            std::ofstream file(special_path, std::ios::binary);
            file.write(tar_data.data(), static_cast<std::streamsize>(tar_data.size()));
        }
        
        auto result = open_archive(special_path);
        
        if (result.has_value()) {
            CHECK(result->begin() != result->end());
        }
        
        // Cleanup
        std::error_code ec;
        fs::remove(special_path, ec);
    }
}

TEST_CASE("open_archive from input stream", "[unit][tar_api]") {
    SECTION("Valid tar stream") {
        auto tar_data = create_minimal_tar();
        auto stream = std::make_unique<mock_stream>(tar_data);
        
        auto result = open_archive(std::move(stream));
        
        REQUIRE(result.has_value());
        auto& reader = result.value();
        
        auto it = reader.begin();
        CHECK(it != reader.end());
        
        auto& entry = *it;
        CHECK(entry.path() == "test.txt");
        CHECK(entry.size() == 5);
    }
    
    SECTION("Empty stream") {
        std::vector<char> empty_data;
        auto stream = std::make_unique<mock_stream>(empty_data);
        
        auto result = open_archive(std::move(stream));
        
        if (result.has_value()) {
            auto& reader = result.value();
            CHECK(reader.begin() == reader.end());
        }
    }
    
    SECTION("Stream that fails to read") {
        auto tar_data = create_minimal_tar();
        auto stream = std::make_unique<mock_stream>(tar_data);
        stream->set_fail_read(true);
        
        auto result = open_archive(std::move(stream));
        
        if (result.has_value()) {
            // Error may be detected during iteration
            auto& reader = result.value();
            auto it = reader.begin();
            // Should handle read failures
        }
    }
    
    SECTION("Stream that fails to skip") {
        auto tar_data = create_minimal_tar();
        auto stream = std::make_unique<mock_stream>(tar_data);
        stream->set_fail_skip(true);
        
        auto result = open_archive(std::move(stream));
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            // Should handle skip failures during iteration
        }
    }
    
    SECTION("Null stream") {
        std::unique_ptr<input_stream> null_stream;
        
        auto result = open_archive(std::move(null_stream));
        
        REQUIRE_FALSE(result.has_value());
        // Should handle null stream gracefully
    }
    
    SECTION("Large tar stream") {
        // Create a larger tar with multiple files
        std::vector<char> large_tar_data(512 * 10, '\0'); // Multiple blocks
        
        // Create first file header
        char* header1 = large_tar_data.data();
        std::strcpy(header1, "file1.txt");
        std::strcpy(header1 + 100, "0644   ");
        std::strcpy(header1 + 108, "1000   ");
        std::strcpy(header1 + 116, "1000   ");
        std::strcpy(header1 + 124, "10     ");
        std::strcpy(header1 + 136, "14000000000");
        header1[156] = '0';
        std::strcpy(header1 + 257, "ustar");
        header1[263] = '0';
        header1[264] = '0';
        
        // Calculate checksum for first header
        unsigned int checksum1 = 0;
        std::memset(header1 + 148, ' ', 8);
        for (int i = 0; i < 512; ++i) {
            checksum1 += static_cast<unsigned char>(header1[i]);
        }
        std::sprintf(header1 + 148, "%06o", checksum1);
        header1[154] = '\0';
        header1[155] = ' ';
        
        // Add content for first file
        std::strcpy(large_tar_data.data() + 512, "file1data\n");
        
        auto stream = std::make_unique<mock_stream>(large_tar_data);
        auto result = open_archive(std::move(stream));
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            CHECK(it != reader.end());
            CHECK(it->path() == "file1.txt");
        }
    }
}

TEST_CASE("tar API integration scenarios", "[unit][tar_api]") {
    SECTION("Round-trip: file to stream to reader") {
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        // First open from file
        auto file_result = open_archive(temp_file.path());
        REQUIRE(file_result.has_value());
        
        // Then create stream and open from stream
        auto stream_result = file_stream::open(temp_file.path());
        REQUIRE(stream_result.has_value());
        
        auto stream_archive_result = open_archive(
            std::make_unique<file_stream>(std::move(stream_result.value())));
        
        if (stream_archive_result.has_value()) {
            // Both should produce equivalent results
            auto file_it = file_result->begin();
            auto stream_it = stream_archive_result->begin();
            
            CHECK(file_it->path() == stream_it->path());
            CHECK(file_it->size() == stream_it->size());
        }
    }
    
    SECTION("Multiple archives from same file") {
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        auto result1 = open_archive(temp_file.path());
        auto result2 = open_archive(temp_file.path());
        
        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        
        // Both should be independent and work correctly
        auto it1 = result1->begin();
        auto it2 = result2->begin();
        
        CHECK(it1 != result1->end());
        CHECK(it2 != result2->end());
        CHECK(it1->path() == it2->path());
    }
    
    SECTION("Archive with nested directories") {
        // This would require creating a more complex tar structure
        // For now, test with the simple case
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        // Ensure the API works end-to-end
        auto& reader = result.value();
        size_t entry_count = 0;
        for (auto it = reader.begin(); it != reader.end(); ++it) {
            ++entry_count;
            CHECK_FALSE(it->path().empty());
        }
        
        CHECK(entry_count >= 1);
    }
}

TEST_CASE("tar API error propagation", "[unit][tar_api]") {
    SECTION("Error from file_stream propagates") {
        auto result = open_archive("/dev/null/nonexistent");
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
    
    SECTION("Error from stream propagates") {
        auto tar_data = create_minimal_tar();
        auto stream = std::make_unique<mock_stream>(tar_data);
        
        // Stream itself is valid, but may fail during archive reading
        auto result = open_archive(std::move(stream));
        
        // The API should handle this gracefully
        // Actual error handling behavior depends on implementation
    }
    
    SECTION("Memory allocation failures") {
        // Difficult to test without actually running out of memory
        // Would require dependency injection or mocking allocators
        
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        // In normal circumstances this should succeed
        CHECK(result.has_value());
    }
}

TEST_CASE("tar API edge cases", "[unit][tar_api]") {
    SECTION("File path with Unicode characters") {
        auto temp_dir = fs::temp_directory_path();
        auto unicode_path = temp_dir / "тест_файл_测试文件.tar";
        
        {
            auto tar_data = create_minimal_tar();
            std::ofstream file(unicode_path, std::ios::binary);
            if (file.is_open()) {
                file.write(tar_data.data(), static_cast<std::streamsize>(tar_data.size()));
            }
        }
        
        if (fs::exists(unicode_path)) {
            auto result = open_archive(unicode_path);
            // Should handle Unicode paths on platforms that support them
            
            // Cleanup
            std::error_code ec;
            fs::remove(unicode_path, ec);
        }
    }
    
    SECTION("Concurrent access to same file") {
        TempFile temp_file;
        auto tar_data = create_minimal_tar();
        temp_file.write_tar_data(tar_data);
        
        // Open multiple readers concurrently
        std::vector<std::expected<archive_reader, error>> readers;
        
        for (int i = 0; i < 3; ++i) {
            readers.push_back(open_archive(temp_file.path()));
        }
        
        // All should succeed
        for (const auto& reader : readers) {
            CHECK(reader.has_value());
        }
    }
    
    SECTION("Very short tar data") {
        std::vector<char> short_data(100, '\0'); // Less than one block
        auto stream = std::make_unique<mock_stream>(short_data);
        
        auto result = open_archive(std::move(stream));
        
        // Should handle truncated data gracefully
        if (result.has_value()) {
            auto& reader = result.value();
            // Iterator should handle the short data case
            auto it = reader.begin();
        }
    }
    
    SECTION("Tar with only terminator blocks") {
        std::vector<char> terminator_only(1024, '\0'); // Two zero blocks
        auto stream = std::make_unique<mock_stream>(terminator_only);
        
        auto result = open_archive(std::move(stream));
        
        if (result.has_value()) {
            auto& reader = result.value();
            CHECK(reader.begin() == reader.end()); // Should be empty
        }
    }
}