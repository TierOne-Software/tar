# Test Directory Structure

This directory contains all tests for the tierone-tar library.

## Directory Layout

```
tests/
├── README.md                    # This file
├── CMakeLists.txt              # Test configuration
├── test_summary.md             # Test coverage summary
├── scripts/                    # Test scripts
│   ├── test_sparse_real.sh     # Comprehensive sparse file tests
│   └── test_sparse_final.sh    # Quick verification tests
├── fixtures/                   # Test data files
│   ├── *.tar                   # Test archive files  
│   ├── *.bin                   # Test binary files
│   └── extracted*/             # Extracted test directories
├── data/                       # Static test data (currently empty)
├── test_*.cpp                  # C++ unit tests
└── (built tests go to build/tests/)
```

## Running Tests

### All Tests
```bash
cd build && ctest
```

### Specific Test Categories
```bash
ctest -L unit         # Unit tests only
ctest -L integration  # Integration tests only  
ctest -L sparse       # Sparse file tests only
ctest -L quick        # Quick verification tests
```

### Individual Tests
```bash
# C++ unit tests
./tests/tierone-tar-tests

# Individual bash scripts  
./tests/test_sparse_real.sh
./tests/test_sparse_final.sh
```

## Test Types

### Unit Tests (C++)
- **test_header_parser.cpp** - TAR header parsing tests
- **test_archive_reader.cpp** - Archive reading functionality  
- **test_gnu_tar.cpp** - GNU TAR extension tests
- **test_sparse.cpp** - Sparse file metadata and PAX parsing tests

### Integration Tests (Bash)
- **test_sparse_real.sh** - Creates real sparse files using `dd` and GNU `tar`, tests reading and extraction
- **test_sparse_final.sh** - Quick verification that all sparse functionality works

## Test Data Management

### Fixtures Directory
The `fixtures/` directory contains:
- Generated test archives (*.tar)
- Sample sparse files (*.bin)  
- Extracted test results (extracted*)

These files are automatically created and cleaned up by the test scripts.

### Static Test Data  
The `data/` directory is reserved for permanent test files that need to be checked into the repository. Currently no static test data is required as all tests generate their own test data.

## Adding New Tests

### C++ Unit Tests
1. Create `test_<feature>.cpp` in the tests directory
2. Add the file to `CMakeLists.txt` in the `tierone-tar-tests` target
3. Use Catch2 framework for test structure

### Integration Tests
1. Create script in `tests/scripts/`
2. Add to `CMakeLists.txt` with `add_test()`
3. Use appropriate labels for categorization

### Test Data
- Temporary test data: Create in test scripts, clean up automatically
- Permanent test data: Add to `tests/data/` directory
- Test fixtures: Use `tests/fixtures/` for generated files

## Dependencies

- **C++ Tests**: Catch2 (automatically found by CMake)
- **Integration Tests**: bash, dd, tar, standard Unix tools
- **Build System**: CMake 3.25+, CTest