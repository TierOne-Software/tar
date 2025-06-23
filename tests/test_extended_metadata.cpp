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
#include <tierone/tar/metadata.hpp>
#include <tierone/tar/pax_parser.hpp>
#include <tierone/tar/header_parser.hpp>
#include <cstring>
#include <iostream>

using namespace tierone::tar;

// Helper function to calculate and set checksum for test headers
void calculate_checksum(ustar_header& header) {
    // Initialize checksum field with spaces
    std::memset(header.checksum, ' ', 8);
    
    // Calculate checksum (sum of all bytes)
    unsigned int checksum = 0;
    const char* header_bytes = reinterpret_cast<const char*>(&header);
    for (size_t i = 0; i < sizeof(header); ++i) {
        checksum += static_cast<unsigned char>(header_bytes[i]);
    }
    
    // Set checksum field (6-digit octal, space-padded, no null terminator)
    // Use a temporary buffer to avoid overwriting adjacent fields
    char temp[9];  // Make buffer bigger to avoid truncation warning
    std::snprintf(temp, sizeof(temp), "%06o ", checksum);
    std::memcpy(header.checksum, temp, 7);  // Copy only 7 characters, leave 8th as space
    header.checksum[7] = ' ';  // Ensure last char is space, not null terminator
}

TEST_CASE("Device number parsing", "[metadata][device]") {
    SECTION("Character device parsing") {
        // Create a mock ustar header for a character device
        ustar_header header = {};
        
        // Set up header fields
        std::strcpy(header.name, "dev_char");
        std::strcpy(header.mode, "0000644");
        std::strcpy(header.uid, "0000000");
        std::strcpy(header.gid, "0000000");
        std::strcpy(header.size, "00000000000");
        std::strcpy(header.mtime, "00000000000");
        std::memcpy(header.magic, "ustar\0", 6);  // Proper ustar magic with null termination
        std::strcpy(header.version, "00");
        
        // Set device numbers
        std::strcpy(header.devmajor, "0000005");  // major = 5
        std::strcpy(header.devminor, "0000001");  // minor = 1
        
        // Set type flag (MUST be after zero initialization)
        header.typeflag = static_cast<char>(entry_type::character_device);
        
        // Calculate checksum
        calculate_checksum(header);
        
        // Create properly sized block
        std::byte block[512] = {};
        std::memcpy(block, &header, sizeof(header));
        
        auto result = detail::parse_header(std::span<const std::byte, 512>{block});
        
        REQUIRE(result.has_value());
        
        CHECK(result->type == entry_type::character_device);
        CHECK(result->is_character_device());
        CHECK(result->is_device());
        CHECK(result->device_major == 5);
        CHECK(result->device_minor == 1);
    }
    
    SECTION("Block device parsing") {
        ustar_header header = {};
        
        std::strcpy(header.name, "dev_block");
        std::strcpy(header.mode, "0000644");
        std::strcpy(header.uid, "0000000");
        std::strcpy(header.gid, "0000000");
        std::strcpy(header.size, "00000000000");
        std::strcpy(header.mtime, "00000000000");
        std::memcpy(header.magic, "ustar\0", 6);  // Proper ustar magic with null termination
        std::strcpy(header.version, "00");
        
        // Set device numbers with larger values
        std::strcpy(header.devmajor, "0000010");  // major = 8 in decimal (010 octal = 8 decimal)
        std::strcpy(header.devminor, "0000001");  // minor = 1 (typical for /dev/sda1)
        
        // Set type flag (MUST be after zero initialization)
        header.typeflag = static_cast<char>(entry_type::block_device);
        
        // Calculate checksum
        calculate_checksum(header);
        
        // Create properly sized block
        std::byte block[512] = {};
        std::memcpy(block, &header, sizeof(header));
        
        auto result = detail::parse_header(std::span<const std::byte, 512>{block});
        
        REQUIRE(result.has_value());
        
        
        CHECK(result->type == entry_type::block_device);
        CHECK(result->is_block_device());
        CHECK(result->is_device());
        CHECK(result->device_major == 8);
        CHECK(result->device_minor == 1);
    }
    
    SECTION("Regular file should have zero device numbers") {
        ustar_header header = {};
        
        std::strcpy(header.name, "regular.txt");
        std::strcpy(header.mode, "0000644");
        std::strcpy(header.uid, "0000000");
        std::strcpy(header.gid, "0000000");
        std::strcpy(header.size, "00000000100");
        std::strcpy(header.mtime, "00000000000");
        std::memcpy(header.magic, "ustar\0", 6);  // Proper ustar magic with null termination
        std::strcpy(header.version, "00");
        
        // Device fields might have garbage, but should be ignored for regular files
        std::strcpy(header.devmajor, "0000999");
        std::strcpy(header.devminor, "0000888");
        
        // Set type flag (MUST be after zero initialization)
        header.typeflag = static_cast<char>(entry_type::regular_file);
        
        // Calculate checksum
        calculate_checksum(header);
        
        // Create properly sized block
        std::byte block[512] = {};
        std::memcpy(block, &header, sizeof(header));
        
        auto result = detail::parse_header(std::span<const std::byte, 512>{block});
        
        REQUIRE(result.has_value());
        CHECK(result->type == entry_type::regular_file);
        CHECK(result->is_regular_file());
        CHECK_FALSE(result->is_device());
        CHECK(result->device_major == 0);  // Should remain 0 for non-devices
        CHECK(result->device_minor == 0);
    }
}

TEST_CASE("Extended attributes parsing", "[metadata][xattr][pax]") {
    SECTION("SCHILY xattr format") {
        std::map<std::string, std::string> pax_headers = {
            {"SCHILY.xattr.user.author", "john.doe"},
            {"SCHILY.xattr.user.description", "Important document"},
            {"SCHILY.xattr.security.selinux", "system_u:object_r:user_home_t:s0"},
            {"SCHILY.xattr.trusted.md5sum", "d41d8cd98f00b204e9800998ecf8427e"},
            {"path", "document.txt"}  // Regular PAX header
        };
        
        auto xattrs = pax::extract_extended_attributes(pax_headers);
        
        CHECK(xattrs.size() == 4);
        CHECK(xattrs.at("user.author") == "john.doe");
        CHECK(xattrs.at("user.description") == "Important document");
        CHECK(xattrs.at("security.selinux") == "system_u:object_r:user_home_t:s0");
        CHECK(xattrs.at("trusted.md5sum") == "d41d8cd98f00b204e9800998ecf8427e");
    }
    
    SECTION("LIBARCHIVE xattr format") {
        std::map<std::string, std::string> pax_headers = {
            {"LIBARCHIVE.xattr.user.comment", "test comment"},
            {"LIBARCHIVE.xattr.system.backup", "yes"},
            {"size", "1024"}  // Regular PAX header
        };
        
        auto xattrs = pax::extract_extended_attributes(pax_headers);
        
        CHECK(xattrs.size() == 2);
        CHECK(xattrs.at("user.comment") == "test comment");
        CHECK(xattrs.at("system.backup") == "yes");
    }
    
    SECTION("No extended attributes") {
        std::map<std::string, std::string> pax_headers = {
            {"path", "regular.txt"},
            {"size", "100"}
        };
        
        auto xattrs = pax::extract_extended_attributes(pax_headers);
        
        CHECK(xattrs.empty());
    }
}

TEST_CASE("POSIX ACL parsing", "[metadata][acl][pax]") {
    SECTION("Simple ACL parsing") {
        std::string acl_text = "user::rwx,group::r-x,other::r--,user:1000:rw-,mask::rwx";
        
        auto result = pax::parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 5);
        
        // Check user:: entry
        const auto& user_obj = (*result)[0];
        CHECK(user_obj.entry_type == acl_entry::type::user_obj);
        CHECK(user_obj.id == 0);  // Not used for user_obj
        CHECK(static_cast<uint8_t>(user_obj.permissions) == 7);  // rwx = 4+2+1 = 7
        
        // Check user:1000: entry
        const auto& user_1000 = (*result)[3];
        CHECK(user_1000.entry_type == acl_entry::type::user);
        CHECK(user_1000.id == 1000);
        CHECK(static_cast<uint8_t>(user_1000.permissions) == 6);  // rw- = 4+2 = 6
    }
    
    SECTION("ACL extraction from PAX headers") {
        std::map<std::string, std::string> pax_headers = {
            {"SCHILY.acl.access", "user::rwx,group::r-x,other::r--,user:1000:rw-"},
            {"SCHILY.acl.default", "user::rwx,group::r-x,other::r--"},
            {"path", "test_dir/"}
        };
        
        auto [access_acl, default_acl] = pax::extract_acls(pax_headers);
        
        CHECK(access_acl.size() == 4);
        CHECK(default_acl.size() == 3);
        
        // Check that we have the user:1000 entry in access ACL
        bool found_user_1000 = false;
        for (const auto& entry : access_acl) {
            if (entry.entry_type == acl_entry::type::user && entry.id == 1000) {
                found_user_1000 = true;
                CHECK(static_cast<uint8_t>(entry.permissions) == 6);  // rw-
                break;
            }
        }
        CHECK(found_user_1000);
    }
    
    SECTION("Invalid ACL format") {
        std::string invalid_acl = "invalid:format:here:too:many:colons";
        
        auto result = pax::parse_acl_text(invalid_acl);
        
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Invalid permission format") {
        std::string invalid_perms = "user::invalid";
        
        auto result = pax::parse_acl_text(invalid_perms);
        
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
}

TEST_CASE("ACL entry types and permissions", "[metadata][acl]") {
    SECTION("ACL permission flags") {
        using perm = acl_entry::perm;
        
        CHECK(static_cast<uint8_t>(perm::read) == 4);
        CHECK(static_cast<uint8_t>(perm::write) == 2);
        CHECK(static_cast<uint8_t>(perm::execute) == 1);
        
        // Test combinations
        uint8_t rwx = static_cast<uint8_t>(perm::read) | 
                     static_cast<uint8_t>(perm::write) | 
                     static_cast<uint8_t>(perm::execute);
        CHECK(rwx == 7);
        
        uint8_t rw = static_cast<uint8_t>(perm::read) | 
                    static_cast<uint8_t>(perm::write);
        CHECK(rw == 6);
    }
    
    SECTION("ACL entry types") {
        using type = acl_entry::type;
        
        CHECK(static_cast<uint8_t>(type::user) == 1);
        CHECK(static_cast<uint8_t>(type::group) == 2);
        CHECK(static_cast<uint8_t>(type::mask) == 4);
        CHECK(static_cast<uint8_t>(type::other) == 8);
        CHECK(static_cast<uint8_t>(type::user_obj) == 16);
        CHECK(static_cast<uint8_t>(type::group_obj) == 32);
    }
}