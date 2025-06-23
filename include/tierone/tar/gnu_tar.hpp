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
#include <tierone/tar/stream.hpp>
#include <expected>
#include <string>
#include <span>

namespace tierone::tar::gnu {

// GNU tar extension data
struct gnu_extension_data {
    std::string longname;     // From 'L' type entries
    std::string longlink;     // From 'K' type entries
    
    [[nodiscard]] bool has_longname() const noexcept { return !longname.empty(); }
    [[nodiscard]] bool has_longlink() const noexcept { return !longlink.empty(); }
    
    void clear() {
        longname.clear();
        longlink.clear();
    }
};

// Read GNU extension data from stream
[[nodiscard]] std::expected<std::string, error> read_gnu_extension_data(
    input_stream& stream, 
    size_t data_size
);

// Apply GNU extensions to metadata
void apply_gnu_extensions(file_metadata& metadata, const gnu_extension_data& extensions);

// Check if magic indicates GNU tar format
[[nodiscard]] bool is_gnu_tar_magic(std::string_view magic);

} // namespace tierone::tar::gnu