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
#include <tierone/tar/stream.hpp>
#include <tierone/tar/error.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <random>
#include <thread>

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
        path_ = temp / ("tierone_test_" + std::to_string(dis(gen)) + ".dat");
    }
    
    ~TempFile() {
        std::error_code ec;
        fs::remove(path_, ec);
    }
    
    const fs::path& path() const { return path_; }
    
    void write(const std::string& content) {
        std::ofstream file(path_, std::ios::binary);
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    
    void write(const std::vector<std::byte>& data) {
        std::ofstream file(path_, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), 
                  static_cast<std::streamsize>(data.size()));
    }
};

std::vector<std::byte> create_test_data(size_t size, std::byte pattern = std::byte{0xAB}) {
    std::vector<std::byte> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<std::byte>((static_cast<uint8_t>(pattern) + i) % 256);
    }
    return data;
}

std::string bytes_to_string(std::span<const std::byte> bytes) {
    std::string result;
    result.reserve(bytes.size());
    for (auto b : bytes) {
        result.push_back(static_cast<char>(b));
    }
    return result;
}

} // anonymous namespace

TEST_CASE("memory_mapped_stream basic operations", "[unit][stream]") {
    auto data = create_test_data(1024);
    std::span<const std::byte> data_span(data);
    memory_mapped_stream stream{data_span};
    
    SECTION("Initial state") {
        CHECK_FALSE(stream.at_end());
        CHECK(stream.position() == 0);
        REQUIRE(stream.size().has_value());
        CHECK(stream.size().value() == 1024);
    }
    
    SECTION("Read full buffer") {
        std::array<std::byte, 100> buffer{};
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 100);
        CHECK(stream.position() == 100);
        CHECK(std::memcmp(buffer.data(), data.data(), 100) == 0);
    }
    
    SECTION("Read beyond available data") {
        std::array<std::byte, 2000> buffer{};
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 1024);
        CHECK(stream.position() == 1024);
        CHECK(stream.at_end());
    }
    
    SECTION("Multiple reads") {
        std::array<std::byte, 100> buffer1{};
        std::array<std::byte, 100> buffer2{};
        
        auto result1 = stream.read(buffer1);
        auto result2 = stream.read(buffer2);
        
        REQUIRE(result1.has_value());
        REQUIRE(result2.has_value());
        CHECK(result1.value() == 100);
        CHECK(result2.value() == 100);
        CHECK(stream.position() == 200);
        CHECK(std::memcmp(buffer1.data(), data.data(), 100) == 0);
        CHECK(std::memcmp(buffer2.data(), data.data() + 100, 100) == 0);
    }
    
    SECTION("Read at end of stream") {
        CHECK(stream.seek(1024).has_value());
        CHECK(stream.at_end());
        
        std::array<std::byte, 100> buffer{};
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 0);
    }
}

TEST_CASE("memory_mapped_stream skip operations", "[unit][stream]") {
    auto data = create_test_data(1024);
    memory_mapped_stream stream{std::span<const std::byte>(data)};
    
    SECTION("Skip within bounds") {
        auto result = stream.skip(100);
        
        REQUIRE(result.has_value());
        CHECK(stream.position() == 100);
        
        // Verify next read starts at correct position
        std::array<std::byte, 10> buffer{};
        auto read_result = stream.read(buffer);
        REQUIRE(read_result.has_value());
        CHECK(std::memcmp(buffer.data(), data.data() + 100, 10) == 0);
    }
    
    SECTION("Skip to exact end") {
        auto result = stream.skip(1024);
        
        REQUIRE(result.has_value());
        CHECK(stream.position() == 1024);
        CHECK(stream.at_end());
    }
    
    SECTION("Skip past end") {
        auto result = stream.skip(2000);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Skip past end"));
        CHECK(stream.position() == 0); // Position unchanged on error
    }
    
    SECTION("Multiple skips") {
        CHECK(stream.skip(100).has_value());
        CHECK(stream.skip(200).has_value());
        CHECK(stream.skip(300).has_value());
        
        CHECK(stream.position() == 600);
    }
}

TEST_CASE("memory_mapped_stream seek operations", "[unit][stream]") {
    auto data = create_test_data(1024);
    memory_mapped_stream stream{std::span<const std::byte>(data)};
    
    SECTION("Seek to valid positions") {
        CHECK(stream.seek(500).has_value());
        CHECK(stream.position() == 500);
        
        CHECK(stream.seek(0).has_value());
        CHECK(stream.position() == 0);
        
        CHECK(stream.seek(1024).has_value());
        CHECK(stream.position() == 1024);
        CHECK(stream.at_end());
    }
    
    SECTION("Seek past end") {
        auto result = stream.seek(2000);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Seek past end"));
    }
    
    SECTION("Seek backwards") {
        CHECK(stream.seek(500).has_value());
        CHECK(stream.seek(100).has_value());
        CHECK(stream.position() == 100);
        
        // Verify read after backward seek
        std::array<std::byte, 10> buffer{};
        auto read_result = stream.read(buffer);
        REQUIRE(read_result.has_value());
        CHECK(std::memcmp(buffer.data(), data.data() + 100, 10) == 0);
    }
}

TEST_CASE("memory_mapped_stream edge cases", "[unit][stream]") {
    SECTION("Empty data span") {
        std::vector<std::byte> empty_data;
        memory_mapped_stream stream{std::span<const std::byte>(empty_data)};
        
        CHECK(stream.at_end());
        CHECK(stream.position() == 0);
        CHECK(stream.size().value() == 0);
        
        std::array<std::byte, 10> buffer{};
        auto result = stream.read(buffer);
        CHECK(result.value() == 0);
        
        CHECK_FALSE(stream.skip(1).has_value());
        CHECK_FALSE(stream.seek(1).has_value());
    }
    
    SECTION("Single byte operations") {
        std::array<std::byte, 1> single_byte{std::byte{0x42}};
        memory_mapped_stream stream{std::span<const std::byte>(single_byte)};
        
        std::array<std::byte, 1> buffer{};
        auto result = stream.read(buffer);
        
        CHECK(result.value() == 1);
        CHECK(buffer[0] == std::byte{0x42});
        CHECK(stream.at_end());
    }
    
    SECTION("Large data span") {
        auto large_data = create_test_data(10 * 1024 * 1024); // 10MB
        memory_mapped_stream stream{std::span<const std::byte>(large_data)};
        
        CHECK(stream.size().value() == 10 * 1024 * 1024);
        
        // Skip large amount
        CHECK(stream.skip(5 * 1024 * 1024).has_value());
        CHECK(stream.position() == 5 * 1024 * 1024);
        
        // Read from middle
        std::array<std::byte, 1024> buffer{};
        auto result = stream.read(buffer);
        CHECK(result.value() == 1024);
    }
}

TEST_CASE("file_stream basic operations", "[unit][stream]") {
    TempFile temp_file;
    auto test_data = create_test_data(1024);
    temp_file.write(test_data);
    
    auto stream_result = file_stream::open(temp_file.path());
    REQUIRE(stream_result.has_value());
    auto& stream = stream_result.value();
    
    SECTION("Initial state") {
        CHECK_FALSE(stream.at_end());
        CHECK(stream.position() == 0);
        REQUIRE(stream.size().has_value());
        CHECK(stream.size().value() == 1024);
    }
    
    SECTION("Read operations") {
        std::array<std::byte, 100> buffer{};
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 100);
        CHECK(stream.position() == 100);
        CHECK(std::memcmp(buffer.data(), test_data.data(), 100) == 0);
    }
    
    SECTION("Read entire file") {
        std::vector<std::byte> buffer(2000);
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 1024);
        CHECK(stream.at_end());
    }
    
    SECTION("Multiple reads") {
        std::array<std::byte, 512> buffer1{};
        std::array<std::byte, 512> buffer2{};
        
        CHECK(stream.read(buffer1).value() == 512);
        CHECK(stream.read(buffer2).value() == 512);
        CHECK(stream.at_end());
        
        CHECK(std::memcmp(buffer1.data(), test_data.data(), 512) == 0);
        CHECK(std::memcmp(buffer2.data(), test_data.data() + 512, 512) == 0);
    }
}

TEST_CASE("file_stream skip and seek", "[unit][stream]") {
    TempFile temp_file;
    auto test_data = create_test_data(1024);
    temp_file.write(test_data);
    
    auto stream_result = file_stream::open(temp_file.path());
    REQUIRE(stream_result.has_value());
    auto& stream = stream_result.value();
    
    SECTION("Skip operations") {
        CHECK(stream.skip(100).has_value());
        CHECK(stream.position() == 100);
        
        std::array<std::byte, 10> buffer{};
        stream.read(buffer);
        CHECK(std::memcmp(buffer.data(), test_data.data() + 100, 10) == 0);
    }
    
    SECTION("Seek operations") {
        CHECK(stream.seek(500).has_value());
        CHECK(stream.position() == 500);
        
        CHECK(stream.seek(0).has_value());
        CHECK(stream.position() == 0);
        
        CHECK(stream.seek(1024).has_value());
        CHECK(stream.at_end());
    }
    
    SECTION("Combined operations") {
        std::array<std::byte, 100> buffer{};
        CHECK(stream.read(buffer).has_value());
        CHECK(stream.skip(200).has_value());
        CHECK(stream.seek(50).has_value());
        CHECK(stream.position() == 50);
    }
}

TEST_CASE("file_stream error handling", "[unit][stream]") {
    SECTION("Open non-existent file") {
        auto result = file_stream::open("/non/existent/file.tar");
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Failed to open"));
    }
    
    SECTION("Open directory") {
        auto temp_dir = fs::temp_directory_path();
        auto result = file_stream::open(temp_dir);
        
        // Behavior may vary by platform
        if (!result.has_value()) {
            CHECK(result.error().code() == error_code::io_error);
        }
    }
    
    SECTION("Empty file") {
        TempFile temp_file;
        temp_file.write("");
        
        auto stream_result = file_stream::open(temp_file.path());
        REQUIRE(stream_result.has_value());
        auto& stream = stream_result.value();
        
        CHECK(stream.size().value() == 0);
        CHECK(stream.at_end());
        
        std::array<std::byte, 10> buffer{};
        CHECK(stream.read(buffer).value() == 0);
    }
}

TEST_CASE("file_stream large file handling", "[unit][stream]") {
    TempFile temp_file;
    
    // Create a 5MB file
    const size_t file_size = 5 * 1024 * 1024;
    auto large_data = create_test_data(file_size);
    temp_file.write(large_data);
    
    auto stream_result = file_stream::open(temp_file.path());
    REQUIRE(stream_result.has_value());
    auto& stream = stream_result.value();
    
    SECTION("Size detection") {
        CHECK(stream.size().value() == file_size);
    }
    
    SECTION("Seek to various positions") {
        CHECK(stream.seek(1024 * 1024).has_value()); // 1MB
        CHECK(stream.position() == 1024 * 1024);
        
        CHECK(stream.seek(4 * 1024 * 1024).has_value()); // 4MB
        CHECK(stream.position() == 4 * 1024 * 1024);
    }
    
    SECTION("Skip large amounts") {
        CHECK(stream.skip(2 * 1024 * 1024).has_value());
        CHECK(stream.position() == 2 * 1024 * 1024);
    }
    
    SECTION("Read from end of large file") {
        CHECK(stream.seek(file_size - 100).has_value());
        
        std::array<std::byte, 200> buffer{};
        auto result = stream.read(buffer);
        
        CHECK(result.value() == 100);
        CHECK(stream.at_end());
    }
}

#ifdef __linux__
TEST_CASE("mmap_stream basic operations", "[unit][stream][linux]") {
    TempFile temp_file;
    auto test_data = create_test_data(1024);
    temp_file.write(test_data);
    
    auto stream_result = mmap_stream::create(temp_file.path());
    REQUIRE(stream_result.has_value());
    auto& stream = stream_result.value();
    
    SECTION("Initial state") {
        CHECK_FALSE(stream.at_end());
        CHECK(stream.position() == 0);
        CHECK(stream.size().value() == 1024);
    }
    
    SECTION("Read operations") {
        std::array<std::byte, 100> buffer{};
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 100);
        CHECK(std::memcmp(buffer.data(), test_data.data(), 100) == 0);
    }
    
    SECTION("Skip and seek") {
        CHECK(stream.skip(500).has_value());
        CHECK(stream.position() == 500);
        
        CHECK(stream.seek(100).has_value());
        CHECK(stream.position() == 100);
    }
}

TEST_CASE("mmap_stream error handling", "[unit][stream][linux]") {
    SECTION("Non-existent file") {
        auto result = mmap_stream::create("/non/existent/file");
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
    
    SECTION("Empty file") {
        TempFile temp_file;
        temp_file.write("");
        
        auto stream_result = mmap_stream::create(temp_file.path());
        REQUIRE(stream_result.has_value());
        auto& stream = stream_result.value();
        
        CHECK(stream.size().value() == 0);
        CHECK(stream.at_end());
    }
}

TEST_CASE("mmap_stream large file", "[unit][stream][linux]") {
    TempFile temp_file;
    
    // Create a 10MB file
    const size_t file_size = 10 * 1024 * 1024;
    auto large_data = create_test_data(file_size);
    temp_file.write(large_data);
    
    auto stream_result = mmap_stream::create(temp_file.path());
    REQUIRE(stream_result.has_value());
    auto& stream = stream_result.value();
    
    SECTION("Memory-mapped access") {
        // Read from middle
        CHECK(stream.seek(5 * 1024 * 1024).has_value());
        
        std::array<std::byte, 1024> buffer{};
        auto result = stream.read(buffer);
        
        CHECK(result.value() == 1024);
        CHECK(std::memcmp(buffer.data(), large_data.data() + 5 * 1024 * 1024, 1024) == 0);
    }
    
    SECTION("Sequential access pattern") {
        // The implementation uses MADV_SEQUENTIAL
        // Read sequentially through the file
        size_t total_read = 0;
        std::array<std::byte, 4096> buffer{};
        
        while (!stream.at_end()) {
            auto result = stream.read(buffer);
            REQUIRE(result.has_value());
            total_read += result.value();
            
            if (result.value() < buffer.size()) {
                break;
            }
        }
        
        CHECK(total_read == file_size);
    }
}
#endif

TEST_CASE("Stream polymorphic usage", "[unit][stream]") {
    auto test_data = create_test_data(512);
    
    SECTION("Using base class pointer") {
        // Test with memory_mapped_stream
        memory_mapped_stream mem_stream{std::span<const std::byte>(test_data)};
        input_stream* base_ptr = &mem_stream;
        
        std::array<std::byte, 100> buffer{};
        CHECK(base_ptr->read(buffer).value() == 100);
        CHECK(base_ptr->skip(100).has_value());
        CHECK_FALSE(base_ptr->at_end());
    }
    
    SECTION("Random access through base") {
        memory_mapped_stream mem_stream{std::span<const std::byte>(test_data)};
        random_access_stream* ra_ptr = &mem_stream;
        
        CHECK(ra_ptr->seek(200).has_value());
        CHECK(ra_ptr->position() == 200);
        CHECK(ra_ptr->size().value() == 512);
    }
}

TEST_CASE("Stream concurrent access patterns", "[unit][stream]") {
    auto test_data = create_test_data(1024);
    
    SECTION("Multiple reads from same position") {
        memory_mapped_stream stream{std::span<const std::byte>(test_data)};
        
        // Read 1
        std::array<std::byte, 50> buffer1{};
        CHECK(stream.read(buffer1).value() == 50);
        
        // Seek back
        CHECK(stream.seek(0).has_value());
        
        // Read 2 - should get same data
        std::array<std::byte, 50> buffer2{};
        CHECK(stream.read(buffer2).value() == 50);
        
        CHECK(std::memcmp(buffer1.data(), buffer2.data(), 50) == 0);
    }
    
    SECTION("Interleaved skip and read") {
        memory_mapped_stream stream{std::span<const std::byte>(test_data)};
        
        for (int i = 0; i < 10; ++i) {
            std::array<std::byte, 10> buffer{};
            CHECK(stream.read(buffer).value() == 10);
            CHECK(stream.skip(90).has_value());
        }
        
        CHECK(stream.position() == 1000);
    }
}