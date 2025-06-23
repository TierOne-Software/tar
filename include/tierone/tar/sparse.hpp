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

#include <tierone/tar/error.hpp>
#include <tierone/tar/stream.hpp>
#include <vector>
#include <cstdint>
#include <optional>
#include <expected>
#include <map>
#include <string>

namespace tierone::tar {
    struct ustar_header;
}

namespace tierone::tar::sparse {

// Represents a data segment in a sparse file
struct sparse_entry {
    uint64_t offset;  // Offset in the logical file
    uint64_t size;    // Size of the data segment
};

// GNU sparse file header layout - they reuse parts of the standard header
struct gnu_sparse_header {
    // Standard ustar header (first 329 bytes)
    char name[100];        // 0-99
    char mode[8];          // 100-107
    char uid[8];           // 108-115  
    char gid[8];           // 116-123
    char size[12];         // 124-135 (sparse data size, not real file size)
    char mtime[12];        // 136-147
    char checksum[8];      // 148-155
    char typeflag;         // 156 ('S' for sparse)
    char linkname[100];    // 157-256
    char magic[6];         // 257-262
    char version[2];       // 263-264
    char uname[32];        // 265-296
    char gname[32];        // 297-328
    char devmajor[8];      // 329-336
    char devminor[8];      // 337-344
    
    // GNU sparse uses part of the prefix field for a sparse map (starts at 345)
    char prefix_part1[39]; // 345-383 (first part of the prefix still available)
    
    // Sparse map starts at offset 384 (0x180)
    struct sparse_entry {
        char offset[12];
        char numbytes[12];
    } sp[4];               // 384-479 (4 entries * 24 bytes = 96 bytes)
    char isextended;       // 480
    char realsize[12];     // 481-492 (real file size)
    char pad2[19];         // 493-511
};

static_assert(sizeof(gnu_sparse_header) == 512, "GNU sparse header must be exactly 512 bytes");

struct gnu_sparse_header_1_0 {
    // GNU.sparse.major/minor version
    // GNU.sparse.name - real file name
    // GNU.sparse.realsize - actual file size
    // GNU.sparse.map - sparse map data
};

// Sparse file metadata
struct sparse_metadata {
    uint64_t real_size = 0;  // Actual size of the file
    std::vector<sparse_entry> segments;  // Non-zero data segments
    
    // Calculate total data size (sum of all segments)
    [[nodiscard]] uint64_t total_data_size() const noexcept {
        uint64_t total = 0;
        for (const auto& seg : segments) {
            total += seg.size;
        }
        return total;
    }
    
    // Check if a given offset falls within a data segment
    [[nodiscard]] std::optional<size_t> find_segment(const uint64_t offset) const noexcept {
        for (size_t i = 0; i < segments.size(); ++i) {
            const auto&[segoffset, size] = segments[i];
            if (offset >= segoffset && offset < segoffset + size) {
                return i;
            }
        }
        return std::nullopt;
    }
};

// Parse old GNU sparse format from header
[[nodiscard]] std::expected<sparse_metadata, error> parse_old_sparse_header(
    const ustar_header& header
);

// Parse GNU sparse 1.0 format from PAX extended headers
[[nodiscard]] std::expected<sparse_metadata, error> parse_sparse_1_0_header(
    const std::map<std::string, std::string>& pax_headers
);

// Parse GNU sparse 1.0 format sparse map from file data block
[[nodiscard]] std::expected<sparse_metadata, error> parse_sparse_1_0_data_map(
    input_stream& stream,
    uint64_t real_size
);

// Read sparse map continuation from archive stream
[[nodiscard]] std::expected<std::vector<sparse_entry>, error> read_sparse_map_continuation(
    input_stream& stream
);

} // namespace tierone::tar::sparse