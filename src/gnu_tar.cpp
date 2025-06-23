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

#include <tierone/tar/gnu_tar.hpp>
#include <tierone/tar/header_parser.hpp>
#include <algorithm>

namespace tierone::tar::gnu {

auto read_gnu_extension_data(
    input_stream &stream,
    const size_t data_size
) -> std::expected<std::string, error> {
    if (data_size == 0) {
        return std::string{};
    }
    
    // Read the data in blocks
    std::string result;
    result.reserve(data_size);
    
    size_t remaining = data_size;
    std::array<std::byte, detail::BLOCK_SIZE> buffer{};
    
    while (remaining > 0) {
        size_t to_read = std::min(remaining, detail::BLOCK_SIZE);
        
        auto read_result = stream.read(std::span{buffer.data(), to_read});
        if (!read_result) {
            return std::unexpected(read_result.error());
        }
        
        if (*read_result == 0) {
            return std::unexpected(error{error_code::corrupt_archive, 
                "Unexpected end of stream while reading GNU extension data"});
        }
        
        // Convert bytes to chars and append
        for (size_t i = 0; i < *read_result; ++i) {
            result.push_back(static_cast<char>(buffer[i]));
        }
        
        remaining -= *read_result;
    }
    
    // Skip padding to next block boundary
    size_t padding = (detail::BLOCK_SIZE - (data_size % detail::BLOCK_SIZE)) % detail::BLOCK_SIZE;
    if (padding > 0) {
        auto skip_result = stream.skip(padding);
        if (!skip_result) {
            return std::unexpected(skip_result.error());
        }
    }
    
    // GNU extensions are null-terminated, so remove trailing nulls
    while (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    
    return result;
}

void apply_gnu_extensions(file_metadata& metadata, const gnu_extension_data& extensions) {
    // Apply longname if present
    if (extensions.has_longname()) {
        metadata.path = std::filesystem::path{extensions.longname};
    }
    
    // Apply longlink if present  
    if (extensions.has_longlink()) {
        metadata.link_target = extensions.longlink;
    }
}

bool is_gnu_tar_magic(std::string_view magic) {
    // GNU tar uses "ustar  " (with spaces) or "ustar\0" 
    return magic == "ustar " || magic == "ustar";
}

} // namespace tierone::tar::gnu