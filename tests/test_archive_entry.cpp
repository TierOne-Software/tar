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
#include <catch2/matchers/catch_matchers_container_properties.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <tierone/tar/archive_entry.hpp>
#include <tierone/tar/error.hpp>
#include <array>
#include <cstring>
#include <filesystem>
#include <ranges>
#include <vector>

using namespace tierone::tar;
using namespace std::chrono_literals;

namespace {

// Helper function to create test data
std::vector<std::byte> create_test_data(const std::string& content) {
    std::vector<std::byte> data;
    data.reserve(content.size());
    std::ranges::transform(content, std::back_inserter(data),
                          [](char c) { return static_cast<std::byte>(c); });
    return data;
}

// Helper function to create a basic file metadata
file_metadata create_test_metadata(entry_type type = entry_type::regular_file,
                                  uint64_t size = 100) {
    file_metadata meta;
    meta.path = "test/file.txt";
    meta.type = type;
    meta.permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
    meta.owner_id = 1000;
    meta.group_id = 1000;
    meta.size = size;
    meta.modification_time = std::chrono::system_clock::now();
    meta.owner_name = "user";
    meta.group_name = "group";
    return meta;
}

// Mock data reader function for testing
data_reader_fn create_mock_reader(const std::vector<std::byte>& data) {
    return [data](size_t offset, size_t length) -> std::expected<std::span<const std::byte>, error> {
        if (offset > data.size()) {
            return std::unexpected(error{error_code::io_error, "Offset beyond data size"});
        }
        size_t available = data.size() - offset;
        size_t to_return = std::min(length, available);
        return std::span<const std::byte>(data.data() + offset, to_return);
    };
}

// Mock failing data reader
data_reader_fn create_failing_reader() {
    return [](size_t, size_t) -> std::expected<std::span<const std::byte>, error> {
        return std::unexpected(error{error_code::io_error, "Mock read failure"});
    };
}

} // anonymous namespace

TEST_CASE("archive_entry construction", "[unit][archive_entry]") {
    SECTION("Constructor with data_reader_fn") {
        auto data = create_test_data("Hello, World!");
        auto metadata = create_test_metadata(entry_type::regular_file, data.size());
        auto reader = create_mock_reader(data);
        
        archive_entry entry(metadata, reader);
        
        CHECK(entry.path() == "test/file.txt");
        CHECK(entry.type() == entry_type::regular_file);
        CHECK(entry.size() == data.size());
        CHECK(entry.is_regular_file());
    }
    
    SECTION("Constructor with memory span") {
        auto data = create_test_data("Hello, World!");
        auto metadata = create_test_metadata(entry_type::regular_file, data.size());
        std::span<const std::byte> data_span(data);
        
        archive_entry entry(metadata, data_span);
        
        CHECK(entry.path() == "test/file.txt");
        CHECK(entry.type() == entry_type::regular_file);
        CHECK(entry.size() == data.size());
        CHECK(entry.is_regular_file());
    }
    
    SECTION("Constructor with empty data span") {
        std::vector<std::byte> empty_data;
        auto metadata = create_test_metadata(entry_type::regular_file, 0);
        std::span<const std::byte> data_span(empty_data);
        
        archive_entry entry(metadata, data_span);
        
        CHECK(entry.size() == 0);
        CHECK(entry.is_regular_file());
    }
}

TEST_CASE("archive_entry metadata accessors", "[unit][archive_entry]") {
    auto metadata = create_test_metadata();
    metadata.link_target = "/path/to/target";
    metadata.device_major = 8;
    metadata.device_minor = 1;
    
    // Add extended attributes
    metadata.xattrs["user.comment"] = "test attribute";
    metadata.xattrs["security.selinux"] = "context";
    
    // Add ACLs
    acl_entry acl1{acl_entry::type::user, 1001,
                   static_cast<acl_entry::perm>(static_cast<uint8_t>(acl_entry::perm::read) | 
                                               static_cast<uint8_t>(acl_entry::perm::write))};
    acl_entry acl2{acl_entry::type::group, 1002, acl_entry::perm::read};
    metadata.access_acl.push_back(acl1);
    metadata.default_acl.push_back(acl2);
    
    archive_entry entry(metadata, create_mock_reader({}));
    
    SECTION("Basic metadata") {
        CHECK(entry.path() == "test/file.txt");
        CHECK(entry.type() == entry_type::regular_file);
        CHECK(entry.permissions() == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
        CHECK(entry.owner_id() == 1000);
        CHECK(entry.group_id() == 1000);
        CHECK(entry.size() == 100);
        CHECK(entry.owner_name() == "user");
        CHECK(entry.group_name() == "group");
    }
    
    SECTION("Optional metadata") {
        REQUIRE(entry.link_target().has_value());
        CHECK(entry.link_target().value() == "/path/to/target");
        CHECK(entry.device_major() == 8);
        CHECK(entry.device_minor() == 1);
    }
    
    SECTION("Extended attributes") {
        CHECK(entry.has_extended_attributes());
        CHECK(entry.get_extended_attributes().size() == 2);
        CHECK(entry.get_extended_attributes().at("user.comment") == "test attribute");
        CHECK(entry.get_extended_attributes().at("security.selinux") == "context");
    }
    
    SECTION("ACLs") {
        CHECK(entry.has_acls());
        CHECK(entry.access_acl().size() == 1);
        CHECK(entry.default_acl().size() == 1);
        CHECK(entry.access_acl()[0].entry_type == acl_entry::type::user);
        CHECK(entry.access_acl()[0].id == 1001);
        CHECK(entry.default_acl()[0].entry_type == acl_entry::type::group);
        CHECK(entry.default_acl()[0].id == 1002);
    }
    
    SECTION("Full metadata access") {
        const auto& full_meta = entry.metadata();
        CHECK(full_meta.path == metadata.path);
        CHECK(full_meta.type == metadata.type);
        CHECK(full_meta.size == metadata.size);
    }
}

TEST_CASE("archive_entry type checking methods", "[unit][archive_entry]") {
    SECTION("Regular file") {
        auto metadata = create_test_metadata(entry_type::regular_file);
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.is_regular_file());
        CHECK_FALSE(entry.is_directory());
        CHECK_FALSE(entry.is_symbolic_link());
        CHECK_FALSE(entry.is_hard_link());
        CHECK_FALSE(entry.is_character_device());
        CHECK_FALSE(entry.is_block_device());
        CHECK_FALSE(entry.is_device());
    }
    
    SECTION("Directory") {
        auto metadata = create_test_metadata(entry_type::directory);
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK_FALSE(entry.is_regular_file());
        CHECK(entry.is_directory());
        CHECK_FALSE(entry.is_symbolic_link());
        CHECK_FALSE(entry.is_device());
    }
    
    SECTION("Symbolic link") {
        auto metadata = create_test_metadata(entry_type::symbolic_link);
        metadata.link_target = "/target";
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK_FALSE(entry.is_regular_file());
        CHECK(entry.is_symbolic_link());
        CHECK_FALSE(entry.is_hard_link());
    }
    
    SECTION("Hard link") {
        auto metadata = create_test_metadata(entry_type::hard_link);
        metadata.link_target = "/target";
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK_FALSE(entry.is_regular_file());
        CHECK_FALSE(entry.is_symbolic_link());
        CHECK(entry.is_hard_link());
    }
    
    SECTION("Character device") {
        auto metadata = create_test_metadata(entry_type::character_device);
        metadata.device_major = 1;
        metadata.device_minor = 3;
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.is_character_device());
        CHECK_FALSE(entry.is_block_device());
        CHECK(entry.is_device());
    }
    
    SECTION("Block device") {
        auto metadata = create_test_metadata(entry_type::block_device);
        metadata.device_major = 8;
        metadata.device_minor = 0;
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK_FALSE(entry.is_character_device());
        CHECK(entry.is_block_device());
        CHECK(entry.is_device());
    }
    
    SECTION("Sparse file") {
        auto metadata = create_test_metadata(entry_type::gnu_sparse);
        metadata.sparse_info = sparse::sparse_metadata{};
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK_FALSE(entry.is_regular_file());
        CHECK(entry.is_sparse());
    }
}

TEST_CASE("archive_entry read_data with streaming mode", "[unit][archive_entry]") {
    auto data = create_test_data("Hello, World! This is test data.");
    auto metadata = create_test_metadata(entry_type::regular_file, data.size());
    auto reader = create_mock_reader(data);
    archive_entry entry(metadata, reader);
    
    SECTION("Read all data") {
        auto result = entry.read_data();
        REQUIRE(result.has_value());
        CHECK(result->size() == data.size());
        CHECK(std::memcmp(result->data(), data.data(), data.size()) == 0);
    }
    
    SECTION("Read with offset") {
        auto result = entry.read_data(7, 5);
        REQUIRE(result.has_value());
        CHECK(result->size() == 5);
        std::string expected = "World";
        CHECK(std::memcmp(result->data(), expected.data(), 5) == 0);
    }
    
    SECTION("Read with offset and length beyond data") {
        auto result = entry.read_data(20, 100);
        REQUIRE(result.has_value());
        CHECK(result->size() == data.size() - 20);
    }
    
    SECTION("Read with offset beyond data size") {
        auto result = entry.read_data(1000, 10);
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
    
    SECTION("Read from non-regular file") {
        auto dir_metadata = create_test_metadata(entry_type::directory);
        archive_entry dir_entry(dir_metadata, reader);
        
        auto result = dir_entry.read_data();
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_operation);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("not a regular file"));
    }
    
    SECTION("Read with failing reader") {
        archive_entry failing_entry(metadata, create_failing_reader());
        
        auto result = failing_entry.read_data();
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
}

TEST_CASE("archive_entry read_data with memory-mapped mode", "[unit][archive_entry]") {
    auto data = create_test_data("Hello, World! This is test data.");
    auto metadata = create_test_metadata(entry_type::regular_file, data.size());
    std::span<const std::byte> data_span(data);
    archive_entry entry(metadata, data_span);
    
    SECTION("Read all data") {
        auto result = entry.read_data();
        REQUIRE(result.has_value());
        CHECK(result->size() == data.size());
        CHECK(result->data() == data.data()); // Zero-copy check
    }
    
    SECTION("Read with offset") {
        auto result = entry.read_data(7, 5);
        REQUIRE(result.has_value());
        CHECK(result->size() == 5);
        CHECK(result->data() == data.data() + 7); // Zero-copy check
        std::string expected = "World";
        CHECK(std::memcmp(result->data(), expected.data(), 5) == 0);
    }
    
    SECTION("Read with offset and length beyond data") {
        auto result = entry.read_data(20, 100);
        REQUIRE(result.has_value());
        CHECK(result->size() == data.size() - 20);
        CHECK(result->data() == data.data() + 20);
    }
    
    SECTION("Read with offset at exact data size") {
        auto result = entry.read_data(data.size(), 10);
        REQUIRE(result.has_value());
        CHECK(result->size() == 0);
    }
    
    SECTION("Read with offset beyond data size") {
        auto result = entry.read_data(1000, 10);
        REQUIRE(result.has_value());
        CHECK(result->size() == 0);
    }
    
    SECTION("Empty data span") {
        std::vector<std::byte> empty_data;
        auto empty_metadata = create_test_metadata(entry_type::regular_file, 0);
        std::span<const std::byte> empty_span(empty_data);
        archive_entry empty_entry(empty_metadata, empty_span);
        
        auto result = empty_entry.read_data();
        REQUIRE(result.has_value());
        CHECK(result->size() == 0);
    }
}

TEST_CASE("archive_entry copy_data_to", "[unit][archive_entry]") {
    auto data = create_test_data("Hello, World!");
    auto metadata = create_test_metadata(entry_type::regular_file, data.size());
    
    SECTION("Copy to vector with streaming mode") {
        auto reader = create_mock_reader(data);
        archive_entry entry(metadata, reader);
        
        std::vector<std::byte> output;
        auto result = entry.copy_data_to(std::back_inserter(output));
        
        REQUIRE(result.has_value());
        CHECK(result.value() == data.size());
        CHECK(output == data);
    }
    
    SECTION("Copy to vector with memory-mapped mode") {
        std::span<const std::byte> data_span(data);
        archive_entry entry(metadata, data_span);
        
        std::vector<std::byte> output;
        auto result = entry.copy_data_to(std::back_inserter(output));
        
        REQUIRE(result.has_value());
        CHECK(result.value() == data.size());
        CHECK(output == data);
    }
    
    SECTION("Copy to array") {
        std::span<const std::byte> data_span(data);
        archive_entry entry(metadata, data_span);
        
        std::array<std::byte, 1024> output{};
        auto result = entry.copy_data_to(output.begin());
        
        REQUIRE(result.has_value());
        CHECK(result.value() == data.size());
        CHECK(std::memcmp(output.data(), data.data(), data.size()) == 0);
    }
    
    SECTION("Copy from non-regular file") {
        auto dir_metadata = create_test_metadata(entry_type::directory);
        archive_entry dir_entry(dir_metadata, create_mock_reader(data));
        
        std::vector<std::byte> output;
        auto result = dir_entry.copy_data_to(std::back_inserter(output));
        
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_operation);
    }
    
    SECTION("Copy with failing reader") {
        archive_entry failing_entry(metadata, create_failing_reader());
        
        std::vector<std::byte> output;
        auto result = failing_entry.copy_data_to(std::back_inserter(output));
        
        CHECK_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
}

TEST_CASE("archive_entry edge cases", "[unit][archive_entry]") {
    SECTION("Very large file size") {
        auto metadata = create_test_metadata(entry_type::regular_file, 
                                           std::numeric_limits<uint64_t>::max());
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.size() == std::numeric_limits<uint64_t>::max());
    }
    
    SECTION("Path with special characters") {
        auto metadata = create_test_metadata();
        metadata.path = "test/file with spaces/é€£¥.txt";
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.path() == "test/file with spaces/é€£¥.txt");
    }
    
    SECTION("Very long path") {
        auto metadata = create_test_metadata();
        metadata.path = std::string(300, 'a') + "/file.txt";
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.path().string().length() > 300);
    }
    
    SECTION("All permission bits set") {
        auto metadata = create_test_metadata();
        metadata.permissions = std::filesystem::perms::all;
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.permissions() == std::filesystem::perms::all);
    }
    
    SECTION("Modification time edge cases") {
        auto metadata = create_test_metadata();
        
        // Very old timestamp
        metadata.modification_time = std::chrono::system_clock::time_point{};
        archive_entry old_entry(metadata, create_mock_reader({}));
        CHECK(old_entry.modification_time() == std::chrono::system_clock::time_point{});
        
        // Future timestamp
        metadata.modification_time = std::chrono::system_clock::now() + 365 * 24h;
        archive_entry future_entry(metadata, create_mock_reader({}));
        CHECK(future_entry.modification_time() > std::chrono::system_clock::now());
    }
    
    SECTION("Empty owner/group names") {
        auto metadata = create_test_metadata();
        metadata.owner_name = "";
        metadata.group_name = "";
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.owner_name().empty());
        CHECK(entry.group_name().empty());
    }
    
    SECTION("Maximum device numbers") {
        auto metadata = create_test_metadata(entry_type::block_device);
        metadata.device_major = std::numeric_limits<uint32_t>::max();
        metadata.device_minor = std::numeric_limits<uint32_t>::max();
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.device_major() == std::numeric_limits<uint32_t>::max());
        CHECK(entry.device_minor() == std::numeric_limits<uint32_t>::max());
    }
    
    SECTION("Sparse file with metadata") {
        auto metadata = create_test_metadata(entry_type::gnu_sparse);
        metadata.sparse_info = sparse::sparse_metadata{};
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.is_sparse());
        CHECK(entry.metadata().sparse_info.has_value());
    }
    
    SECTION("Many extended attributes") {
        auto metadata = create_test_metadata();
        for (int i = 0; i < 100; ++i) {
            metadata.xattrs["user.attr" + std::to_string(i)] = "value" + std::to_string(i);
        }
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.has_extended_attributes());
        CHECK(entry.get_extended_attributes().size() == 100);
    }
    
    SECTION("Complex ACLs") {
        auto metadata = create_test_metadata();
        
        // Add many ACL entries
        for (uint32_t i = 1000; i < 1020; ++i) {
            metadata.access_acl.push_back({acl_entry::type::user, i, acl_entry::perm::read});
            metadata.default_acl.push_back({acl_entry::type::group, i,
                                           static_cast<acl_entry::perm>(static_cast<uint8_t>(acl_entry::perm::read) | 
                                                                        static_cast<uint8_t>(acl_entry::perm::write))});
        }
        
        archive_entry entry(metadata, create_mock_reader({}));
        
        CHECK(entry.has_acls());
        CHECK(entry.access_acl().size() == 20);
        CHECK(entry.default_acl().size() == 20);
    }
}

TEST_CASE("archive_entry concurrent data access", "[unit][archive_entry]") {
    auto data = create_test_data("Concurrent test data");
    auto metadata = create_test_metadata(entry_type::regular_file, data.size());
    
    SECTION("Multiple read_data calls") {
        std::span<const std::byte> data_span(data);
        archive_entry entry(metadata, data_span);
        
        // Multiple reads should all succeed
        auto result1 = entry.read_data(0, 10);
        auto result2 = entry.read_data(5, 10);
        auto result3 = entry.read_data(10, 10);
        
        CHECK(result1.has_value());
        CHECK(result2.has_value());
        CHECK(result3.has_value());
        
        // Verify they point to correct locations
        CHECK(result1->data() == data.data());
        CHECK(result2->data() == data.data() + 5);
        CHECK(result3->data() == data.data() + 10);
    }
}