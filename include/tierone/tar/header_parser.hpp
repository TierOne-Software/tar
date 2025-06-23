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
#include <tierone/tar/metadata.hpp>
#include <expected>
#include <span>
#include <bit>
#include <algorithm>
#include <ranges>

namespace tierone::tar::detail {

constexpr size_t BLOCK_SIZE = 512;

// Concepts for type safety
template<typename T>
concept OctalField = requires(T t) {
    { std::span{t} } -> std::convertible_to<std::span<const char>>;
};

// Parse octal field with proper error handling
template<size_t N>
constexpr std::expected<uint64_t, error> parse_octal(std::span<const char, N> field) {
    uint64_t result = 0;
    bool found_digit = false;
    
    for (char c : field) {
        if (c == '\0' || c == ' ') {
            if (!found_digit) continue;
            break;
        }
        if (c < '0' || c > '7') {
            return std::unexpected(error{error_code::invalid_header, "Invalid octal digit"});
        }
        found_digit = true;
        
        // Check for overflow
        if (result > (UINT64_MAX >> 3)) {
            return std::unexpected(error{error_code::invalid_header, "Octal value overflow"});
        }
        
        result = (result << 3) | static_cast<uint64_t>(c - '0');
    }
    
    return found_digit ? result : 0;
}

// Calculate checksum for header validation
[[nodiscard]] uint32_t calculate_checksum(std::span<const std::byte, BLOCK_SIZE> block);

// Parse a complete tar header block
[[nodiscard]] std::expected<file_metadata, error> parse_header(std::span<const std::byte, BLOCK_SIZE> block);

// Check if block is all zeros (end-of-archive marker)
[[nodiscard]] bool is_zero_block(std::span<const std::byte, BLOCK_SIZE> block);

// Helper to safely extract null-terminated string from a fixed-size field
[[nodiscard]] std::string_view extract_string(std::span<const char> field);

} // namespace tierone::tar::detail