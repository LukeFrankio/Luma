cmake_minimum_required(VERSION 4.1 FATAL_ERROR)

# Project definition
project(LUMA VERSION 1.0.0 LANGUAGES CXX)

# Set C++26 standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(LUMA_BUILD_TESTS "Build Google Test suites" ON)
option(LUMA_ENABLE_ASAN "Enable AddressSanitizer in debug builds" OFF)  # disabled due to GCC 15 linking issues on Windows
option(LUMA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer in debug builds" OFF)  # disabled due to GCC 15 linking issues on Windows
option(LUMA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Export compile commands for IDE/tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(FetchDependencies)

# Find Vulkan SDK (required)
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found! Please install from https://vulkan.lunarg.com/")
endif()

message(STATUS "Vulkan SDK version: ${Vulkan_VERSION}")
message(STATUS "Vulkan include dir: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan libraries: ${Vulkan_LIBRARIES}")

# Fetch dependencies
fetch_dependencies()

# Enable testing
if(LUMA_BUILD_TESTS)
    enable_testing()
    include(CTest)
endif()

# Add subdirectories (will be uncommented as modules are created)
add_subdirectory(src/core)
add_subdirectory(src/vulkan)
add_subdirectory(src/input)
# add_subdirectory(src/scene)
# add_subdirectory(src/asset)
# add_subdirectory(src/audio)
# add_subdirectory(src/pathtracer)
# add_subdirectory(src/physics)
# add_subdirectory(src/settings)
# add_subdirectory(src/editor)

# Add tests
if(LUMA_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== LUMA Engine Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build tests: ${LUMA_BUILD_TESTS}")
message(STATUS "Warnings as errors: ${LUMA_WARNINGS_AS_ERRORS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "AddressSanitizer: ${LUMA_ENABLE_ASAN}")
    message(STATUS "UBSanitizer: ${LUMA_ENABLE_UBSAN}")
endif()
message(STATUS "=================================")
message(STATUS "")
