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
 * debug_iteration - Debug tool for testing archive iterator behavior.
 * 
 * Usage: ./debug_iteration <tar_file>
 * 
 * Features:
 * - Iterator advancement tracking
 * - End-of-archive detection
 * - Error state checking during iteration
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
    try {
        auto it = reader->begin();
        auto end = reader->end();
        
        while (it != end) {
            ++count;
            const auto& entry = *it;
            
            std::println("{}: {}", count, entry.path().string());
            
            // Try to advance
            auto old_it = it;
            ++it;
            
            // Check if we've hit end unexpectedly
            if (it == end) {
                std::println("Iterator reached end after entry {}", count);
                break;
            }
        }
        
        if (it.has_error()) {
            std::println(stderr, "Iterator error detected at entry {}", count);
        }
        
    } catch (const std::exception& e) {
        std::println(stderr, "Exception at entry {}: {}", count, e.what());
    }
    
    std::println("Final count: {}", count);
    return 0;
}