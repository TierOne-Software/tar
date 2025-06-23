# TierOne Tar Library

A modern C++23 implementation of a tar archive reading library, designed for embedded Linux systems with memory constraints while providing a clean, type-safe API.

## Features

- **C++23 Implementation**: Leverages modern C++ features for safety and performance
- **POSIX ustar Support**: Full support for standard POSIX.1-1988 tar format
- **GNU tar Extensions**: Support for GNU long filenames, link targets, and sparse files
- **Memory Efficient**: Support for both streaming and memory-mapped access patterns
- **Type Safe**: Uses `std::expected` for error handling and concepts for type constraints
- **Zero-Copy**: Memory-mapped files provide zero-copy data access where possible
- **Embedded Friendly**: No dependencies (aside from the test framework) and predictable memory usage
- **Sparse File Support**: Efficient handling of sparse files with hole detection

## API Overview

### Basic Usage

```cpp
#include <tierone/tar/tar.hpp>

// Open a tar archive
auto reader = tierone::tar::open_archive("archive.tar");
if (!reader) {
    std::println(stderr, "Failed to open: {}", reader.error().message());
    return;
}

// Iterate through entries
for (const auto& entry : *reader) {
    std::println("{} ({} bytes)", entry.path(), entry.size());
    
    if (entry.is_regular_file()) {
        // Read file data
        auto data = entry.read_data();
        if (data) {
            // Process file content
        }
    }
}
```

### Extract Files

```cpp
// Extract specific entries
for (const auto& entry : reader) {
    if (entry.path().string().starts_with("docs/")) {
        auto dest_path = std::filesystem::path("extracted") / entry.path();
        if (auto result = entry.extract_to_path(dest_path); !result) {
            std::println(stderr, "Failed to extract: {}", result.error().message());
        }
    }
}
```

### Stream Types

The library supports multiple stream types:

- `file_stream`: Standard file I/O using buffered reads (portable)
- `memory_mapped_stream`: Operates on pre-loaded memory data (portable)
- `mmap_stream`: Zero-copy memory-mapped file access using mmap() (Linux-only)

## Building

### Requirements

- C++23 compatible compiler (Clang 19+ or 20+ recommended)
- CMake 3.25+ (for building)
- Catch2 3.6.0 (for building tests)
- Conan 2.0.5+ (for installing Catch2)

### Supported Platforms

- Linux (tested on Ubuntu 22.04+)
- Other POSIX systems (should work, limited testing)

### Compiler Support

- **Clang 19+**: Full support, CI tested
- **Clang 20+**: Full support, CI tested
- **GCC**: Limited support (GCC 13 lacks std::print support)

### Build Instructions

Conan profiles and CMake toolchains are optional but provide control on what
compiler to use.

```bash
conan install . --build=missing \
  --profile=.conan-profile-debug \
  --profile:build=.conan-profile-debug \
  --output-folder=cmake-build-debug
cmake -B cmake-build-debug -S . \
  -DCMAKE_TOOLCHAIN_FILE=toolchainfile-amd64-clang20.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

### Running Tests

```bash
ctest --test-dir cmake-build-debug
```

### Examples

Multiple examples can be found under the `examples` directory.
See [examples/README.md](examples/README.md)

## Design Features

### Error Handling
- Uses `std::expected<T, error>` throughout for composable error handling
- Rich error information with context messages
- No exceptions for predictable behavior

### Memory Management
- RAII throughout, no manual memory management
- Configurable buffer sizes for memory-constrained environments
- Support for both streaming (constant memory) and random access patterns

### Type Safety
- Strong typing for entry types, permissions, and metadata
- Concepts used for template constraints
- `std::filesystem::path` for proper path handling

### Performance
- Zero-copy operations where possible
- Memory-mapped I/O for large archives
- Lazy loading of entry data
- Single-pass streaming for memory efficiency

## Supported Entry Types

### POSIX ustar Format
- Regular files
- Directories  
- Symbolic links
- Hard links
- Character devices
- Block devices
- FIFOs

### GNU tar Extensions
- Long filenames (>100 characters) via 'L' type entries
- Long link targets (>100 characters) via 'K' type entries
- Automatic detection and processing of GNU tar format

## Implementation Status

✅ **Completed:**
- POSIX ustar header parsing and validation
- GNU tar long filename/linkname support (L/K entries)
- Streaming archive reader with iterator support
- Memory-mapped and file-based streams
- Entry extraction to filesystem
- Comprehensive test suite including GNU tar tests
- Example applications demonstrating both POSIX and GNU formats
- Basic GNU sparse file support (old format with octal sparse maps)
- Sparse file infrastructure (sparse metadata, sparse-aware readers)

🚧 **Future Enhancements:**
- Full GNU sparse file support (modern binary formats)
- PAX format support (including PAX sparse format)
- Archive writing capabilities
- Compression integration (zstd, gzip)
- Async I/O support

## Architecture

The library is organized into several key components:

- **Core Types** (`error.hpp`, `metadata.hpp`): Error handling and data structures
- **Streams** (`stream.hpp`): Abstract interfaces for data access
- **Header Parsing** (`header_parser.hpp`): POSIX ustar format parsing
- **GNU tar Support** (`gnu_tar.hpp`): GNU tar extension handling
- **Archive Reader** (`archive_reader.hpp`): Main API for reading archives
- **Archive Entry** (`archive_entry.hpp`): Individual file/directory entries

## License

Licensed under the Apache License, Version 2.0. See [LICENSE-2.0.txt](LICENSE-2.0.txt) for the full license text.

This implementation follows modern C++ best practices and is designed to be a clean, safe alternative to traditional C-based tar libraries like libtar.

## Contributing

We welcome contributions to TierOne Tar! Here's how you can help:

### Submitting Changes

1. Fork the repository on GitHub
2. Create a feature branch from `main`
3. Make your changes with appropriate tests
4. Ensure all tests pass and code follows the existing style
5. Submit a pull request with a clear description of your changes

### Development Guidelines

- Follow the existing code style and naming conventions
- Add tests for new features or bug fixes
- Update documentation for API changes
- Use modern C++23 features appropriately
- Ensure compatibility with supported compilers (Clang 19+)

### Reporting Issues

Please report bugs, feature requests, or questions by opening an issue on GitHub.

## Copyright

Copyright 2025 TierOne Software. All rights reserved.