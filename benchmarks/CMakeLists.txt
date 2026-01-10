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
    target_link_libraries(bench_comparison ${DPDK_LIBRARIES})
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
