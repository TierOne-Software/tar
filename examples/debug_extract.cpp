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
 * debug_extract - Debug extraction process with manual iterator control.
 * 
 * Usage: ./debug_extract <tar_file>
 * 
 * Features:
 * - Step-by-step iterator control
 * - Limited entry processing (10 entries)
 * - Iterator state validation
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
            const auto& entry = *it;
            ++count;
            
            std::println("{}: {}", count, entry.path().string());
            
            if (count > 10) {
                std::println("...stopping at 10 entries for debug");
                break;
            }
            
            ++it;
        }
        
        if (it.has_error()) {
            std::println(stderr, "Iterator error detected");
        }
        
    } catch (const std::exception& e) {
        std::println(stderr, "Exception: {}", e.what());
    }
    
    std::println(stderr, "Processed {} entries total", count);
    return 0;
}