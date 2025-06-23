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
#include <tierone/tar/archive_reader.hpp>
#include <tierone/tar/archive_entry.hpp>
#include <tierone/tar/gnu_tar.hpp>

namespace tierone::tar {

// Main convenience API
[[nodiscard]] std::expected<archive_reader, error> open_archive(const std::filesystem::path& path);
[[nodiscard]] std::expected<archive_reader, error> open_archive(std::unique_ptr<input_stream> stream);

} // namespace tierone::tar