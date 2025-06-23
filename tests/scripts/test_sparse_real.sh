#!/bin/bash

# Test script for GNU tar sparse file support
# Creates various sparse files and tests our library's ability to read them

set -e

echo "=== GNU Tar Sparse File Test Suite ==="
echo

if [ -f "CMakeCache.txt" ]; then
    # We're in the build directory
    TEST_ROOT="../tests"
    BUILD_DIR="."
else
    # We're being run from somewhere else, assume we're in project root
    TEST_ROOT="tests"
    BUILD_DIR="build"
fi


# Clean up any existing test files
cleanup() {
    rm -f sparse_*.bin test_sparse_*.tar extracted_* test_output.txt
}

cleanup
trap cleanup EXIT

# Check for test tools
echo "Checking for test tools..."
# Check if we're already in the build directory
if [ -f "CMakeCache.txt" ]; then
    BUILD_DIR="."
else
    BUILD_DIR="build"
fi

# Check if executables exist
if [ -f "$BUILD_DIR/examples/debug_sparse" ] && \
   [ -f "$BUILD_DIR/examples/sparse_demo" ] && \
   [ -f "$BUILD_DIR/examples/extract_files" ]; then
    echo "✓ Test tools found"
else
    echo "Error: Required test tools not found in $BUILD_DIR/examples/"
    echo "Please build debug_sparse, sparse_demo, and extract_files examples first"
    exit 1
fi
echo

# Test 1: Simple sparse file with hole at beginning
echo "Test 1: Hole at beginning (Format 0.0)"
dd if=/dev/zero of=sparse_begin.bin bs=1M count=3 seek=2 2>/dev/null
tar --sparse -cf test_sparse_begin_00.tar sparse_begin.bin
echo "  Created: $(ls -lh sparse_begin.bin | awk '{print $5}') logical, $(du -h sparse_begin.bin | awk '{print $1}') actual"

# Test with our library
./$BUILD_DIR/examples/sparse_demo test_sparse_begin_00.tar > test_output.txt
if grep -q "Sparse files: 1" test_output.txt; then
    echo "  ✓ Format 0.0 detection works"
else
    echo "  ✗ Format 0.0 detection failed"
    cat test_output.txt
fi

# Test 2: Same file with format 1.0
echo
echo "Test 2: Hole at beginning (Format 1.0)"
tar --sparse --sparse-version=1.0 --format=pax -cf test_sparse_begin_10.tar sparse_begin.bin

./$BUILD_DIR/examples/sparse_demo test_sparse_begin_10.tar > test_output.txt
if grep -q "Sparse files: 1" test_output.txt; then
    echo "  ✓ Format 1.0 detection works"
else
    echo "  ✗ Format 1.0 detection failed"
    cat test_output.txt
fi

# Test 3: Hole in the middle
echo
echo "Test 3: Hole in middle"
# Create file with: data, hole, data
dd if=/dev/urandom of=sparse_middle.bin bs=1M count=2 2>/dev/null
dd if=/dev/zero of=sparse_middle.bin bs=1M count=2 seek=2 oflag=append conv=notrunc 2>/dev/null
dd if=/dev/urandom of=sparse_middle.bin bs=1M count=2 seek=4 conv=notrunc 2>/dev/null
# Force sparseness by making actual hole
cp --sparse=always sparse_middle.bin sparse_middle_sparse.bin
tar --sparse --sparse-version=1.0 --format=pax -cf test_sparse_middle.tar sparse_middle_sparse.bin
echo "  Created: $(ls -lh sparse_middle.bin | awk '{print $5}') logical, $(du -h sparse_middle.bin | awk '{print $1}') actual"

./$BUILD_DIR/examples/sparse_demo test_sparse_middle.tar > test_output.txt
if grep -q "Sparse files: 1" test_output.txt; then
    echo "  ✓ Middle hole detection works"
    grep "Total real size\|Total data size\|Space saved" test_output.txt
else
    echo "  ✗ Middle hole detection failed"
    cat test_output.txt
fi

# Test 4: Multiple holes
echo
echo "Test 4: Multiple holes"
dd if=/dev/urandom of=sparse_multi.bin bs=512K count=2 2>/dev/null
dd if=/dev/zero of=sparse_multi.bin bs=512K count=4 seek=4 2>/dev/null
dd if=/dev/urandom of=sparse_multi.bin bs=512K count=2 seek=8 conv=notrunc 2>/dev/null
dd if=/dev/zero of=sparse_multi.bin bs=512K count=4 seek=12 2>/dev/null
dd if=/dev/urandom of=sparse_multi.bin bs=512K count=2 seek=16 conv=notrunc 2>/dev/null
tar --sparse --sparse-version=1.0 --format=pax -cf test_sparse_multi.tar sparse_multi.bin
echo "  Created: $(ls -lh sparse_multi.bin | awk '{print $5}') logical, $(du -h sparse_multi.bin | awk '{print $1}') actual"

./$BUILD_DIR/examples/sparse_demo test_sparse_multi.tar > test_output.txt
echo "  Sparse analysis:"
grep "Segments:\|Total real size\|Total data size\|Space saved" test_output.txt

# Test 5: Large sparse file
echo
echo "Test 5: Large sparse file (100MB logical, ~10MB actual)"
dd if=/dev/zero of=sparse_large.bin bs=1M count=10 seek=90 2>/dev/null
tar --sparse --sparse-version=1.0 --format=pax -cf test_sparse_large.tar sparse_large.bin
echo "  Created: $(ls -lh sparse_large.bin | awk '{print $5}') logical, $(du -h sparse_large.bin | awk '{print $1}') actual"

./$BUILD_DIR/examples/sparse_demo test_sparse_large.tar > test_output.txt
echo "  Large file analysis:"
grep "Total real size\|Total data size\|Space saved" test_output.txt

# Test 6: Mixed archive (sparse + regular files)
echo
echo "Test 6: Mixed archive"
echo "This is a regular file" > regular.txt
dd if=/dev/zero of=sparse_mixed.bin bs=1M count=5 seek=5 2>/dev/null
tar --sparse --sparse-version=1.0 --format=pax -cf test_mixed.tar regular.txt sparse_mixed.bin

echo "  Archive contents:"
./$BUILD_DIR/examples/debug_sparse test_mixed.tar | grep "Entry\|is_sparse"

# Test 7: Extraction test
echo
echo "Test 7: Extraction verification"
rm -rf extracted && mkdir -p extracted
if ./$BUILD_DIR/examples/extract_files test_sparse_begin_10.tar extracted/; then
    echo "  ✓ Extraction completed successfully"
    
    # Find the extracted file (accounting for the GNU sparse directory structure)
    extracted_file=$(find extracted -name "sparse_begin.bin" | head -1)
    if [ -f "$extracted_file" ]; then
        original_size=$(stat -c%s sparse_begin.bin 2>/dev/null || stat -f%z sparse_begin.bin)
        extracted_size=$(stat -c%s "$extracted_file" 2>/dev/null || stat -f%z "$extracted_file")
        
        if [ "$original_size" = "$extracted_size" ]; then
            echo "  ✓ File sizes match: $extracted_size bytes"
            
            # Check if the extracted file has the same content
            if cmp -s sparse_begin.bin "$extracted_file"; then
                echo "  ✓ Extracted file content matches original"
            else
                echo "  ✗ Extracted file content differs from original"
            fi
        else
            echo "  ✗ File size mismatch: extracted=$extracted_size, original=$original_size"
        fi
    else
        echo "  ✗ Extracted file not found"
    fi
else
    echo "  ✗ Extraction failed"
fi

# Test 8: Performance comparison
echo
echo "Test 8: Performance comparison"
time_regular=$(tar -tf test_sparse_large.tar | wc -l)
time_our_lib=$(./$BUILD_DIR/examples/sparse_demo test_sparse_large.tar | grep "Total files:" | awk '{print $3}')

if [ "$time_regular" -eq "$time_our_lib" ]; then
    echo "  ✓ Entry count matches standard tar"
else
    echo "  ✗ Entry count mismatch: tar=$time_regular, our_lib=$time_our_lib"
fi

echo
echo "=== Test Summary ==="
echo "All sparse file tests completed. Check output above for any failures."
echo "Archive files created for manual inspection:"
ls -lh test_sparse_*.tar test_mixed.tar 2>/dev/null || true