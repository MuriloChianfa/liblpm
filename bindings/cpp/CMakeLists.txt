cmake_minimum_required(VERSION 3.16)

# C++ wrapper requires C++17 for std::span and other features
project(liblpm_cpp VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include GNUInstallDirs for CMAKE_INSTALL_* variables
include(GNUInstallDirs)

# Find or import lpm library
if(NOT TARGET lpm)
    # Standalone build - find system lpm or use parent project
    set(LPM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    if(EXISTS "${LPM_ROOT_DIR}/include/lpm.h")
        # Build from parent project source
        add_library(lpm SHARED IMPORTED)
        set_target_properties(lpm PROPERTIES
            IMPORTED_LOCATION "${LPM_ROOT_DIR}/build/liblpm.so"
            INTERFACE_INCLUDE_DIRECTORIES "${LPM_ROOT_DIR}/include"
        )
    else()
        # Try to find installed liblpm
        find_library(LPM_LIBRARY NAMES lpm)
        # Headers are now in lpm/ subdirectory
        find_path(LPM_INCLUDE_DIR lpm/lpm.h)
        if(LPM_LIBRARY AND LPM_INCLUDE_DIR)
            add_library(lpm SHARED IMPORTED)
            set_target_properties(lpm PROPERTIES
                IMPORTED_LOCATION "${LPM_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LPM_INCLUDE_DIR}/lpm"
            )
        else()
            message(FATAL_ERROR "liblpm not found. Build the main project first or install liblpm.")
        endif()
    endif()
endif()

# C++ wrapper - header-only interface library
add_library(lpm_cpp INTERFACE)
target_include_directories(lpm_cpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(lpm_cpp INTERFACE lpm)
target_compile_features(lpm_cpp INTERFACE cxx_std_17)

# Optional: compile helper functions (string parsing)
add_library(lpm_cpp_impl STATIC src/lpm_impl.cpp)
target_link_libraries(lpm_cpp_impl PUBLIC lpm_cpp)
target_include_directories(lpm_cpp_impl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Install C++ headers (both liblpm and liblpm.hpp)
install(FILES 
    include/liblpm
    include/liblpm.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install libraries
install(TARGETS lpm_cpp lpm_cpp_impl
    EXPORT liblpmCppTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Export targets
install(EXPORT liblpmCppTargets
    FILE liblpmCppTargets.cmake
    NAMESPACE liblpm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liblpm
)

# Examples
add_subdirectory(examples)

# Tests (optional)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "C++ wrapper configuration:")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Header-only: YES")
message(STATUS "  Helper library: lpm_cpp_impl")

