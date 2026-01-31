cmake_minimum_required(VERSION 3.16)

# C# wrapper for liblpm
project(liblpm_csharp VERSION 2.0.0)

# Find .NET SDK
find_program(DOTNET_EXECUTABLE dotnet)

if(NOT DOTNET_EXECUTABLE)
    message(FATAL_ERROR "dotnet not found. Install .NET SDK to build C# wrapper.")
endif()

# Get .NET version
execute_process(
    COMMAND ${DOTNET_EXECUTABLE} --version
    OUTPUT_VARIABLE DOTNET_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found .NET SDK: ${DOTNET_VERSION}")

# Find or import lpm library
if(NOT TARGET lpm)
    set(LPM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    if(EXISTS "${LPM_ROOT_DIR}/include/lpm.h")
        message(STATUS "Using liblpm from parent project")
    else()
        find_library(LPM_LIBRARY NAMES lpm)
        if(LPM_LIBRARY)
            message(STATUS "Found system liblpm: ${LPM_LIBRARY}")
        else()
            message(FATAL_ERROR "liblpm not found. Build the main project first or install liblpm.")
        endif()
    endif()
endif()

# Configuration
set(CSHARP_BUILD_CONFIG "$<IF:$<CONFIG:Debug>,Debug,Release>")

# Build C# wrapper
add_custom_target(csharp_wrapper ALL
    COMMAND ${DOTNET_EXECUTABLE} build --configuration ${CSHARP_BUILD_CONFIG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS lpm
    COMMENT "Building C# wrapper"
    VERBATIM
)

# Test C# wrapper
add_custom_target(csharp_test
    COMMAND ${DOTNET_EXECUTABLE} test --configuration ${CSHARP_BUILD_CONFIG} --no-build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS csharp_wrapper
    COMMENT "Testing C# wrapper"
    VERBATIM
)

# Pack NuGet package
add_custom_target(csharp_pack
    COMMAND ${DOTNET_EXECUTABLE} pack --configuration Release -o ${CMAKE_CURRENT_BINARY_DIR}/nupkg
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS csharp_wrapper
    COMMENT "Creating NuGet package"
    VERBATIM
)

# Run examples
add_custom_target(csharp_example
    COMMAND ${DOTNET_EXECUTABLE} run --configuration ${CSHARP_BUILD_CONFIG} --project LibLpm.Examples
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS csharp_wrapper
    COMMENT "Running C# examples"
    VERBATIM
)

# Copy native library to runtimes directory for local development
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(NATIVE_LIB_DIR "runtimes/linux-x64/native")
    set(NATIVE_LIB_NAME "liblpm.so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(NATIVE_LIB_DIR "runtimes/win-x64/native")
    set(NATIVE_LIB_NAME "lpm.dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(NATIVE_LIB_DIR "runtimes/osx-x64/native")
    set(NATIVE_LIB_NAME "liblpm.dylib")
endif()

# Only copy if building as part of parent project (target exists)
if(TARGET lpm)
    add_custom_command(TARGET csharp_wrapper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/${NATIVE_LIB_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:lpm>
            ${CMAKE_CURRENT_SOURCE_DIR}/${NATIVE_LIB_DIR}/${NATIVE_LIB_NAME}
        COMMENT "Copying native library to runtimes directory"
        VERBATIM
    )
else()
    # When building standalone, try to copy from system installation
    add_custom_command(TARGET csharp_wrapper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/${NATIVE_LIB_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${LPM_LIBRARY}
            ${CMAKE_CURRENT_SOURCE_DIR}/${NATIVE_LIB_DIR}/${NATIVE_LIB_NAME}
        COMMENT "Copying native library from system installation to runtimes directory"
        VERBATIM
    )
endif()

message(STATUS "C# wrapper configuration:")
message(STATUS "  .NET SDK: ${DOTNET_VERSION}")
message(STATUS "  Build targets:")
message(STATUS "    make csharp_wrapper - Build C# wrapper")
message(STATUS "    make csharp_test    - Run C# tests")
message(STATUS "    make csharp_pack    - Create NuGet package")
message(STATUS "    make csharp_example - Run examples")
