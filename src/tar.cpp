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

#include <tierone/tar/tar.hpp>

namespace tierone::tar {

auto open_archive(const std::filesystem::path &path) -> std::expected<archive_reader, error> {
    return archive_reader::from_file(path);
}

auto open_archive(std::unique_ptr<input_stream> stream) -> std::expected<archive_reader, error> {
    return archive_reader::from_stream(std::move(stream));
}

} // namespace tierone::tar