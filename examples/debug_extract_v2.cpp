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
 * debug_extract_v2 - Debug extraction process with manual iterator control.
 * 
 * Usage: ./debug_extract_v2 <tar_file>
 * 
 * Features:
 * - Step-by-step iterator control
 * - Limited entry processing (10 entries)
 * - Iterator state validation
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
    try {
        for (const auto& entry : *reader) {
            ++count;
            
            std::cout << count << ": " << entry.path().string();
            
            // Try extract_to_path like the real extractor does
            auto temp_path = std::filesystem::temp_directory_path() / "test_extract_temp";
            auto dest_path = temp_path / entry.path();
            
            auto result = entry.extract_to_path(dest_path);
            if (result) {
                std::cout << " [extracted]";
            } else {
                std::cout << " [extract failed: " << result.error().message() << "]";
            }
            
            std::cout << '\n';
            
            if (count >= 10) {
                std::cout << "Stopping at 10 for debug\n";
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception after " << count << " entries: " << e.what() << '\n';
    }
    
    std::cerr << "Total processed: " << count << '\n';
    return 0;
}