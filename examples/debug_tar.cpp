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
 * debug_tar - General-purpose debug tool for analyzing tar archives.
 * 
 * Usage: ./debug_tar <tar_file>
 * 
 * Features:
 * - GNU extension detection
 * - Error tracking during iteration
 * - Progress reporting for large archives
 * - Detailed exception information
 */

#include <tierone/tar/tar.hpp>
#include <iostream>
#include <format>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <tar_file>", argv[0]);
        return 1;
    }
    
    // Open the tar archive
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    std::println("Debug: Starting archive iteration...");
    
    size_t entry_count = 0;
    size_t gnu_extension_count = 0;
    size_t error_count = 0;
    
    try {
        auto it = reader->begin();
        auto end = reader->end();
        
        while (it != end) {
            try {
                const auto& entry = *it;
                ++entry_count;
                
                if (entry_count % 1000 == 0) {
                    std::println("Debug: Processed {} entries...", entry_count);
                }
                
                // Check if this is a GNU extension that we might be miscounting
                if (entry.metadata().is_gnu_extension()) {
                    ++gnu_extension_count;
                    std::println("Debug: Found GNU extension entry: {} for {}", 
                              static_cast<char>(entry.metadata().type), 
                              entry.path().string());
                }
                
                ++it;
            } catch (const std::exception& e) {
                std::println(stderr, "Error processing entry {}: {}", entry_count, e.what());
                ++error_count;
                break;
            }
        }
        
        // Check if iterator indicates an error
        if (it.has_error()) {
            std::println(stderr, "Iterator reported error after {} entries", entry_count);
            ++error_count;
        }
        
    } catch (const std::exception& e) {
        std::println(stderr, "Exception during iteration: {}", e.what());
        ++error_count;
    }
    
    std::println("\nDebug Summary:");
    std::println("==============");
    std::println("Total entries processed: {}", entry_count);
    std::println("GNU extension entries: {}", gnu_extension_count);
    std::println("Errors encountered: {}", error_count);
    
    return 0;
}