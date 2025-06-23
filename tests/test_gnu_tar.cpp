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
#include <tierone/tar/gnu_tar.hpp>
#include <tierone/tar/header_parser.hpp>
#include <tierone/tar/stream.hpp>
#include <array>
#include <cstring>

using namespace tierone::tar;

std::array<std::byte, 512> create_gnu_longname_header(const std::string& longname) {
    std::array<std::byte, 512> block{};
    auto* header = std::bit_cast<ustar_header*>(block.data());
    
    // Create a GNU longname entry
    std::strncpy(header->name, "././@LongLink", sizeof(header->name));
    std::strncpy(header->mode, "0000000", sizeof(header->mode));
    std::strncpy(header->uid, "0000000", sizeof(header->uid));
    std::strncpy(header->gid, "0000000", sizeof(header->gid));
    
    // Size is the length of the longname + null terminator
    size_t size = longname.length() + 1;
    std::snprintf(header->size, sizeof(header->size), "%011o", static_cast<unsigned>(size));
    std::strncpy(header->mtime, "00000000000", sizeof(header->mtime));
    
    header->typeflag = 'L';  // GNU longname
    std::strncpy(header->magic, "ustar ", sizeof(header->magic));
    std::strncpy(header->version, " ", sizeof(header->version));
    
    // Calculate and set checksum
    uint32_t checksum = detail::calculate_checksum(block);
    std::snprintf(header->checksum, sizeof(header->checksum), "%06o ", checksum);
    
    return block;
}

std::array<std::byte, 512> create_gnu_longlink_header(const std::string& longlink) {
    std::array<std::byte, 512> block{};
    auto* header = std::bit_cast<ustar_header*>(block.data());
    
    // Create a GNU longlink entry
    std::strncpy(header->name, "././@LongLink", sizeof(header->name));
    std::strncpy(header->mode, "0000000", sizeof(header->mode));
    std::strncpy(header->uid, "0000000", sizeof(header->uid));
    std::strncpy(header->gid, "0000000", sizeof(header->gid));
    
    // Size is the length of the longlink + null terminator
    size_t size = longlink.length() + 1;
    std::snprintf(header->size, sizeof(header->size), "%011o", static_cast<unsigned>(size));
    std::strncpy(header->mtime, "00000000000", sizeof(header->mtime));
    
    header->typeflag = 'K';  // GNU longlink
    std::strncpy(header->magic, "ustar ", sizeof(header->magic));
    std::strncpy(header->version, " ", sizeof(header->version));
    
    // Calculate and set checksum
    uint32_t checksum = detail::calculate_checksum(block);
    std::snprintf(header->checksum, sizeof(header->checksum), "%06o ", checksum);
    
    return block;
}

std::vector<std::byte> create_data_blocks(const std::string& data) {
    std::vector<std::byte> blocks;
    
    // Calculate how many 512-byte blocks we need
    size_t blocks_needed = (data.length() + 512) / 512;
    blocks.resize(blocks_needed * 512, std::byte{0});
    
    // Copy the string data
    for (size_t i = 0; i < data.length(); ++i) {
        blocks[i] = static_cast<std::byte>(data[i]);
    }
    
    return blocks;
}

class mock_gnu_stream : public input_stream {
private:
    std::vector<std::byte> data_;
    size_t position_ = 0;

public:
    explicit mock_gnu_stream(std::vector<std::byte> data) : data_(std::move(data)) {}

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

TEST_CASE("GNU tar magic detection", "[gnu_tar]") {
    CHECK(gnu::is_gnu_tar_magic("ustar "));
    CHECK(gnu::is_gnu_tar_magic("ustar"));
    CHECK_FALSE(gnu::is_gnu_tar_magic("posix"));
    CHECK_FALSE(gnu::is_gnu_tar_magic(""));
}

TEST_CASE("Parse GNU longname header", "[gnu_tar]") {
    std::string test_longname = "very/long/path/that/exceeds/the/normal/100/character/limit/imposed/by/posix/ustar/format.txt";
    auto block = create_gnu_longname_header(test_longname);
    
    auto result = detail::parse_header(block);
    REQUIRE(result.has_value());
    
    const auto& meta = *result;
    CHECK(meta.type == entry_type::gnu_longname);
    CHECK(meta.is_gnu_longname());
    CHECK(meta.is_gnu_extension());
    CHECK(meta.size == test_longname.length() + 1);
}

TEST_CASE("Parse GNU longlink header", "[gnu_tar]") {
    std::string test_longlink = "very/long/link/target/that/exceeds/normal/limits.txt";
    auto block = create_gnu_longlink_header(test_longlink);
    
    auto result = detail::parse_header(block);
    REQUIRE(result.has_value());
    
    const auto& meta = *result;
    CHECK(meta.type == entry_type::gnu_longlink);
    CHECK(meta.is_gnu_longlink());
    CHECK(meta.is_gnu_extension());
    CHECK(meta.size == test_longlink.length() + 1);
}

TEST_CASE("Read GNU extension data", "[gnu_tar]") {
    std::string test_data = "this/is/test/data/for/gnu/extension";
    auto data_blocks = create_data_blocks(test_data);
    
    mock_gnu_stream stream{data_blocks};
    
    auto result = gnu::read_gnu_extension_data(stream, test_data.length() + 1);
    REQUIRE(result.has_value());
    CHECK(*result == test_data);
}

TEST_CASE("Apply GNU extensions to metadata", "[gnu_tar]") {
    file_metadata meta;
    meta.path = "short_path.txt";
    meta.link_target = "short_link";
    
    gnu::gnu_extension_data extensions;
    extensions.longname = "very/long/path/name.txt";
    extensions.longlink = "very/long/link/target.txt";
    
    gnu::apply_gnu_extensions(meta, extensions);
    
    CHECK(meta.path == "very/long/path/name.txt");
    CHECK(meta.link_target == "very/long/link/target.txt");
}

TEST_CASE("GNU extension data management", "[gnu_tar]") {
    gnu::gnu_extension_data extensions;
    
    SECTION("Initially empty") {
        CHECK_FALSE(extensions.has_longname());
        CHECK_FALSE(extensions.has_longlink());
    }
    
    SECTION("After setting data") {
        extensions.longname = "test_name";
        extensions.longlink = "test_link";
        
        CHECK(extensions.has_longname());
        CHECK(extensions.has_longlink());
    }
    
    SECTION("After clearing") {
        extensions.longname = "test_name";
        extensions.longlink = "test_link";
        extensions.clear();
        
        CHECK_FALSE(extensions.has_longname());
        CHECK_FALSE(extensions.has_longlink());
    }
}