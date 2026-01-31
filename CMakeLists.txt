cmake_minimum_required(VERSION 3.16)

# Set GCC as default compiler
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "gcc" CACHE STRING "C compiler" FORCE)
endif()

project(liblpm VERSION 2.0.0 LANGUAGES C)

# Add libdynemit submodule for SIMD detection and ifunc support (required)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/libdynemit/CMakeLists.txt")
    message(FATAL_ERROR "libdynemit submodule not found. Run: git submodule update --init --recursive")
endif()
add_subdirectory(external/libdynemit)

# Set C standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ============================================================================
# Algorithm Selection Options
# ============================================================================

set(LPM_IPV4_DEFAULT "stride8" CACHE STRING "Default IPv4 algorithm (dir24, stride8)")
set_property(CACHE LPM_IPV4_DEFAULT PROPERTY STRINGS "dir24" "stride8")

set(LPM_IPV6_DEFAULT "wide16" CACHE STRING "Default IPv6 algorithm (wide16, stride8)")
set_property(CACHE LPM_IPV6_DEFAULT PROPERTY STRINGS "wide16" "stride8")

# Validate algorithm choices
if(NOT LPM_IPV4_DEFAULT STREQUAL "dir24" AND NOT LPM_IPV4_DEFAULT STREQUAL "stride8")
    message(FATAL_ERROR "LPM_IPV4_DEFAULT must be 'dir24' or 'stride8'")
endif()

if(NOT LPM_IPV6_DEFAULT STREQUAL "wide16" AND NOT LPM_IPV6_DEFAULT STREQUAL "stride8")
    message(FATAL_ERROR "LPM_IPV6_DEFAULT must be 'wide16' or 'stride8'")
endif()

# Generate compile definitions for algorithm selection
if(LPM_IPV4_DEFAULT STREQUAL "dir24")
    add_compile_definitions(LPM_IPV4_DEFAULT_dir24=1)
else()
    add_compile_definitions(LPM_IPV4_DEFAULT_stride8=1)
endif()

if(LPM_IPV6_DEFAULT STREQUAL "wide16")
    add_compile_definitions(LPM_IPV6_DEFAULT_wide16=1)
else()
    add_compile_definitions(LPM_IPV6_DEFAULT_stride8=1)
endif()

# ============================================================================
# Build Options
# ============================================================================

option(BUILD_TESTS "Build test programs" ON)
option(BUILD_BENCHMARKS "Build benchmark programs" OFF)
option(ENABLE_NATIVE_ARCH "Enable native architecture optimizations" OFF)
option(WITH_DPDK_BENCHMARK "Build DPDK comparison benchmark" OFF)
option(WITH_EXTERNAL_LPM_BENCHMARK "Build benchmarks with external LPM libraries" OFF)
option(BUILD_GO_WRAPPER "Build Go wrapper and bindings" OFF)
option(BUILD_CPP_WRAPPER "Build C++ wrapper and bindings" OFF)
option(BUILD_PERL_WRAPPER "Build Perl wrapper and bindings" OFF)
option(BUILD_PHP_WRAPPER "Build PHP wrapper and bindings" OFF)
option(BUILD_PYTHON_WRAPPER "Build Python wrapper and bindings" OFF)
option(LPM_TS_RESOLVERS "Enable thread-safe resolvers (for dlopen contexts)" OFF)

# Thread-safe resolver configuration
if(LPM_TS_RESOLVERS)
    add_compile_definitions(LPM_TS_RESOLVERS=1)
    message(STATUS "Thread-safe resolvers: ENABLED (for dlopen contexts)")
else()
    message(STATUS "Thread-safe resolvers: DISABLED (standard C library)")
endif()

# External LPM libraries directory
set(EXTERNAL_LPM_DIR "" CACHE PATH "Directory containing external LPM libraries")

# Check for external LPM libraries if requested
if(WITH_EXTERNAL_LPM_BENCHMARK)
    if(EXTERNAL_LPM_DIR AND EXISTS "${EXTERNAL_LPM_DIR}")
        message(STATUS "External LPM libraries directory: ${EXTERNAL_LPM_DIR}")
        set(HAVE_EXTERNAL_LPM TRUE)
        
        # Check for individual libraries (only C libraries are supported)
        if(EXISTS "${EXTERNAL_LPM_DIR}/librmindlpm.a")
            set(HAVE_RMIND_LPM TRUE)
            message(STATUS "  Found: rmind/liblpm")
        endif()
        if(EXISTS "${EXTERNAL_LPM_DIR}/libpatricia.a")
            set(HAVE_LIBPATRICIA TRUE)
            message(STATUS "  Found: libpatricia")
        endif()
        # Note: SAIL and XorOffsetTrie are C++ libraries
        # and are not currently supported in the C benchmark
    else()
        message(STATUS "External LPM libraries not found at: ${EXTERNAL_LPM_DIR}")
        message(STATUS "External LPM benchmarks will be disabled")
        set(HAVE_EXTERNAL_LPM FALSE)
    endif()
else()
    set(HAVE_EXTERNAL_LPM FALSE)
endif()

# Find DPDK if requested
if(WITH_DPDK_BENCHMARK)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(DPDK QUIET libdpdk)
        if(DPDK_FOUND)
            message(STATUS "Found DPDK: ${DPDK_VERSION}")
            set(HAVE_DPDK TRUE)
            # Use link directories to avoid individual library resolution issues
            link_directories(${DPDK_LIBRARY_DIRS})
        else()
            message(STATUS "DPDK not found. DPDK comparison benchmark will not be built.")
            message(STATUS "To install DPDK, run: sudo ./scripts/install_dpdk.sh")
            set(HAVE_DPDK FALSE)
        endif()
    else()
        message(STATUS "pkg-config not found. Cannot detect DPDK.")
        set(HAVE_DPDK FALSE)
    endif()
else()
    set(HAVE_DPDK FALSE)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -O3")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3")

# Warning suppressions for test code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare -Wno-address-of-packed-member")
endif()

# CPU feature detection - simplified with ifunc runtime dispatch
include(CheckCCompilerFlag)

# Detect x86 architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i386|i686")
    add_compile_definitions(LPM_X86_ARCH=1)
    set(LPM_X86_ARCH TRUE)
else()
    set(LPM_X86_ARCH FALSE)
endif()

# With ifunc, we compile all SIMD variants and resolve at program load time
# No need for compile-time feature detection macros
if(ENABLE_NATIVE_ARCH AND LPM_X86_ARCH)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
    message(STATUS "Using native architecture compilation with ifunc dispatch")
else()
    if(LPM_X86_ARCH)
        # Compile with baseline SSE2 as minimum - ensures compatibility with most x86_64 machines
        # AVX/AVX2/AVX512 variants are compiled separately via target attributes and dispatched at runtime via ifunc
        # Explicitly disable AVX in baseline to ensure only SSE2 is used (AVX variants use target attributes)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mno-avx -mno-avx2 -mno-avx512f -mtune=generic")
        message(STATUS "Using baseline SSE2 (minimum) with ifunc runtime dispatch for AVX/AVX2/AVX512")
        message(STATUS "  This ensures compatibility with a wide range of x86_64 machines")
    else()
        message(STATUS "Non-x86 architecture detected, using scalar implementations")
    endif()
endif()

# ============================================================================
# Source files - organized by algorithm
# ============================================================================

set(LPM_SOURCES
    # Core shared functionality
    src/core.c
    src/api.c
    
    # IPv4 8-bit stride algorithm
    src/4stride8/core.c
    src/4stride8/single.c
    src/4stride8/batch.c
    
    # IPv6 8-bit stride algorithm
    src/6stride8/core.c
    src/6stride8/single.c
    src/6stride8/batch.c
    
    # IPv4 DIR-24-8 algorithm
    src/dir24/core.c
    src/dir24/single.c
    src/dir24/batch.c
    
    # IPv6 Wide 16-bit stride algorithm
    src/wide16/core.c
    src/wide16/single.c
    src/wide16/batch.c
)

# Create shared library
add_library(lpm SHARED ${LPM_SOURCES})
target_include_directories(lpm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/lpm>
)

# Add libdynemit include directory
target_include_directories(lpm PRIVATE ${CMAKE_SOURCE_DIR}/external/libdynemit/include)

# Link required libraries
target_link_libraries(lpm PUBLIC m)

# Link libdynemit_core for SIMD detection and ifunc dispatch
target_link_libraries(lpm PRIVATE dynemit_core)

# Set library properties
set_target_properties(lpm PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Create static library - include dynemit_core objects to make it self-contained
add_library(lpm_static STATIC ${LPM_SOURCES} $<TARGET_OBJECTS:dynemit_core_obj>)
target_include_directories(lpm_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/lpm>
)

# Add libdynemit include directory
target_include_directories(lpm_static PRIVATE ${CMAKE_SOURCE_DIR}/external/libdynemit/include)

# Link required libraries
target_link_libraries(lpm_static PUBLIC m)

# Set library properties - use same output name "lpm" for both
set_target_properties(lpm_static PROPERTIES
    OUTPUT_NAME lpm
)

# Enable Link Time Optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
    
    if(LTO_SUPPORTED)
        set_property(TARGET lpm PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        set_property(TARGET lpm_static PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO enabled for liblpm")
    else()
        message(WARNING "LTO not supported: ${LTO_ERROR}")
    endif()
endif()

# Installation
include(GNUInstallDirs)

# Install shared library (runtime component)
install(TARGETS lpm
    EXPORT liblpmTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT runtime
)

# Install static library (devel component)
install(TARGETS lpm_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT devel
)

# Install headers to /usr/include/lpm/ (devel component)
install(FILES
    include/lpm.h
    include/internal.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lpm
    COMPONENT devel
)

# Install algo headers preserving subdirectory structure
install(DIRECTORY include/algo
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lpm
    COMPONENT devel
    FILES_MATCHING PATTERN "*.h"
)

# Generate and install pkg-config file (devel component)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/liblpm.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/liblpm.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/liblpm.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT devel
)

# Install man pages (devel component)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/man/man3
    DESTINATION ${CMAKE_INSTALL_MANDIR}
    COMPONENT devel
    FILES_MATCHING PATTERN "*.3"
)

# Install localized man pages (if available)
set(MAN_LANGUAGES pt_BR de es fr zh_CN)
foreach(LANG ${MAN_LANGUAGES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/man/${LANG}/man3")
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/man/${LANG}/man3
            DESTINATION ${CMAKE_INSTALL_MANDIR}/${LANG}
            COMPONENT devel
            FILES_MATCHING PATTERN "*.3"
        )
        message(STATUS "Found localized man pages: ${LANG}")
    endif()
endforeach()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# C++ wrapper
if(BUILD_CPP_WRAPPER)
    add_subdirectory(bindings/cpp)
endif()

# Python wrapper
if(BUILD_PYTHON_WRAPPER)
    find_package(Python COMPONENTS Interpreter Development.Module)
    find_program(CYTHON_EXECUTABLE NAMES cython cython3)
    if(Python_FOUND AND CYTHON_EXECUTABLE)
        message(STATUS "Found Python: ${Python_EXECUTABLE} (${Python_VERSION})")
        message(STATUS "Found Cython: ${CYTHON_EXECUTABLE}")
        add_subdirectory(bindings/python)
    else()
        if(NOT Python_FOUND)
            message(WARNING "Python not found, skipping Python bindings")
        endif()
        if(NOT CYTHON_EXECUTABLE)
            message(WARNING "Cython not found, skipping Python bindings. Install with: pip install cython")
        endif()
    endif()
endif()

# Package configuration for find_package(liblpm)
include(CMakePackageConfigHelpers)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/liblpmConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Generate config file from template
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/liblpmConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/liblpmConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liblpm
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

# Install CMake config files (devel component)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/liblpmConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/liblpmConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liblpm
    COMPONENT devel
)

# Export targets (devel component)
install(EXPORT liblpmTargets
    FILE liblpmTargets.cmake
    NAMESPACE liblpm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liblpm
    COMPONENT devel
)

# Go wrapper
if(BUILD_GO_WRAPPER)
    find_program(GO_EXECUTABLE go)
    if(GO_EXECUTABLE)
        message(STATUS "Found Go: ${GO_EXECUTABLE}")
        
        # Custom target to build Go wrapper
        add_custom_target(go_wrapper ALL
            COMMAND ${CMAKE_COMMAND} -E echo "Building Go wrapper..."
            COMMAND ${GO_EXECUTABLE} build ./...
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/go
            DEPENDS lpm
            COMMENT "Building Go wrapper and bindings"
        )
        
        # Custom target to test Go wrapper
        add_custom_target(go_test
            COMMAND ${GO_EXECUTABLE} test -v ./liblpm/
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/go
            DEPENDS go_wrapper
            COMMENT "Testing Go wrapper"
        )
        
        # Custom target to run Go benchmarks
        add_custom_target(go_bench
            COMMAND ${GO_EXECUTABLE} test -bench=. -benchmem ./benchmarks/
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/go
            DEPENDS go_wrapper
            COMMENT "Running Go benchmarks"
        )
        
        message(STATUS "Go wrapper targets added:")
        message(STATUS "  make go_wrapper - Build Go wrapper")
        message(STATUS "  make go_test    - Run Go tests")
        message(STATUS "  make go_bench   - Run Go benchmarks")
    else()
        message(WARNING "Go not found. Go wrapper will not be built.")
        message(WARNING "Install Go to build the wrapper: sudo apt install golang-go")
    endif()
endif()

# Perl wrapper
if(BUILD_PERL_WRAPPER)
    find_program(PERL_EXECUTABLE perl)
    if(PERL_EXECUTABLE)
        message(STATUS "Found Perl: ${PERL_EXECUTABLE}")
        
        # Custom target to configure Perl wrapper
        add_custom_target(perl_configure
            COMMAND ${PERL_EXECUTABLE} Makefile.PL
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/perl
            DEPENDS lpm
            COMMENT "Configuring Perl wrapper"
        )
        
        # Custom target to build Perl wrapper
        add_custom_target(perl_wrapper ALL
            COMMAND make
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/perl
            DEPENDS perl_configure
            COMMENT "Building Perl wrapper and bindings"
        )
        
        # Custom target to test Perl wrapper
        add_custom_target(perl_test
            COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}" make test
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/perl
            DEPENDS perl_wrapper
            COMMENT "Testing Perl wrapper"
        )
        
        message(STATUS "Perl wrapper targets added:")
        message(STATUS "  make perl_wrapper - Build Perl wrapper")
        message(STATUS "  make perl_test    - Run Perl tests")
    else()
        message(WARNING "Perl not found. Perl wrapper will not be built.")
        message(WARNING "Install Perl to build the wrapper: sudo apt install perl")
    endif()
endif()

# PHP wrapper
if(BUILD_PHP_WRAPPER)
    find_program(PHP_EXECUTABLE php)
    find_program(PHPIZE_EXECUTABLE phpize)
    if(PHP_EXECUTABLE AND PHPIZE_EXECUTABLE)
        message(STATUS "Found PHP: ${PHP_EXECUTABLE}")
        message(STATUS "Found phpize: ${PHPIZE_EXECUTABLE}")
        
        # Custom target to configure PHP extension
        add_custom_target(php_configure
            COMMAND ${PHPIZE_EXECUTABLE}
            COMMAND ./configure --with-liblpm=${CMAKE_INSTALL_PREFIX}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/php
            DEPENDS lpm
            COMMENT "Configuring PHP extension"
        )
        
        # Custom target to build PHP extension
        add_custom_target(php_wrapper ALL
            COMMAND make
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/php
            DEPENDS php_configure
            COMMENT "Building PHP extension and bindings"
        )
        
        # Custom target to test PHP extension
        add_custom_target(php_test
            COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}" make test
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/php
            DEPENDS php_wrapper
            COMMENT "Testing PHP extension"
        )
        
        message(STATUS "PHP wrapper targets added:")
        message(STATUS "  make php_wrapper - Build PHP extension")
        message(STATUS "  make php_test    - Run PHP tests")
    else()
        if(NOT PHP_EXECUTABLE)
            message(WARNING "PHP not found. PHP extension will not be built.")
            message(WARNING "Install PHP to build the extension: sudo apt install php php-dev")
        endif()
        if(NOT PHPIZE_EXECUTABLE)
            message(WARNING "phpize not found. PHP extension will not be built.")
            message(WARNING "Install PHP development tools: sudo apt install php-dev")
        endif()
    endif()
endif()

# Print configuration summary
message(STATUS "liblpm configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C flags: ${CMAKE_C_FLAGS}")
message(STATUS "  Default IPv4 algorithm: ${LPM_IPV4_DEFAULT}")
message(STATUS "  Default IPv6 algorithm: ${LPM_IPV6_DEFAULT}")
if(ENABLE_NATIVE_ARCH)
    message(STATUS "  CPU optimizations: native architecture")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND LTO_SUPPORTED)
    message(STATUS "  Link Time Optimization: ON")
else()
    message(STATUS "  Link Time Optimization: OFF")
endif()
message(STATUS "  Build shared library: ON")
message(STATUS "  Build static library: ON")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "  ifunc dispatch: ON (via libdynemit)")
if(WITH_DPDK_BENCHMARK)
    message(STATUS "  DPDK benchmark: ${HAVE_DPDK}")
endif()
if(WITH_EXTERNAL_LPM_BENCHMARK)
    message(STATUS "  External LPM benchmark: ${HAVE_EXTERNAL_LPM}")
    if(HAVE_EXTERNAL_LPM)
        if(HAVE_RMIND_LPM)
            message(STATUS "    - rmind/liblpm")
        endif()
        if(HAVE_LIBPATRICIA)
            message(STATUS "    - libpatricia")
        endif()
    endif()
endif()
if(BUILD_GO_WRAPPER AND GO_EXECUTABLE)
    message(STATUS "  Build Go wrapper: ON")
else()
    message(STATUS "  Build Go wrapper: OFF")
endif()
if(BUILD_CPP_WRAPPER)
    message(STATUS "  Build C++ wrapper: ON")
else()
    message(STATUS "  Build C++ wrapper: OFF")
endif()
if(BUILD_PERL_WRAPPER AND PERL_EXECUTABLE)
    message(STATUS "  Build Perl wrapper: ON")
else()
    message(STATUS "  Build Perl wrapper: OFF")
endif()
if(BUILD_PHP_WRAPPER AND PHP_EXECUTABLE AND PHPIZE_EXECUTABLE)
    message(STATUS "  Build PHP wrapper: ON")
else()
    message(STATUS "  Build PHP wrapper: OFF")
endif()
if(BUILD_PYTHON_WRAPPER AND Python_FOUND AND CYTHON_EXECUTABLE)
    message(STATUS "  Build Python wrapper: ON")
else()
    message(STATUS "  Build Python wrapper: OFF")
endif()

# ============================================================================
# CPack Configuration for .deb and .rpm packages
# ============================================================================

# Define install components
set(CPACK_COMPONENTS_ALL runtime devel)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime Library")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "liblpm shared library for runtime use")
set(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVEL_DESCRIPTION "Headers, static library, and development files for liblpm")
set(CPACK_COMPONENT_DEVEL_DEPENDS runtime)

# Common package metadata
set(CPACK_PACKAGE_NAME "liblpm")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "liblpm")
set(CPACK_PACKAGE_CONTACT "Murilo Chianfa <murilo.chianfa@outlook.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance Longest Prefix Match library for IP routing")
set(CPACK_PACKAGE_DESCRIPTION "liblpm is a high-performance Longest Prefix Match (LPM) library
optimized for IP routing table lookups. It supports both IPv4 and IPv6
with multiple algorithm implementations including DIR-24-8 for IPv4
and wide 16-bit stride for IPv6. The library uses SIMD optimizations
with runtime CPU feature detection via GNU ifunc.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/MuriloChianfa/liblpm")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# ============================================================================
# DEB Package Configuration
# ============================================================================

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Murilo Chianfa <murilo.chianfa@outlook.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# Enable automatic shared library dependency detection
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Use Debian multiarch library directory
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

# Runtime package: liblpm
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "liblpm")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_RUNTIME_FILE_NAME "liblpm_${PROJECT_VERSION}_amd64.deb")

# Development package: liblpm-dev
set(CPACK_DEBIAN_DEVEL_PACKAGE_NAME "liblpm-dev")
set(CPACK_DEBIAN_DEVEL_PACKAGE_SECTION "libdevel")
set(CPACK_DEBIAN_DEVEL_PACKAGE_DEPENDS "liblpm (= ${PROJECT_VERSION})")
set(CPACK_DEBIAN_DEVEL_FILE_NAME "liblpm-dev_${PROJECT_VERSION}_amd64.deb")

# ============================================================================
# RPM Package Configuration
# ============================================================================

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_PACKAGE_LICENSE "BSD-2-Clause")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
set(CPACK_RPM_PACKAGE_URL "https://github.com/MuriloChianfa/liblpm")

# Runtime package: liblpm
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "liblpm")
set(CPACK_RPM_RUNTIME_PACKAGE_SUMMARY "High-performance LPM library - runtime")
set(CPACK_RPM_RUNTIME_FILE_NAME "liblpm-${PROJECT_VERSION}.x86_64.rpm")

# Development package: liblpm-devel
set(CPACK_RPM_DEVEL_PACKAGE_NAME "liblpm-devel")
set(CPACK_RPM_DEVEL_PACKAGE_SUMMARY "High-performance LPM library - development files")
set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES "liblpm = ${PROJECT_VERSION}")
set(CPACK_RPM_DEVEL_FILE_NAME "liblpm-devel-${PROJECT_VERSION}.x86_64.rpm")

# RPM automatic dependency detection
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_AUTOPROV ON)

# Include CPack module (must be last)
include(CPack)
