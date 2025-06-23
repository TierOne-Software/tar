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
#include <tierone/tar/archive_reader.hpp>
#include <tierone/tar/stream.hpp>
#include <sstream>
#include <vector>
#include <numeric>

using namespace tierone::tar;

// Mock stream for testing
class mock_stream : public input_stream {
private:
    std::vector<std::byte> data_;
    size_t position_ = 0;

public:
    explicit mock_stream(std::vector<std::byte> data) : data_(std::move(data)) {}

    std::expected<size_t, error> read(std::span<std::byte> buffer) override {
        size_t available = data_.size() - position_;
        size_t to_read = std::min(buffer.size(), available);
        
        std::ranges::copy_n(data_.begin() + position_, to_read, buffer.begin());
        position_ += to_read;
        
        return to_read;
    }

    std::expected<void, error> skip(size_t bytes) override {
        if (position_ + bytes > data_.size()) {
            return std::unexpected(error{error_code::io_error, "Skip past end"});
        }
        position_ += bytes;
        return {};
    }

    bool at_end() const override {
        return position_ >= data_.size();
    }
};

TEST_CASE("Memory mapped stream", "[stream]") {
    std::vector<std::byte> test_data(1024);
    for (size_t i = 0; i < test_data.size(); ++i) {
        test_data[i] = static_cast<std::byte>(i & 0xFF);
    }
    
    memory_mapped_stream stream{test_data};
    
    SECTION("Read data") {
        std::array<std::byte, 10> buffer;
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(*result == 10);
        CHECK(buffer[0] == std::byte{0});
        CHECK(buffer[9] == std::byte{9});
    }
    
    SECTION("Seek and read") {
        auto seek_result = stream.seek(100);
        REQUIRE(seek_result.has_value());
        
        std::array<std::byte, 5> buffer;
        auto read_result = stream.read(buffer);
        
        REQUIRE(read_result.has_value());
        CHECK(*read_result == 5);
        CHECK(buffer[0] == std::byte{100});
    }
    
    SECTION("Skip") {
        auto skip_result = stream.skip(50);
        REQUIRE(skip_result.has_value());
        
        CHECK(stream.position() == 50);
    }
}

TEST_CASE("Mock stream basic functionality", "[stream]") {
    std::vector<std::byte> test_data{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    mock_stream stream{test_data};
    
    SECTION("Read all data") {
        std::array<std::byte, 4> buffer;
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(*result == 4);
        CHECK(buffer[0] == std::byte{1});
        CHECK(buffer[3] == std::byte{4});
    }
    
    SECTION("Read partial data") {
        std::array<std::byte, 2> buffer;
        auto result = stream.read(buffer);
        
        REQUIRE(result.has_value());
        CHECK(*result == 2);
        CHECK(buffer[0] == std::byte{1});
        CHECK(buffer[1] == std::byte{2});
    }
}