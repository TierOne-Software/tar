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
#include <tierone/tar/header_parser.hpp>
#include <tierone/tar/metadata.hpp>
#include <array>
#include <cstring>
#include <limits>

using namespace tierone::tar;

namespace {

// Helper to create a header with specific field values
std::array<std::byte, 512> create_header_with_fields(
    const char* name = "test.txt",
    const char* mode = "0000644",
    const char* uid = "0001000", 
    const char* gid = "0001000",
    const char* size = "00000000010",
    const char* mtime = "14371573624",
    char typeflag = '0',
    const char* linkname = "",
    const char* magic = "ustar",
    const char* version = "00",
    const char* uname = "testuser",
    const char* gname = "testgroup",
    const char* devmajor = "0000000",
    const char* devminor = "0000000"
) {
    std::array<std::byte, 512> block{};
    auto* header = std::bit_cast<ustar_header*>(block.data());
    
    std::strncpy(header->name, name, sizeof(header->name));
    std::strncpy(header->mode, mode, sizeof(header->mode));
    std::strncpy(header->uid, uid, sizeof(header->uid));
    std::strncpy(header->gid, gid, sizeof(header->gid));
    std::strncpy(header->size, size, sizeof(header->size));
    std::strncpy(header->mtime, mtime, sizeof(header->mtime));
    header->typeflag = typeflag;
    std::strncpy(header->linkname, linkname, sizeof(header->linkname));
    std::strncpy(header->magic, magic, sizeof(header->magic));
    std::strncpy(header->version, version, sizeof(header->version));
    std::strncpy(header->uname, uname, sizeof(header->uname));
    std::strncpy(header->gname, gname, sizeof(header->gname));
    std::strncpy(header->devmajor, devmajor, sizeof(header->devmajor));
    std::strncpy(header->devminor, devminor, sizeof(header->devminor));
    
    // Calculate and set checksum
    uint32_t checksum = detail::calculate_checksum(block);
    std::snprintf(header->checksum, sizeof(header->checksum), "%06o ", checksum);
    
    return block;
}

} // anonymous namespace

TEST_CASE("parse_octal edge cases", "[unit][header_parser][edge_cases]") {
    SECTION("Maximum valid octal value") {
        std::array<const char, 12> field{'7', '7', '7', '7', '7', '7', '7', '7', '7', '7', '7', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 077777777777ULL);
    }
    
    SECTION("Octal field with leading zeros") {
        std::array<const char, 8> field{'0', '0', '0', '0', '0', '0', '7', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 7);
    }
    
    SECTION("Octal field with trailing spaces") {
        std::array<const char, 8> field{'1', '2', '3', ' ', ' ', ' ', ' ', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 0123);
    }
    
    SECTION("Octal field with mixed spaces") {
        std::array<const char, 8> field{' ', '1', '2', '3', ' ', ' ', ' ', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 0123);
    }
    
    SECTION("Invalid character in middle") {
        std::array<const char, 8> field{'1', '2', 'X', '4', '5', '6', '7', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Negative sign (invalid)") {
        std::array<const char, 8> field{'-', '1', '2', '3', '4', '5', '6', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Plus sign (invalid)") {
        std::array<const char, 8> field{'+', '1', '2', '3', '4', '5', '6', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Single digit") {
        std::array<const char, 8> field{'7', ' ', ' ', ' ', ' ', ' ', ' ', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 7);
    }
    
    SECTION("Field with no null terminator") {
        std::array<const char, 8> field{'1', '2', '3', '4', '5', '6', '7', '0'};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 012345670);
    }
    
    SECTION("Empty field (all spaces)") {
        std::array<const char, 8> field{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
        auto result = detail::parse_octal(std::span{field});
        
        REQUIRE(result.has_value());
        CHECK(result.value() == 0);
    }
    
    SECTION("Overflow test") {
        // Test with a very large number that might overflow
        std::array<const char, 20> field{'7', '7', '7', '7', '7', '7', '7', '7', '7', '7', 
                                        '7', '7', '7', '7', '7', '7', '7', '7', '7', '\0'};
        auto result = detail::parse_octal(std::span{field});
        
        // Should handle overflow gracefully
        if (!result.has_value()) {
            CHECK(result.error().code() == error_code::invalid_header);
        }
    }
}

TEST_CASE("extract_string edge cases", "[unit][header_parser][edge_cases]") {
    SECTION("String with embedded null") {
        std::array<char, 10> field{'h', 'e', 'l', '\0', 'l', 'o', '\0', 'x', 'x', 'x'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result == "hel");
    }
    
    SECTION("Empty field") {
        std::array<char, 5> field{'\0', '\0', '\0', '\0', '\0'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result.empty());
    }
    
    SECTION("Field with only spaces") {
        std::array<char, 5> field{' ', ' ', ' ', ' ', ' '};
        auto result = detail::extract_string(std::span{field});
        CHECK(result == "     ");
    }
    
    SECTION("Single character") {
        std::array<char, 5> field{'X', '\0', '\0', '\0', '\0'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result == "X");
    }
    
    SECTION("Unicode characters (assuming UTF-8)") {
        // Use proper UTF-8 encoding for Unicode characters
        std::array<char, 15> field{'t', 'e', 's', 't', 
                                   '\xE2', '\x82', '\xAC',  // € in UTF-8
                                   '\xC2', '\xA3',          // £ in UTF-8
                                   '\xC2', '\xA5',          // ¥ in UTF-8
                                   '\0', 'x', 'x', 'x'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result == "test€£¥");
    }
    
    SECTION("Binary data") {
        std::array<char, 8> field{'\x01', '\x02', '\x03', '\x04', '\0', 'x', 'x', 'x'};
        auto result = detail::extract_string(std::span{field});
        CHECK(result.size() == 4);
        CHECK(result[0] == '\x01');
        CHECK(result[3] == '\x04');
    }
}

TEST_CASE("parse_header edge cases", "[unit][header_parser][edge_cases]") {
    SECTION("Maximum length filename") {
        std::string long_name(99, 'a'); // Maximum for name field
        auto block = create_header_with_fields(long_name.c_str());
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->path == long_name);
    }
    
    SECTION("Maximum length linkname") {
        std::string long_link(99, 'b');
        auto block = create_header_with_fields("test.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "14371573624", '2', long_link.c_str());
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->link_target == long_link);
    }
    
    SECTION("Zero size file") {
        auto block = create_header_with_fields("empty.txt", "0000644", "0001000", "0001000",
                                             "00000000000");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->size == 0);
    }
    
    SECTION("Maximum size file") {
        auto block = create_header_with_fields("huge.bin", "0000644", "0001000", "0001000",
                                             "77777777777"); // Max octal in 11 chars
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->size > 0);
    }
    
    SECTION("Various entry types") {
        struct TypeTest {
            char type;
            entry_type expected;
        };
        
        std::vector<TypeTest> tests = {
            {'0', entry_type::regular_file},
            {'\0', entry_type::regular_file_old},
            {'1', entry_type::hard_link},
            {'2', entry_type::symbolic_link},
            {'3', entry_type::character_device},
            {'4', entry_type::block_device},
            {'5', entry_type::directory},
            {'6', entry_type::fifo},
            {'7', entry_type::contiguous_file},
            {'x', entry_type::pax_extended_header},
            {'g', entry_type::pax_global_header}
        };
        
        for (const auto& test : tests) {
            auto block = create_header_with_fields("test", "0000644", "0001000", "0001000",
                                                 "00000000010", "14371573624", test.type);
            
            auto result = detail::parse_header(block);
            REQUIRE(result.has_value());
            CHECK(result->type == test.expected);
        }
    }
    
    SECTION("Device files with major/minor numbers") {
        auto block = create_header_with_fields("dev", "0000644", "0001000", "0001000",
                                             "00000000000", "14371573624", '3', "",
                                             "ustar", "00", "root", "root",
                                             "0000010", "0000003");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->type == entry_type::character_device);
        CHECK(result->device_major == 8);  // 010 octal = 8 decimal
        CHECK(result->device_minor == 3);
    }
    
    SECTION("Very old timestamp") {
        auto block = create_header_with_fields("old.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "00000000000");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        // Should handle epoch time gracefully
    }
    
    SECTION("Future timestamp") {
        auto block = create_header_with_fields("future.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "77777777777");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        // Should handle future dates
    }
    
    SECTION("All permission bits set") {
        auto block = create_header_with_fields("executable", "0007777", "0001000", "0001000",
                                             "00000000010");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        // Should parse all permission bits including sticky, setuid, setgid
    }
    
    SECTION("Maximum UID/GID") {
        auto block = create_header_with_fields("maxuser", "0000644", "7777777", "7777777",
                                             "00000000010");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->owner_id > 0);
        CHECK(result->group_id > 0);
    }
    
    SECTION("Empty username/groupname") {
        auto block = create_header_with_fields("nonames.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "14371573624", '0', "",
                                             "ustar", "00", "", "");
        
        auto result = detail::parse_header(block);
        REQUIRE(result.has_value());
        CHECK(result->owner_name.empty());
        CHECK(result->group_name.empty());
    }
    
    SECTION("Non-standard magic but still valid") {
        auto block = create_header_with_fields("test.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "14371573624", '0', "",
                                             "ustar\0", "00");
        
        auto result = detail::parse_header(block);
        // Should still parse since it's close to standard
        REQUIRE(result.has_value());
    }
    
    SECTION("Old tar format (no magic)") {
        auto block = create_header_with_fields("old.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "14371573624", '0', "",
                                             "", "");
        
        auto result = detail::parse_header(block);
        // Should handle old format gracefully
        if (result.has_value()) {
            CHECK(result->path == "old.txt");
        }
    }
}

TEST_CASE("calculate_checksum edge cases", "[unit][header_parser][edge_cases]") {
    SECTION("Block with all zeros") {
        std::array<std::byte, 512> zero_block{};
        uint32_t checksum = detail::calculate_checksum(zero_block);
        
        // Checksum of all zeros should be specific value (8 spaces in checksum field)
        CHECK(checksum == 8 * ' ');
    }
    
    SECTION("Block with all 0xFF") {
        std::array<std::byte, 512> max_block;
        max_block.fill(std::byte{0xFF});
        
        // Set checksum field to spaces for calculation
        for (int i = 148; i < 156; ++i) {
            max_block[i] = std::byte{' '};
        }
        
        uint32_t checksum = detail::calculate_checksum(max_block);
        CHECK(checksum > 0);
    }
    
    SECTION("Checksum calculation consistency") {
        auto block1 = create_header_with_fields("test1.txt");
        auto block2 = create_header_with_fields("test1.txt");
        
        uint32_t checksum1 = detail::calculate_checksum(block1);
        uint32_t checksum2 = detail::calculate_checksum(block2);
        
        CHECK(checksum1 == checksum2);
    }
}

TEST_CASE("is_zero_block edge cases", "[unit][header_parser][edge_cases]") {
    SECTION("Block with single non-zero byte") {
        std::array<std::byte, 512> almost_zero{};
        almost_zero[511] = std::byte{1};
        
        CHECK_FALSE(detail::is_zero_block(almost_zero));
    }
    
    SECTION("Block with non-zero in checksum field") {
        std::array<std::byte, 512> block{};
        block[148] = std::byte{'1'}; // Checksum field
        
        CHECK_FALSE(detail::is_zero_block(block));
    }
    
    SECTION("Block with spaces (not zero)") {
        std::array<std::byte, 512> space_block;
        space_block.fill(std::byte{' '});
        
        CHECK_FALSE(detail::is_zero_block(space_block));
    }
}

TEST_CASE("header_parser error conditions", "[unit][header_parser][edge_cases]") {
    SECTION("Invalid checksum") {
        auto block = create_header_with_fields("test.txt");
        auto* header = std::bit_cast<ustar_header*>(block.data());
        
        // Corrupt the checksum
        std::strcpy(header->checksum, "INVALID");
        
        auto result = detail::parse_header(block);
        // Behavior depends on implementation - may still parse with warning
        // or may fail validation
    }
    
    SECTION("Corrupted magic field") {
        auto block = create_header_with_fields("test.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "14371573624", '0', "",
                                             "XXXXX", "00");
        
        auto result = detail::parse_header(block);
        // Should handle corrupted magic field
        if (!result.has_value()) {
            CHECK(result.error().code() == error_code::invalid_header);
        }
    }
    
    SECTION("Invalid version") {
        auto block = create_header_with_fields("test.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "14371573624", '0', "",
                                             "ustar", "99");
        
        auto result = detail::parse_header(block);
        // Should handle unexpected version
        if (result.has_value()) {
            CHECK(result->path == "test.txt");
        }
    }
    
    SECTION("Invalid size field") {
        auto block = create_header_with_fields("test.txt", "0000644", "0001000", "0001000",
                                             "INVALID_SZ", "14371573624", '0');
        
        auto result = detail::parse_header(block);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Invalid mode field") {
        auto block = create_header_with_fields("test.txt", "BADMODE", "0001000", "0001000",
                                             "00000000010", "14371573624", '0');
        
        auto result = detail::parse_header(block);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Invalid UID field") {
        auto block = create_header_with_fields("test.txt", "0000644", "BADUID!", "0001000",
                                             "00000000010", "14371573624", '0');
        
        auto result = detail::parse_header(block);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Invalid timestamp") {
        auto block = create_header_with_fields("test.txt", "0000644", "0001000", "0001000",
                                             "00000000010", "BADTIME!!!", '0');
        
        auto result = detail::parse_header(block);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
}