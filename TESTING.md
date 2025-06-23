# Testing Guide

This document describes the testing infrastructure for the tierone-tar library.

## Quick Start

```bash
# Run all tests
ctest

# Run specific test categories
ctest -L unit         # C++ unit tests only
ctest -L integration  # Integration tests only  
ctest -L sparse       # Sparse file tests only
ctest -L quick        # Quick verification tests
```

## Test Organization

All test files are organized under the `tests/` directory:

```
tests/
├── README.md                    # Detailed test documentation
├── CMakeLists.txt              # Test configuration
├── .gitignore                  # Ignore generated test files
├── test_summary.md             # Test coverage summary
├── scripts/                    # Integration test scripts
│   ├── test_sparse_real.sh     # Comprehensive sparse file tests
│   └── test_sparse_final.sh    # Quick verification tests
├── fixtures/                   # Generated test data (gitignored)
│   ├── *.tar                   # Test archive files  
│   ├── *.bin                   # Test binary files
│   └── extracted*/             # Extracted test directories
├── data/                       # Static test data (version controlled)
│   └── srec-utilities.tar      # Reference archive
└── test_*.cpp                  # C++ unit tests (Catch2)
```

## Test Categories

### Unit Tests (C++)
- **Framework**: Catch2 v3
- **Coverage**: Header parsing, archive reading, GNU extensions, sparse files, PAX headers
- **Run time**: < 0.1 seconds
- **Command**: `ctest -L unit`

### Integration Tests (Bash)
- **Real sparse files**: Created with `dd` and GNU `tar`
- **Format testing**: Both 0.0 and 1.0 sparse formats
- **Extraction verification**: Bit-perfect content validation
- **Run time**: ~0.6 seconds
- **Command**: `ctest -L integration`

## Test Results

Current test status:
```
100% tests passed, 0 tests failed out of 20

Label Time Summary:
integration    =   0.57 sec*proc (2 tests)
quick          =   0.19 sec*proc (1 test)
sparse         =   0.57 sec*proc (2 tests)

Total Test time (real) =   0.62 sec
```

## Key Features Tested

### GNU Tar Sparse File Support
- ✅ Format 0.0 (old sparse format) detection and extraction
- ✅ Format 1.0 (PAX-based) detection and extraction  
- ✅ Transparent sparse reading (holes return zeros)
- ✅ Memory-safe large file handling
- ✅ Bit-perfect extraction with content verification

### Archive Compatibility
- ✅ Standard POSIX tar archives
- ✅ GNU tar extensions (longname, longlink)
- ✅ PAX extended headers
- ✅ Mixed archive types

## Adding Tests

### New C++ Unit Tests
1. Create `test_<feature>.cpp` in `tests/`
2. Add to `CMakeLists.txt` target
3. Use Catch2 `TEST_CASE` macro

### New Integration Tests  
1. Create script in `tests/scripts/`
2. Add to `CMakeLists.txt` with `add_test()`
3. Use appropriate test labels

## Dependencies

- **C++**: Catch2 (found automatically)
- **Integration**: bash, dd, tar, standard Unix tools
- **Build**: CMake 3.25+, CTest

## CI/CD Integration

Tests are designed to run in CI environments:
- No external dependencies beyond standard Unix tools
- Deterministic results
- Clear pass/fail indicators
- Reasonable execution time (~1 second total)