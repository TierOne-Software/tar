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
 * test_error - Tests error conditions and empty archives.
 * 
 * Usage: ./test_error <tar_file>
 * 
 * Features:
 * - Archive validation
 * - Iterator error states
 * - Empty archive detection
 */

#include <tierone/tar/tar.hpp>
#include <iostream>
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
    
    std::println("Archive opened successfully");

    const auto it = reader->begin();

    if (const auto end = reader->end(); it == end) {
        std::println("No entries in archive");
    } else {
        std::println("Archive has entries");
    }
    
    if (it.has_error()) {
        std::println("Iterator has error");
    } else {
        std::println("Iterator is OK");
    }
    
    return 0;
}