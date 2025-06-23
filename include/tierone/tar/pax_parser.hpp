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
#include <map>
#include <string>
#include <string_view>
#include <expected>
#include <span>
#include <vector>

namespace tierone::tar {
    // Forward declarations
    struct acl_entry;
    using extended_attributes = std::map<std::string, std::string>;
}

namespace tierone::tar::pax {

// Parse PAX extended header format
// : "length key=value\n"
// Example: "25 path=long/file/name.txt\n"
[[nodiscard]] std::expected<std::map<std::string, std::string>, error>
parse_pax_headers(std::span<const std::byte> data);

// Check if PAX headers contain GNU sparse format markers
[[nodiscard]] bool has_gnu_sparse_markers(const std::map<std::string, std::string>& headers);

// Extract GNU sparse version from PAX headers
[[nodiscard]] std::pair<int, int> get_gnu_sparse_version(const std::map<std::string, std::string>& headers);

// Extract extended attributes from PAX headers
// Looks for keys like SCHILY.xattr.user.*, SCHILY.xattr.security.*, etc.
[[nodiscard]] extended_attributes extract_extended_attributes(const std::map<std::string, std::string>& headers);

// Extract POSIX ACLs from PAX headers  
// Looks for SCHILY.acl.access and SCHILY.acl.default
[[nodiscard]] std::pair<std::vector<acl_entry>, std::vector<acl_entry>> 
extract_acls(const std::map<std::string, std::string>& headers);

// Parse ACL text format to ACL entries
// Format: "user::rwx,group::r-x,other::r--,user:1000:rwx,mask::rwx"
[[nodiscard]] std::expected<std::vector<acl_entry>, error>
parse_acl_text(const std::string& acl_text);

} // namespace tierone::tar::pax