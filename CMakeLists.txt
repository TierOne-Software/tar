cmake_minimum_required(VERSION 3.25)

# Setup Hunter package manager
include("${CMAKE_CURRENT_LIST_DIR}/cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.26.1.tar.gz"
    SHA1 "e41ac7a18c49b35ebac99ff2b5244317b2638a65"
)

project(tierone-tar VERSION 0.1.0 LANGUAGES CXX)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
endif()
set(CMAKE_BUILD_TYPE Debug)

# Require C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
include(FetchContent)

# Catch2 v3.6.0
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)


# Options
option(TIERONE_TAR_BUILD_TESTS "Build tests" ON)
option(TIERONE_TAR_BUILD_EXAMPLES "Build examples" ON)
option(TIERONE_TAR_ENABLE_WARNINGS "Enable extra warnings" ON)

# Create the main library
add_library(tierone-tar
    src/tar.cpp
    src/header_parser.cpp
    src/archive_reader.cpp
    src/archive_entry.cpp
    src/stream.cpp
    src/gnu_tar.cpp
    src/sparse.cpp
    src/pax_parser.cpp
)

# Alias for easier use
add_library(tierone::tar ALIAS tierone-tar)

# Include directories
target_include_directories(tierone-tar
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile features
target_compile_features(tierone-tar PUBLIC cxx_std_23)


# Find required dependencies
find_package(Threads REQUIRED)
target_link_libraries(tierone-tar PUBLIC Threads::Threads)

# Compiler warnings
if(TIERONE_TAR_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(tierone-tar PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wcast-align
            -Wcast-qual
            -Wconversion
            -Wctor-dtor-privacy
            -Wdisabled-optimization
            -Wformat=2
            -Winit-self
            -Wmissing-declarations
            -Wmissing-include-dirs
            -Wold-style-cast
            -Woverloaded-virtual
            -Wredundant-decls
            -Wshadow
            -Wsign-conversion
            -Wsign-promo
            -Wstrict-overflow=5
            -Wswitch-default
            -Wundef
            -Wno-unused
        )
    endif()
endif()

# Tests
if(TIERONE_TAR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(TIERONE_TAR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS tierone-tar
    EXPORT tierone-tar-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT tierone-tar-targets
    FILE tierone-tar-config.cmake
    NAMESPACE tierone::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tierone-tar
)
