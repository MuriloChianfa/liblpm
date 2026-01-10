# Test programs
add_executable(test_basic test_basic.c)
target_link_libraries(test_basic lpm)

add_executable(test_fuzz test_fuzz.c)
target_link_libraries(test_fuzz lpm)

add_executable(test_fuzz_advanced test_fuzz_advanced.c)
target_link_libraries(test_fuzz_advanced lpm)

# Add test
add_test(NAME basic_tests COMMAND test_basic)
add_test(NAME fuzz_tests COMMAND test_fuzz)
add_test(NAME fuzz_advanced_tests COMMAND test_fuzz_advanced)

# Set test properties
set_tests_properties(basic_tests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

set_tests_properties(fuzz_tests PROPERTIES
    TIMEOUT 60
    LABELS "fuzz"
)

set_tests_properties(fuzz_advanced_tests PROPERTIES
    TIMEOUT 120
    LABELS "fuzz"
)

# Add cppcheck test
add_test(NAME cppcheck_tests COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_cppcheck.sh)
set_tests_properties(cppcheck_tests PROPERTIES
    TIMEOUT 300
    LABELS "static_analysis"
)
