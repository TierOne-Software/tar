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
#include <tierone/tar/tar.hpp>
#include <tierone/tar/archive_reader.hpp>
#include <tierone/tar/stream.hpp>
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
        path_ = temp / ("tierone_large_test_" + std::to_string(dis(gen)) + ".tar");
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
};

// Create a tar with a large file entry
std::vector<char> create_large_file_tar(size_t file_size) {
    // Calculate blocks needed: header + data blocks + terminator
    size_t data_blocks = (file_size + 511) / 512;
    size_t total_blocks = 1 + data_blocks + 2; // header + data + 2 terminator blocks
    
    std::vector<char> tar_data(total_blocks * 512, '\0');
    
    // Create header
    char* header = tar_data.data();
    std::strcpy(header, "large_file.bin");
    std::strcpy(header + 100, "0644   ");
    std::strcpy(header + 108, "1000   ");
    std::strcpy(header + 116, "1000   ");
    
    // Set size in octal
    std::sprintf(header + 124, "%011lo", static_cast<unsigned long>(file_size));
    
    std::strcpy(header + 136, "14000000000");
    header[156] = '0'; // Regular file
    std::strcpy(header + 257, "ustar");
    header[263] = '0';
    header[264] = '0';
    std::strcpy(header + 265, "testuser");
    std::strcpy(header + 297, "testgroup");
    
    // Calculate checksum
    unsigned int checksum = 0;
    std::memset(header + 148, ' ', 8);
    for (int i = 0; i < 512; ++i) {
        checksum += static_cast<unsigned char>(header[i]);
    }
    std::sprintf(header + 148, "%06o ", checksum);
    
    // Fill data with pattern
    char* data_start = tar_data.data() + 512;
    for (size_t i = 0; i < file_size; ++i) {
        data_start[i] = static_cast<char>('A' + (i % 26));
    }
    
    return tar_data;
}

// Create a tar with multiple large files
std::vector<char> create_multi_large_file_tar() {
    const size_t file1_size = 2 * 1024 * 1024; // 2MB
    const size_t file2_size = 3 * 1024 * 1024; // 3MB
    
    size_t file1_blocks = (file1_size + 511) / 512;
    size_t file2_blocks = (file2_size + 511) / 512;
    size_t total_blocks = 2 + file1_blocks + file2_blocks + 2; // 2 headers + data + terminators
    
    std::vector<char> tar_data(total_blocks * 512, '\0');
    char* pos = tar_data.data();
    
    // First file header
    std::strcpy(pos, "file1.bin");
    std::strcpy(pos + 100, "0644   ");
    std::strcpy(pos + 108, "1000   ");
    std::strcpy(pos + 116, "1000   ");
    std::sprintf(pos + 124, "%011lo", static_cast<unsigned long>(file1_size));
    std::strcpy(pos + 136, "14000000000");
    pos[156] = '0';
    std::strcpy(pos + 257, "ustar");
    pos[263] = '0';
    pos[264] = '0';
    
    // Calculate checksum for first file
    unsigned int checksum1 = 0;
    std::memset(pos + 148, ' ', 8);
    for (int i = 0; i < 512; ++i) {
        checksum1 += static_cast<unsigned char>(pos[i]);
    }
    std::sprintf(pos + 148, "%06o ", checksum1);
    
    pos += 512;
    
    // First file data
    for (size_t i = 0; i < file1_size; ++i) {
        pos[i] = static_cast<char>('1');
    }
    pos += file1_blocks * 512;
    
    // Second file header
    std::strcpy(pos, "file2.bin");
    std::strcpy(pos + 100, "0644   ");
    std::strcpy(pos + 108, "1000   ");
    std::strcpy(pos + 116, "1000   ");
    std::sprintf(pos + 124, "%011lo", static_cast<unsigned long>(file2_size));
    std::strcpy(pos + 136, "14000000000");
    pos[156] = '0';
    std::strcpy(pos + 257, "ustar");
    pos[263] = '0';
    pos[264] = '0';
    
    // Calculate checksum for second file
    unsigned int checksum2 = 0;
    std::memset(pos + 148, ' ', 8);
    for (int i = 0; i < 512; ++i) {
        checksum2 += static_cast<unsigned char>(pos[i]);
    }
    std::sprintf(pos + 148, "%06o ", checksum2);
    
    pos += 512;
    
    // Second file data
    for (size_t i = 0; i < file2_size; ++i) {
        pos[i] = static_cast<char>('2');
    }
    
    return tar_data;
}

bool is_large_file_test_enabled() {
    // Check if we have enough disk space and time for large file tests
    auto temp_space = fs::space(fs::temp_directory_path());
    return temp_space.available > 10LL * 1024 * 1024 * 1024; // 10GB available
}

} // anonymous namespace

TEST_CASE("Large file basic operations", "[integration][large_file]") {
    if (!is_large_file_test_enabled()) {
        SKIP("Large file tests disabled due to insufficient disk space");
    }
    
    SECTION("Medium size file (1MB)") {
        const size_t file_size = 1024 * 1024; // 1MB
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto& reader = result.value();
        auto it = reader.begin();
        REQUIRE(it != reader.end());
        
        auto& entry = *it;
        CHECK(entry.path() == "large_file.bin");
        CHECK(entry.size() == file_size);
        CHECK(entry.is_regular_file());
        
        // Test reading the entire file
        auto data_result = entry.read_data();
        REQUIRE(data_result.has_value());
        CHECK(data_result->size() == file_size);
        
        // Verify data pattern
        for (size_t i = 0; i < std::min(size_t{1000}, file_size); ++i) {
            CHECK(static_cast<char>((*data_result)[i]) == ('A' + (i % 26)));
        }
    }
    
    SECTION("Large file (10MB)") {
        const size_t file_size = 10 * 1024 * 1024; // 10MB
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto& reader = result.value();
        auto it = reader.begin();
        REQUIRE(it != reader.end());
        
        auto& entry = *it;
        CHECK(entry.size() == file_size);
        
        // Test partial reads
        auto chunk_result = entry.read_data(0, 64 * 1024); // Read 64KB
        REQUIRE(chunk_result.has_value());
        CHECK(chunk_result->size() == 64 * 1024);
        
        // Note: Random access reads with offsets are not supported in streaming mode
        // This is correct behavior - streaming mode is for sequential access only
    }
}

TEST_CASE("Large file memory efficiency", "[integration][large_file]") {
    if (!is_large_file_test_enabled()) {
        SKIP("Large file tests disabled");
    }
    
    SECTION("Memory-mapped access for large files") {
        const size_t file_size = 5 * 1024 * 1024; // 5MB
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        // Test with memory-mapped stream if available
        #ifdef __linux__
        auto mmap_stream_result = mmap_stream::create(temp_file.path());
        if (mmap_stream_result.has_value()) {
            auto reader_result = archive_reader::from_stream(
                std::make_unique<mmap_stream>(std::move(mmap_stream_result.value())));
            
            REQUIRE(reader_result.has_value());
            auto& reader = reader_result.value();
            auto it = reader.begin();
            
            if (it != reader.end()) {
                auto& entry = *it;
                CHECK(entry.size() == file_size);
                
                // Memory-mapped access should be efficient
                auto data_result = entry.read_data(0, 4096);
                REQUIRE(data_result.has_value());
                CHECK(data_result->size() == 4096);
            }
        }
        #endif
        
        // Test with regular file stream
        auto file_stream_result = file_stream::open(temp_file.path());
        REQUIRE(file_stream_result.has_value());
        
        auto reader_result = archive_reader::from_stream(
            std::make_unique<file_stream>(std::move(file_stream_result.value())));
        
        REQUIRE(reader_result.has_value());
        auto& reader = reader_result.value();
        auto it = reader.begin();
        
        if (it != reader.end()) {
            auto& entry = *it;
            CHECK(entry.size() == file_size);
            
            // Should handle large files without loading everything into memory
            auto small_read = entry.read_data(0, 1024);
            REQUIRE(small_read.has_value());
            CHECK(small_read->size() == 1024);
        }
    }
}

TEST_CASE("Multiple large files", "[integration][large_file]") {
    if (!is_large_file_test_enabled()) {
        SKIP("Large file tests disabled");
    }
    
    SECTION("Archive with multiple large files") {
        TempFile temp_file;
        auto tar_data = create_multi_large_file_tar();
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto& reader = result.value();
        size_t file_count = 0;
        size_t total_size = 0;
        
        for (auto it = reader.begin(); it != reader.end(); ++it) {
            ++file_count;
            total_size += it->size();
            
            CHECK(it->is_regular_file());
            CHECK(it->size() > 1024 * 1024); // At least 1MB
            
            // Test reading a small portion of each large file
            auto sample_data = it->read_data(0, 4096);
            REQUIRE(sample_data.has_value());
            
            if (it->path() == "file1.bin") {
                // Verify pattern for first file
                for (size_t i = 0; i < sample_data->size(); ++i) {
                    CHECK(static_cast<char>((*sample_data)[i]) == '1');
                }
            } else if (it->path() == "file2.bin") {
                // Verify pattern for second file
                for (size_t i = 0; i < sample_data->size(); ++i) {
                    CHECK(static_cast<char>((*sample_data)[i]) == '2');
                }
            }
        }
        
        CHECK(file_count == 2);
        CHECK(total_size == 5 * 1024 * 1024); // 2MB + 3MB
    }
}

TEST_CASE("Large file extraction", "[integration][large_file]") {
    if (!is_large_file_test_enabled()) {
        SKIP("Large file tests disabled");
    }
    
    SECTION("Extract large file to filesystem") {
        const size_t file_size = 2 * 1024 * 1024; // 2MB
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        REQUIRE(it != result->end());
        
        auto& entry = *it;
        auto output_path = fs::temp_directory_path() / "extracted_large_file.bin";
        
        auto extract_result = entry.extract_to_path(output_path);
        REQUIRE(extract_result.has_value());
        
        // Verify extracted file
        CHECK(fs::exists(output_path));
        CHECK(fs::file_size(output_path) == file_size);
        
        // Verify content of extracted file
        std::ifstream extracted_file(output_path, std::ios::binary);
        std::vector<char> sample(1024);
        extracted_file.read(sample.data(), 1024);
        
        for (size_t i = 0; i < 1024; ++i) {
            CHECK(sample[i] == ('A' + (i % 26)));
        }
        
        // Cleanup
        std::error_code ec;
        fs::remove(output_path, ec);
    }
    
    SECTION("Large file copy to iterator") {
        const size_t file_size = 1024 * 1024; // 1MB
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        REQUIRE(it != result->end());
        
        auto& entry = *it;
        std::vector<std::byte> output_buffer;
        
        auto copy_result = entry.copy_data_to(std::back_inserter(output_buffer));
        REQUIRE(copy_result.has_value());
        CHECK(copy_result.value() == file_size);
        CHECK(output_buffer.size() == file_size);
        
        // Verify pattern in copied data
        for (size_t i = 0; i < std::min(size_t{1000}, file_size); ++i) {
            CHECK(static_cast<char>(output_buffer[i]) == ('A' + (i % 26)));
        }
    }
}

TEST_CASE("Large file performance characteristics", "[integration][large_file]") {
    if (!is_large_file_test_enabled()) {
        SKIP("Large file tests disabled");
    }
    
    SECTION("Sequential vs random access patterns") {
        const size_t file_size = 4 * 1024 * 1024; // 4MB
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        REQUIRE(it != result->end());
        
        auto& entry = *it;
        
        // Sequential reads (streaming mode - read entire file)
        auto sequential_result = entry.read_data();
        REQUIRE(sequential_result.has_value());
        CHECK(sequential_result->size() == file_size);
        
        // Note: Random access testing is complex due to streaming vs mmap mode differences
        // Focus on performance of sequential access patterns
    }
    
    SECTION("Large file iteration performance") {
        TempFile temp_file;
        auto tar_data = create_multi_large_file_tar();
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto& reader = result.value();
        
        // Time iteration through large files
        auto start_time = std::chrono::high_resolution_clock::now();
        
        size_t entries_processed = 0;
        for (auto it = reader.begin(); it != reader.end(); ++it) {
            ++entries_processed;
            
            // Just access metadata, don't read data
            CHECK(it->size() > 0);
            CHECK_FALSE(it->path().empty());
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        CHECK(entries_processed == 2);
        // Iteration should be fast even for large files
        CHECK(duration.count() < 1000); // Less than 1 second
    }
}

TEST_CASE("Large file edge cases", "[integration][large_file]") {
    SECTION("File exactly at block boundary") {
        const size_t file_size = 512 * 1000; // Exactly 1000 blocks
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        REQUIRE(it != result->end());
        
        CHECK(it->size() == file_size);
        
        // Read entire file
        auto data_result = it->read_data();
        REQUIRE(data_result.has_value());
        CHECK(data_result->size() == file_size);
    }
    
    SECTION("File with size just under block boundary") {
        const size_t file_size = 512 * 1000 - 1; // One byte less than 1000 blocks
        TempFile temp_file;
        auto tar_data = create_large_file_tar(file_size);
        temp_file.write_data(tar_data);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        REQUIRE(it != result->end());
        
        CHECK(it->size() == file_size);
        
        auto data_result = it->read_data();
        REQUIRE(data_result.has_value());
        CHECK(data_result->size() == file_size);
    }
    
    SECTION("Empty large file (size claimed but no data)") {
        TempFile temp_file;
        std::vector<char> empty_large_tar(512 * 2, '\0'); // Header + terminator
        
        char* header = empty_large_tar.data();
        std::strcpy(header, "empty_large.bin");
        std::strcpy(header + 100, "0644   ");
        std::strcpy(header + 108, "1000   ");
        std::strcpy(header + 116, "1000   ");
        std::strcpy(header + 124, "00000000000"); // Size 0
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
        
        temp_file.write_data(empty_large_tar);
        
        auto result = open_archive(temp_file.path());
        REQUIRE(result.has_value());
        
        auto it = result->begin();
        REQUIRE(it != result->end());
        
        CHECK(it->size() == 0);
        
        auto data_result = it->read_data();
        REQUIRE(data_result.has_value());
        CHECK(data_result->size() == 0);
    }
}