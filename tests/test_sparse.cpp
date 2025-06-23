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

#include <catch2/catch_test_macros.hpp>
#include <tierone/tar/sparse.hpp>
#include <tierone/tar/pax_parser.hpp>
#include <tierone/tar/stream.hpp>
#include <sstream>

using namespace tierone::tar;

TEST_CASE("Sparse metadata operations", "[sparse]") {
    SECTION("find_segment") {
        sparse::sparse_metadata meta;
        meta.real_size = 1000;
        meta.segments = {
            {0, 100},      // 0-99
            {200, 100},    // 200-299
            {500, 100}     // 500-599
        };
        
        // Test finding segments
        CHECK(meta.find_segment(50) == 0);
        CHECK(meta.find_segment(250) == 1);
        CHECK(meta.find_segment(550) == 2);
        
        // Test holes
        CHECK(!meta.find_segment(150));
        CHECK(!meta.find_segment(350));
        CHECK(!meta.find_segment(700));
    }
    
    SECTION("total_data_size") {
        sparse::sparse_metadata meta;
        meta.segments = {
            {0, 100},
            {200, 200},
            {500, 300}
        };
        
        CHECK(meta.total_data_size() == 600);
    }
}

TEST_CASE("PAX header parsing", "[sparse][pax]") {
    SECTION("Parse GNU sparse version") {
        // PAX format: "length key=value\n" where length includes the entire record
        std::string pax_data = "22 GNU.sparse.major=1\n"     
                              "22 GNU.sparse.minor=0\n"     
                              "28 GNU.sparse.realsize=1000\n";
        
        auto headers = pax::parse_pax_headers(std::span{
            reinterpret_cast<const std::byte*>(pax_data.data()),
            pax_data.size()
        });
        
        if (!headers.has_value()) {
            INFO("PAX parsing error: " << headers.error().message());
        }
        REQUIRE(headers.has_value());
        CHECK(headers->size() == 3);
        
        auto version = pax::get_gnu_sparse_version(*headers);
        CHECK(version.first == 1);
        CHECK(version.second == 0);
        
        CHECK(pax::has_gnu_sparse_markers(*headers));
    }
    
    SECTION("Parse mixed PAX headers") {
        std::string pax_data = "13 path=test\n"         
                              "22 GNU.sparse.major=1\n" 
                              "16 size=1234567\n";
        
        auto headers = pax::parse_pax_headers(std::span{
            reinterpret_cast<const std::byte*>(pax_data.data()),
            pax_data.size()
        });
        
        if (!headers.has_value()) {
            INFO("PAX parsing error: " << headers.error().message());
        }
        REQUIRE(headers.has_value());
        CHECK(headers->size() == 3);
        CHECK(headers->contains("path"));
        CHECK(headers->contains("GNU.sparse.major"));
        CHECK(headers->contains("size"));
    }
}

TEST_CASE("Sparse 1.0 data map parsing", "[sparse]") {
    SECTION("Valid sparse map") {
        // Format: count offset1 size1 offset2 size2 ... realsize 0
        std::stringstream ss;
        ss << "2\n";           // 2 segments
        ss << "0\n100\n";      // segment 1: offset=0, size=100
        ss << "200\n100\n";    // segment 2: offset=200, size=100
        ss << "1000\n";        // real size
        ss << "0\n";           // terminator
        
        std::string data = ss.str();
        
        memory_mapped_stream stream{std::span{
            reinterpret_cast<const std::byte*>(data.data()), 
            data.size()
        }};
        
        auto result = sparse::parse_sparse_1_0_data_map(stream, 1000);
        
        REQUIRE(result.has_value());
        CHECK(result->segments.size() == 2);
        CHECK(result->segments[0].offset == 0);
        CHECK(result->segments[0].size == 100);
        CHECK(result->segments[1].offset == 200);
        CHECK(result->segments[1].size == 100);
        CHECK(result->real_size == 1000);
    }
    
    SECTION("Empty sparse map") {
        std::string data = "0\n1000\n0\n";
        
        memory_mapped_stream stream{std::span{
            reinterpret_cast<const std::byte*>(data.data()), 
            data.size()
        }};
        
        auto result = sparse::parse_sparse_1_0_data_map(stream, 1000);
        
        REQUIRE(result.has_value());
        CHECK(result->segments.empty());
        CHECK(result->real_size == 1000);
    }
}