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
 * debug_large_entry - Tests handling of large files in archives.
 * 
 * Usage: ./debug_large_entry <tar_file>
 * 
 * Features:
 * - Focus on specific large entries
 * - Iterator advancement tracking
 * - Error detection after large reads
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
    auto it = reader->begin();
    auto end = reader->end();
    
    while (it != end && count < 145) {
        ++count;
        const auto& entry = *it;
        
        std::cout << count << ": " << entry.path().string() 
                  << " (size: " << entry.size() << ")\n";
        
        // Focus on entry 142 (the large file)
        if (count == 142) {
            std::println("Processing large entry 142...");
            
            // Try to read some data to see if that causes issues
            auto data_result = entry.read_data(0, 1024);  // Read first 1KB
            if (data_result) {
                std::println("Successfully read {} bytes from entry 142", data_result->size());
            } else {
                std::println("Failed to read data from entry 142: {}", data_result.error().message());
            }
        }
        
        std::println("About to advance iterator after entry {}...", count);
        
        // Try to advance
        ++it;
        
        std::println("Advanced iterator successfully");
        
        // Check if we've hit an error
        if (it.has_error()) {
            std::println(stderr, "Iterator error detected after advancing from entry {}", count);
            break;
        }
        
        // Check if we've hit end
        if (it == end) {
            std::println("Iterator reached end after entry {}", count);
            break;
        }
    }
    
    std::println("Final count: {}", count);
    return 0;
}