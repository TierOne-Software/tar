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
#include <catch2/matchers/catch_matchers_string.hpp>
#include <tierone/tar/archive_entry.hpp>
#include <tierone/tar/error.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <random>

using namespace tierone::tar;
namespace fs = std::filesystem;

namespace {

class TempDirectory {
    fs::path path_;
public:
    TempDirectory() {
        // Create unique temp directory
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        auto temp = fs::temp_directory_path();
        path_ = temp / ("tierone_test_" + std::to_string(dis(gen)));
        fs::create_directories(path_);
    }
    
    ~TempDirectory() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    
    const fs::path& path() const { return path_; }
    operator fs::path() const { return path_; }
};

// Helper to create test data
std::vector<std::byte> create_test_data(const std::string& content) {
    std::vector<std::byte> data;
    data.reserve(content.size());
    std::ranges::transform(content, std::back_inserter(data),
                          [](char c) { return static_cast<std::byte>(c); });
    return data;
}

// Helper to read file content
std::string read_file_content(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), 
                      std::istreambuf_iterator<char>());
}

// Mock data reader
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

// Create basic file metadata
file_metadata create_file_metadata(const std::string& path, 
                                  entry_type type = entry_type::regular_file,
                                  uint64_t size = 0) {
    file_metadata meta;
    meta.path = path;
    meta.type = type;
    meta.permissions = fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read;
    meta.owner_id = 1000;
    meta.group_id = 1000;
    meta.size = size;
    meta.modification_time = std::chrono::system_clock::now();
    meta.owner_name = "user";
    meta.group_name = "group";
    return meta;
}

} // anonymous namespace

TEST_CASE("archive_entry extract regular files", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Extract simple file") {
        auto data = create_test_data("Hello, World!");
        auto metadata = create_file_metadata("test.txt", entry_type::regular_file, data.size());
        archive_entry entry(metadata, create_mock_reader(data));
        
        auto dest = temp_dir.path() / "output" / "test.txt";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::is_regular_file(dest));
        CHECK(read_file_content(dest) == "Hello, World!");
    }
    
    SECTION("Extract file with subdirectories") {
        auto data = create_test_data("Nested content");
        auto metadata = create_file_metadata("dir1/dir2/nested.txt", 
                                           entry_type::regular_file, data.size());
        archive_entry entry(metadata, create_mock_reader(data));
        
        auto dest = temp_dir.path() / "dir1" / "dir2" / "nested.txt";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::exists(dest.parent_path()));
        CHECK(read_file_content(dest) == "Nested content");
    }
    
    SECTION("Extract empty file") {
        std::vector<std::byte> empty_data;
        auto metadata = create_file_metadata("empty.txt", entry_type::regular_file, 0);
        archive_entry entry(metadata, create_mock_reader(empty_data));
        
        auto dest = temp_dir.path() / "empty.txt";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::file_size(dest) == 0);
    }
    
    SECTION("Extract large file") {
        std::string large_content(1024 * 1024, 'X'); // 1MB
        auto data = create_test_data(large_content);
        auto metadata = create_file_metadata("large.bin", 
                                           entry_type::regular_file, data.size());
        archive_entry entry(metadata, create_mock_reader(data));
        
        auto dest = temp_dir.path() / "large.bin";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::file_size(dest) == data.size());
    }
    
    SECTION("Overwrite existing file") {
        // Create existing file
        auto dest = temp_dir.path() / "existing.txt";
        {
            std::ofstream file(dest);
            file << "Old content";
        }
        CHECK(fs::exists(dest));
        
        auto data = create_test_data("New content");
        auto metadata = create_file_metadata("existing.txt", 
                                           entry_type::regular_file, data.size());
        archive_entry entry(metadata, create_mock_reader(data));
        
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(read_file_content(dest) == "New content");
    }
}

TEST_CASE("archive_entry extract directories", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Extract simple directory") {
        auto metadata = create_file_metadata("testdir", entry_type::directory);
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "testdir";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::is_directory(dest));
    }
    
    SECTION("Extract nested directory") {
        auto metadata = create_file_metadata("dir1/dir2/dir3", entry_type::directory);
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "dir1" / "dir2" / "dir3";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::is_directory(dest));
        CHECK(fs::exists(dest.parent_path()));
    }
    
    SECTION("Extract directory that already exists") {
        auto dest = temp_dir.path() / "existing_dir";
        fs::create_directories(dest);
        
        auto metadata = create_file_metadata("existing_dir", entry_type::directory);
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::is_directory(dest));
    }
}

TEST_CASE("archive_entry extract symbolic links", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Extract absolute symlink") {
        auto metadata = create_file_metadata("symlink", entry_type::symbolic_link);
        metadata.link_target = "/usr/bin/test";
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "symlink";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::is_symlink(dest));
        CHECK(fs::read_symlink(dest) == "/usr/bin/test");
    }
    
    SECTION("Extract relative symlink") {
        auto metadata = create_file_metadata("link_to_file", entry_type::symbolic_link);
        metadata.link_target = "../target.txt";
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "subdir" / "link_to_file";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::is_symlink(dest));  // Use is_symlink for broken links
        CHECK(fs::read_symlink(dest) == "../target.txt");
    }
    
    SECTION("Extract symlink without target") {
        auto metadata = create_file_metadata("bad_symlink", entry_type::symbolic_link);
        // No link_target set
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "bad_symlink";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_operation);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("no target"));
    }
}

TEST_CASE("archive_entry extract hard links", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Extract hard link to existing file") {
        // Create target file first
        auto target_path = temp_dir.path() / "target.txt";
        {
            std::ofstream file(target_path);
            file << "Target content";
        }
        
        auto metadata = create_file_metadata("hardlink", entry_type::hard_link);
        metadata.link_target = target_path.string();
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "hardlink";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        CHECK(fs::hard_link_count(dest) == 2);
        CHECK(fs::hard_link_count(target_path) == 2);
        CHECK(read_file_content(dest) == "Target content");
    }
    
    SECTION("Extract hard link to non-existent file") {
        auto metadata = create_file_metadata("hardlink", entry_type::hard_link);
        metadata.link_target = temp_dir.path() / "nonexistent.txt";
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "hardlink";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
    
    SECTION("Extract hard link without target") {
        auto metadata = create_file_metadata("bad_hardlink", entry_type::hard_link);
        // No link_target set
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "bad_hardlink";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_operation);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("no target"));
    }
}

TEST_CASE("archive_entry extract unsupported types", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Character device") {
        auto metadata = create_file_metadata("chardev", entry_type::character_device);
        metadata.device_major = 1;
        metadata.device_minor = 3;
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "chardev";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::unsupported_feature);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("not supported"));
    }
    
    SECTION("Block device") {
        auto metadata = create_file_metadata("blockdev", entry_type::block_device);
        metadata.device_major = 8;
        metadata.device_minor = 0;
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "blockdev";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::unsupported_feature);
    }
    
    SECTION("FIFO") {
        auto metadata = create_file_metadata("fifo", entry_type::fifo);
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "fifo";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::unsupported_feature);
    }
}

TEST_CASE("archive_entry extract error handling", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Read error during extraction") {
        auto metadata = create_file_metadata("file.txt", entry_type::regular_file, 100);
        
        // Create a failing reader
        auto failing_reader = [](size_t, size_t) -> std::expected<std::span<const std::byte>, error> {
            return std::unexpected(error{error_code::io_error, "Simulated read failure"});
        };
        
        archive_entry entry(metadata, failing_reader);
        
        auto dest = temp_dir.path() / "file.txt";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
    
    SECTION("Invalid destination path") {
        auto data = create_test_data("content");
        auto metadata = create_file_metadata("file.txt", entry_type::regular_file, data.size());
        archive_entry entry(metadata, create_mock_reader(data));
        
        // Try to extract to a path that can't be created
        auto dest = fs::path("/root/no_permission/file.txt");
        auto result = entry.extract_to_path(dest);
        
        // This should fail due to permissions
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::io_error);
    }
    
    SECTION("Path with null bytes") {
        auto data = create_test_data("content");
        auto metadata = create_file_metadata("file.txt", entry_type::regular_file, data.size());
        archive_entry entry(metadata, create_mock_reader(data));
        
        // Create path with embedded null
        std::string bad_path = "file";
        bad_path.push_back('\0');
        bad_path += "name.txt";
        
        auto dest = temp_dir.path() / bad_path;
        auto result = entry.extract_to_path(dest);
        
        // Filesystem operations should handle this gracefully
        // The actual behavior depends on the filesystem implementation
    }
}

TEST_CASE("archive_entry extract permissions", "[integration][archive_entry][extract]") {
    TempDirectory temp_dir;
    
    SECTION("Extract with specific permissions") {
        auto data = create_test_data("content");
        auto metadata = create_file_metadata("file.txt", entry_type::regular_file, data.size());
        metadata.permissions = fs::perms::owner_read | fs::perms::owner_write | 
                              fs::perms::group_read | fs::perms::others_read;
        archive_entry entry(metadata, create_mock_reader(data));
        
        auto dest = temp_dir.path() / "file.txt";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::exists(dest));
        
        // Check permissions were applied (may be modified by umask)
        auto actual_perms = fs::status(dest).permissions();
        CHECK((actual_perms & fs::perms::owner_read) == fs::perms::owner_read);
        CHECK((actual_perms & fs::perms::owner_write) == fs::perms::owner_write);
    }
    
    SECTION("Extract directory with permissions") {
        auto metadata = create_file_metadata("dir", entry_type::directory);
        metadata.permissions = fs::perms::owner_all | fs::perms::group_read | 
                              fs::perms::group_exec;
        archive_entry entry(metadata, create_mock_reader({}));
        
        auto dest = temp_dir.path() / "dir";
        auto result = entry.extract_to_path(dest);
        
        REQUIRE(result.has_value());
        CHECK(fs::is_directory(dest));
        
        auto actual_perms = fs::status(dest).permissions();
        CHECK((actual_perms & fs::perms::owner_all) == fs::perms::owner_all);
    }
}