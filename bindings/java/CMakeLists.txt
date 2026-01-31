# liblpm Java JNI Bindings
# CMake configuration for building the JNI native library

cmake_minimum_required(VERSION 3.16)
project(liblpm_java VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include GNUInstallDirs for CMAKE_INSTALL_* variables
include(GNUInstallDirs)

# Find JNI
find_package(JNI REQUIRED)
if(NOT JNI_FOUND)
    message(FATAL_ERROR "JNI not found. Please install a JDK.")
endif()

message(STATUS "JNI include directories: ${JNI_INCLUDE_DIRS}")

# Find or import lpm library
if(NOT TARGET lpm)
    # Standalone build - find system lpm or use parent project
    set(LPM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    
    if(EXISTS "${LPM_ROOT_DIR}/include/lpm.h")
        # Build from parent project - find the built library
        find_library(LPM_LIBRARY 
            NAMES lpm
            HINTS "${LPM_ROOT_DIR}/build"
            NO_DEFAULT_PATH
        )
        
        if(NOT LPM_LIBRARY)
            # Try system-installed version
            find_library(LPM_LIBRARY NAMES lpm)
        endif()
        
        if(LPM_LIBRARY)
            message(STATUS "Found liblpm: ${LPM_LIBRARY}")
            add_library(lpm SHARED IMPORTED)
            set_target_properties(lpm PROPERTIES
                IMPORTED_LOCATION "${LPM_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LPM_ROOT_DIR}/include"
            )
            set(LPM_INCLUDE_DIR "${LPM_ROOT_DIR}/include")
        else()
            message(FATAL_ERROR "liblpm library not found. Build the main project first.")
        endif()
    else()
        # Try to find installed liblpm
        find_library(LPM_LIBRARY NAMES lpm)
        find_path(LPM_INCLUDE_DIR lpm/lpm.h)
        
        if(LPM_LIBRARY AND LPM_INCLUDE_DIR)
            add_library(lpm SHARED IMPORTED)
            set_target_properties(lpm PROPERTIES
                IMPORTED_LOCATION "${LPM_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LPM_INCLUDE_DIR}"
            )
        else()
            message(FATAL_ERROR "liblpm not found. Build and install the main project first.")
        endif()
    endif()
else()
    # Built as part of parent project
    get_target_property(LPM_INCLUDE_DIR lpm INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT LPM_INCLUDE_DIR)
        set(LPM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
    endif()
endif()

# JNI shared library
add_library(lpmjni SHARED
    src/main/native/liblpm_jni.c
)

target_include_directories(lpmjni PRIVATE
    ${JNI_INCLUDE_DIRS}
    ${LPM_INCLUDE_DIR}
)

# Define LPM_INSTALLED if building against installed version
if(LPM_INCLUDE_DIR MATCHES "/usr/local/include")
    target_compile_definitions(lpmjni PRIVATE LPM_INSTALLED)
endif()

# Link against lpm library
if(TARGET lpm)
    target_link_libraries(lpmjni PRIVATE lpm)
else()
    target_link_libraries(lpmjni PRIVATE ${LPM_LIBRARY})
endif()

# Set library properties
set_target_properties(lpmjni PROPERTIES
    OUTPUT_NAME "lpmjni"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    # Remove lib prefix on all platforms for consistent naming
    PREFIX ""
)

# Platform-specific settings
if(WIN32)
    set_target_properties(lpmjni PROPERTIES
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(lpmjni PROPERTIES
        SUFFIX ".dylib"
    )
else()
    set_target_properties(lpmjni PROPERTIES
        SUFFIX ".so"
        # Add RPATH for finding liblpm
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lpmjni PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Install target
install(TARGETS lpmjni
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib
)

# Also copy to resources directory for JAR bundling
if(DEFINED ENV{PLATFORM_DIR})
    set(PLATFORM_DIR $ENV{PLATFORM_DIR})
else()
    # Auto-detect platform
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(OS_NAME "linux")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(OS_NAME "darwin")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(OS_NAME "windows")
    else()
        set(OS_NAME "unknown")
    endif()
    
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        set(ARCH_NAME "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ARCH_NAME "aarch64")
    else()
        set(ARCH_NAME "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    
    set(PLATFORM_DIR "${OS_NAME}-${ARCH_NAME}")
endif()

message(STATUS "Platform: ${PLATFORM_DIR}")

# Custom target to copy native library to resources
add_custom_command(TARGET lpmjni POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/resources/native/${PLATFORM_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:lpmjni>"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/resources/native/${PLATFORM_DIR}/"
    COMMENT "Copying native library to resources directory"
)

message(STATUS "liblpm Java JNI configuration:")
message(STATUS "  JNI include: ${JNI_INCLUDE_DIRS}")
message(STATUS "  LPM include: ${LPM_INCLUDE_DIR}")
message(STATUS "  Platform: ${PLATFORM_DIR}")
