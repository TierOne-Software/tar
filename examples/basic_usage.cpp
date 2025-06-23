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
 * basic_usage - Shows how to open and iterate through a tar archive, displaying metadata and content previews.
 * 
 * Usage: ./basic_usage <tar_file>
 * 
 * Features demonstrated:
 * - Opening tar archives
 * - Iterating through entries
 * - Displaying entry metadata (type, size, modification time, path)
 * - Reading file content previews
 */

#include <tierone/tar/tar.hpp>
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
    
    std::println("Archive contents:");
    std::println("================");
    
    // Iterate through all entries
    for (const auto& entry : *reader) {
        // Print entry information
        char type_char = 'f';  // regular file
        if (entry.is_directory()) type_char = 'd';
        else if (entry.is_symbolic_link()) type_char = 'l';
        else if (entry.is_hard_link()) type_char = 'h';
        
        auto time_t = std::chrono::system_clock::to_time_t(entry.modification_time());
        
        std::println("{} {:>10} {} {}",
            type_char,
            entry.size(),
            std::format("{:%Y-%m-%d %H:%M}", 
                std::chrono::system_clock::from_time_t(time_t)),
            entry.path().string());
        
        // For text files, show first few bytes
        if (entry.is_regular_file() && 
            entry.path().extension() == ".txt" && 
            entry.size() > 0) {
            
            auto data = entry.read_data(0, 50);  // Read first 50 bytes
            if (data && !data->empty()) {
                std::print("  Preview: ");
                for (auto byte : *data) {
                    auto c = static_cast<char>(byte);
                    if (std::isprint(c) && c != '\n' && c != '\r') {
                        std::print("{}", c);
                    } else if (c == '\n') {
                        break;  // Stop at first newline
                    } else {
                        std::print(".");
                    }
                }
                std::println("");
            }
        }
    }
    
    return 0;
}