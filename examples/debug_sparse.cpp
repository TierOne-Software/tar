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
 * debug_sparse - Debug tool for sparse file format issues.
 * 
 * Usage: ./debug_sparse <tar_file>
 * 
 * Features:
 * - Sparse format debugging
 * - Version-specific testing
 * - Segment information display
 */

#include <tierone/tar/tar.hpp>
#include <tierone/tar/header_parser.hpp>
#include <iostream>
#include <fstream>
#include <array>
#include <iomanip>
#include <print>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::println(stderr, "Usage: {} <tar_file>", argv[0]);
        return 1;
    }
    
    // Read the first block manually
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1;
    }
    
    std::array<std::byte, 512> block;
    file.read(reinterpret_cast<char*>(block.data()), block.size());
    
    if (!file) {
        std::cerr << "Failed to read first block\n";
        return 1;
    }
    
    std::cout << "First block analysis:\n";
    
    // Check typeflag
    auto* header = reinterpret_cast<const tierone::tar::ustar_header*>(block.data());
    std::cout << "  typeflag: '" << header->typeflag << "' (0x" 
              << std::hex << static_cast<int>(header->typeflag) << std::dec << ")\n";
    
    // Check magic
    std::string_view magic(header->magic, 6);
    std::cout << "  magic: '" << magic << "'\n";
    
    // Check version
    std::string_view version(header->version, 2);
    std::cout << "  version: '" << version << "'\n";
    
    // Check name
    std::string_view name(header->name, 100);
    std::cout << "  name: '" << name.substr(0, name.find('\0')) << "'\n";
    
    // Print sparse area in hex
    std::cout << "\nSparse area (padding[0:104]):\n";
    for (int i = 0; i < 104; i += 12) {
        std::cout << "  [" << std::setw(3) << i << "]: ";
        for (int j = 0; j < 12 && i+j < 104; ++j) {
            std::cout << std::setw(2) << std::setfill('0') << std::hex 
                      << static_cast<int>(static_cast<unsigned char>(header->padding[i+j])) 
                      << " ";
        }
        std::cout << " | ";
        for (int j = 0; j < 12 && i+j < 104; ++j) {
            char c = header->padding[i+j];
            std::cout << (c >= ' ' && c < 127 ? c : '.');
        }
        std::cout << std::dec << "\n";
    }
    
    // Try parsing sparse data manually
    std::cout << "\nManual sparse parsing:\n";
    const auto* sparse_header = reinterpret_cast<const tierone::tar::sparse::gnu_sparse_header*>(header);
    
    for (int i = 0; i < 4; ++i) {
        std::cout << "  Entry " << i << ":\n";
        std::cout << "    offset: '";
        for (int j = 0; j < 12; ++j) {
            std::cout << sparse_header->sp[i].offset[j];
        }
        std::cout << "'\n    numbytes: '";
        for (int j = 0; j < 12; ++j) {
            std::cout << sparse_header->sp[i].numbytes[j];
        }
        std::cout << "'\n";
    }
    
    std::cout << "  realsize: '";
    for (int j = 0; j < 12; ++j) {
        std::cout << sparse_header->realsize[j];
    }
    std::cout << "'\n";
    
    // Try to parse the header
    auto result = tierone::tar::detail::parse_header(block);
    if (result) {
        std::cout << "\nHeader parsed successfully:\n";
        std::cout << "  path: " << result->path << "\n";
        std::cout << "  type: " << static_cast<int>(result->type) << "\n";
        std::cout << "  size: " << result->size << "\n";
        std::cout << "  is_sparse: " << (result->sparse_info.has_value() ? "yes" : "no") << "\n";
        
        if (result->sparse_info) {
            std::cout << "  sparse segments: " << result->sparse_info->segments.size() << "\n";
        }
    } else {
        std::cout << "\nFailed to parse header: " << result.error().message() << "\n";
    }
    
    // Now test with the library
    std::cout << "\nTesting with library:\n";
    auto reader = tierone::tar::open_archive(argv[1]);
    if (!reader) {
        std::println(stderr, "Failed to open archive: {}", reader.error().message());
        return 1;
    }
    
    size_t count = 0;
    for (const auto& entry : *reader) {
        ++count;
        std::cout << "Entry " << count << ": " << entry.path() << "\n";
    }
    std::cout << "Total entries: " << count << "\n";
    
    return 0;
}