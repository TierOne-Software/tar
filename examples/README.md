# tierone-archive Examples

This directory contains example programs demonstrating various features and use cases of the tierone-archive library. The examples range from basic usage patterns to advanced features and debugging tools.

## Building the Examples

The examples are built as part of the main project build process:

```bash
mkdir build && cd build
cmake ..
make
```

All example binaries will be created in the `build/examples/` directory.

## Example Categories

### 1. Basic Usage Examples

These examples demonstrate fundamental operations with tar archives.

#### **basic_usage**
Shows how to open and iterate through a tar archive, displaying metadata and content previews.

```bash
./basic_usage <tar_file>
```

**Parameters:**
- `<tar_file>`: Path to the tar archive to read

**Features demonstrated:**
- Opening tar archives
- Iterating through entries
- Displaying entry metadata (type, size, modification time, path)
- Reading file content previews

#### **simple_count**
Counts the total number of entries in a tar archive with progress reporting.

```bash
./simple_count <tar_file>
```

**Parameters:**
- `<tar_file>`: Path to the tar archive to count

**Features demonstrated:**
- Efficient iteration for large archives
- Progress reporting (every 1000 entries)
- Minimal memory usage

#### **extract_files**
Extracts all files from a tar archive to a specified directory.

```bash
./extract_files <tar_file> <output_dir>
```

**Parameters:**
- `<tar_file>`: Path to the tar archive to extract
- `<output_dir>`: Directory where files will be extracted

**Features demonstrated:**
- Directory creation
- File extraction with error handling
- Progress tracking with byte count statistics

### 2. GNU Tar Extension Examples

These examples showcase GNU tar format extensions and advanced features.

#### **gnu_tar_demo**
Demonstrates GNU tar format extensions including long filenames and link targets.

```bash
./gnu_tar_demo <tar_file>
```

**Parameters:**
- `<tar_file>`: Path to a tar archive (preferably with GNU extensions)

**Features demonstrated:**
- Long filename support (>100 characters)
- Long link target support
- GNU vs POSIX format detection
- Extension usage statistics

#### **sparse_demo**
Handles sparse files in tar archives, showing hole information and compression ratios.

```bash
./sparse_demo <tar_file>
```

**Parameters:**
- `<tar_file>`: Path to a tar archive containing sparse files

**Features demonstrated:**
- Sparse file detection
- Hole/data segment mapping
- Compression ratio calculation
- Sparse data reading

**Creating test sparse files:**
```bash
# Create a 1GB sparse file with data at specific offsets
truncate -s 1G sparse_file
echo "data" | dd of=sparse_file bs=1 seek=1000 conv=notrunc
echo "more data" | dd of=sparse_file bs=1 seek=1000000 conv=notrunc
tar --sparse -cf sparse.tar sparse_file
```

### 3. Extended Metadata Examples

#### **extended_metadata_demo**
Extracts and displays extended metadata including device files, extended attributes, and POSIX ACLs.

```bash
./extended_metadata_demo <tar_file>
```

**Parameters:**
- `<tar_file>`: Path to a tar archive with extended metadata

**Features demonstrated:**
- Device file information (major/minor numbers)
- Extended attributes (xattr)
- POSIX ACLs parsing and display
- Comprehensive entry type detection

**Creating test archives with extended metadata:**
```bash
# Create a file with extended attributes
touch test_file
setfattr -n user.comment -v "test attribute" test_file

# Create a file with ACLs
setfacl -m u:1000:rw test_file

# Create device files (requires root)
sudo mknod test_char c 1 3
sudo mknod test_block b 8 0

# Create archive preserving all metadata
tar --xattrs --acls -cf metadata.tar test_file test_char test_block
```

### 4. Debugging and Testing Tools

These tools help debug issues and test specific scenarios.

#### **debug_tar**
General-purpose debug tool for analyzing tar archives.

```bash
./debug_tar <tar_file>
```

**Features:**
- GNU extension detection
- Error tracking during iteration
- Progress reporting for large archives
- Detailed exception information

#### **debug_extract** / **debug_extract_v2**
Debug extraction process with manual iterator control.

```bash
./debug_extract <tar_file>
```

**Features:**
- Step-by-step iterator control
- Limited entry processing (10 entries)
- Iterator state validation

#### **debug_streaming_v2**
Tests reading data while iterating through archives.

```bash
./debug_streaming_v2 <tar_file>
```

**Features:**
- Data reading during iteration
- Iterator stability testing
- Small file content display

#### **debug_large_entry**
Tests handling of large files in archives.

```bash
./debug_large_entry <tar_file>
```

**Features:**
- Focus on specific large entries
- Iterator advancement tracking
- Error detection after large reads

#### **debug_sparse** / **debug_sparse_1_0**
Debug tools for sparse file format issues.

```bash
./debug_sparse <tar_file>
./debug_sparse_1_0 <tar_file>  # For PAX 1.0 sparse format
```

**Features:**
- Sparse format debugging
- Version-specific testing
- Segment information display

#### **test_error**
Tests error conditions and empty archives.

```bash
./test_error <tar_file>
```

**Features:**
- Archive validation
- Iterator error states
- Empty archive detection

#### **test_pax_parser**
Unit test for PAX header parsing (no parameters required).

```bash
./test_pax_parser
```

**Features:**
- PAX header format validation
- GNU sparse marker detection
- Hardcoded test cases

## Common Use Cases

### Inspecting an Archive
```bash
# Get basic information about archive contents
./basic_usage archive.tar

# Count entries in a large archive
./simple_count large_archive.tar

# Debug issues with a problematic archive
./debug_tar problematic.tar
```

### Extracting Archives
```bash
# Extract all files
./extract_files archive.tar output_directory/

# Debug extraction issues
./debug_extract archive.tar
```

### Working with Special Formats
```bash
# Handle GNU tar extensions
./gnu_tar_demo gnu_archive.tar

# Process sparse files
./sparse_demo sparse_archive.tar

# Extract extended metadata
./extended_metadata_demo metadata_archive.tar
```

## Error Handling

All examples include error handling and will report:
- File not found errors
- Invalid tar format errors
- Permission errors during extraction
- Memory allocation failures
- Corrupted archive data

## Performance Considerations

- The examples use memory-mapped I/O when possible for better performance
- Large file support is included (files >4GB)
- Sparse file handling minimizes memory usage
- Progress reporting helps monitor long-running operations

## Contributing

When adding new examples:
1. Follow the existing naming convention (`category_description.cpp`)
2. Include clear command-line parameter documentation
3. Add appropriate error handling
4. Update this README with the new example
5. Add the example to `CMakeLists.txt`