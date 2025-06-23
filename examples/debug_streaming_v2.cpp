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
 * debug_streaming_v2 - Tests reading data while iterating through archives.
 * 
 * Usage: ./debug_streaming_v2 <tar_file>
 * 
 * Features:
 * - Data reading during iteration
 * - Iterator stability testing
 * - Small file content display
 */

#include <tierone/tar/tar.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <tar_file>\n";
        return 1;
    }
    
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::cerr << "Failed to open archive: " << reader.error().message() << '\n';
        return 1;
    }
    
    size_t count = 0;
    for (const auto& entry : *reader) {
        ++count;
        
        std::cout << count << ": " << entry.path().string();
        std::cout << " (size: " << entry.size() << ")";
        
        // Try to read data from entry 3 to see if it breaks the iterator
        if (count == 3 && entry.is_regular_file()) {
            std::cout << " [reading data...]";
            auto data = entry.read_data();
            if (data) {
                std::cout << " [read " << data->size() << " bytes]";
            } else {
                std::cout << " [read failed: " << data.error().message() << "]";
            }
        }
        
        std::cout << '\n';
        
        if (count >= 10) {
            std::cout << "Checking if we can continue after reading data...\n";
        }
    }
    
    std::cerr << "Total processed: " << count << '\n';
    return 0;
}