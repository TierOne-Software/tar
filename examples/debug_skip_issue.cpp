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
 * debug_skip_issue - Debug tool for testing iterator advancement issues.
 * 
 * Usage: ./debug_skip_issue <tar_file>
 * 
 * Features:
 * - Skips to specific entry (142)
 * - Tests data reading from large entries
 * - Iterator advancement error detection
 * - Detailed progress logging
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
    auto it = reader->begin();
    auto end = reader->end();
    
    // Skip to entry 142
    while (it != end && count < 142 && !it.has_error()) {
        ++count;
        std::println("Processing entry {}: {}", count, (*it).path().string());
        std::println("About to advance from entry {}...", count);
        ++it;
        std::println("Advanced successfully");
    }
    
    if (it.has_error()) {
        std::println(stderr, "Iterator error at entry {}", count);
        return 1;
    }
    
    if (count != 142 || it == end) {
        std::println(stderr, "Could not reach entry 142. Count: {}, at end: {}", count, it == end);
        return 1;
    }
    
    // Now we're at entry 142 (the large file)
    const auto& entry = *it;
    std::println("Entry 142: {} (size: {})", entry.path().string(), entry.size());
    
    // Read some data from it
    auto data_result = entry.read_data(0, 1024);
    if (!data_result) {
        std::println(stderr, "Failed to read data: {}", data_result.error().message());
        return 1;
    }
    
    std::println("Successfully read {} bytes", data_result->size());
    
    // Now try to advance - this is where the error should occur
    std::println("About to advance iterator from entry 142...");
    ++it;
    
    if (it.has_error()) {
        std::println(stderr, "Iterator error after advancing");
    } else if (it == end) {
        std::println("Reached end after advancing");
    } else {
        const auto& next_entry = *it;
        std::println("Successfully advanced to entry 143: {}", next_entry.path().string());
    }
    
    return 0;
}