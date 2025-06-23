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
 * gnu_tar_demo - Demonstrates GNU tar format extensions including long filenames and link targets.
 * 
 * Usage: ./gnu_tar_demo <tar_file>
 * 
 * Features demonstrated:
 * - Long filename support (>100 characters)
 * - Long link target support
 * - GNU vs POSIX format detection
 * - Extension usage statistics
 */

#include <tierone/tar/tar.hpp>
#include <format>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <tar_file>", argv[0]);
        std::println(stderr, "This example demonstrates GNU tar format support including long filenames.");
        return 1;
    }
    
    // Open the tar archive
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    std::println("Archive contents (with GNU tar support):");
    std::println("========================================");
    
    size_t entry_count = 0;
    size_t long_filename_count = 0;
    size_t long_linkname_count = 0;
    
    // Iterate through all entries
    for (const auto& entry : *reader) {
        ++entry_count;
        
        // Progress indicator for large archives
        if (entry_count % 1000 == 0) {
            std::println(stderr, "Processed {} entries...", entry_count);
        }
        
        // Determine entry type character
        char type_char = '?';
        std::string type_desc;
        
        if (entry.is_regular_file()) {
            type_char = 'f';
            type_desc = "file";
        } else if (entry.is_directory()) {
            type_char = 'd';
            type_desc = "directory";
        } else if (entry.is_symbolic_link()) {
            type_char = 'l';
            type_desc = "symlink";
        } else if (entry.is_hard_link()) {
            type_char = 'h';
            type_desc = "hardlink";
        }
        
        // Check for long filenames (GNU extension)
        std::string path_str = entry.path().string();
        bool is_long_filename = path_str.length() > 100;
        if (is_long_filename) {
            ++long_filename_count;
        }
        
        // Check for long link targets
        bool is_long_linkname = false;
        if (entry.link_target() && entry.link_target()->length() > 100) {
            is_long_linkname = true;
            ++long_linkname_count;
        }
        
        // Format timestamp
        const auto time_t = std::chrono::system_clock::to_time_t(entry.modification_time());
        
        // Only print first 50 entries to avoid overwhelming output for large archives
        if (entry_count <= 50) {
            // Print entry information
            std::print("{} {:>10} {} {}{}",
                type_char,
                entry.size(),
                std::format("{:%Y-%m-%d %H:%M}", 
                    std::chrono::system_clock::from_time_t(time_t)),
                path_str,
                is_long_filename ? " [LONG-NAME]" : "");
            
            // Show link target if present
            if (entry.link_target()) {
                std::print(" -> {}{}",
                    *entry.link_target(),
                    is_long_linkname ? " [LONG-LINK]" : "");
            }
            
            std::println("");
        }
        
        // For small text files in the first few entries, show a preview
        if (entry_count <= 10 && entry.is_regular_file() && 
            entry.path().extension() == ".txt" && 
            entry.size() > 0 && entry.size() < 1000) {
            
            auto data = entry.read_data(0, 100);  // Read first 100 bytes
            if (data && !data->empty()) {
                std::print("    Preview: ");
                for (auto byte : *data) {
                    if (auto c = static_cast<char>(byte); std::isprint(c) && c != '\n' && c != '\r') {
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
    
    // Print summary
    std::println("\nSummary:");
    std::println("========");
    std::println("Total entries: {}", entry_count);
    std::println("Long filenames (>100 chars): {}", long_filename_count);
    std::println("Long link targets (>100 chars): {}", long_linkname_count);
    
    if (long_filename_count > 0 || long_linkname_count > 0) {
        std::println("\nThis archive uses GNU tar extensions for long names!");
        std::println("Standard POSIX tar would truncate names at 100 characters.");
    } else {
        std::println("\nThis archive is compatible with standard POSIX tar format.");
    }
    
    return 0;
}