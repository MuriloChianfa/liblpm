# CMakeLists.txt for liblpm PHP extension
#
# This file enables building the PHP extension using CMake directly.
# For production use, prefer the phpize/configure/make workflow.
#
# Usage:
#   mkdir build && cd build
#   cmake -DBUILD_PHP_WRAPPER=ON ..
#   make php_wrapper
#   make php_test
#   make php_install
#
# Or standalone (requires liblpm to be installed):
#   cd bindings/php
#   phpize
#   ./configure --with-liblpm=/usr/local
#   make
#   make test
#   sudo make install

cmake_minimum_required(VERSION 3.16)
project(liblpm_php LANGUAGES C)

# Find php-config
find_program(PHP_CONFIG_EXECUTABLE NAMES php-config php-config8.3 php-config8.2 php-config8.1)

if(NOT PHP_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "php-config not found. Install PHP development files.")
endif()

# Get PHP configuration
execute_process(
    COMMAND ${PHP_CONFIG_EXECUTABLE} --version
    OUTPUT_VARIABLE PHP_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${PHP_CONFIG_EXECUTABLE} --includes
    OUTPUT_VARIABLE PHP_INCLUDES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${PHP_CONFIG_EXECUTABLE} --extension-dir
    OUTPUT_VARIABLE PHP_EXTENSION_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${PHP_CONFIG_EXECUTABLE} --php-binary
    OUTPUT_VARIABLE PHP_BINARY
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "PHP version: ${PHP_VERSION}")
message(STATUS "PHP extension dir: ${PHP_EXTENSION_DIR}")

# Parse include flags
separate_arguments(PHP_INCLUDE_FLAGS UNIX_COMMAND "${PHP_INCLUDES}")

# Find liblpm
find_library(LPM_LIBRARY NAMES lpm PATHS /usr/local/lib /usr/lib)
find_path(LPM_INCLUDE_DIR lpm/lpm.h PATHS /usr/local/include /usr/include)

if(NOT LPM_LIBRARY OR NOT LPM_INCLUDE_DIR)
    # Try parent project
    if(TARGET lpm)
        set(LPM_LIBRARY lpm)
        set(LPM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
    else()
        message(FATAL_ERROR "liblpm not found. Install liblpm or build from parent project.")
    endif()
endif()

message(STATUS "liblpm library: ${LPM_LIBRARY}")
message(STATUS "liblpm include: ${LPM_INCLUDE_DIR}")

# Build the PHP extension as a shared library
add_library(liblpm_php MODULE
    liblpm.c
)

# Set output name to liblpm (produces liblpm.so)
set_target_properties(liblpm_php PROPERTIES
    OUTPUT_NAME liblpm
    PREFIX ""
    SUFFIX ".so"
)

# Add PHP include paths
target_include_directories(liblpm_php PRIVATE
    ${LPM_INCLUDE_DIR}
)

# Add PHP include flags
target_compile_options(liblpm_php PRIVATE
    ${PHP_INCLUDE_FLAGS}
    -DCOMPILE_DL_LIBLPM
    -DHAVE_CONFIG_H
)

# Link against liblpm
target_link_libraries(liblpm_php PRIVATE
    ${LPM_LIBRARY}
)

# Installation
install(TARGETS liblpm_php
    LIBRARY DESTINATION ${PHP_EXTENSION_DIR}
)

# Test target
add_custom_target(test_php
    COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}"
        ${PHP_BINARY} -d "extension=${CMAKE_CURRENT_BINARY_DIR}/liblpm.so" 
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.php
    DEPENDS liblpm_php
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running PHP extension tests"
)
