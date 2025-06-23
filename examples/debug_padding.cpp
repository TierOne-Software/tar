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
 * debug_padding - Debug tool for checking tar entry padding calculations.
 * 
 * Usage: ./debug_padding <tar_file>
 * 
 * Features:
 * - Calculates padding requirements for entries
 * - Focuses on specific entry range (140-145)
 * - Shows size and padding information
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
    
    size_t count = 0;
    for (const auto& entry : *reader) {
        ++count;
        
        // Focus on entries around 142
        if (count >= 140) {
            std::cout << count << ": " << entry.path().string() 
                      << " (size: " << entry.size() 
                      << ", padding needed: " << ((512 - (entry.size() % 512)) % 512) << ")\n";
        }
        
        if (count >= 145) {
            std::cout << "Stopping at 145 for debug\n";
            break;
        }
    }
    
    std::cout << "Final count: " << count << '\n';
    return 0;
}