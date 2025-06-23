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
 * extract_files - Extracts all files from a tar archive to a specified directory.
 * 
 * Usage: ./extract_files <tar_file> <output_dir>
 * 
 * Features demonstrated:
 * - Directory creation
 * - File extraction with error handling
 * - Progress tracking with byte count statistics
 */

#include <tierone/tar/tar.hpp>
#include <filesystem>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::println(stderr, "Usage: {} <tar_file> <output_dir>", argv[0]);
        return 1;
    }

    const std::filesystem::path tar_file = argv[1];
    const std::filesystem::path output_dir = argv[2];
    
    // Create output directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        std::println(stderr, "Failed to create output directory: {}", ec.message());
        return 1;
    }
    
    // Open the tar archive
    auto reader = tierone::tar::open_archive(tar_file);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    std::println("Extracting archive to: {}", output_dir.string());
    
    size_t extracted_count = 0;
    size_t total_bytes = 0;
    
    // Extract all entries
    for (const auto& entry : *reader) {
        auto dest_path = output_dir / entry.path();
        
        std::print("Extracting: {}", entry.path().string());
        
        auto result = entry.extract_to_path(dest_path);
        if (result) {
            std::println(" ✓");
            extracted_count++;
            total_bytes += entry.size();
        } else {
            std::println(" ✗ ({})", result.error().message());
        }
    }
    
    std::println("\nExtraction complete:");
    std::println("  Files extracted: {}", extracted_count);
    std::println("  Total bytes: {}", total_bytes);
    
    return 0;
}