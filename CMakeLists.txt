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

# Options
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_BENCHMARKS "Build benchmark programs" ON)
option(ENABLE_NATIVE_ARCH "Enable native architecture optimizations" OFF)
option(WITH_DPDK_BENCHMARK "Build DPDK comparison benchmark (requires DPDK)" OFF)
option(BUILD_GO_WRAPPER "Build Go wrapper and bindings" OFF)
option(BUILD_CPP_WRAPPER "Build C++ wrapper and bindings" OFF)

# Find DPDK if requested
if(WITH_DPDK_BENCHMARK)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(DPDK QUIET libdpdk)
        if(DPDK_FOUND)
            message(STATUS "Found DPDK: ${DPDK_VERSION}")
            set(HAVE_DPDK TRUE)
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

# Source files - core functionality
set(LPM_SOURCES
    src/lpm_core.c
    src/lpm_ipv6_wide.c
    src/lpm_ipv4_dir24.c
)

list(APPEND LPM_SOURCES src/lpm_lookup_single.c)
list(APPEND LPM_SOURCES src/lpm_lookup_batch.c)

# Create shared library
add_library(lpm SHARED ${LPM_SOURCES})
target_include_directories(lpm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
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
    PUBLIC_HEADER include/lpm.h
)

# Create static library - include dynemit_core objects to make it self-contained
add_library(lpm_static STATIC ${LPM_SOURCES} $<TARGET_OBJECTS:dynemit_core_obj>)
target_include_directories(lpm_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add libdynemit include directory
target_include_directories(lpm_static PRIVATE ${CMAKE_SOURCE_DIR}/external/libdynemit/include)

# Link required libraries
target_link_libraries(lpm_static PUBLIC m)

# Set library properties - use same output name "lpm" for both
set_target_properties(lpm_static PROPERTIES
    OUTPUT_NAME lpm
    PUBLIC_HEADER include/lpm.h
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
install(TARGETS lpm lpm_static
    EXPORT liblpmTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

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

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/liblpmConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/liblpmConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liblpm
)

# Export targets
install(EXPORT liblpmTargets
    FILE liblpmTargets.cmake
    NAMESPACE liblpm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/liblpm
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

# Print configuration summary
message(STATUS "liblpm configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C flags: ${CMAKE_C_FLAGS}")
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
