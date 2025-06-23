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

#pragma once

#include <tierone/tar/error.hpp>
#include <expected>
#include <span>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <ranges>
#include <cstdio>

#ifdef __linux__
#include <sys/mman.h>
#endif

namespace tierone::tar {

// Base interface for reading data streams
class input_stream {
public:
    virtual ~input_stream() = default;

    // Read up to buffer.size() bytes into buffer, returns actual bytes read
    [[nodiscard]] virtual std::expected<size_t, error> read(std::span<std::byte> buffer) = 0;

    // Skip n bytes in the stream
    [[nodiscard]] virtual std::expected<void, error> skip(size_t bytes) = 0;

    // Check if at end of stream
    [[nodiscard]] virtual bool at_end() const = 0;
};

// Extended interface for streams that support random access
class random_access_stream : public input_stream {
public:
    // Seek to absolute position
    [[nodiscard]] virtual std::expected<void, error> seek(size_t position) = 0;

    // Get current position
    [[nodiscard]] virtual size_t position() const = 0;

    // Get total size (if known)
    [[nodiscard]] virtual std::optional<size_t> size() const = 0;
};

// Memory-mapped stream implementation
class memory_mapped_stream : public random_access_stream {
private:
    std::span<const std::byte> data_;
    size_t position_ = 0;

public:
    explicit memory_mapped_stream(std::span<const std::byte> data) 
        : data_(data) {}

    [[nodiscard]] std::expected<size_t, error> read(std::span<std::byte> buffer) override {
        size_t available = data_.size() - position_;
        size_t to_read = std::min(buffer.size(), available);
        
        std::ranges::copy_n(data_.begin() + static_cast<std::ptrdiff_t>(position_), 
                           static_cast<std::ptrdiff_t>(to_read), buffer.begin());
        position_ += to_read;
        
        return to_read;
    }

    [[nodiscard]] std::expected<void, error> skip(size_t bytes) override {
        if (position_ + bytes > data_.size()) {
            return std::unexpected(error{error_code::io_error, "Skip past end of stream"});
        }
        position_ += bytes;
        return {};
    }

    [[nodiscard]] std::expected<void, error> seek(size_t position) override {
        if (position > data_.size()) {
            return std::unexpected(error{error_code::io_error, "Seek past end of stream"});
        }
        position_ = position;
        return {};
    }

    [[nodiscard]] bool at_end() const override { 
        return position_ >= data_.size(); 
    }

    [[nodiscard]] size_t position() const override { 
        return position_; 
    }

    [[nodiscard]] std::optional<size_t> size() const override { 
        return data_.size(); 
    }
};

// File-based stream
class file_stream : public random_access_stream {
private:
    struct file_deleter {
        void operator()(std::FILE* f) const {
            if (f) std::fclose(f);
        }
    };
    
    std::unique_ptr<std::FILE, file_deleter> file_;
    std::optional<size_t> file_size_;

public:
    [[nodiscard]] static std::expected<file_stream, error> open(const std::filesystem::path& path);

    [[nodiscard]] std::expected<size_t, error> read(std::span<std::byte> buffer) override;
    [[nodiscard]] std::expected<void, error> skip(size_t bytes) override;
    [[nodiscard]] std::expected<void, error> seek(size_t position) override;
    [[nodiscard]] bool at_end() const override;
    [[nodiscard]] size_t position() const override;
    [[nodiscard]] std::optional<size_t> size() const override;

private:
    explicit file_stream(std::FILE* file, std::optional<size_t> size);
};

// Memory-mapped file stream (Linux-specific)
#ifdef __linux__
class mmap_stream : public random_access_stream {
private:
    struct mapping_deleter {
        size_t size;
        void operator()(void* ptr) const {
            if (ptr && ptr != MAP_FAILED) ::munmap(ptr, size);
        }
    };
    
    std::unique_ptr<void, mapping_deleter> mapping_;
    std::span<const std::byte> data_;
    size_t position_ = 0;

public:
    [[nodiscard]] static std::expected<mmap_stream, error> create(const std::filesystem::path& path);

    [[nodiscard]] std::expected<size_t, error> read(std::span<std::byte> buffer) override;
    [[nodiscard]] std::expected<void, error> skip(size_t bytes) override;
    [[nodiscard]] std::expected<void, error> seek(size_t position) override;
    [[nodiscard]] bool at_end() const override;
    [[nodiscard]] size_t position() const override;
    [[nodiscard]] std::optional<size_t> size() const override;

private:
    mmap_stream(void* ptr, size_t size);
};
#endif

} // namespace tierone::tar