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
#include <variant>
#include <functional>
#include <filesystem>
#include <limits>
#include <algorithm>
#include <ranges>
#include <iterator>

namespace tierone::tar {

// Function signature for reading entry data
using data_reader_fn = std::function<std::expected<std::span<const std::byte>, error>(size_t offset, size_t length)>;

class archive_entry {
private:
    file_metadata metadata_;
    std::variant<
        data_reader_fn,                      // Back to function for simplicity
        std::span<const std::byte>           // For memory-mapped mode
    > data_source_;

public:
    // Constructor for streaming mode
    archive_entry(file_metadata metadata, data_reader_fn reader)
        : metadata_(std::move(metadata)), data_source_(std::move(reader)) {}

    // Constructor for memory-mapped mode
    archive_entry(file_metadata metadata, std::span<const std::byte> data)
        : metadata_(std::move(metadata)), data_source_(data) {}

    // Metadata accessors
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return metadata_.path; }
    [[nodiscard]] entry_type type() const noexcept { return metadata_.type; }
    [[nodiscard]] std::filesystem::perms permissions() const noexcept { return metadata_.permissions; }
    [[nodiscard]] uint32_t owner_id() const noexcept { return metadata_.owner_id; }
    [[nodiscard]] uint32_t group_id() const noexcept { return metadata_.group_id; }
    [[nodiscard]] uint64_t size() const noexcept { return metadata_.size; }
    [[nodiscard]] const std::chrono::system_clock::time_point& modification_time() const noexcept { 
        return metadata_.modification_time; 
    }
    [[nodiscard]] const std::string& owner_name() const noexcept { return metadata_.owner_name; }
    [[nodiscard]] const std::string& group_name() const noexcept { return metadata_.group_name; }
    [[nodiscard]] const std::optional<std::string>& link_target() const noexcept { return metadata_.link_target; }
    
    // Device numbers (for character and block devices)
    [[nodiscard]] uint32_t device_major() const noexcept { return metadata_.device_major; }
    [[nodiscard]] uint32_t device_minor() const noexcept { return metadata_.device_minor; }
    
    // Extended attributes and ACLs
    [[nodiscard]] const extended_attributes& get_extended_attributes() const noexcept { return metadata_.xattrs; }
    [[nodiscard]] const std::vector<acl_entry>& access_acl() const noexcept { return metadata_.access_acl; }
    [[nodiscard]] const std::vector<acl_entry>& default_acl() const noexcept { return metadata_.default_acl; }

    // Type checking convenience methods
    [[nodiscard]] bool is_regular_file() const noexcept { return metadata_.is_regular_file(); }
    [[nodiscard]] bool is_directory() const noexcept { return metadata_.is_directory(); }
    [[nodiscard]] bool is_symbolic_link() const noexcept { return metadata_.is_symbolic_link(); }
    [[nodiscard]] bool is_hard_link() const noexcept { return metadata_.is_hard_link(); }
    [[nodiscard]] bool is_sparse() const noexcept { return metadata_.is_sparse(); }
    [[nodiscard]] bool is_character_device() const noexcept { return metadata_.is_character_device(); }
    [[nodiscard]] bool is_block_device() const noexcept { return metadata_.is_block_device(); }
    [[nodiscard]] bool is_device() const noexcept { return metadata_.is_device(); }
    [[nodiscard]] bool has_extended_attributes() const noexcept { return metadata_.has_extended_attributes(); }
    [[nodiscard]] bool has_acls() const noexcept { return metadata_.has_acls(); }

    // Data access
    [[nodiscard]] auto read_data(
        size_t offset = 0,
        size_t length = std::numeric_limits<size_t>::max()
    ) const -> std::expected<std::span<const std::byte>, error> {
        if (!is_regular_file()) {
            return std::unexpected(error{error_code::invalid_operation, "Entry is not a regular file"});
        }

        return std::visit([=](const auto& source) -> std::expected<std::span<const std::byte>, error> {
            using T = std::decay_t<decltype(source)>;
            if constexpr (std::is_same_v<T, std::span<const std::byte>>) {
                // Zero-copy for memory-mapped data
                size_t start_offset = std::min(offset, source.size());
                size_t available = source.size() - start_offset;
                size_t to_return = std::min(length, available);
                return source.subspan(start_offset, to_return);
            } else {
                // Streaming mode
                return source(offset, length);
            }
        }, data_source_);
    }

    // Extract entry to filesystem
    [[nodiscard]] std::expected<void, error> extract_to_path(const std::filesystem::path& dest_path) const;

    // Copy data to output iterator
    template<std::output_iterator<std::byte> OutputIt>
    [[nodiscard]] auto copy_data_to(OutputIt output) const -> std::expected<size_t, error> {
        auto data = read_data();
        if (!data) return std::unexpected(data.error());
        
        std::ranges::copy(*data, output);
        return data->size();
    }

    // Get full metadata
    [[nodiscard]] const file_metadata& metadata() const noexcept { return metadata_; }
};

} // namespace tierone::tar