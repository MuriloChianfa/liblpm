# ============================================================================
# CMakeLists.txt for liblpm Lua bindings
# ============================================================================
#
# Supports:
#   - Lua 5.3, 5.4, and LuaJIT 2.1+
#   - CMake 3.16+
#
# Build options:
#   cmake -DBUILD_LUA_WRAPPER=ON ..
#
# Targets:
#   lpm_lua     - Lua C module (shared library)
#   lua_test    - Run Lua tests
#   lua_example - Run basic example
#
# ============================================================================

cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Find Lua
# ============================================================================

# Try to find Lua using CMake's FindLua module
# This will search for Lua 5.4, 5.3, 5.2, 5.1 in order
find_package(Lua)

if(NOT LUA_FOUND)
    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        # Try specific versions
        pkg_check_modules(LUA QUIET lua5.4)
        if(NOT LUA_FOUND)
            pkg_check_modules(LUA QUIET lua5.3)
        endif()
        if(NOT LUA_FOUND)
            pkg_check_modules(LUA QUIET luajit)
        endif()
        if(NOT LUA_FOUND)
            pkg_check_modules(LUA QUIET lua)
        endif()
    endif()
endif()

if(NOT LUA_FOUND)
    message(WARNING "Lua not found. Lua bindings will not be built.")
    message(WARNING "Install Lua development files:")
    message(WARNING "  Ubuntu/Debian: sudo apt install liblua5.4-dev")
    message(WARNING "  Fedora:        sudo dnf install lua-devel")
    message(WARNING "  macOS:         brew install lua")
    return()
endif()

# Check Lua version (require 5.3+)
if(LUA_VERSION_STRING VERSION_LESS "5.3")
    message(WARNING "Lua ${LUA_VERSION_STRING} found, but 5.3+ is required.")
    message(WARNING "Lua bindings will not be built.")
    return()
endif()

message(STATUS "Found Lua: ${LUA_VERSION_STRING}")
message(STATUS "  Include: ${LUA_INCLUDE_DIR}")
message(STATUS "  Library: ${LUA_LIBRARIES}")

# ============================================================================
# Lua C Module
# ============================================================================

# Source files
set(LPM_LUA_SOURCES
    src/liblpm.c
    src/liblpm_utils.c
)

# Create shared library module
add_library(lpm_lua MODULE ${LPM_LUA_SOURCES})

# Set output name to match Lua's require() expectations
# The module will be named "liblpm.so" (or .dll on Windows)
set_target_properties(lpm_lua PROPERTIES
    OUTPUT_NAME "liblpm"
    PREFIX ""
    SUFFIX ".so"
)

# Platform-specific suffix
if(APPLE)
    set_target_properties(lpm_lua PROPERTIES SUFFIX ".so")
elseif(WIN32)
    set_target_properties(lpm_lua PROPERTIES SUFFIX ".dll")
endif()

# Include directories
target_include_directories(lpm_lua PRIVATE
    ${LUA_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Link against liblpm shared library
# Note: The liblpm.so must be loadable (in LD_LIBRARY_PATH or installed)
# before the Lua module is loaded, due to ifunc resolver requirements
target_link_libraries(lpm_lua PRIVATE
    lpm
    ${LUA_LIBRARIES}
)

# Set RPATH to find liblpm.so in the build directory and install location
set_target_properties(lpm_lua PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
)

# C standard
set_target_properties(lpm_lua PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lpm_lua PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# ============================================================================
# Installation
# ============================================================================

# Determine Lua install path
if(DEFINED LUA_CPATH)
    set(LUA_INSTALL_CPATH ${LUA_CPATH})
else()
    # Default to standard Lua cpath
    # This can be overridden with -DLUA_CPATH=/path/to/lua/cpath
    set(LUA_INSTALL_CPATH "${CMAKE_INSTALL_LIBDIR}/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")
endif()

install(TARGETS lpm_lua
    LIBRARY DESTINATION ${LUA_INSTALL_CPATH}
    RUNTIME DESTINATION ${LUA_INSTALL_CPATH}
)

# ============================================================================
# Test Target
# ============================================================================

# Find Lua interpreter
find_program(LUA_EXECUTABLE
    NAMES lua${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR} lua
    HINTS ${LUA_INCLUDE_DIR}/../bin
)

if(LUA_EXECUTABLE)
    message(STATUS "Lua interpreter: ${LUA_EXECUTABLE}")
    
    # Test target
    # Note: LD_PRELOAD is required to ensure liblpm.so's ifunc resolvers are
    # executed before the Lua module is loaded via dlopen
    add_custom_target(lua_test
        COMMAND ${CMAKE_COMMAND} -E env 
            "LD_PRELOAD=${CMAKE_BINARY_DIR}/liblpm.so"
            "LUA_CPATH=${CMAKE_CURRENT_BINARY_DIR}/?.so"
            ${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lpm.lua
        DEPENDS lpm_lua lpm
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running Lua tests"
    )
    
    # Example target
    add_custom_target(lua_example
        COMMAND ${CMAKE_COMMAND} -E env 
            "LD_PRELOAD=${CMAKE_BINARY_DIR}/liblpm.so"
            "LUA_CPATH=${CMAKE_CURRENT_BINARY_DIR}/?.so"
            ${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_example.lua
        DEPENDS lpm_lua lpm
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running Lua basic example"
    )
    
    # Add to CTest
    if(BUILD_TESTS)
        add_test(NAME lua_wrapper_test
            COMMAND ${CMAKE_COMMAND} -E env
                "LD_PRELOAD=${CMAKE_BINARY_DIR}/liblpm.so"
                "LUA_CPATH=${CMAKE_CURRENT_BINARY_DIR}/?.so"
                ${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lpm.lua
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
else()
    message(WARNING "Lua interpreter not found. Test targets will not be available.")
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "Lua bindings configuration:")
message(STATUS "  Lua version: ${LUA_VERSION_STRING}")
message(STATUS "  Install path: ${LUA_INSTALL_CPATH}")
if(LUA_EXECUTABLE)
    message(STATUS "  Test command: make lua_test")
    message(STATUS "  Example command: make lua_example")
endif()
