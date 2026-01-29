cmake_minimum_required(VERSION 3.16)

project(liblpm_python VERSION 2.0.0 LANGUAGES C)

# ============================================================================
# Options
# ============================================================================

option(LIBLPM_PYTHON_STANDALONE "Build Python bindings standalone (not from parent)" OFF)

# ============================================================================
# Find Dependencies
# ============================================================================

# Find Python
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

# Find Cython
find_program(CYTHON_EXECUTABLE
    NAMES cython cython3
    HINTS ${Python_EXECUTABLE}/../
    DOC "Cython executable"
)

if(NOT CYTHON_EXECUTABLE)
    message(FATAL_ERROR "Cython not found. Install with: pip install cython>=3.0.0")
endif()

message(STATUS "Found Cython: ${CYTHON_EXECUTABLE}")

# ============================================================================
# Find liblpm
# ============================================================================

if(NOT TARGET lpm)
    # Not building from parent project
    set(LIBLPM_PYTHON_STANDALONE ON)
    
    # Try parent project build directory first
    set(LPM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    
    if(EXISTS "${LPM_ROOT_DIR}/include/lpm.h")
        message(STATUS "Found liblpm in parent directory")
        
        # Check for build directory
        if(EXISTS "${LPM_ROOT_DIR}/build/liblpm.so")
            set(LPM_LIBRARY "${LPM_ROOT_DIR}/build/liblpm.so")
        elseif(EXISTS "${LPM_ROOT_DIR}/build/liblpm.dylib")
            set(LPM_LIBRARY "${LPM_ROOT_DIR}/build/liblpm.dylib")
        else()
            message(FATAL_ERROR 
                "liblpm library not found. Build the main project first:\n"
                "  cd ${LPM_ROOT_DIR} && mkdir -p build && cd build && cmake .. && make"
            )
        endif()
        
        set(LPM_INCLUDE_DIR "${LPM_ROOT_DIR}/include")
        
        # Create imported target
        add_library(lpm SHARED IMPORTED)
        set_target_properties(lpm PROPERTIES
            IMPORTED_LOCATION "${LPM_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LPM_INCLUDE_DIR}"
        )
    else()
        # Try system-installed liblpm
        find_library(LPM_LIBRARY NAMES lpm
            PATHS /usr/local/lib /usr/lib
        )
        find_path(LPM_INCLUDE_DIR lpm.h
            PATHS /usr/local/include/lpm /usr/include/lpm
        )
        
        if(LPM_LIBRARY AND LPM_INCLUDE_DIR)
            message(STATUS "Found system liblpm: ${LPM_LIBRARY}")
            add_library(lpm SHARED IMPORTED)
            set_target_properties(lpm PROPERTIES
                IMPORTED_LOCATION "${LPM_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LPM_INCLUDE_DIR}"
            )
        else()
            message(FATAL_ERROR 
                "liblpm not found. Install it system-wide or build from source first."
            )
        endif()
    endif()
else()
    message(STATUS "Using liblpm from parent project")
    # Get include directory from parent target
    get_target_property(LPM_INCLUDE_DIR lpm INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT LPM_INCLUDE_DIR)
        set(LPM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
    endif()
endif()

# ============================================================================
# Cython Compilation
# ============================================================================

set(CYTHON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/liblpm")
set(CYTHON_PYX_FILE "${CYTHON_SOURCE_DIR}/_liblpm.pyx")
set(CYTHON_PXD_FILE "${CYTHON_SOURCE_DIR}/_liblpm.pxd")
set(CYTHON_C_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_liblpm.c")

# Custom command to run Cython
add_custom_command(
    OUTPUT ${CYTHON_C_OUTPUT}
    COMMAND ${CYTHON_EXECUTABLE}
        -3
        --fast-fail
        -o ${CYTHON_C_OUTPUT}
        ${CYTHON_PYX_FILE}
    DEPENDS ${CYTHON_PYX_FILE} ${CYTHON_PXD_FILE}
    COMMENT "Generating C code from Cython source"
    VERBATIM
)

# ============================================================================
# Build Python Extension Module
# ============================================================================

# Create the extension module
Python_add_library(_liblpm MODULE WITH_SOABI
    ${CYTHON_C_OUTPUT}
)

# Include directories
target_include_directories(_liblpm PRIVATE
    ${LPM_INCLUDE_DIR}
    ${Python_INCLUDE_DIRS}
)

# Link against liblpm
target_link_libraries(_liblpm PRIVATE lpm)

# Set module properties
set_target_properties(_liblpm PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(_liblpm PRIVATE
        -O3
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# ============================================================================
# Installation
# ============================================================================

# Install the extension module
install(TARGETS _liblpm
    LIBRARY DESTINATION liblpm
    ARCHIVE DESTINATION liblpm
)

# Install Python source files
install(FILES
    ${CYTHON_SOURCE_DIR}/__init__.py
    ${CYTHON_SOURCE_DIR}/table.py
    ${CYTHON_SOURCE_DIR}/exceptions.py
    DESTINATION liblpm
)

# Install type stubs
install(FILES
    ${CYTHON_SOURCE_DIR}/__init__.pyi
    ${CYTHON_SOURCE_DIR}/table.pyi
    ${CYTHON_SOURCE_DIR}/exceptions.pyi
    ${CYTHON_SOURCE_DIR}/py.typed
    DESTINATION liblpm
)

# ============================================================================
# Development Targets
# ============================================================================

# Target to run tests
add_custom_target(python_test
    COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
    DEPENDS _liblpm
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Python tests"
)

# Target to run benchmarks
add_custom_target(python_benchmark
    COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks --benchmark-only
    DEPENDS _liblpm
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Python benchmarks"
)

# Target to run examples
add_custom_target(python_example
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_example.py
    DEPENDS _liblpm
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Python basic example"
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Python bindings configuration:")
message(STATUS "  Python executable: ${Python_EXECUTABLE}")
message(STATUS "  Python version: ${Python_VERSION}")
message(STATUS "  Cython executable: ${CYTHON_EXECUTABLE}")
message(STATUS "  liblpm include: ${LPM_INCLUDE_DIR}")
if(LIBLPM_PYTHON_STANDALONE)
    message(STATUS "  liblpm library: ${LPM_LIBRARY}")
endif()
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
