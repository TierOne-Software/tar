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

#include <tierone/tar/sparse.hpp>
#include <tierone/tar/metadata.hpp>
#include <tierone/tar/stream.hpp>
#include <tierone/tar/header_parser.hpp>
#include <charconv>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace tierone::tar::sparse {

namespace {

// GNU sparse format can have embedded nulls and leading junk in octal fields
// This is a more tolerant parser for sparse-specific fields
auto parse_sparse_octal(const char *field, size_t size) -> std::optional<uint64_t> {
    // Find the longest sequence of octal digits
    uint64_t best_value = 0;
    size_t best_len = 0;
    
    for (size_t start = 0; start < size; ++start) {
        if (field[start] >= '0' && field[start] <= '7') {
            uint64_t value = 0;
            size_t len = 0;
            
            // Parse octal sequence starting at this position
            for (size_t i = start; i < size && field[i] >= '0' && field[i] <= '7'; ++i) {
                value = value * 8 + static_cast<uint64_t>(field[i] - '0');
                len++;
            }
            
            // Keep the longest sequence (or the first one if the same length)
            if (len > best_len) {
                best_value = value;
                best_len = len;
            }
        }
    }
    
    // Remove debug output
    return best_len > 0 ? std::optional{best_value} : std::nullopt;
}

} // anonymous namespace

auto parse_old_sparse_header(
    const tierone::tar::ustar_header &header) -> std::expected<sparse_metadata, tierone::tar::error> {
    sparse_metadata result;
    
    // GNU sparse format reuses part of the header for sparse data
    // Cast the header to our GNU sparse layout
    const auto* sparse_header = reinterpret_cast<const gnu_sparse_header*>(&header);
    
    // Remove debug output
    
    // Parse the sparse entries from the header at offset 384
    for (const auto & i : sparse_header->sp) {
        auto offset = parse_sparse_octal(i.offset, 12);
        auto size = parse_sparse_octal(i.numbytes, 12);
        
        if (!offset || !size || *size == 0) {
            break;  // No more entries
        }
        
        result.segments.push_back({*offset, *size});
    }
    
    // Check if extended sparse headers follow
    if (sparse_header->isextended == '1') {
        // Extended sparse headers follow - we'll need to read them later
        // This is handled in read_sparse_map_continuation
    }
    
    // Parse real size from header (at offset 481)
    if (const auto real_size = parse_sparse_octal(sparse_header->realsize, 12)) {
        result.real_size = *real_size;
    } else if (!result.segments.empty()) {
        // Fallback: compute from sparse map
        const auto& last = result.segments.back();
        result.real_size = last.offset + last.size;
    }
    return result;
}

auto parse_sparse_1_0_header(
    const std::map<std::string, std::string> &pax_headers) -> std::expected<sparse_metadata, tierone::tar::error> {
    
    sparse_metadata result;
    
    // GNU sparse 1.0 format uses PAX extended headers:
    // GNU.sparse.major = 1
    // GNU.sparse.minor = 0
    // GNU.sparse.name = actual filename
    // GNU.sparse.realsize = actual size of the file
    // GNU.sparse.map = "offset,size,offset,size,..."
    
    auto major_it = pax_headers.find("GNU.sparse.major");
    auto minor_it = pax_headers.find("GNU.sparse.minor");
    
    if (major_it == pax_headers.end() || minor_it == pax_headers.end()) {
        return std::unexpected(tierone::tar::error{tierone::tar::error_code::invalid_header, "Missing GNU sparse version headers"});
    }
    
    if (major_it->second != "1" || minor_it->second != "0") {
        return std::unexpected(tierone::tar::error{tierone::tar::error_code::unsupported_feature, 
            "Unsupported GNU sparse version: " + major_it->second + "." + minor_it->second});
    }
    
    // Parse real size
    if (const auto realsize_it = pax_headers.find("GNU.sparse.realsize"); realsize_it != pax_headers.end()) {
        auto fc_result = std::from_chars(realsize_it->second.data(),
                                        realsize_it->second.data() + realsize_it->second.size(),
                                        result.real_size);
        if (fc_result.ec != std::errc{}) {
            return std::unexpected(tierone::tar::error{tierone::tar::error_code::invalid_header, "Invalid GNU.sparse.realsize"});
        }
    }
    
    // Parse sparse map
    if (const auto map_it = pax_headers.find("GNU.sparse.map"); map_it != pax_headers.end()) {
        const std::string& map_str = map_it->second;
        size_t pos = 0;
        
        while (pos < map_str.size()) {
            // Parse offset
            size_t comma_pos = map_str.find(',', pos);
            if (comma_pos == std::string::npos) break;
            
            uint64_t offset;
            auto fc_result = std::from_chars(map_str.data() + pos, 
                                           map_str.data() + comma_pos,
                                           offset);
            if (fc_result.ec != std::errc{}) {
                return std::unexpected(tierone::tar::error{tierone::tar::error_code::invalid_header, "Invalid sparse map offset"});
            }
            
            pos = comma_pos + 1;
            
            // Parse size
            comma_pos = map_str.find(',', pos);
            if (comma_pos == std::string::npos) {
                comma_pos = map_str.size();
            }
            
            uint64_t size;
            fc_result = std::from_chars(map_str.data() + pos, 
                                      map_str.data() + comma_pos,
                                      size);
            if (fc_result.ec != std::errc{}) {
                return std::unexpected(tierone::tar::error{tierone::tar::error_code::invalid_header, "Invalid sparse map size"});
            }
            
            result.segments.push_back({offset, size});
            pos = comma_pos + 1;
        }
    }
    
    return result;
}

auto parse_sparse_1_0_data_map(
    tierone::tar::input_stream &stream,
    const uint64_t real_size) -> std::expected<sparse_metadata, tierone::tar::error> {
    
    sparse_metadata result;
    result.real_size = real_size;
    
    // GNU sparse 1.0 stores the sparse map as decimal numbers at the start of the file data
    // Format: "offset1\nsize1\noffset2\nsize2\n...\n"
    // Read one block to get the sparse map
    std::array<std::byte, 512> block;
    auto read_result = stream.read(block);
    if (!read_result) {
        return std::unexpected(read_result.error());
    }
    
    if (*read_result == 0) {
        // Empty file
        return result;
    }
    
    // Convert to string for parsing
    std::string_view data(reinterpret_cast<const char*>(block.data()), *read_result);
    
    // Find the end of the sparse map (double newline or null terminator)
    size_t map_end = data.find("\n\n");
    if (map_end == std::string_view::npos) {
        map_end = data.find('\0');
        if (map_end == std::string_view::npos) {
            map_end = data.size();
        }
    }
    
    std::string_view map_data = data.substr(0, map_end);
    
    // Parse the sparse map - GNU format 1.0 uses this structure:
    // segments_count\noffset1\nsize1\noffset2\nsize2\n...\nreal_size\n0\n
    std::vector<uint64_t> numbers;
    
    size_t pos = 0;
    while (pos < map_data.size()) {
        // Skip whitespace and newlines
        while (pos < map_data.size() && (map_data[pos] == ' ' || map_data[pos] == '\n' || map_data[pos] == '\t')) {
            ++pos;
        }
        
        if (pos >= map_data.size()) break;
        
        // Parse number
        size_t number_start = pos;
        while (pos < map_data.size() && map_data[pos] >= '0' && map_data[pos] <= '9') {
            ++pos;
        }
        
        if (pos == number_start) break; // No more numbers
        
        uint64_t number;
        const auto parse_result = std::from_chars(map_data.data() + number_start,
                                          map_data.data() + pos, number);
        if (parse_result.ec != std::errc{}) {
            return std::unexpected(tierone::tar::error{tierone::tar::error_code::invalid_header, 
                "Invalid number in sparse map data block"});
        }
        
        numbers.push_back(number);
    }
    
    // Parse the numbers according to GNU sparse 1.0 format
    // Based on analysis: version, offset1, size1, ..., real_size, 0
    if (numbers.size() >= 4) {
        // Skip the first number (might be version/format indicator)
        // Parse pairs starting from index 1
        for (size_t i = 1; i + 1 < numbers.size(); i += 2) {
            const uint64_t offset = numbers[i];
            const uint64_t size = numbers[i + 1];
            
            // Stop if we hit what looks like the real size (larger than reasonable offset+size)
            // or if we hit the end marker (0)
            if (size == 0 || size > real_size || offset + size > real_size * 2) {
                break;
            }
            
            result.segments.push_back({offset, size});
        }
    }
    
    return result;
}

auto read_sparse_map_continuation(
    tierone::tar::input_stream &stream) -> std::expected<std::vector<sparse_entry>, tierone::tar::error> {
    
    std::vector<sparse_entry> result;
    
    // Read extended sparse headers
    // Each extended header block contains 21 sparse entries and a continuation flag
    constexpr size_t SPARSE_EXT_ENTRIES = 21;
    constexpr size_t SPARSE_EXT_SIZE = 512;
    
    while (true) {
        std::array<std::byte, SPARSE_EXT_SIZE> block{};
        auto read_result = stream.read(block);
        if (!read_result) {
            return std::unexpected(read_result.error());
        }
        
        if (*read_result != SPARSE_EXT_SIZE) {
            return std::unexpected(tierone::tar::error{tierone::tar::error_code::corrupt_archive, "Incomplete sparse extension block"});
        }
        
        // Parse sparse entries
        const char* data = reinterpret_cast<const char*>(block.data());
        for (size_t i = 0; i < SPARSE_EXT_ENTRIES; ++i) {
            const char* offset_field = data + (i * 24);
            const char* size_field = data + (i * 24) + 12;
            
            auto offset = parse_sparse_octal(offset_field, 12);
            auto size = parse_sparse_octal(size_field, 12);
            
            if (!offset || !size || *size == 0) {
                break;  // No more entries
            }
            
            result.push_back({*offset, *size});
        }
        
        // Check continuation flag
        char is_extended = data[SPARSE_EXT_ENTRIES * 24];
        if (is_extended != '1') {
            break;  // No more extensions
        }
    }
    
    return result;
}

} // namespace tierone::tar::sparse