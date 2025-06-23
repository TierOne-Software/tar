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

/**
 * simple_count - Counts the total number of entries in a tar archive with progress reporting.
 * 
 * Usage: ./simple_count <tar_file>
 * 
 * Features demonstrated:
 * - Efficient iteration for large archives
 * - Progress reporting (every 1000 entries)
 * - Minimal memory usage
 */

#include <tierone/tar/tar.hpp>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <tar_file>", argv[0]);
        return 1;
    }
    
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    size_t count = 0;
    for (const auto& entry : *reader) {
        ++count;
        if (count % 1000 == 0) {
            std::println(stderr, "Processed {} entries...", count);
        }
        std::println("{}", entry.path().string());
    }
    
    std::println(stderr, "Total entries: {}", count);
    return 0;
}