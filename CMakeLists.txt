cmake_minimum_required(VERSION 3.16)
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

# CPU feature detection
include(CheckCCompilerFlag)

# Function to check and add CPU feature flags
function(check_cpu_feature flag define)
    check_c_compiler_flag(${flag} COMPILER_SUPPORTS_${define})
    if(COMPILER_SUPPORTS_${define})
        set(CPU_FEATURE_FLAGS "${CPU_FEATURE_FLAGS} ${flag}" PARENT_SCOPE)
        add_compile_definitions(${define}=1)
    endif()
endfunction()

# Check for various CPU features
if(ENABLE_NATIVE_ARCH)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
else()
    # Check individual features
    check_cpu_feature("-msse" "LPM_HAVE_SSE")
    check_cpu_feature("-msse2" "LPM_HAVE_SSE2")
    check_cpu_feature("-msse3" "LPM_HAVE_SSE3")
    check_cpu_feature("-mssse3" "LPM_HAVE_SSSE3")
    check_cpu_feature("-msse4.1" "LPM_HAVE_SSE4_1")
    check_cpu_feature("-msse4.2" "LPM_HAVE_SSE4_2")
    check_cpu_feature("-mavx" "LPM_HAVE_AVX")
    check_cpu_feature("-mavx2" "LPM_HAVE_AVX2")
    check_cpu_feature("-mavx512f" "LPM_HAVE_AVX512F")
    check_cpu_feature("-mavx512vl" "LPM_HAVE_AVX512VL")
    check_cpu_feature("-mavx512dq" "LPM_HAVE_AVX512DQ")
    check_cpu_feature("-mavx512bw" "LPM_HAVE_AVX512BW")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FEATURE_FLAGS}")
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