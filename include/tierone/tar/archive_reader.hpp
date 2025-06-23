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
#include <tierone/tar/archive_entry.hpp>
#include <tierone/tar/header_parser.hpp>
#include <tierone/tar/gnu_tar.hpp>
#include <tierone/tar/pax_parser.hpp>
#include <expected>
#include <memory>
#include <optional>
#include <iterator>
#include <map>
#include <string>

namespace tierone::tar {

class archive_reader {
private:
    std::unique_ptr<input_stream> stream_;  // Back to unique_ptr
    std::optional<archive_entry> current_entry_;
    size_t current_entry_data_remaining_ = 0;  // Data remaining for the current entry
    size_t current_entry_data_consumed_ = 0;   // Data already consumed from the current entry
    bool finished_ = false;
    gnu::gnu_extension_data pending_gnu_extensions_;
    std::optional<sparse::sparse_metadata> pending_sparse_info_;
    std::map<std::string, std::string> pending_pax_headers_;
    bool needs_sparse_1_0_processing_ = false;

    // Read exactly one 512-byte block
    [[nodiscard]] std::expected<std::array<std::byte, detail::BLOCK_SIZE>, error> read_block();

    // Skip padding to the next 512-byte boundary
    [[nodiscard]] std::expected<void, error> skip_padding(size_t data_size);

    // Skip remaining data from the current entry
    [[nodiscard]] std::expected<void, error> skip_current_entry_data();

    // Process GNU extension entry
    [[nodiscard]] std::expected<bool, error> process_gnu_extension(const file_metadata& meta);
    
    // Process PAX extended header entry
    [[nodiscard]] std::expected<bool, error> process_pax_header(const file_metadata& meta);
    
    // Process sparse file entry
    [[nodiscard]] std::expected<void, error> process_sparse_file(const file_metadata& meta);

public:
    explicit archive_reader(std::unique_ptr<input_stream> stream)
        : stream_(std::move(stream)) {}

    // Factory methods
    [[nodiscard]] static std::expected<archive_reader, error> from_file(const std::filesystem::path& path);
    [[nodiscard]] static std::expected<archive_reader, error> from_stream(std::unique_ptr<input_stream> stream);

    // Get next entry in archive
    [[nodiscard]] std::expected<std::optional<archive_entry>, error> next_entry();

    // Iterator support
    class iterator {
    private:
        archive_reader* reader_ = nullptr;
        std::optional<archive_entry> current_;
        bool error_occurred_ = false;

    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = archive_entry;
        using difference_type = std::ptrdiff_t;
        using pointer = const archive_entry*;
        using reference = const archive_entry&;

        iterator() = default;
        explicit iterator(archive_reader* reader) : reader_(reader) {
            ++(*this);  // Load the first entry
        }

        [[nodiscard]] const archive_entry& operator*() const { return *current_; }
        [[nodiscard]] const archive_entry* operator->() const { return &*current_; }

        iterator& operator++() {
            if (reader_ && !error_occurred_) {
                if (auto result = reader_->next_entry(); result && *result) {
                    current_ = std::move(**result);
                } else {
                    if (!result) {
                        error_occurred_ = true;
                    }
                    reader_ = nullptr;
                    current_.reset();
                }
            }
            return *this;
        }

        iterator operator++(int) {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] bool operator==(const iterator& other) const {
            return reader_ == other.reader_;
        }

        [[nodiscard]] bool has_error() const noexcept { return error_occurred_; }
    };

    [[nodiscard]] iterator begin() { return iterator{this}; }
    [[nodiscard]] iterator end() const { return {}; }

    // Check if archive processing is complete
    [[nodiscard]] bool finished() const noexcept { return finished_; }
};

} // namespace tierone::tar