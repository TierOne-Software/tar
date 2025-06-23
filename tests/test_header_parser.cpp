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
#include <tierone/tar/header_parser.hpp>
#include <array>
#include <cstring>

using namespace tierone::tar;

std::array<std::byte, 512> create_test_header() {
    std::array<std::byte, 512> block{};
    auto* header = std::bit_cast<ustar_header*>(block.data());
    
    // Fill with valid test data
    std::strncpy(header->name, "test.txt", sizeof(header->name));
    std::strncpy(header->mode, "0000644", sizeof(header->mode));
    std::strncpy(header->uid, "0001000", sizeof(header->uid));
    std::strncpy(header->gid, "0001000", sizeof(header->gid));
    std::strncpy(header->size, "00000000010", sizeof(header->size));
    std::strncpy(header->mtime, "14371573624", sizeof(header->mtime));
    header->typeflag = '0';  // Regular file
    std::strncpy(header->magic, "ustar", sizeof(header->magic));
    std::strncpy(header->version, "00", sizeof(header->version));
    std::strncpy(header->uname, "testuser", sizeof(header->uname));
    std::strncpy(header->gname, "testgroup", sizeof(header->gname));
    
    // Calculate and set checksum
    uint32_t checksum = detail::calculate_checksum(block);
    std::snprintf(header->checksum, sizeof(header->checksum), "%06o ", checksum);
    
    return block;
}

TEST_CASE("Parse valid octal field", "[header_parser]") {
    std::array<const char, 8> octal_field{'0', '0', '0', '6', '4', '4', ' ', '\0'};
    auto result = detail::parse_octal(std::span{octal_field});
    
    REQUIRE(result.has_value());
    CHECK(*result == 0644);
}

TEST_CASE("Parse invalid octal field", "[header_parser]") {
    std::array<const char, 8> octal_field{'0', '0', '0', '8', '4', '4', ' ', '\0'};  // '8' is invalid
    auto result = detail::parse_octal(std::span{octal_field});
    
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == error_code::invalid_header);
}

TEST_CASE("Parse empty octal field", "[header_parser]") {
    std::array<const char, 8> octal_field{' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'};
    auto result = detail::parse_octal(std::span{octal_field});
    
    REQUIRE(result.has_value());
    CHECK(*result == 0);
}

TEST_CASE("Parse valid header", "[header_parser]") {
    auto block = create_test_header();
    auto result = detail::parse_header(block);
    
    REQUIRE(result.has_value());
    
    const auto& meta = *result;
    CHECK(meta.path == "test.txt");
    CHECK(meta.type == entry_type::regular_file);
    CHECK(meta.size == 8);
    CHECK(meta.owner_name == "testuser");
    CHECK(meta.group_name == "testgroup");
}

TEST_CASE("Detect zero block", "[header_parser]") {
    std::array<std::byte, 512> zero_block{};
    CHECK(detail::is_zero_block(zero_block));
    
    auto test_block = create_test_header();
    CHECK_FALSE(detail::is_zero_block(test_block));
}

TEST_CASE("Calculate checksum", "[header_parser]") {
    auto block = create_test_header();
    uint32_t checksum = detail::calculate_checksum(block);
    
    // The checksum should be non-zero for a valid header
    CHECK(checksum > 0);
}

TEST_CASE("Extract string from field", "[header_parser]") {
    SECTION("With null terminator") {
        std::array<char, 10> field{'h', 'e', 'l', 'l', 'o', '\0', 'x', 'x', 'x', 'x'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result == "hello");
    }
    
    SECTION("Without null terminator") {
        std::array<char, 5> field{'h', 'e', 'l', 'l', 'o'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result == "hello");
    }
}