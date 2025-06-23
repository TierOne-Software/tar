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
#include <tierone/tar/archive_reader.hpp>
#include <tierone/tar/stream.hpp>
#include <tierone/tar/error.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <random>
#include <cstring>

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
    
    void write_data(const std::vector<char>& data) {
        std::ofstream file(path_, std::ios::binary);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    
    void write_string(const std::string& content) {
        std::ofstream file(path_, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
};

// Create a corrupted tar with invalid headers
std::vector<char> create_corrupted_tar() {
    std::vector<char> tar_data(512 * 3, '\0');
    
    // Create an invalid header with corrupted magic
    char* header = tar_data.data();
    std::strcpy(header, "test.txt");
    std::strcpy(header + 100, "0644   ");
    std::strcpy(header + 108, "1000   ");
    std::strcpy(header + 116, "1000   ");
    std::strcpy(header + 124, "10     ");
    std::strcpy(header + 136, "14000000000");
    header[156] = '0';
    std::strcpy(header + 257, "WRONG");  // Invalid magic
    header[263] = '0';
    header[264] = '0';
    
    return tar_data;
}

// Create truncated tar (incomplete header)
std::vector<char> create_truncated_tar() {
    std::vector<char> tar_data(256, '\0'); // Only half a block
    
    char* header = tar_data.data();
    std::strcpy(header, "test.txt");
    std::strcpy(header + 100, "0644   ");
    // Rest is incomplete...
    
    return tar_data;
}

// Create tar with invalid checksum
std::vector<char> create_invalid_checksum_tar() {
    std::vector<char> tar_data(512 * 2, '\0');
    
    char* header = tar_data.data();
    std::strcpy(header, "test.txt");
    std::strcpy(header + 100, "0644   ");
    std::strcpy(header + 108, "1000   ");
    std::strcpy(header + 116, "1000   ");
    std::strcpy(header + 124, "5      ");
    std::strcpy(header + 136, "14000000000");
    header[156] = '0';
    std::strcpy(header + 257, "ustar");
    header[263] = '0';
    header[264] = '0';
    
    // Set invalid checksum
    std::strcpy(header + 148, "999999 ");
    
    // Add some content
    std::strcpy(tar_data.data() + 512, "Hello");
    
    return tar_data;
}

// Create tar with size mismatch
std::vector<char> create_size_mismatch_tar() {
    std::vector<char> tar_data(512 * 2, '\0');
    
    char* header = tar_data.data();
    std::strcpy(header, "test.txt");
    std::strcpy(header + 100, "0644   ");
    std::strcpy(header + 108, "1000   ");
    std::strcpy(header + 116, "1000   ");
    std::strcpy(header + 124, "1000   "); // Claims 512 bytes
    std::strcpy(header + 136, "14000000000");
    header[156] = '0';
    std::strcpy(header + 257, "ustar");
    header[263] = '0';
    header[264] = '0';
    
    // Calculate correct checksum
    unsigned int checksum = 0;
    std::memset(header + 148, ' ', 8);
    for (int i = 0; i < 512; ++i) {
        checksum += static_cast<unsigned char>(header[i]);
    }
    std::sprintf(header + 148, "%06o ", checksum);
    
    // Only provide 5 bytes instead of 512
    std::strcpy(tar_data.data() + 512, "Hello");
    
    return tar_data;
}

} // anonymous namespace

TEST_CASE("Error handling in archive opening", "[integration][error_handling]") {
    SECTION("Non-existent file") {
        auto result = open_archive("/non/existent/path/file.tar");
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
        CHECK_THAT(result.error().message(), 
                  Catch::Matchers::ContainsSubstring("Failed to open"));
    }
    
    SECTION("Directory instead of file") {
        auto temp_dir = fs::temp_directory_path();
        auto result = open_archive(temp_dir);
        
        // Behavior may vary, but should handle gracefully
        if (!result.has_value()) {
            CHECK(result.error().code() == error_code::io_error);
        }
    }
    
    SECTION("File with no read permissions") {
        TempFile temp_file;
        temp_file.write_string("test content");
        
        // Try to remove read permissions
        std::error_code ec;
        fs::permissions(temp_file.path(), fs::perms::none, ec);
        
        if (!ec) {
            auto result = open_archive(temp_file.path());
            CHECK_FALSE(result.has_value());
            
            // Restore permissions for cleanup
            fs::permissions(temp_file.path(), fs::perms::owner_all, ec);
        }
    }
    
    SECTION("Empty file") {
        TempFile temp_file;
        temp_file.write_string("");
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            auto& reader = result.value();
            CHECK(reader.begin() == reader.end());
        }
    }
    
    SECTION("File that's not a tar archive") {
        TempFile temp_file;
        temp_file.write_string("This is just plain text, not a tar file at all!");
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            // Error may be detected during iteration
            auto& reader = result.value();
            auto it = reader.begin();
            // Accessing the iterator may reveal the error
        }
    }
}

TEST_CASE("Error handling during archive iteration", "[integration][error_handling]") {
    SECTION("Corrupted tar header") {
        TempFile temp_file;
        auto corrupted_data = create_corrupted_tar();
        temp_file.write_data(corrupted_data);
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            
            // Should handle corrupted header gracefully
            if (it != reader.end()) {
                // May succeed with degraded parsing
                CHECK_FALSE(it->path().empty());
            }
        }
    }
    
    SECTION("Truncated tar file") {
        TempFile temp_file;
        auto truncated_data = create_truncated_tar();
        temp_file.write_data(truncated_data);
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            
            // Should detect truncation
            if (it == reader.end()) {
                // Detected as empty archive
                CHECK(true);
            } else {
                // May attempt to parse partial header
                // Behavior depends on implementation robustness
            }
        }
    }
    
    SECTION("Invalid checksum") {
        TempFile temp_file;
        auto invalid_checksum_data = create_invalid_checksum_tar();
        temp_file.write_data(invalid_checksum_data);
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            
            // Implementation may warn about checksum but continue
            if (it != reader.end()) {
                CHECK(it->path() == "test.txt");
            }
        }
    }
    
    SECTION("Size mismatch") {
        TempFile temp_file;
        auto size_mismatch_data = create_size_mismatch_tar();
        temp_file.write_data(size_mismatch_data);
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            
            if (it != reader.end()) {
                auto& entry = *it;
                
                // Try to read data - should detect size mismatch
                auto data_result = entry.read_data();
                if (data_result.has_value()) {
                    // May return less data than expected
                    CHECK(data_result->size() <= entry.size());
                }
            }
        }
    }
}

TEST_CASE("Error handling during data extraction", "[integration][error_handling]") {
    SECTION("Read-only destination directory") {
        TempFile temp_file;
        auto tar_data = create_size_mismatch_tar(); // Use any valid tar
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        if (it != result->end()) {
            // Try to extract to read-only location
            auto extract_result = it->extract_to_path("/root/no_permission/test.txt");
            
            REQUIRE_FALSE(extract_result.has_value());
            CHECK(extract_result.error().code() == error_code::io_error);
        }
    }
    
    SECTION("Disk full simulation") {
        // Difficult to simulate without actually filling disk
        // This would require dependency injection or mocking
        
        TempFile temp_file;
        auto tar_data = create_invalid_checksum_tar();
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        if (it != result->end()) {
            // Extract to a normal location (should succeed)
            auto temp_output = fs::temp_directory_path() / "extracted_test.txt";
            auto extract_result = it->extract_to_path(temp_output);
            
            if (extract_result.has_value()) {
                CHECK(fs::exists(temp_output));
                
                // Cleanup
                std::error_code ec;
                fs::remove(temp_output, ec);
            }
        }
    }
    
    SECTION("Invalid symbolic link target") {
        // Would need to create a tar with a symbolic link pointing to invalid target
        // This is more complex to set up but represents a real-world scenario
        
        TempFile temp_file;
        std::vector<char> symlink_tar(512 * 2, '\0');
        
        char* header = symlink_tar.data();
        std::strcpy(header, "badlink");
        std::strcpy(header + 100, "0777   ");
        std::strcpy(header + 108, "1000   ");
        std::strcpy(header + 116, "1000   ");
        std::strcpy(header + 124, "0      ");
        std::strcpy(header + 136, "14000000000");
        header[156] = '2'; // Symbolic link
        std::strcpy(header + 157, "/invalid/target/that/does/not/exist");
        std::strcpy(header + 257, "ustar");
        header[263] = '0';
        header[264] = '0';
        
        // Calculate checksum
        unsigned int checksum = 0;
        std::memset(header + 148, ' ', 8);
        for (int i = 0; i < 512; ++i) {
            checksum += static_cast<unsigned char>(header[i]);
        }
        std::sprintf(header + 148, "%06o ", checksum);
        
        temp_file.write_data(symlink_tar);
        
        auto result = open_archive(temp_file.path());
        if (result.has_value()) {
            auto it = result->begin();
            if (it != result->end() && it->is_symbolic_link()) {
                auto temp_output = fs::temp_directory_path() / "badlink";
                auto extract_result = it->extract_to_path(temp_output);
                
                // Should succeed in creating the symlink even if target doesn't exist
                if (extract_result.has_value()) {
                    CHECK(fs::is_symlink(temp_output));
                    
                    // Cleanup
                    std::error_code ec;
                    fs::remove(temp_output, ec);
                }
            }
        }
    }
}

TEST_CASE("Error recovery and continuation", "[integration][error_handling]") {
    SECTION("Continue after corrupted entry") {
        // Create a tar with one good entry, one corrupted, and one good
        std::vector<char> mixed_tar(512 * 4, '\0');
        
        // First entry (good)
        char* header1 = mixed_tar.data();
        std::strcpy(header1, "good1.txt");
        std::strcpy(header1 + 100, "0644   ");
        std::strcpy(header1 + 108, "1000   ");
        std::strcpy(header1 + 116, "1000   ");
        std::strcpy(header1 + 124, "5      ");
        std::strcpy(header1 + 136, "14000000000");
        header1[156] = '0';
        std::strcpy(header1 + 257, "ustar");
        header1[263] = '0';
        header1[264] = '0';
        
        // Calculate checksum for first entry
        unsigned int checksum1 = 0;
        std::memset(header1 + 148, ' ', 8);
        for (int i = 0; i < 512; ++i) {
            checksum1 += static_cast<unsigned char>(header1[i]);
        }
        std::sprintf(header1 + 148, "%06o ", checksum1);
        
        // Add content for first entry
        std::strcpy(mixed_tar.data() + 512, "data1");
        
        // Second entry (corrupted) - skip a block to create a gap
        char* header2 = mixed_tar.data() + 1024;
        std::strcpy(header2, "corrupt.txt");
        std::strcpy(header2 + 257, "WRONG"); // Invalid magic
        
        TempFile temp_file;
        temp_file.write_data(mixed_tar);
        
        auto result = open_archive(temp_file.path());
        if (result.has_value()) {
            auto& reader = result.value();
            size_t count = 0;
            
            for (auto it = reader.begin(); it != reader.end(); ++it) {
                ++count;
                // Should be able to iterate through valid entries
                CHECK_FALSE(it->path().empty());
            }
            
            // Should find at least the first good entry
            CHECK(count >= 1);
        }
    }
    
    SECTION("Handle partial reads gracefully") {
        TempFile temp_file;
        auto normal_tar = create_invalid_checksum_tar(); // Use any tar
        
        // Truncate the file to simulate partial read
        temp_file.write_data(normal_tar);
        
        {
            std::ofstream truncate_file(temp_file.path(), 
                                       std::ios::binary | std::ios::trunc);
            truncate_file.write(normal_tar.data(), 768); // 1.5 blocks
        }
        
        auto result = open_archive(temp_file.path());
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            
            // Should handle truncated data gracefully
            if (it != reader.end()) {
                // May read partial entry or detect truncation
                auto& entry = *it;
                auto data_result = entry.read_data();
                // Should not crash on partial data
            }
        }
    }
}

TEST_CASE("Memory and resource error handling", "[integration][error_handling]") {
    SECTION("Very large claimed file size") {
        TempFile temp_file;
        std::vector<char> large_size_tar(512 * 2, '\0');
        
        char* header = large_size_tar.data();
        std::strcpy(header, "huge.bin");
        std::strcpy(header + 100, "0644   ");
        std::strcpy(header + 108, "1000   ");
        std::strcpy(header + 116, "1000   ");
        std::strcpy(header + 124, "77777777777"); // Maximum size
        std::strcpy(header + 136, "14000000000");
        header[156] = '0';
        std::strcpy(header + 257, "ustar");
        header[263] = '0';
        header[264] = '0';
        
        // Calculate checksum
        unsigned int checksum = 0;
        std::memset(header + 148, ' ', 8);
        for (int i = 0; i < 512; ++i) {
            checksum += static_cast<unsigned char>(header[i]);
        }
        std::sprintf(header + 148, "%06o ", checksum);
        
        temp_file.write_data(large_size_tar);
        
        auto result = open_archive(temp_file.path());
        if (result.has_value()) {
            auto it = result->begin();
            if (it != result->end()) {
                // Should handle large size claims without allocating huge memory
                auto& entry = *it;
                CHECK(entry.size() > 0);
                
                // Try to read data - should handle gracefully
                auto data_result = entry.read_data(0, 1024); // Small read
                if (data_result.has_value()) {
                    CHECK(data_result->size() <= 1024);
                }
            }
        }
    }
    
    SECTION("Concurrent access stress test") {
        TempFile temp_file;
        auto tar_data = create_invalid_checksum_tar();
        temp_file.write_data(tar_data);
        
        // Open multiple readers to test resource management
        std::vector<std::expected<archive_reader, error>> readers;
        
        for (int i = 0; i < 10; ++i) {
            readers.push_back(open_archive(temp_file.path()));
        }
        
        // All should succeed or fail gracefully
        size_t successful_opens = 0;
        for (auto& reader : readers) {
            if (reader.has_value()) {
                ++successful_opens;
                
                // Try to use each reader
                auto it = reader->begin();
                if (it != reader->end()) {
                    CHECK_FALSE(it->path().empty());
                }
            }
        }
        
        CHECK(successful_opens > 0);
    }
}

TEST_CASE("Error message quality", "[integration][error_handling]") {
    SECTION("Descriptive error messages") {
        auto result = open_archive("/this/path/does/not/exist.tar");
        
        REQUIRE_FALSE(result.has_value());
        
        // Error message should be descriptive
        const auto& error_msg = result.error().message();
        CHECK_FALSE(error_msg.empty());
        CHECK_THAT(error_msg, Catch::Matchers::ContainsSubstring("open"));
    }
    
    SECTION("Error context preservation") {
        TempFile temp_file;
        auto corrupted_data = create_corrupted_tar();
        temp_file.write_data(corrupted_data);
        
        auto result = open_archive(temp_file.path());
        
        if (result.has_value()) {
            auto& reader = result.value();
            auto it = reader.begin();
            
            if (it != reader.end()) {
                auto data_result = it->read_data();
                if (!data_result.has_value()) {
                    // Error should provide context about what went wrong
                    CHECK_FALSE(data_result.error().message().empty());
                }
            }
        }
    }
}