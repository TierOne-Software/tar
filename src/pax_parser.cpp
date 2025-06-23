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

#include <tierone/tar/pax_parser.hpp>
#include <tierone/tar/metadata.hpp>
#include <charconv>
#include <algorithm>
#include <sstream>
#include <utility>
#include <format>

namespace tierone::tar::pax {

auto parse_pax_headers(
    const std::span<const std::byte> data) -> std::expected<std::map<std::string, std::string>, error> {
    std::map<std::string, std::string> result;

    const auto start = reinterpret_cast<const char*>(data.data());
    const char* end = start + data.size();
    const char* pos = start;
    
    while (pos < end && *pos != '\0') {
        // Parse length field
        const char* length_start = pos;
        while (pos < end && *pos >= '0' && *pos <= '9') {
            ++pos;
        }
        
        if (pos == length_start || pos >= end || *pos != ' ') {
            const std::string debug_str(length_start, std::min(pos, end));
            return std::unexpected(error{error_code::invalid_header, 
                std::format("Invalid PAX header length field, found: '{}'", debug_str)});
        }
        
        // Extract length
        size_t length;
        auto result_code = std::from_chars(length_start, pos, length);
        if (result_code.ec != std::errc{}) {
            return std::unexpected(error{error_code::invalid_header, "Failed to parse PAX header length"});
        }
        
        // Check for zero length - invalid PAX record
        if (length == 0) {
            return std::unexpected(error{error_code::invalid_header, "PAX header record length cannot be zero"});
        }
        
        ++pos; // Skip space
        
        // Find the record boundary
        const char* record_start = length_start;
        const char* record_end = record_start + length;
        
        if (record_end > end) {
            return std::unexpected(error{error_code::corrupt_archive, "PAX header record extends beyond data"});
        }
        
        // Find key=value part
        const char* key_start = pos;
        
        // Skip the newline at the end when looking for '='
        const char* value_end = record_end;
        if (value_end > key_start && *(value_end - 1) == '\n') {
            --value_end;
        }
        
        const char* equals_pos = std::find(key_start, value_end, '=');
        
        if (equals_pos == value_end) {
            return std::unexpected(error{error_code::invalid_header, "PAX header missing '=' separator"});
        }
        
        // Extract key and value
        const std::string key(key_start, equals_pos);
        const std::string value(equals_pos + 1, value_end);
        
        result[key] = value;
        
        // Move to the next record
        pos = record_end;
    }
    
    return result;
}

bool has_gnu_sparse_markers(const std::map<std::string, std::string>& headers) {
    return headers.contains("GNU.sparse.major") ||
           headers.contains("GNU.sparse.minor") ||
           headers.contains("GNU.sparse.map");
}

std::pair<int, int> get_gnu_sparse_version(const std::map<std::string, std::string>& headers) {
    int major = 0;
    int minor = 0;

    if (const auto major_it = headers.find("GNU.sparse.major"); major_it != headers.end()) {
        std::from_chars(major_it->second.data(), 
                       major_it->second.data() + major_it->second.size(), 
                       major);
    }

    if (const auto minor_it = headers.find("GNU.sparse.minor"); minor_it != headers.end()) {
        std::from_chars(minor_it->second.data(), 
                       minor_it->second.data() + minor_it->second.size(), 
                       minor);
    }
    
    return {major, minor};
}

extended_attributes extract_extended_attributes(const std::map<std::string, std::string>& headers) {
    extended_attributes result;
    
    // Look for extended attributes in PAX headers
    // Common prefixes: SCHILY.xattr.*, LIBARCHIVE.xattr.*
    for (const auto& [key, value] : headers) {
        if (key.starts_with("SCHILY.xattr.")) {
            // Extract the attribute name (everything after "SCHILY.xattr.")
            std::string attr_name = key.substr(13); // strlen("SCHILY.xattr.") = 13
            result[attr_name] = value;
        } else if (key.starts_with("LIBARCHIVE.xattr.")) {
            // Alternative format used by libarchive
            std::string attr_name = key.substr(17); // strlen("LIBARCHIVE.xattr.") = 17
            result[attr_name] = value;
        }
    }
    
    return result;
}

std::pair<std::vector<acl_entry>, std::vector<acl_entry>> 
extract_acls(const std::map<std::string, std::string>& headers) {
    std::vector<acl_entry> access_acl;
    std::vector<acl_entry> default_acl;
    
    // Look for ACL data in PAX headers
    if (const auto access_it = headers.find("SCHILY.acl.access"); access_it != headers.end()) {
        if (auto parsed_access = parse_acl_text(access_it->second)) {
            access_acl = std::move(*parsed_access);
        }
    }

    if (const auto default_it = headers.find("SCHILY.acl.default"); default_it != headers.end()) {
        if (auto parsed_default = parse_acl_text(default_it->second)) {
            default_acl = std::move(*parsed_default);
        }
    }
    
    return {std::move(access_acl), std::move(default_acl)};
}

std::expected<std::vector<acl_entry>, error>
parse_acl_text(const std::string& acl_text) {
    std::vector<acl_entry> result;
    
    // Parse ACL text format
    // Example: "user::rwx,group::r-x,other::r--,user:1000:rwx,mask::rwx"
    std::istringstream stream(acl_text);
    std::string entry_str;
    
    while (std::getline(stream, entry_str, ',')) {
        // Trim whitespace
        entry_str.erase(0, entry_str.find_first_not_of(" \t"));
        entry_str.erase(entry_str.find_last_not_of(" \t") + 1);
        
        if (entry_str.empty()) continue;
        
        // Parse single ACL entry: "type:id:permissions"
        std::istringstream entry_stream(entry_str);
        std::string type_str;
        std::string id_str;
        std::string perm_str;

        if (!std::getline(entry_stream, type_str, ':') ||
            !std::getline(entry_stream, id_str, ':') ||
            !std::getline(entry_stream, perm_str)) {
            return std::unexpected(error{error_code::invalid_header, 
                "Invalid ACL entry format: " + entry_str});
        }
        
        acl_entry entry;
        
        // Parse entry type
        if (type_str == "user") {
            if (id_str.empty()) {
                entry.entry_type = acl_entry::type::user_obj;
            } else {
                entry.entry_type = acl_entry::type::user;
            }
        } else if (type_str == "group") {
            if (id_str.empty()) {
                entry.entry_type = acl_entry::type::group_obj;
            } else {
                entry.entry_type = acl_entry::type::group;
            }
        } else if (type_str == "mask") {
            entry.entry_type = acl_entry::type::mask;
        } else if (type_str == "other") {
            entry.entry_type = acl_entry::type::other;
        } else {
            return std::unexpected(error{error_code::invalid_header,
                "Unknown ACL entry type: " + type_str});
        }
        
        // Parse ID (if present)
        if (!id_str.empty()) {
            auto parse_result = std::from_chars(id_str.data(), id_str.data() + id_str.size(), entry.id);
            if (parse_result.ec != std::errc{}) {
                return std::unexpected(error{error_code::invalid_header,
                    "Invalid ACL ID: " + id_str});
            }
        }
        
        // Parse permissions
        if (perm_str.length() != 3) {
            return std::unexpected(error{error_code::invalid_header,
                "Invalid ACL permission format: " + perm_str});
        }
        
        uint8_t perms = 0;
        if (perm_str[0] == 'r') perms |= std::to_underlying(acl_entry::perm::read);
        if (perm_str[1] == 'w') perms |= std::to_underlying(acl_entry::perm::write);
        if (perm_str[2] == 'x') perms |= std::to_underlying(acl_entry::perm::execute);

        entry.permissions = static_cast<acl_entry::perm>(perms);
        
        result.push_back(std::move(entry));
    }
    
    return result;
}

} // namespace tierone::tar::pax