add_executable(tierone-tar-tests
    test_header_parser.cpp
    test_header_parser_edge_cases.cpp
    test_archive_reader.cpp
    test_gnu_tar.cpp
    test_sparse.cpp
    test_extended_metadata.cpp
    test_archive_entry.cpp
    test_archive_entry_extract.cpp
    test_stream.cpp
    test_pax_parser.cpp
    test_tar_api.cpp
    test_error_handling_integration.cpp
    test_large_file_integration.cpp
)

target_link_libraries(tierone-tar-tests
    PRIVATE
        tierone::tar
        Catch2::Catch2WithMain
)

# Register tests with CTest
include(Catch)
catch_discover_tests(tierone-tar-tests)

# Add bash script tests
if(UNIX)
    # Copy test scripts to build directory
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_sparse_real.sh
                   ${CMAKE_CURRENT_BINARY_DIR}/test_sparse_real.sh
                   COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_sparse_final.sh
                   ${CMAKE_CURRENT_BINARY_DIR}/test_sparse_final.sh
                   COPYONLY)
    
    # Make scripts executable
    file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/test_sparse_real.sh
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)
    file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/test_sparse_final.sh
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)
    
    # Add sparse file tests
    add_test(NAME sparse_real_archives
             COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_sparse_real.sh
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    
    add_test(NAME sparse_final_verification
             COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_sparse_final.sh
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    
    # Set test properties
    set_tests_properties(sparse_real_archives PROPERTIES
        LABELS "integration;sparse"
        TIMEOUT 60)
    
    set_tests_properties(sparse_final_verification PROPERTIES
        LABELS "integration;sparse;quick"
        TIMEOUT 30)
endif()