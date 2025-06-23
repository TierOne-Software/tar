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

#include <tierone/tar/archive_reader.hpp>
#include <tierone/tar/stream.hpp>
#include <tierone/tar/sparse.hpp>
#include <tierone/tar/sparse_reader.hpp>
#include <tierone/tar/pax_parser.hpp>
#include <algorithm>
#include <charconv>

namespace tierone::tar {

auto archive_reader::from_file(const std::filesystem::path &path) -> std::expected<archive_reader, error> {
    auto stream = file_stream::open(path);
    if (!stream) {
        return std::unexpected(stream.error());
    }
    
    return archive_reader{std::make_unique<file_stream>(std::move(*stream))};
}

auto archive_reader::from_stream(std::unique_ptr<input_stream> stream) -> std::expected<archive_reader, error> {
    if (!stream) {
        return std::unexpected(error{error_code::invalid_operation, "Null stream provided"});
    }
    
    return archive_reader{std::move(stream)};
}

auto archive_reader::read_block() -> std::expected<std::array<std::byte, detail::BLOCK_SIZE>, error> {
    std::array<std::byte, detail::BLOCK_SIZE> block{};
    auto result = stream_->read(block);
    if (!result) {
        return std::unexpected(result.error());
    }
    
    if (*result != detail::BLOCK_SIZE) {
        if (*result == 0 && stream_->at_end()) {
            return std::unexpected(error{error_code::end_of_archive, "Unexpected end of archive"});
        }
        return std::unexpected(error{error_code::corrupt_archive, "Incomplete block read"});
    }
    
    return block;
}

auto archive_reader::skip_padding(size_t data_size) -> std::expected<void, error> {
    const size_t padding = (detail::BLOCK_SIZE - (data_size % detail::BLOCK_SIZE)) % detail::BLOCK_SIZE;
    if (padding > 0) {
        return stream_->skip(padding);
    }
    return {};
}

auto archive_reader::skip_current_entry_data() -> std::expected<void, error> {
    // Calculate how much data still needs to be skipped
    const size_t total_entry_size = current_entry_ ? current_entry_->size() : 0;
    const size_t data_to_skip = current_entry_data_remaining_;
    
    
    // Skip any remaining data
    if (data_to_skip > 0) {
        auto skip_result = stream_->skip(data_to_skip);
        if (!skip_result) {
            return std::unexpected(skip_result.error());
        }
    }
    
    // ALWAYS skip padding if we had any data (even if it was all consumed)
    if (total_entry_size > 0) {
        auto padding_result = skip_padding(total_entry_size);
        if (!padding_result) {
            return std::unexpected(padding_result.error());
        }
    }
    
    current_entry_data_remaining_ = 0;
    current_entry_data_consumed_ = 0;
    
    return {};
}

auto archive_reader::next_entry() -> std::expected<std::optional<archive_entry>, error> {
    if (finished_) {
        return std::nullopt;
    }
    
    
    // Skip any remaining data from the previous entry
    if (auto skip_result = skip_current_entry_data(); !skip_result) {
        return std::unexpected(skip_result.error());
    }
    
    // Clear the current entry state after skipping data to prevent recursive issues
    current_entry_.reset();
    current_entry_data_remaining_ = 0;
    current_entry_data_consumed_ = 0;
    
    // Read header block
    auto block_result = read_block();
    if (!block_result) {
        if (block_result.error().code() == error_code::end_of_archive) {
            finished_ = true;
            return std::nullopt;
        }
        return std::unexpected(block_result.error());
    }
    
    // Check for end-of-archive (two zero blocks)
    if (detail::is_zero_block(*block_result)) {
        // Try to read the second zero block
        if (auto second_block = read_block(); second_block && detail::is_zero_block(*second_block)) {
            finished_ = true;
            return std::nullopt;  // Normal end of archive
        }
        // Not end of archive, this is an error
        return std::unexpected(error{error_code::corrupt_archive, "Single zero block in archive"});
    }
    
    // Parse header
    auto metadata_result = detail::parse_header(*block_result);
    if (!metadata_result) {
        return std::unexpected(metadata_result.error());
    }
    
    // Check if this is a GNU extension entry
    if (metadata_result->is_gnu_extension()) {
        auto process_result = process_gnu_extension(*metadata_result);
        if (!process_result) {
            return std::unexpected(process_result.error());
        }
        
        if (*process_result) {
            // GNU extension processed, continue to the next entry
            return next_entry();
        }
        // Fall through for unsupported GNU extensions
    }
    
    // Check if this is a PAX extended header entry
    if (metadata_result->is_pax_header()) {
        auto process_result = process_pax_header(*metadata_result);
        if (!process_result) {
            return std::unexpected(process_result.error());
        }
        
        if (*process_result) {
            // PAX header processed, continue to the next entry
            return next_entry();
        }
        // Fall through for unsupported PAX extensions
    }
    
    // Apply any pending GNU extensions to the metadata
    auto final_metadata = *metadata_result;
    gnu::apply_gnu_extensions(final_metadata, pending_gnu_extensions_);
    pending_gnu_extensions_.clear();
    
    // Apply PAX headers to metadata
    if (!pending_pax_headers_.empty()) {
        // Apply standard PAX header overrides
        if (auto path_it = pending_pax_headers_.find("path"); path_it != pending_pax_headers_.end()) {
            final_metadata.path = path_it->second;
        }
        if (auto size_it = pending_pax_headers_.find("size"); size_it != pending_pax_headers_.end()) {
            uint64_t pax_size;
            if (std::from_chars(size_it->second.data(), size_it->second.data() + size_it->second.size(), pax_size).ec == std::errc{}) {
                final_metadata.size = pax_size;
            }
        }
        
        // Check for GNU sparse format 1.0
        if (pax::has_gnu_sparse_markers(pending_pax_headers_)) {
            auto version = pax::get_gnu_sparse_version(pending_pax_headers_);
            if (version.first == 1 && version.second == 0) {
                // GNU sparse format 1.0 - sparse map is in a data block
                // Get the real size from PAX headers
                uint64_t real_size = final_metadata.size;
                if (auto realsize_it = pending_pax_headers_.find("GNU.sparse.realsize"); 
                    realsize_it != pending_pax_headers_.end()) {
                    std::from_chars(realsize_it->second.data(), 
                                  realsize_it->second.data() + realsize_it->second.size(), 
                                  real_size);
                }
                
                // Mark that this file needs sparse 1.0 processing
                // We'll read the sparse map from the data block later
                final_metadata.size = real_size;
                
                // Set a special marker to indicate sparse 1.0 processing needed
                sparse::sparse_metadata placeholder;
                placeholder.real_size = real_size;
                // Empty segments - will be filled when we read the data block
                final_metadata.sparse_info = std::move(placeholder);
                needs_sparse_1_0_processing_ = true;
            }
        }
        
        // Extract extended attributes from PAX headers
        final_metadata.xattrs = pax::extract_extended_attributes(pending_pax_headers_);
        
        // Extract POSIX ACLs from PAX headers
        auto [access_acl, default_acl] = pax::extract_acls(pending_pax_headers_);
        final_metadata.access_acl = std::move(access_acl);
        final_metadata.default_acl = std::move(default_acl);
        
        pending_pax_headers_.clear();
    }
    
    // Apply pending sparse info if we have any (from the old format)
    if (pending_sparse_info_ && final_metadata.is_regular_file()) {
        final_metadata.sparse_info = std::move(pending_sparse_info_);
        pending_sparse_info_.reset();
    }
    
    // Handle GNU sparse format 1.0 data block processing
    if (needs_sparse_1_0_processing_ && final_metadata.sparse_info) {
        // Read the sparse map from the data block
        auto sparse_1_0_result = sparse::parse_sparse_1_0_data_map(*stream_, final_metadata.sparse_info->real_size);
        if (sparse_1_0_result) {
            final_metadata.sparse_info = std::move(*sparse_1_0_result);
        }
        needs_sparse_1_0_processing_ = false;
    }
    
    // Create entry with data reader
    // For sparse files, we need to adjust the data size and reader
    if (final_metadata.sparse_info) {
        // For sparse files, the size in the header is the sparse data size,
        // not the real file size
        current_entry_data_remaining_ = final_metadata.sparse_info->total_data_size();
        current_entry_data_consumed_ = 0;
        
        // Update the metadata size to reflect the real file size
        final_metadata.size = final_metadata.sparse_info->real_size;
    } else {
        current_entry_data_remaining_ = final_metadata.size;
        current_entry_data_consumed_ = 0;
    }
    
    // Capture the necessary variables for the lambda
    auto* stream_ptr = stream_.get();
    auto* remaining_ptr = &current_entry_data_remaining_;
    auto* consumed_ptr = &current_entry_data_consumed_;
    
    data_reader_fn base_reader = [stream_ptr, remaining_ptr, consumed_ptr](const size_t offset, const size_t length)
        -> std::expected<std::span<const std::byte>, error> {
        
        if (offset > 0) {
            return std::unexpected(error{error_code::unsupported_feature, 
                "Streaming mode doesn't support offset reads"});
        }
        
        // Only read from where we left off
        if (offset < *consumed_ptr) {
            return std::unexpected(error{error_code::invalid_operation, 
                "Cannot seek backwards in streaming mode"});
        }
        
        // Skip any gap between consumed and requested offset
        if (size_t skip_amount = offset - *consumed_ptr; skip_amount > 0) {
            if (auto skip_result = stream_ptr->skip(skip_amount); !skip_result) {
                return std::unexpected(skip_result.error());
            }
            *consumed_ptr += skip_amount;
            *remaining_ptr -= skip_amount;
        }

        const size_t to_read = std::min(length, *remaining_ptr);
        if (to_read == 0) {
            return std::span<const std::byte>{};
        }
        
        // Use thread_local buffer for data
        thread_local std::vector<std::byte> buffer;
        buffer.resize(to_read);
        
        auto result = stream_ptr->read(std::span{buffer.data(), to_read});
        if (!result) {
            return std::unexpected(result.error());
        }
        
        *remaining_ptr -= *result;
        *consumed_ptr += *result;
        return std::span{buffer.data(), *result};
    };
    
    // Wrap with a sparse reader if needed
    data_reader_fn reader;
    if (final_metadata.sparse_info) {
        reader = sparse::make_sparse_reader(*final_metadata.sparse_info, std::move(base_reader));
    } else {
        reader = std::move(base_reader);
    }
    
    archive_entry entry{std::move(final_metadata), std::move(reader)};
    current_entry_ = entry;
    
    return entry;
}

auto archive_reader::process_gnu_extension(const file_metadata &meta) -> std::expected<bool, error> {
    if (meta.is_gnu_longname()) {
        // Read the long filename
        auto longname_result = gnu::read_gnu_extension_data(*stream_, meta.size);
        if (!longname_result) {
            return std::unexpected(longname_result.error());
        }
        
        pending_gnu_extensions_.longname = std::move(*longname_result);
        return true;  // Extension processed
    }
    
    if (meta.is_gnu_longlink()) {
        // Read the long link target
        auto longlink_result = gnu::read_gnu_extension_data(*stream_, meta.size);
        if (!longlink_result) {
            return std::unexpected(longlink_result.error());
        }
        
        pending_gnu_extensions_.longlink = std::move(*longlink_result);
        return true;  // Extension processed
    }
    
    if (meta.type == entry_type::gnu_sparse) {
        // Handle sparse files
        if (auto sparse_result = process_sparse_file(meta); !sparse_result) {
            return std::unexpected(sparse_result.error());
        }
        return true;  // Sparse file processed
    }
    
    // Other GNU extensions - skip for now
    if (meta.type == entry_type::gnu_volhdr ||
        meta.type == entry_type::gnu_multivol) {
        
        // Skip the data for unsupported GNU extensions
        if (auto skip_result = stream_->skip(meta.size); !skip_result) {
            return std::unexpected(skip_result.error());
        }
        
        // Skip padding
        if (auto padding_result = skip_padding(meta.size); !padding_result) {
            return std::unexpected(padding_result.error());
        }
        
        return true;  // Extension skipped
    }
    
    return false;  // Not a supported GNU extension
}

auto archive_reader::process_pax_header(const file_metadata &meta) -> std::expected<bool, error> {
    if (meta.type == entry_type::pax_extended_header) {
        // Read PAX header data
        std::vector<std::byte> pax_data(meta.size);
        auto read_result = stream_->read(std::span{pax_data.data(), meta.size});
        if (!read_result) {
            return std::unexpected(read_result.error());
        }
        
        if (*read_result != meta.size) {
            return std::unexpected(error{error_code::corrupt_archive, "Incomplete PAX header data"});
        }
        
        // Parse PAX headers
        auto parse_result = pax::parse_pax_headers(std::span{pax_data});
        if (!parse_result) {
            return std::unexpected(parse_result.error());
        }
        
        // Store PAX headers for the next entry
        pending_pax_headers_ = std::move(*parse_result);
        
        // Skip padding
        if (auto padding_result = skip_padding(meta.size); !padding_result) {
            return std::unexpected(padding_result.error());
        }
        
        return true;  // PAX header processed
    }
    
    if (meta.type == entry_type::pax_global_header) {
        // For now, skip global PAX headers (they apply to all subsequent entries)
        if (auto skip_result = stream_->skip(meta.size); !skip_result) {
            return std::unexpected(skip_result.error());
        }

        if (auto padding_result = skip_padding(meta.size); !padding_result) {
            return std::unexpected(padding_result.error());
        }
        
        return true;  // Global PAX header skipped
    }
    
    return false;  // Not a PAX header
}

auto archive_reader::process_sparse_file(const file_metadata &meta) -> std::expected<void, error> {
    // This is called when we have a regular file with sparse info that has extended headers
    // We need to read the extended sparse header blocks
    
    if (!meta.sparse_info) {
        return std::unexpected(error{error_code::invalid_header, "Sparse file entry without sparse info"});
    }
    
    // Read extended sparse headers
    auto extended_segments = sparse::read_sparse_map_continuation(*stream_);
    if (!extended_segments) {
        return std::unexpected(extended_segments.error());
    }
    
    // Add the extended segments to our sparse info
    pending_sparse_info_ = *meta.sparse_info;
    pending_sparse_info_->segments.insert(
        pending_sparse_info_->segments.end(),
        extended_segments->begin(),
        extended_segments->end()
    );
    
    // Recalculate real size based on all segments
    if (!pending_sparse_info_->segments.empty()) {
        const auto& last = pending_sparse_info_->segments.back();
        pending_sparse_info_->real_size = last.offset + last.size;
    }
    
    return {};
}

} // namespace tierone::tar