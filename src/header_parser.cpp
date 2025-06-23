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

#include <tierone/tar/header_parser.hpp>
#include <tierone/tar/gnu_tar.hpp>
#include <tierone/tar/sparse.hpp>
#include <algorithm>
#include <cstring>
#include <ranges>
#include <utility>
#include <utility>

namespace tierone::tar::detail {

uint32_t calculate_checksum(std::span<const std::byte, BLOCK_SIZE> block) {
    uint32_t sum = 0;
    
    // Create a copy to zero out the checksum field
    std::array<std::byte, BLOCK_SIZE> temp_block{};
    std::ranges::copy(block, temp_block.begin());
    
    // Zero out checksum field (bytes 148-155) and fill with spaces
    auto* header = std::bit_cast<ustar_header*>(temp_block.data());
    std::ranges::fill(std::span{header->checksum}, ' ');
    
    // Calculate sum
    for (auto byte : temp_block) {
        sum += static_cast<uint8_t>(byte);
    }
    
    return sum;
}

std::string_view extract_string(std::span<const char> field) {
    const auto null_pos = std::ranges::find(field, '\0');
    const size_t length = null_pos != field.end() ?
        static_cast<size_t>(std::distance(field.begin(), null_pos)) : 
        field.size();
    return std::string_view{field.data(), length};
}

bool is_zero_block(std::span<const std::byte, BLOCK_SIZE> block) {
    return std::ranges::all_of(block, [](auto b) { return b == std::byte{0}; });
}

auto parse_header(std::span<const std::byte, BLOCK_SIZE> block) -> std::expected<file_metadata, error> {
    const auto* header = std::bit_cast<const ustar_header*>(block.data());
    
    // Verify magic number for POSIX ustar or GNU tar format
    std::string_view magic = extract_string(std::span{header->magic, 6});
    bool is_ustar = (magic == "ustar");
    bool is_gnu = gnu::is_gnu_tar_magic(magic);
    
    if (!is_ustar && !is_gnu) {
        return std::unexpected(error{error_code::invalid_header, 
            "Not a POSIX ustar or GNU tar archive (magic: '" + std::string{magic} + "')"});
    }
    
    // Verify version
    std::string_view version = extract_string(std::span{header->version, 2});
    if (version != "00" && version != " ") {  // Some implementations use space-padded
        return std::unexpected(error{error_code::invalid_header, "Unsupported tar version"});
    }
    
    // Parse and verify checksum
    auto stored_checksum = parse_octal(std::span{header->checksum});
    if (!stored_checksum) {
        return std::unexpected(stored_checksum.error());
    }
    
    uint32_t calculated_checksum = calculate_checksum(block);
    if (calculated_checksum != *stored_checksum) {
        return std::unexpected(error{error_code::corrupt_archive, "Header checksum mismatch"});
    }
    
    // Parse numeric fields
    auto mode = parse_octal(std::span{header->mode});
    auto uid = parse_octal(std::span{header->uid});
    auto gid = parse_octal(std::span{header->gid});
    auto size = parse_octal(std::span{header->size});
    auto mtime = parse_octal(std::span{header->mtime});
    
    if (!mode || !uid || !gid || !size || !mtime) {
        return std::unexpected(error{error_code::invalid_header, "Failed to parse numeric fields"});
    }
    
    // Build metadata
    file_metadata meta;
    
    // Build path from prefix + name
    std::string_view prefix = extract_string(std::span{header->prefix});
    std::string_view name = extract_string(std::span{header->name});
    
    if (!prefix.empty()) {
        meta.path = std::filesystem::path{prefix} / name;
    } else {
        meta.path = name;
    }
    
    // Check for sparse file information in GNU tar format
    // GNU tar can store sparse files in two ways:
    // 1. Regular files with sparse data in padding area
    // 2. Files with type 'S' (GNU sparse)
    if (is_gnu) {
        if (header->typeflag == std::to_underlying(entry_type::regular_file) ||
            header->typeflag == std::to_underlying(entry_type::gnu_sparse)) {
            // Check if this is a sparse file by looking at the padding area
            // GNU sparse format uses header->padding for sparse entries
            auto sparse_result = sparse::parse_old_sparse_header(*header);
            if (sparse_result && !sparse_result->segments.empty()) {
                meta.sparse_info = std::move(*sparse_result);
                // For type 'S' files, convert them to regular files since we have the sparse info
                if (header->typeflag == std::to_underlying(entry_type::gnu_sparse)) {
                    meta.type = entry_type::regular_file;
                }
            }
        }
    }
    
    // Validate path
    if (meta.path.empty()) {
        return std::unexpected(error{error_code::invalid_header, "Empty file path"});
    }
    
    // Set metadata fields
    // Note: meta.type may have already been set in sparse detection logic above
    if (meta.sparse_info.has_value()) {
        // Type was already set correctly during sparse detection
    } else {
        meta.type = static_cast<entry_type>(header->typeflag);
    }
    meta.permissions = static_cast<std::filesystem::perms>(*mode & 07777);
    meta.owner_id = static_cast<uint32_t>(*uid);
    meta.group_id = static_cast<uint32_t>(*gid);
    meta.size = *size;
    meta.modification_time = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(*mtime));
    
    // Parse string fields
    std::string_view uname = extract_string(std::span{header->uname});
    std::string_view gname = extract_string(std::span{header->gname});
    meta.owner_name = std::string{uname};
    meta.group_name = std::string{gname};
    
    // Parse device numbers for character and block devices
    if (meta.type == entry_type::character_device || meta.type == entry_type::block_device) {
        auto major = parse_octal(std::span{header->devmajor});
        auto minor = parse_octal(std::span{header->devminor});
        if (major) {
            meta.device_major = static_cast<uint32_t>(*major);
        }
        if (minor) {
            meta.device_minor = static_cast<uint32_t>(*minor);
        }
    }
    
    // Handle links
    if (meta.type == entry_type::symbolic_link || meta.type == entry_type::hard_link) {
        std::string_view linkname = extract_string(std::span{header->linkname});
        if (!linkname.empty()) {
            meta.link_target = std::string{linkname};
        }
    }
    
    // Validate entry type
    switch (meta.type) {
        case entry_type::regular_file:
        case entry_type::regular_file_old:
        case entry_type::hard_link:
        case entry_type::symbolic_link:
        case entry_type::character_device:
        case entry_type::block_device:
        case entry_type::directory:
        case entry_type::fifo:
        case entry_type::contiguous_file:
        case entry_type::pax_extended_header:
        case entry_type::pax_global_header:
        case entry_type::gnu_longname:
        case entry_type::gnu_longlink:
        case entry_type::gnu_sparse:
        case entry_type::gnu_volhdr:
        case entry_type::gnu_multivol:
            break;
        default:
            return std::unexpected(error{error_code::unsupported_feature, 
                std::format("Unsupported entry type: {}", std::to_string(header->typeflag))});
    }
    
    return meta;
}

} // namespace tierone::tar::detail