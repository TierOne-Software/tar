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

#include <tierone/tar/archive_entry.hpp>
#include <fstream>
#include <filesystem>

namespace tierone::tar {

auto archive_entry::extract_to_path(const std::filesystem::path &dest_path) const -> std::expected<void, error> {
    // Create parent directories if they don't exist
    std::error_code ec;
    std::filesystem::create_directories(dest_path.parent_path(), ec);
    if (ec) {
        return std::unexpected(error{error_code::io_error, 
            "Failed to create directories: " + ec.message()});
    }
    
    switch (type()) {
        case entry_type::regular_file:
        case entry_type::regular_file_old:
        case entry_type::contiguous_file: {
            // Extract regular file
            std::ofstream file{dest_path, std::ios::binary};
            if (!file) {
                return std::unexpected(error{error_code::io_error, 
                    "Failed to create output file: " + dest_path.string()});
            }
            
            auto data = read_data();
            if (!data) {
                return std::unexpected(data.error());
            }
            
            file.write(reinterpret_cast<const char*>(data->data()), 
                      static_cast<std::streamsize>(data->size()));
            
            if (!file) {
                return std::unexpected(error{error_code::io_error, 
                    "Failed to write file data"});
            }
            break;
        }
        
        case entry_type::directory: {
            std::filesystem::create_directories(dest_path, ec);
            if (ec) {
                return std::unexpected(error{error_code::io_error, 
                    "Failed to create directory: " + ec.message()});
            }
            break;
        }
        
        case entry_type::symbolic_link: {
            if (!link_target()) {
                return std::unexpected(error{error_code::invalid_operation, 
                    "Symbolic link has no target"});
            }
            
            std::filesystem::create_symlink(*link_target(), dest_path, ec);
            if (ec) {
                return std::unexpected(error{error_code::io_error, 
                    "Failed to create symbolic link: " + ec.message()});
            }
            break;
        }
        
        case entry_type::hard_link: {
            if (!link_target()) {
                return std::unexpected(error{error_code::invalid_operation, 
                    "Hard link has no target"});
            }
            
            std::filesystem::create_hard_link(*link_target(), dest_path, ec);
            if (ec) {
                return std::unexpected(error{error_code::io_error, 
                    "Failed to create hard link: " + ec.message()});
            }
            break;
        }
        
        default:
            return std::unexpected(error{error_code::unsupported_feature, 
                "Extraction of this entry type is not supported"});
    }
    
    // Set file permissions (best effort)
    std::filesystem::permissions(dest_path, permissions(), ec);
    // Ignore permission errors as they're not critical
    
    return {};
}

} // namespace tierone::tar