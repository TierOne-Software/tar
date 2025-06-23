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
 * test_pax_parser - Unit test for PAX header parsing (no parameters required).
 * 
 * Usage: ./test_pax_parser
 * 
 * Features:
 * - PAX header format validation
 * - GNU sparse marker detection
 * - Hardcoded test cases
 */

#include <tierone/tar/pax_parser.hpp>
#include <iostream>
#include <span>
#include <print>

int main() {
    // Test PAX header parsing with sample data
    // Format: "length key=value\n" where length includes the entire record
    const std::string pax_data =
        "27 path=long/file/name.txt\n"        // 27 chars total (including \n)
        "20 GNU.sparse.major=1\n"             // 20 chars total  
        "20 GNU.sparse.minor=0\n"             // 20 chars total
        "25 GNU.sparse.realsize=1024\n"       // 25 chars total
        "27 GNU.sparse.map=0,512,1024,0\n";   // 27 chars total

    const std::span data_span{
        reinterpret_cast<const std::byte*>(pax_data.data()), 
        pax_data.size()
    };
    
    auto result = tierone::tar::pax::parse_pax_headers(data_span);
    if (!result) {
        std::println(stderr, "Failed to parse PAX headers: {}", result.error().message());
        return 1;
    }
    
    std::println("Parsed PAX headers:");
    for (const auto& [key, value] : *result) {
        std::println("  {} = {}", key, value);
    }
    
    // Test GNU sparse detection
    const bool has_sparse = tierone::tar::pax::has_gnu_sparse_markers(*result);
    std::println("Has GNU sparse markers: {}", (has_sparse ? "yes" : "no"));
    
    if (has_sparse) {
        auto version = tierone::tar::pax::get_gnu_sparse_version(*result);
        std::println("GNU sparse version: {}.{}", version.first, version.second);
    }
    
    return 0;
}