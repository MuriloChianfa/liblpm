# Benchmark programs
add_executable(bench_lookup bench_lookup.c)
target_link_libraries(bench_lookup lpm)

add_executable(bench_lookup_all bench_lookup_all.c)
target_link_libraries(bench_lookup_all lpm)

# Link with pthread for clock_gettime on some systems
find_package(Threads REQUIRED)
target_link_libraries(bench_lookup Threads::Threads)
target_link_libraries(bench_lookup_all Threads::Threads)

# Add benchmarks as tests
add_test(NAME benchmark_lookup COMMAND bench_lookup)
add_test(NAME benchmark_lookup_all COMMAND bench_lookup_all)

# Set test properties for benchmarks
set_tests_properties(benchmark_lookup PROPERTIES
    TIMEOUT 300
    LABELS "benchmark"
)

set_tests_properties(benchmark_lookup_all PROPERTIES
    TIMEOUT 300
    LABELS "benchmark"
)
