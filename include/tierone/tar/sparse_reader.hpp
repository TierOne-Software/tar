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
#include <tierone/tar/sparse.hpp>
#include <functional>
#include <span>
#include <expected>
#include <vector>
#include <algorithm>
#include <iostream>

namespace tierone::tar::sparse {

// Create a sparse-aware data reader that returns zeros for holes
// and actual data for non-sparse regions
inline auto make_sparse_reader(
    const sparse_metadata& sparse_info,
    std::function<std::expected<std::span<const std::byte>, error>(size_t, size_t)> base_reader
) -> std::function<std::expected<std::span<const std::byte>, error>(size_t, size_t)> {
    
    return [sparse_info, base_reader = std::move(base_reader)](size_t offset, size_t length) 
        -> std::expected<std::span<const std::byte>, error> {
        
        // Thread-local buffers for zero-filled data and combined results
        thread_local std::vector<std::byte> zero_buffer;
        thread_local std::vector<std::byte> result_buffer;
        
        // Clamp length to not exceed the sparse file's real size
        if (offset >= sparse_info.real_size) {
            // Reading beyond end-of-file
            result_buffer.clear();
            return std::span{result_buffer.data(), size_t{0}};
        }

        const size_t max_readable = sparse_info.real_size - offset;
        length = std::min(length, max_readable);
        
        // Handle reads that span multiple segments or holes
        result_buffer.clear();
        result_buffer.reserve(length);
        
        size_t current_offset = offset;
        size_t remaining = length;
        
        while (remaining > 0) {
            // Find which segment (if any) contains current_offset

            if (auto segment_idx = sparse_info.find_segment(current_offset)) {
                // We're in a data segment
                const auto& segment = sparse_info.segments[*segment_idx];
                const size_t segment_offset = current_offset - segment.offset;
                size_t segment_remaining = segment.size - segment_offset;
                const size_t to_read = std::min(remaining, segment_remaining);
                
                // Calculate the actual offset in the sparse data
                size_t sparse_data_offset = 0;
                for (size_t i = 0; i < *segment_idx; ++i) {
                    sparse_data_offset += sparse_info.segments[i].size;
                }
                sparse_data_offset += segment_offset;
                
                // Read from the underlying reader
                auto read_result = base_reader(sparse_data_offset, to_read);
                if (!read_result) {
                    return read_result;
                }
                
                // Append to result
                result_buffer.insert(result_buffer.end(), 
                                   read_result->begin(), 
                                   read_result->end());
                
                current_offset += read_result->size();
                remaining -= read_result->size();
            } else {
                // We're in a hole - return zeros
                size_t next_segment_start = sparse_info.real_size;
                
                // Find the start of the next segment after current_offset
                for (const auto& seg : sparse_info.segments) {
                    if (seg.offset > current_offset) {
                        next_segment_start = seg.offset;
                        break;
                    }
                }
                
                size_t hole_size = next_segment_start - current_offset;
                const size_t to_fill = std::min(remaining, hole_size);
                
                
                // Resize zero buffer if needed
                if (zero_buffer.size() < to_fill) {
                    zero_buffer.resize(to_fill, std::byte{0});
                }
                
                // Append zeros to result
                result_buffer.insert(result_buffer.end(), 
                                   zero_buffer.begin(), 
                                   zero_buffer.begin() + static_cast<std::ptrdiff_t>(to_fill));
                
                current_offset += to_fill;
                remaining -= to_fill;
            }
        }
        
        return std::span{result_buffer.data(), result_buffer.size()};
    };
}

} // namespace tierone::tar::sparse