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
 * sparse_demo - Handles sparse files in tar archives, showing hole information and compression ratios.
 * 
 * Usage: ./sparse_demo <tar_file>
 * 
 * Features demonstrated:
 * - Sparse file detection
 * - Hole/data segment mapping
 * - Compression ratio calculation
 * - Sparse data reading
 * 
 * Creating test sparse files:
 * truncate -s 1G sparse_file
 * echo "data" | dd of=sparse_file bs=1 seek=1000 conv=notrunc
 * echo "more data" | dd of=sparse_file bs=1 seek=1000000 conv=notrunc
 * tar --sparse -cf sparse.tar sparse_file
 */

#include <tierone/tar/tar.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <print>

void print_sparse_info(const tierone::tar::archive_entry& entry) {
    if (!entry.is_sparse()) {
        return;
    }
    
    auto& metadata = entry.metadata();
    if (!metadata.sparse_info) {
        return;
    }
    
    const auto& sparse_info = *metadata.sparse_info;
    std::println("  Sparse file information:");
    std::println("    Real size: {} bytes", sparse_info.real_size);
    std::println("    Data size: {} bytes", sparse_info.total_data_size());
    std::println("    Compression ratio: {:.1f}%", 
              (100.0 * (1.0 - static_cast<double>(sparse_info.total_data_size()) / 
                                  static_cast<double>(sparse_info.real_size))));
    std::println("    Data segments: {}", sparse_info.segments.size());
    
    // Show first few segments
    const size_t show_count = std::min(static_cast<size_t>(5), sparse_info.segments.size());
    for (size_t i = 0; i < show_count; ++i) {
        const auto& seg = sparse_info.segments[i];
        std::println("      [{}] offset={}, size={}", i, seg.offset, seg.size);
    }
    if (sparse_info.segments.size() > show_count) {
        std::println("      ... and {} more segments", (sparse_info.segments.size() - show_count));
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <tar_file>", argv[0]);
        std::println(stderr, "\nThis demo shows sparse file handling in tar archives.");
        std::println(stderr, "Sparse files contain 'holes' (regions of zeros) that are");
        std::println(stderr, "stored efficiently in the archive.");
        return 1;
    }
    
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    std::println("Scanning for sparse files in archive...");
    std::println("{}\n", std::string(50, '='));
    
    size_t total_files = 0;
    size_t sparse_files = 0;
    uint64_t total_real_size = 0;
    uint64_t total_data_size = 0;
    
    for (const auto& entry : *reader) {
        ++total_files;
        
        if (entry.is_regular_file() && entry.metadata().sparse_info) {
            ++sparse_files;
            const auto& sparse_info = *entry.metadata().sparse_info;
            total_real_size += sparse_info.real_size;
            total_data_size += sparse_info.total_data_size();
            
            std::println("Sparse file: {}", entry.path().string());
            print_sparse_info(entry);
            
            // Demonstrate reading from a sparse file
            std::print("  Reading first 256 bytes:\n    ");
            if (auto data_result = entry.read_data(0, 256)) {
                // Count zeros vs non-zeros
                size_t zeros = 0;
                size_t non_zeros = 0;
                for (auto byte : *data_result) {
                    if (byte == std::byte{0}) {
                        ++zeros;
                    } else {
                        ++non_zeros;
                    }
                }
                std::println("Read {} bytes: {} zeros, {} non-zeros", data_result->size(), zeros, non_zeros);
            } else {
                std::println("Failed to read: {}", data_result.error().message());
            }
            std::println("");
        }
    }
    
    std::println("Summary:");
    std::println("========");
    std::println("Total files: {}", total_files);
    std::println("Sparse files: {}", sparse_files);
    
    if (sparse_files > 0) {
        std::println("Total real size: {} bytes", total_real_size);
        std::println("Total data size: {} bytes", total_data_size);
        std::println("Space saved: {} bytes ({:.1f}%)", 
                  (total_real_size - total_data_size),
                  (100.0 * (1.0 - static_cast<double>(total_data_size) / 
                                      static_cast<double>(total_real_size))));
    } else {
        std::println("\nNo sparse files found in this archive.");
        std::println("To test sparse file support, create a sparse file with:");
        std::println("  dd if=/dev/zero of=sparse.bin bs=1M seek=100 count=0");
        std::println("  echo 'data' | dd of=sparse.bin bs=1 seek=1000 conv=notrunc");
        std::println("Then add it to a tar archive with GNU tar:");
        std::println("  tar -cSf sparse.tar sparse.bin");
    }
    
    return 0;
}