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

/**
 * extended_metadata_demo - Extracts and displays extended metadata including device files, extended attributes, and POSIX ACLs.
 * 
 * Usage: ./extended_metadata_demo <tar_file>
 * 
 * Features demonstrated:
 * - Device file information (major/minor numbers)
 * - Extended attributes (xattr)
 * - POSIX ACLs parsing and display
 * - Comprehensive entry type detection
 * 
 * Creating test archives with extended metadata:
 * # Create a file with extended attributes
 * touch test_file
 * setfattr -n user.comment -v "test attribute" test_file
 * 
 * # Create a file with ACLs
 * setfacl -m u:1000:rw test_file
 * 
 * # Create device files (requires root)
 * sudo mknod test_char c 1 3
 * sudo mknod test_block b 8 0
 * 
 * # Create archive preserving all metadata
 * tar --xattrs --acls -cf metadata.tar test_file test_char test_block
 */

#include <tierone/tar/tar.hpp>
#include <iostream>
#include <iomanip>
#include <print>

void print_device_info(const tierone::tar::archive_entry& entry) {
    if (entry.is_device()) {
        std::print("  Device: {}:{}", entry.device_major(), entry.device_minor());
        if (entry.is_character_device()) {
            std::print(" (character device)");
        } else if (entry.is_block_device()) {
            std::print(" (block device)");
        }
        std::println("");
    }
}

void print_extended_attributes(const tierone::tar::archive_entry& entry) {
    if (entry.has_extended_attributes()) {
        std::println("  Extended Attributes:");
        for (const auto& [name, value] : entry.get_extended_attributes()) {
            std::println("    {} = \"{}\"", name, value);
        }
    }
}

void print_acl_entry(const tierone::tar::acl_entry& acl) {
    // Print entry type
    switch (acl.entry_type) {
        case tierone::tar::acl_entry::type::user_obj:
            std::print("user::");
            break;
        case tierone::tar::acl_entry::type::group_obj:
            std::print("group::");
            break;
        case tierone::tar::acl_entry::type::user:
            std::print("user:{}:", acl.id);
            break;
        case tierone::tar::acl_entry::type::group:
            std::print("group:{}:", acl.id);
            break;
        case tierone::tar::acl_entry::type::mask:
            std::print("mask::");
            break;
        case tierone::tar::acl_entry::type::other:
            std::print("other::");
            break;
    }
    
    // Print permissions
    uint8_t perms = static_cast<uint8_t>(acl.permissions);
    std::print("{}{}{}", 
        ((perms & 4) ? 'r' : '-'), 
        ((perms & 2) ? 'w' : '-'), 
        ((perms & 1) ? 'x' : '-'));
}

void print_acls(const tierone::tar::archive_entry& entry) {
    if (entry.has_acls()) {
        std::println("  POSIX ACLs:");
        
        if (!entry.access_acl().empty()) {
            std::print("    Access ACL: ");
            bool first = true;
            for (const auto& acl : entry.access_acl()) {
                if (!first) std::print(",");
                print_acl_entry(acl);
                first = false;
            }
            std::println("");
        }
        
        if (!entry.default_acl().empty()) {
            std::print("    Default ACL: ");
            bool first = true;
            for (const auto& acl : entry.default_acl()) {
                if (!first) std::print(",");
                print_acl_entry(acl);
                first = false;
            }
            std::println("");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <tar_file>", argv[0]);
        std::println(stderr, "\nThis tool demonstrates extended metadata extraction including:");
        std::println(stderr, "  - Device major/minor numbers");
        std::println(stderr, "  - Extended attributes (xattr)");
        std::println(stderr, "  - POSIX ACLs");
        return 1;
    }
    
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    std::println("Extended Metadata Analysis for: {}", argv[1]);
    std::println("================================================================\n");
    
    size_t entry_count = 0;
    size_t device_count = 0;
    size_t xattr_count = 0;
    size_t acl_count = 0;
    
    for (const auto& entry : *reader) {
        entry_count++;
        
        std::println("Entry: {}", entry.path().string());
        std::print("  Type: ");
        
        switch (entry.type()) {
            case tierone::tar::entry_type::regular_file:
            case tierone::tar::entry_type::regular_file_old:
                std::print("Regular file");
                break;
            case tierone::tar::entry_type::directory:
                std::print("Directory");
                break;
            case tierone::tar::entry_type::symbolic_link:
                std::print("Symbolic link");
                if (entry.link_target()) {
                    std::print(" -> {}", *entry.link_target());
                }
                break;
            case tierone::tar::entry_type::hard_link:
                std::print("Hard link");
                if (entry.link_target()) {
                    std::print(" -> {}", *entry.link_target());
                }
                break;
            case tierone::tar::entry_type::character_device:
                std::print("Character device");
                device_count++;
                break;
            case tierone::tar::entry_type::block_device:
                std::print("Block device");
                device_count++;
                break;
            case tierone::tar::entry_type::fifo:
                std::print("FIFO");
                break;
            default:
                std::print("Other ({})", static_cast<char>(entry.type()));
                break;
        }
        std::println("");
        
        std::println("  Size: {} bytes", entry.size());
        std::println("  Owner: {} ({})", entry.owner_name(), entry.owner_id());
        std::println("  Group: {} ({})", entry.group_name(), entry.group_id());
        
        // Print device information
        print_device_info(entry);
        
        // Print extended attributes
        if (entry.has_extended_attributes()) {
            xattr_count++;
            print_extended_attributes(entry);
        }
        
        // Print ACLs
        if (entry.has_acls()) {
            acl_count++;
            print_acls(entry);
        }
        
        std::println("");
    }
    
    std::println("================================================================");
    std::println("Summary:");
    std::println("  Total entries: {}", entry_count);
    std::println("  Device files: {}", device_count);
    std::println("  Files with extended attributes: {}", xattr_count);
    std::println("  Files with ACLs: {}", acl_count);
    
    if (device_count == 0 && xattr_count == 0 && acl_count == 0) {
        std::println("\nNo extended metadata found in this archive.");
        std::println("To test extended metadata features, create archives with:");
        std::println("  - Device files: sudo mknod /tmp/testdev c 1 5 && tar -cf test.tar /tmp/testdev");
        std::println("  - Extended attributes: setfattr -n user.test -v \"value\" file && tar --xattrs -cf test.tar file");
        std::println("  - ACLs: setfacl -m u:1000:rw file && tar --acls -cf test.tar file");
    }
    
    return 0;
}