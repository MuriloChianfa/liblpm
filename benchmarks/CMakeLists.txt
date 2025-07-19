# Benchmark programs
add_executable(bench_lookup bench_lookup.c)
target_link_libraries(bench_lookup lpm)

add_executable(bench_lookup_all bench_lookup_all.c)
target_link_libraries(bench_lookup_all lpm)

# Link with pthread for clock_gettime on some systems
find_package(Threads REQUIRED)
target_link_libraries(bench_lookup Threads::Threads)
target_link_libraries(bench_lookup_all Threads::Threads) 