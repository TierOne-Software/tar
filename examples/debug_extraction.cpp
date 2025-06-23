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
 * debug_extraction - Debug tool for testing data extraction with detailed output.
 * 
 * Usage: ./debug_extraction <tar_file>
 * 
 * Features:
 * - Sparse file detection and segment analysis
 * - Safe partial data reading (16 bytes)
 * - Full file extraction testing
 * - Exception handling during reads
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
    
    for (const auto& entry : *reader) {
        std::println("Entry: {}", entry.path().string());
        std::println("  Size: {}", entry.size());
        std::println("  Is sparse: {}", entry.metadata().sparse_info.has_value() ? "yes" : "no");
        
        if (entry.metadata().sparse_info) {
            const auto& sparse = *entry.metadata().sparse_info;
            std::println("  Real size: {}", sparse.real_size);
            std::println("  Segments: {}", sparse.segments.size());
            for (size_t i = 0; i < sparse.segments.size(); ++i) {
                const auto& seg = sparse.segments[i];
                std::println("    Segment {}: offset={} size={}", i, seg.offset, seg.size);
            }
            std::println("  Total data size: {}", sparse.total_data_size());
        }
        
        // Try to read just first few bytes safely
        std::println("  Attempting to read first 16 bytes...");
        
        try {
            if (auto result = entry.read_data(0, 16)) {
                std::println("    Successfully read {} bytes", result->size());
            } else {
                std::println("    Read failed: {}", result.error().message());
            }
        } catch (const std::exception& e) {
            std::println("    Exception during read: {}", e.what());
        }
        
        // Try to read full file to trigger the crash
        std::println("  Attempting to read full file...");
        try {
            if (auto full_result = entry.read_data()) {
                std::println("    Successfully read full file: {} bytes", full_result->size());
            } else {
                std::println("    Full read failed: {}", full_result.error().message());
            }
        } catch (const std::exception& e) {
            std::println("    Exception during full read: {}", e.what());
        }
        
        std::println("");
    }
    
    return 0;
}