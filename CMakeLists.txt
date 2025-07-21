cmake_minimum_required(VERSION 3.16)

# Set Clang as default compiler
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)
endif()

project(liblpm VERSION 1.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_BENCHMARKS "Build benchmark programs" ON)
option(ENABLE_NATIVE_ARCH "Enable native architecture optimizations" ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -O3")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Warning suppressions for test code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare -Wno-address-of-packed-member")
endif()

# CPU feature detection
include(CheckCCompilerFlag)

# Detect x86 architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i386|i686")
    add_compile_definitions(LPM_X86_ARCH=1)
    set(LPM_X86_ARCH TRUE)
else()
    set(LPM_X86_ARCH FALSE)
endif()

# Function to check and add CPU feature flags
function(check_cpu_feature flag define)
    check_c_compiler_flag(${flag} COMPILER_SUPPORTS_${define})
    if(COMPILER_SUPPORTS_${define})
        # Only add the flag if the compiler supports it
        set(CPU_FEATURE_FLAGS "${CPU_FEATURE_FLAGS} ${flag}" PARENT_SCOPE)
        add_compile_definitions(${define}=1)
    endif()
endfunction()

# Function to check if CPU supports a feature at runtime
function(check_cpu_supports_feature feature_name)
    # This is a simplified check - in practice you'd want more sophisticated detection
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        # For x86_64, we'll be conservative and only enable basic features
        # Advanced features should be detected at runtime
        set(CPU_SUPPORTS_${feature_name} TRUE PARENT_SCOPE)
    else()
        set(CPU_SUPPORTS_${feature_name} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Runtime CPU feature detection approach
# We'll compile all SIMD code but use runtime detection to select the best implementation
if(ENABLE_NATIVE_ARCH AND LPM_X86_ARCH)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
elseif(LPM_X86_ARCH)
    # Enable SIMD features that are safe for the target CPU
    check_cpu_feature("-msse" "LPM_HAVE_SSE")
    check_cpu_feature("-msse2" "LPM_HAVE_SSE2")
    check_cpu_feature("-msse3" "LPM_HAVE_SSE3")
    check_cpu_feature("-mssse3" "LPM_HAVE_SSSE3")
    check_cpu_feature("-msse4.1" "LPM_HAVE_SSE4_1")
    check_cpu_feature("-msse4.2" "LPM_HAVE_SSE4_2")
    check_cpu_feature("-mavx" "LPM_HAVE_AVX")

    # Disable AVX2 and AVX512 for compatibility with older CPUs
    # These will be detected at runtime but not compiled in

    # check_cpu_feature("-mavx2" "LPM_HAVE_AVX2")
    # check_cpu_feature("-mavx512f" "LPM_HAVE_AVX512F")
    # check_cpu_feature("-mavx512vl" "LPM_HAVE_AVX512VL")
    # check_cpu_feature("-mavx512dq" "LPM_HAVE_AVX512DQ")
    # check_cpu_feature("-mavx512bw" "LPM_HAVE_AVX512BW")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FEATURE_FLAGS}")
else()
    # Non-x86 architecture - disable all x86-specific optimizations
    message(STATUS "Non-x86 architecture detected, disabling x86-specific optimizations")
endif()

# Source files
set(LPM_SOURCES
    src/lpm_core.c
    src/lpm_lookup_single.c
    src/lpm_lookup_batch.c
    src/lpm_lookup_all.c
)

# Create library
add_library(lpm ${LPM_SOURCES})
target_include_directories(lpm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link required libraries
target_link_libraries(lpm PUBLIC m)

# Set library properties
set_target_properties(lpm PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/lpm.h
)

# Installation
include(GNUInstallDirs)
install(TARGETS lpm
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

# Print configuration summary
message(STATUS "liblpm configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C flags: ${CMAKE_C_FLAGS}")
if(ENABLE_NATIVE_ARCH)
    message(STATUS "  CPU optimizations: native architecture")
else()
    message(STATUS "  CPU features: ${CPU_FEATURE_FLAGS}")
endif()
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build benchmarks: ${BUILD_BENCHMARKS}")
