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
#include <tierone/tar/pax_parser.hpp>
#include <tierone/tar/metadata.hpp>
#include <tierone/tar/error.hpp>
#include <vector>
#include <string>
#include <array>

using namespace tierone::tar;
using namespace tierone::tar::pax;

namespace {

std::vector<std::byte> string_to_bytes(const std::string& str) {
    std::vector<std::byte> bytes;
    bytes.reserve(str.size());
    for (char c : str) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

std::span<const std::byte> make_span(const std::vector<std::byte>& vec) {
    return std::span<const std::byte>(vec.data(), vec.size());
}

} // anonymous namespace

TEST_CASE("parse_pax_headers valid formats", "[unit][pax_parser]") {
    SECTION("Single header entry") {
        std::string pax_data = "27 path=long/file/name.txt\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 1);
        CHECK(result->at("path") == "long/file/name.txt");
    }
    
    SECTION("Multiple header entries") {
        std::string pax_data = "27 path=long/file/name.txt\n"
                              "19 size=1234567890\n"
                              "22 mtime=1609459200.5\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 3);
        CHECK(result->at("path") == "long/file/name.txt");
        CHECK(result->at("size") == "1234567890");
        CHECK(result->at("mtime") == "1609459200.5");
    }
    
    SECTION("Entry with empty value") {
        std::string pax_data = "7 key=\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 1);
        CHECK(result->at("key") == "");
    }
    
    SECTION("Entry with special characters in value") {
        std::string pax_data = "36 comment=Hello, 世界! é€£¥\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 1);
        CHECK(result->at("comment") == "Hello, 世界! é€£¥");
    }
    
    SECTION("Entry with spaces and special chars in key") {
        std::string pax_data = "27 SCHILY.xattr.user=value\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("SCHILY.xattr.user") == "value");
    }
    
    SECTION("Large entry") {
        std::string large_value(1000, 'X');
        // Calculate correct PAX length: content + length field + space
        size_t content_length = 7 + large_value.length() + 1; // " large=" + value + "\n"
        // For 1000 X's: content is 1008, total should be 1008 + 4 + 1 = 1013 but actual is 1012
        // This suggests the calculation should be content + digits - 1 
        size_t total_length = content_length + 4; // "1012" + content
        std::string pax_data = std::to_string(total_length) + " large=" + large_value + "\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("large") == large_value);
    }
    
    SECTION("Entry with equals sign in value") {
        std::string pax_data = "19 formula=a=b+c=d\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("formula") == "a=b+c=d");
    }
}

TEST_CASE("parse_pax_headers error cases", "[unit][pax_parser]") {
    SECTION("Invalid length field - non-numeric") {
        std::string pax_data = "abc path=test\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Invalid PAX header length"));
    }
    
    SECTION("Missing space after length") {
        std::string pax_data = "25path=test\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Missing equals separator") {
        std::string pax_data = "12 pathtest\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("missing '='"));
    }
    
    SECTION("Length field too large") {
        std::string pax_data = "1000 path=test\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::corrupt_archive);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("extends beyond data"));
    }
    
    SECTION("Zero length") {
        std::string pax_data = "0 \n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        // Should handle gracefully or error appropriately
        CHECK_FALSE(result.has_value());
    }
    
    SECTION("Empty data") {
        std::vector<std::byte> empty_bytes;
        
        auto result = parse_pax_headers(make_span(empty_bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->empty());
    }
    
    SECTION("Data with null terminator") {
        std::string pax_data = "27 path=long/file/name.txt\n\0extra";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 1);
        CHECK(result->at("path") == "long/file/name.txt");
    }
    
    SECTION("Very large length number") {
        std::string pax_data = "99999999999999999999 path=test\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Negative length (leading minus)") {
        std::string pax_data = "-25 path=test\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
}

TEST_CASE("parse_pax_headers edge cases", "[unit][pax_parser]") {
    SECTION("Entry without newline") {
        std::string pax_data = "26 path=long/file/name.txt";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("path") == "long/file/name.txt");
    }
    
    SECTION("Multiple consecutive newlines") {
        std::string pax_data = "29 path=long/file/name.txt\n\n\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 1);
    }
    
    SECTION("Key with dots and underscores") {
        std::string pax_data = "30 GNU.sparse.major.version=1\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("GNU.sparse.major.version") == "1");
    }
    
    SECTION("Value with newlines") {
        std::string pax_data = "27 comment=line1\nline2\nend\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("comment") == "line1\nline2\nend");
    }
    
    SECTION("Minimum valid entry") {
        std::string pax_data = "5 a=\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        REQUIRE(result.has_value());
        CHECK(result->at("a") == "");
    }
}

TEST_CASE("has_gnu_sparse_markers", "[unit][pax_parser]") {
    SECTION("Has GNU sparse major marker") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.major", "1"},
            {"path", "test.txt"}
        };
        
        CHECK(has_gnu_sparse_markers(headers));
    }
    
    SECTION("Has GNU sparse minor marker") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.minor", "0"},
            {"size", "1000"}
        };
        
        CHECK(has_gnu_sparse_markers(headers));
    }
    
    SECTION("Has GNU sparse map") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.map", "0,100,200,50"},
            {"path", "sparse.dat"}
        };
        
        CHECK(has_gnu_sparse_markers(headers));
    }
    
    SECTION("No GNU sparse markers") {
        std::map<std::string, std::string> headers{
            {"path", "regular.txt"},
            {"size", "1000"},
            {"mtime", "1234567890"}
        };
        
        CHECK_FALSE(has_gnu_sparse_markers(headers));
    }
    
    SECTION("Empty headers") {
        std::map<std::string, std::string> headers;
        
        CHECK_FALSE(has_gnu_sparse_markers(headers));
    }
}

TEST_CASE("get_gnu_sparse_version", "[unit][pax_parser]") {
    SECTION("Both major and minor present") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.major", "1"},
            {"GNU.sparse.minor", "5"}
        };
        
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 1);
        CHECK(minor == 5);
    }
    
    SECTION("Only major present") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.major", "2"}
        };
        
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 2);
        CHECK(minor == 0);
    }
    
    SECTION("Only minor present") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.minor", "3"}
        };
        
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 0);
        CHECK(minor == 3);
    }
    
    SECTION("Neither present") {
        std::map<std::string, std::string> headers{
            {"path", "test.txt"}
        };
        
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 0);
        CHECK(minor == 0);
    }
    
    SECTION("Invalid version numbers") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.major", "abc"},
            {"GNU.sparse.minor", "xyz"}
        };
        
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 0);
        CHECK(minor == 0);
    }
    
    SECTION("Large version numbers") {
        std::map<std::string, std::string> headers{
            {"GNU.sparse.major", "999"},
            {"GNU.sparse.minor", "123"}
        };
        
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 999);
        CHECK(minor == 123);
    }
}

TEST_CASE("extract_extended_attributes", "[unit][pax_parser]") {
    SECTION("SCHILY format attributes") {
        std::map<std::string, std::string> headers{
            {"SCHILY.xattr.user.comment", "test comment"},
            {"SCHILY.xattr.security.selinux", "unconfined_u:object_r:user_home_t:s0"},
            {"SCHILY.xattr.system.posix_acl_access", "base64data"},
            {"path", "test.txt"}
        };
        
        auto xattrs = extract_extended_attributes(headers);
        
        CHECK(xattrs.size() == 3);
        CHECK(xattrs.at("user.comment") == "test comment");
        CHECK(xattrs.at("security.selinux") == "unconfined_u:object_r:user_home_t:s0");
        CHECK(xattrs.at("system.posix_acl_access") == "base64data");
    }
    
    SECTION("LIBARCHIVE format attributes") {
        std::map<std::string, std::string> headers{
            {"LIBARCHIVE.xattr.user.mime_type", "text/plain"},
            {"LIBARCHIVE.xattr.trusted.overlay.opaque", "y"},
            {"size", "1000"}
        };
        
        auto xattrs = extract_extended_attributes(headers);
        
        CHECK(xattrs.size() == 2);
        CHECK(xattrs.at("user.mime_type") == "text/plain");
        CHECK(xattrs.at("trusted.overlay.opaque") == "y");
    }
    
    SECTION("Mixed formats") {
        std::map<std::string, std::string> headers{
            {"SCHILY.xattr.user.comment", "schily comment"},
            {"LIBARCHIVE.xattr.user.label", "libarchive label"},
            {"path", "mixed.txt"}
        };
        
        auto xattrs = extract_extended_attributes(headers);
        
        CHECK(xattrs.size() == 2);
        CHECK(xattrs.at("user.comment") == "schily comment");
        CHECK(xattrs.at("user.label") == "libarchive label");
    }
    
    SECTION("No extended attributes") {
        std::map<std::string, std::string> headers{
            {"path", "regular.txt"},
            {"size", "1000"}
        };
        
        auto xattrs = extract_extended_attributes(headers);
        
        CHECK(xattrs.empty());
    }
    
    SECTION("Empty attribute values") {
        std::map<std::string, std::string> headers{
            {"SCHILY.xattr.user.empty", ""},
            {"LIBARCHIVE.xattr.system.null", ""}
        };
        
        auto xattrs = extract_extended_attributes(headers);
        
        CHECK(xattrs.size() == 2);
        CHECK(xattrs.at("user.empty") == "");
        CHECK(xattrs.at("system.null") == "");
    }
    
    SECTION("Attribute names with dots") {
        std::map<std::string, std::string> headers{
            {"SCHILY.xattr.user.my.custom.attr", "value"},
            {"LIBARCHIVE.xattr.security.ima", "hash"}
        };
        
        auto xattrs = extract_extended_attributes(headers);
        
        CHECK(xattrs.at("user.my.custom.attr") == "value");
        CHECK(xattrs.at("security.ima") == "hash");
    }
}

TEST_CASE("parse_acl_text valid formats", "[unit][pax_parser]") {
    SECTION("Simple ACL entries") {
        std::string acl_text = "user::rwx,group::r-x,other::r--";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 3);
        
        // Check user:: entry
        CHECK(result->at(0).entry_type == acl_entry::type::user_obj);
        CHECK(result->at(0).id == 0);
        CHECK(static_cast<uint8_t>(result->at(0).permissions) == 
              (static_cast<uint8_t>(acl_entry::perm::read) | 
               static_cast<uint8_t>(acl_entry::perm::write) | 
               static_cast<uint8_t>(acl_entry::perm::execute)));
        
        // Check group:: entry
        CHECK(result->at(1).entry_type == acl_entry::type::group_obj);
        CHECK(static_cast<uint8_t>(result->at(1).permissions) == 
              (static_cast<uint8_t>(acl_entry::perm::read) | 
               static_cast<uint8_t>(acl_entry::perm::execute)));
        
        // Check other:: entry
        CHECK(result->at(2).entry_type == acl_entry::type::other);
        CHECK(static_cast<uint8_t>(result->at(2).permissions) == 
              static_cast<uint8_t>(acl_entry::perm::read));
    }
    
    SECTION("ACL with specific user and group") {
        std::string acl_text = "user:1000:rwx,group:1000:r--";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 2);
        
        CHECK(result->at(0).entry_type == acl_entry::type::user);
        CHECK(result->at(0).id == 1000);
        
        CHECK(result->at(1).entry_type == acl_entry::type::group);
        CHECK(result->at(1).id == 1000);
    }
    
    SECTION("Complete ACL with mask") {
        std::string acl_text = "user::rwx,user:1000:rwx,group::r-x,mask::rwx,other::r--";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 5);
        
        // Find mask entry
        auto mask_it = std::find_if(result->begin(), result->end(),
                                   [](const acl_entry& e) { 
                                       return e.entry_type == acl_entry::type::mask; 
                                   });
        REQUIRE(mask_it != result->end());
        CHECK(static_cast<uint8_t>(mask_it->permissions) == 
              (static_cast<uint8_t>(acl_entry::perm::read) | 
               static_cast<uint8_t>(acl_entry::perm::write) | 
               static_cast<uint8_t>(acl_entry::perm::execute)));
    }
    
    SECTION("ACL with no permissions") {
        std::string acl_text = "user:2000:---,group:2000:---";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 2);
        CHECK(static_cast<uint8_t>(result->at(0).permissions) == 0);
        CHECK(static_cast<uint8_t>(result->at(1).permissions) == 0);
    }
    
    SECTION("ACL with whitespace") {
        std::string acl_text = " user::rwx , group::r-x , other::r-- ";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->size() == 3);
    }
}

TEST_CASE("parse_acl_text error cases", "[unit][pax_parser]") {
    SECTION("Invalid entry format - missing colon") {
        std::string acl_text = "userrwx";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Invalid ACL entry format"));
    }
    
    SECTION("Invalid ACL type") {
        std::string acl_text = "unknown:1000:rwx";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Unknown ACL entry type"));
    }
    
    SECTION("Invalid ID format") {
        std::string acl_text = "user:abc:rwx";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Invalid ACL ID"));
    }
    
    SECTION("Invalid permission format - too short") {
        std::string acl_text = "user:1000:rw";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
        CHECK_THAT(result.error().message(), Catch::Matchers::ContainsSubstring("Invalid ACL permission format"));
    }
    
    SECTION("Invalid permission format - too long") {
        std::string acl_text = "user:1000:rwxs";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Negative ID") {
        std::string acl_text = "user:-1:rwx";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code() == error_code::invalid_header);
    }
    
    SECTION("Empty ACL text") {
        std::string acl_text = "";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->empty());
    }
    
    SECTION("Only whitespace") {
        std::string acl_text = "   \t  ";
        
        auto result = parse_acl_text(acl_text);
        
        REQUIRE(result.has_value());
        CHECK(result->empty());
    }
}

TEST_CASE("extract_acls", "[unit][pax_parser]") {
    SECTION("Both access and default ACLs") {
        std::map<std::string, std::string> headers{
            {"SCHILY.acl.access", "user::rwx,group::r-x,other::r--"},
            {"SCHILY.acl.default", "user::rwx,group::r-x,other::---,mask::rwx"}
        };
        
        auto [access_acl, default_acl] = extract_acls(headers);
        
        CHECK(access_acl.size() == 3);
        CHECK(default_acl.size() == 4);
        
        CHECK(access_acl[0].entry_type == acl_entry::type::user_obj);
        CHECK(default_acl[3].entry_type == acl_entry::type::mask);
    }
    
    SECTION("Only access ACL") {
        std::map<std::string, std::string> headers{
            {"SCHILY.acl.access", "user::rwx,user:1000:r--"}
        };
        
        auto [access_acl, default_acl] = extract_acls(headers);
        
        CHECK(access_acl.size() == 2);
        CHECK(default_acl.empty());
    }
    
    SECTION("Only default ACL") {
        std::map<std::string, std::string> headers{
            {"SCHILY.acl.default", "user::rwx,group::r-x"}
        };
        
        auto [access_acl, default_acl] = extract_acls(headers);
        
        CHECK(access_acl.empty());
        CHECK(default_acl.size() == 2);
    }
    
    SECTION("No ACLs") {
        std::map<std::string, std::string> headers{
            {"path", "regular.txt"},
            {"size", "1000"}
        };
        
        auto [access_acl, default_acl] = extract_acls(headers);
        
        CHECK(access_acl.empty());
        CHECK(default_acl.empty());
    }
    
    SECTION("Invalid ACL text is ignored") {
        std::map<std::string, std::string> headers{
            {"SCHILY.acl.access", "invalid:format"},
            {"SCHILY.acl.default", "user::rwx"}
        };
        
        auto [access_acl, default_acl] = extract_acls(headers);
        
        CHECK(access_acl.empty()); // Invalid ACL ignored
        CHECK(default_acl.size() == 1); // Valid ACL processed
    }
}

TEST_CASE("pax_parser integration scenarios", "[unit][pax_parser]") {
    SECTION("GNU sparse file with extended metadata") {
        std::string pax_data = "22 GNU.sparse.major=1\n"
                              "22 GNU.sparse.minor=0\n"
                              "43 GNU.sparse.map=0,1000,2000,500,3000,200\n"
                              "36 SCHILY.xattr.user.comment=sparse\n"
                              "42 SCHILY.acl.access=user::rwx,other::r--\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto headers_result = parse_pax_headers(make_span(bytes));
        REQUIRE(headers_result.has_value());
        auto& headers = headers_result.value();
        
        // Test GNU sparse detection
        CHECK(has_gnu_sparse_markers(headers));
        auto [major, minor] = get_gnu_sparse_version(headers);
        CHECK(major == 1);
        CHECK(minor == 0);
        
        // Test extended attributes
        auto xattrs = extract_extended_attributes(headers);
        CHECK(xattrs.size() == 1);
        CHECK(xattrs.at("user.comment") == "sparse");
        
        // Test ACLs
        auto [access_acl, default_acl] = extract_acls(headers);
        CHECK(access_acl.size() == 2);
        CHECK(default_acl.empty());
    }
    
    SECTION("File with extensive metadata") {
        std::string pax_data = "27 path=very/long/path.txt\n"
                              "15 size=123456\n"
                              "37 SCHILY.xattr.user.author=John Doe\n"
                              "41 SCHILY.xattr.security.selinux=context\n"
                              "56 SCHILY.acl.access=user::rwx,user:1000:r--,group::r-x\n"
                              "32 SCHILY.acl.default=user::rwx\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto headers_result = parse_pax_headers(make_span(bytes));
        REQUIRE(headers_result.has_value());
        auto& headers = headers_result.value();
        
        CHECK(headers.size() == 6);
        CHECK(headers.at("path") == "very/long/path.txt");
        CHECK(headers.at("size") == "123456");
        
        auto xattrs = extract_extended_attributes(headers);
        CHECK(xattrs.size() == 2);
        
        auto [access_acl, default_acl] = extract_acls(headers);
        CHECK(access_acl.size() == 3);
        CHECK(default_acl.size() == 1);
    }
    
    SECTION("Empty and malformed entries mixed") {
        std::string pax_data = "25 path=valid/path.txt\n"
                              "invalid entry here\n"
                              "20 size=12345\n";
        auto bytes = string_to_bytes(pax_data);
        
        auto result = parse_pax_headers(make_span(bytes));
        
        // Should fail on the invalid entry
        REQUIRE_FALSE(result.has_value());
    }
}