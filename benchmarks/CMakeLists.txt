# Benchmark programs
add_executable(bench_lookup bench_lookup.c)
target_link_libraries(bench_lookup lpm)

# Link with pthread for clock_gettime on some systems
find_package(Threads REQUIRED)
target_link_libraries(bench_lookup Threads::Threads)

# Enable LTO for benchmarks in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND LTO_SUPPORTED)
    set_property(TARGET bench_lookup PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Add benchmarks as tests
add_test(NAME benchmark_lookup COMMAND bench_lookup)

# Set test properties for benchmarks
set_tests_properties(benchmark_lookup PROPERTIES
    TIMEOUT 300
    LABELS "benchmark"
)

# Algorithm Scaling Benchmark (statically linked for portability)
message(STATUS "Building algorithm scaling benchmark (static)")

add_executable(bench_algorithm_scaling bench_algorithm_scaling.c)

# Link statically against lpm_static for portability across machines
target_link_libraries(bench_algorithm_scaling lpm_static)

# Link with pthread and math (must come after lpm_static)
target_link_libraries(bench_algorithm_scaling Threads::Threads m)

# Enable external LPM library support (for Docker builds)
if(HAVE_EXTERNAL_LPM)
    message(STATUS "  External LPM libraries: enabled (static)")
    target_compile_definitions(bench_algorithm_scaling PRIVATE HAVE_EXTERNAL_LPM)
    target_include_directories(bench_algorithm_scaling PRIVATE ${EXTERNAL_LPM_DIR}/include ${EXTERNAL_LPM_DIR})
    
    if(HAVE_RMIND_LPM)
            target_compile_definitions(bench_algorithm_scaling PRIVATE HAVE_RMIND_LPM)
            target_link_libraries(bench_algorithm_scaling ${EXTERNAL_LPM_DIR}/librmindlpm.a)
        endif()
        if(HAVE_LIBPATRICIA)
            target_compile_definitions(bench_algorithm_scaling PRIVATE HAVE_LIBPATRICIA)
            target_link_libraries(bench_algorithm_scaling ${EXTERNAL_LPM_DIR}/libpatricia.a)
        endif()
    endif()

# Full static linking - creates a completely portable binary
# This removes all dynamic library dependencies including glibc
target_link_options(bench_algorithm_scaling PRIVATE 
    -static
    -pthread
)

# Ensure we're using static versions of system libraries
set_target_properties(bench_algorithm_scaling PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

# Enable LTO for benchmark in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND LTO_SUPPORTED)
    set_property(TARGET bench_algorithm_scaling PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Add as a test
add_test(NAME benchmark_algorithm_scaling COMMAND bench_algorithm_scaling --quiet)
set_tests_properties(benchmark_algorithm_scaling PROPERTIES
    TIMEOUT 600
    LABELS "benchmark"
)

# Algorithm Scaling Benchmark with DPDK support (dynamically linked)
if(HAVE_DPDK)
    message(STATUS "Building algorithm scaling benchmark with DPDK support")
    
    add_executable(bench_algorithm_scaling_dpdk bench_algorithm_scaling.c)
    
    # Link with liblpm (dynamic) - required for DPDK compatibility
    target_link_libraries(bench_algorithm_scaling_dpdk lpm)
    
    # Link with pthread and math
    target_link_libraries(bench_algorithm_scaling_dpdk Threads::Threads m)
    
    # Enable DPDK support
    target_compile_definitions(bench_algorithm_scaling_dpdk PRIVATE HAVE_DPDK)
    target_include_directories(bench_algorithm_scaling_dpdk PRIVATE ${DPDK_INCLUDE_DIRS})
    
    # Use --as-needed to only link libraries that are actually used
    target_link_options(bench_algorithm_scaling_dpdk PRIVATE "-Wl,--as-needed")
    target_link_libraries(bench_algorithm_scaling_dpdk ${DPDK_LINK_LIBRARIES})
    target_compile_options(bench_algorithm_scaling_dpdk PRIVATE ${DPDK_CFLAGS_OTHER})
    
    # Enable external LPM library support
    if(HAVE_EXTERNAL_LPM)
        message(STATUS "  External LPM libraries: enabled")
        target_compile_definitions(bench_algorithm_scaling_dpdk PRIVATE HAVE_EXTERNAL_LPM)
        target_include_directories(bench_algorithm_scaling_dpdk PRIVATE ${EXTERNAL_LPM_DIR}/include ${EXTERNAL_LPM_DIR})
        
        if(HAVE_RMIND_LPM)
            target_compile_definitions(bench_algorithm_scaling_dpdk PRIVATE HAVE_RMIND_LPM)
            target_link_libraries(bench_algorithm_scaling_dpdk ${EXTERNAL_LPM_DIR}/librmindlpm.a)
        endif()
        if(HAVE_LIBPATRICIA)
            target_compile_definitions(bench_algorithm_scaling_dpdk PRIVATE HAVE_LIBPATRICIA)
            target_link_libraries(bench_algorithm_scaling_dpdk ${EXTERNAL_LPM_DIR}/libpatricia.a)
        endif()
    endif()
    
    # Enable LTO for benchmark in Release mode
    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND LTO_SUPPORTED)
        set_property(TARGET bench_algorithm_scaling_dpdk PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    
    # Add as a test
    add_test(NAME benchmark_algorithm_scaling_dpdk COMMAND bench_algorithm_scaling_dpdk --quiet)
    set_tests_properties(benchmark_algorithm_scaling_dpdk PROPERTIES
        TIMEOUT 900
        LABELS "benchmark;dpdk"
    )
else()
    message(STATUS "DPDK not available - bench_algorithm_scaling_dpdk will not be built")
endif()

# LPM Comparison Benchmark (always built, DPDK support optional)
message(STATUS "Building LPM comparison benchmark")

add_executable(bench_comparison bench_comparison.c)

# Link with liblpm
target_link_libraries(bench_comparison lpm)

# Link with pthread
target_link_libraries(bench_comparison Threads::Threads)

# Conditionally enable DPDK support
if(HAVE_DPDK)
    message(STATUS "  DPDK support: enabled")
    target_compile_definitions(bench_comparison PRIVATE HAVE_DPDK)
    target_include_directories(bench_comparison PRIVATE ${DPDK_INCLUDE_DIRS})
    # Use --as-needed to only link libraries that are actually used
    target_link_options(bench_comparison PRIVATE "-Wl,--as-needed")
    target_link_libraries(bench_comparison ${DPDK_LINK_LIBRARIES})
    target_compile_options(bench_comparison PRIVATE ${DPDK_CFLAGS_OTHER})
else()
    message(STATUS "  DPDK support: disabled (comparing liblpm algorithms only)")
endif()

# Enable LTO for benchmark in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND LTO_SUPPORTED)
    set_property(TARGET bench_comparison PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Add as a test
add_test(NAME benchmark_comparison COMMAND bench_comparison)
set_tests_properties(benchmark_comparison PROPERTIES
    TIMEOUT 600
    LABELS "benchmark"
)
