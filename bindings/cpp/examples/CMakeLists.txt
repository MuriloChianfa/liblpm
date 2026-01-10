# C++ Examples

# Basic example demonstrating the API
add_executable(cpp_basic_example basic_example.cpp)
target_link_libraries(cpp_basic_example PRIVATE lpm_cpp lpm_cpp_impl)
target_compile_options(cpp_basic_example PRIVATE 
    -Wall -Wextra -O3
)

# Set output directory
set_target_properties(cpp_basic_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bindings/cpp/examples
)

# Add custom target for convenience
add_custom_target(cpp_example
    COMMAND cpp_basic_example
    DEPENDS cpp_basic_example
    COMMENT "Running C++ basic example"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bindings/cpp/examples
)

message(STATUS "C++ examples configured:")
message(STATUS "  Basic example: cpp_basic_example")
message(STATUS "  Run with: make cpp_example")

