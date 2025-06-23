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
 * debug_sparse_1_0 - Debug tool for PAX 1.0 sparse format issues.
 * 
 * Usage: ./debug_sparse_1_0 <tar_file>
 * 
 * Features:
 * - Sparse format debugging
 * - Version-specific testing (PAX 1.0)
 * - Segment information display
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
    
    for (const auto& entry : *reader) {
        std::cout << "Entry: " << entry.path() << '\n';
        std::cout << "  Type: " << static_cast<int>(entry.type()) << '\n';
        std::cout << "  Size: " << entry.size() << '\n';
        std::cout << "  Is sparse: " << (entry.metadata().sparse_info.has_value() ? "yes" : "no") << '\n';
        
        if (entry.metadata().sparse_info) {
            const auto& sparse = *entry.metadata().sparse_info;
            std::cout << "  Real size: " << sparse.real_size << '\n';
            std::cout << "  Segments: " << sparse.segments.size() << '\n';
            for (size_t i = 0; i < sparse.segments.size(); ++i) {
                const auto& seg = sparse.segments[i];
                std::cout << "    Segment " << i << ": offset=" << seg.offset << " size=" << seg.size << '\n';
            }
            std::cout << "  Total data size: " << sparse.total_data_size() << '\n';
        }
        std::cout << '\n';
    }
    
    return 0;
}