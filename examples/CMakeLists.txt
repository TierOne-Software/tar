add_executable(basic_usage
    basic_usage.cpp
)

target_link_libraries(basic_usage
    PRIVATE
        tierone::tar
)

add_executable(extract_files
    extract_files.cpp
)

target_link_libraries(extract_files
    PRIVATE
        tierone::tar
)

add_executable(gnu_tar_demo
    gnu_tar_demo.cpp
)

target_link_libraries(gnu_tar_demo
    PRIVATE
        tierone::tar
)

add_executable(debug_tar
    debug_tar.cpp
)

target_link_libraries(debug_tar
    PRIVATE
        tierone::tar
)

add_executable(simple_count
    simple_count.cpp
)

target_link_libraries(simple_count
    PRIVATE
        tierone::tar
)

add_executable(debug_extract
    debug_extract.cpp
)

target_link_libraries(debug_extract
    PRIVATE
        tierone::tar
)

add_executable(test_streaming_issue
    test_streaming_issue.cpp
)

target_link_libraries(test_streaming_issue
    PRIVATE
        tierone::tar
)

add_executable(debug_streaming_v2
    debug_streaming_v2.cpp
)

target_link_libraries(debug_streaming_v2
    PRIVATE
        tierone::tar
)

add_executable(debug_extract_v2
    debug_extract_v2.cpp
)

target_link_libraries(debug_extract_v2
    PRIVATE
        tierone::tar
)

add_executable(debug_iteration
    debug_iteration.cpp
)

target_link_libraries(debug_iteration
    PRIVATE
        tierone::tar
)

add_executable(debug_padding
    debug_padding.cpp
)

target_link_libraries(debug_padding
    PRIVATE
        tierone::tar
)

add_executable(debug_large_entry
    debug_large_entry.cpp
)

target_link_libraries(debug_large_entry
    PRIVATE
        tierone::tar
)

add_executable(debug_skip_issue
    debug_skip_issue.cpp
)

target_link_libraries(debug_skip_issue
    PRIVATE
        tierone::tar
)

add_executable(sparse_demo
    sparse_demo.cpp
)

target_link_libraries(sparse_demo
    PRIVATE
        tierone::tar
)

add_executable(debug_sparse
    debug_sparse.cpp
)

target_link_libraries(debug_sparse
    PRIVATE
        tierone::tar
)

add_executable(test_error
    test_error.cpp
)

target_link_libraries(test_error
    PRIVATE
        tierone::tar
)

add_executable(test_pax_parser
    test_pax_parser.cpp
)

target_link_libraries(test_pax_parser
    PRIVATE
        tierone::tar
)

add_executable(debug_sparse_1_0
    debug_sparse_1_0.cpp
)

target_link_libraries(debug_sparse_1_0
    PRIVATE
        tierone::tar
)

add_executable(debug_extraction
    debug_extraction.cpp
)

target_link_libraries(debug_extraction
    PRIVATE
        tierone::tar
)

add_executable(extended_metadata_demo
    extended_metadata_demo.cpp
)

target_link_libraries(extended_metadata_demo
    PRIVATE
        tierone::tar
)