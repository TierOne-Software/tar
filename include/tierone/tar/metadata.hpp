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

#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <map>
#include <vector>
#include <tierone/tar/sparse.hpp>

namespace tierone::tar {

// POSIX ACL entry
struct acl_entry {
    enum class type : uint8_t {
        user = 1,
        group = 2,
        mask = 4,
        other = 8,
        user_obj = 16,
        group_obj = 32
    };
    
    enum class perm : uint8_t {
        read = 4,
        write = 2,
        execute = 1
    };
    
    type entry_type;
    uint32_t id = 0;  // uid/gid (not used for USER_OBJ, GROUP_OBJ, MASK, OTHER)
    perm permissions;
    std::string name;  // username/groupname (optional)
};

// Extended attributes map
using extended_attributes = std::map<std::string, std::string>;

enum class entry_type : char {
    regular_file = '0',
    regular_file_old = '\0',  // Old tar format
    hard_link = '1',
    symbolic_link = '2',
    character_device = '3',
    block_device = '4',
    directory = '5',
    fifo = '6',
    contiguous_file = '7',
    // PAX extensions
    pax_extended_header = 'x',
    pax_global_header = 'g',
    // GNU extensions
    gnu_longname = 'L',
    gnu_longlink = 'K',
    // Additional GNU types for future support
    gnu_sparse = 'S',
    gnu_volhdr = 'V',
    gnu_multivol = 'M'
};

struct file_metadata {
    std::filesystem::path path;
    entry_type type = entry_type::regular_file;
    std::filesystem::perms permissions = std::filesystem::perms::owner_read;
    uint32_t owner_id = 0;
    uint32_t group_id = 0;
    uint64_t size = 0;
    std::chrono::system_clock::time_point modification_time;
    std::string owner_name;
    std::string group_name;
    std::optional<std::string> link_target;
    
    // Device numbers (for character and block devices)
    uint32_t device_major = 0;
    uint32_t device_minor = 0;
    
    // Sparse file information (if applicable)
    std::optional<sparse::sparse_metadata> sparse_info;
    
    // Extended attributes (xattr)
    extended_attributes xattrs;
    
    // POSIX ACLs
    std::vector<acl_entry> access_acl;
    std::vector<acl_entry> default_acl;

    [[nodiscard]] bool is_regular_file() const noexcept {
        return type == entry_type::regular_file || type == entry_type::regular_file_old;
    }

    [[nodiscard]] bool is_directory() const noexcept {
        return type == entry_type::directory;
    }

    [[nodiscard]] bool is_symbolic_link() const noexcept {
        return type == entry_type::symbolic_link;
    }

    [[nodiscard]] bool is_hard_link() const noexcept {
        return type == entry_type::hard_link;
    }

    [[nodiscard]] bool is_gnu_longname() const noexcept {
        return type == entry_type::gnu_longname;
    }

    [[nodiscard]] bool is_gnu_longlink() const noexcept {
        return type == entry_type::gnu_longlink;
    }

    [[nodiscard]] bool is_gnu_extension() const noexcept {
        return is_gnu_longname() || is_gnu_longlink() || 
               type == entry_type::gnu_sparse ||
               type == entry_type::gnu_volhdr ||
               type == entry_type::gnu_multivol;
    }
    
    [[nodiscard]] bool is_pax_header() const noexcept {
        return type == entry_type::pax_extended_header ||
               type == entry_type::pax_global_header;
    }
    
    [[nodiscard]] bool is_sparse() const noexcept {
        return type == entry_type::gnu_sparse;
    }
    
    [[nodiscard]] bool is_character_device() const noexcept {
        return type == entry_type::character_device;
    }
    
    [[nodiscard]] bool is_block_device() const noexcept {
        return type == entry_type::block_device;
    }
    
    [[nodiscard]] bool is_device() const noexcept {
        return is_character_device() || is_block_device();
    }
    
    [[nodiscard]] bool has_extended_attributes() const noexcept {
        return !xattrs.empty();
    }
    
    [[nodiscard]] bool has_acls() const noexcept {
        return !access_acl.empty() || !default_acl.empty();
    }
};

// POSIX ustar header layout (512 bytes)
struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];      // "ustar\0"
    char version[2];    // "00"
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

static_assert(sizeof(ustar_header) == 512, "POSIX ustar header must be exactly 512 bytes");

} // namespace tierone::tar