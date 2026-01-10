# C++ Tests

# Basic unit tests
add_executable(cpp_test_lpm test_lpm.cpp)
target_link_libraries(cpp_test_lpm PRIVATE lpm_cpp lpm_cpp_impl)
target_compile_options(cpp_test_lpm PRIVATE 
    -Wall -Wextra -O3
)

# Set output directory
set_target_properties(cpp_test_lpm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bindings/cpp/tests
)

# Add test to CTest
add_test(NAME cpp_wrapper_test 
         COMMAND cpp_test_lpm
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bindings/cpp/tests)

# Add custom target for convenience
add_custom_target(cpp_test
    COMMAND cpp_test_lpm
    DEPENDS cpp_test_lpm
    COMMENT "Running C++ wrapper tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bindings/cpp/tests
)

message(STATUS "C++ tests configured:")
message(STATUS "  Test executable: cpp_test_lpm")
message(STATUS "  Run with: make cpp_test or ctest")

