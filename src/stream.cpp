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

#include <tierone/tar/stream.hpp>
#include <cstdio>
#include <cerrno>
#include <cstring>

#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tierone::tar {

// file_stream implementation
file_stream::file_stream(std::FILE* file, const std::optional<size_t> size)
    : file_(file), file_size_(size) {}

auto file_stream::open(const std::filesystem::path &path) -> std::expected<file_stream, error> {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return std::unexpected(error{error_code::io_error, 
            "Failed to open file: " + std::string{std::strerror(errno)}});
    }
    
    // Try to get file size
    std::optional<size_t> file_size;
    if (std::fseek(file, 0, SEEK_END) == 0) {
        if (const long pos = std::ftell(file); pos >= 0) {
            file_size = static_cast<size_t>(pos);
        }
        std::fseek(file, 0, SEEK_SET);
    }
    
    return file_stream{file, file_size};
}

auto file_stream::read(std::span<std::byte> buffer) -> std::expected<size_t, error> {
    size_t bytes_read = std::fread(buffer.data(), 1, buffer.size(), file_.get());
    
    if (bytes_read == 0 && std::ferror(file_.get())) {
        return std::unexpected(error{error_code::io_error, 
            "File read error: " + std::string{std::strerror(errno)}});
    }
    
    return bytes_read;
}

auto file_stream::skip(size_t bytes) -> std::expected<void, error> {
    if (std::fseek(file_.get(), static_cast<long>(bytes), SEEK_CUR) != 0) {
        return std::unexpected(error{error_code::io_error, 
            "File seek error: " + std::string{std::strerror(errno)}});
    }
    return {};
}

auto file_stream::seek(size_t position) -> std::expected<void, error> {
    if (std::fseek(file_.get(), static_cast<long>(position), SEEK_SET) != 0) {
        return std::unexpected(error{error_code::io_error, 
            "File seek error: " + std::string{std::strerror(errno)}});
    }
    return {};
}

bool file_stream::at_end() const {
    // If we know the file size, check if position equals size
    if (file_size_.has_value()) {
        long pos = std::ftell(file_.get());
        if (pos >= 0) {
            return static_cast<size_t>(pos) >= file_size_.value();
        }
    }
    
    // Fall back to checking EOF flag
    return std::feof(file_.get()) != 0;
}

size_t file_stream::position() const {
    long pos = std::ftell(file_.get());
    return pos >= 0 ? static_cast<size_t>(pos) : 0;
}

auto file_stream::size() const -> std::optional<size_t> {
    return file_size_;
}

#ifdef __linux__
// mmap_stream implementation
mmap_stream::mmap_stream(void* ptr, const size_t size)
    : mapping_{ptr, mapping_deleter{size}}
    , data_{static_cast<const std::byte*>(ptr), size} {}

auto mmap_stream::create(const std::filesystem::path &path) -> std::expected<mmap_stream, error> {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return std::unexpected(error{error_code::io_error, 
            "Failed to open file: " + std::string{std::strerror(errno)}});
    }
    
    struct stat st{};
    if (::fstat(fd, &st) == -1) {
        ::close(fd);
        return std::unexpected(error{error_code::io_error, 
            "Failed to stat file: " + std::string{std::strerror(errno)}});
    }

    const size_t file_size = static_cast<size_t>(st.st_size);
    
    void* ptr;
    if (file_size == 0) {
        // Handle empty files - use nullptr for empty mapping
        ptr = nullptr;
    } else {
        ptr = ::mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd);
            return std::unexpected(error{error_code::io_error, 
                "Memory mapping failed: " + std::string{std::strerror(errno)}});
        }
        
        // Advise kernel about access pattern
        ::madvise(ptr, file_size, MADV_SEQUENTIAL);
    }
    
    ::close(fd);  // Can close fd after mmap
    
    return mmap_stream{ptr, file_size};
}

auto mmap_stream::read(std::span<std::byte> buffer) -> std::expected<size_t, error> {
    size_t available = data_.size() - position_;
    size_t to_read = std::min(buffer.size(), available);
    
    std::ranges::copy_n(data_.begin() + static_cast<std::ptrdiff_t>(position_), 
                       static_cast<std::ptrdiff_t>(to_read), buffer.begin());
    position_ += to_read;
    
    return to_read;
}

auto mmap_stream::skip(size_t bytes) -> std::expected<void, error> {
    if (position_ + bytes > data_.size()) {
        return std::unexpected(error{error_code::io_error, "Skip past end of stream"});
    }
    position_ += bytes;
    return {};
}

auto mmap_stream::seek(size_t position) -> std::expected<void, error> {
    if (position > data_.size()) {
        return std::unexpected(error{error_code::io_error, "Seek past end of stream"});
    }
    position_ = position;
    return {};
}

bool mmap_stream::at_end() const {
    return position_ >= data_.size();
}

size_t mmap_stream::position() const {
    return position_;
}

auto mmap_stream::size() const -> std::optional<size_t> {
    return data_.size();
}
#endif

} // namespace tierone::tar