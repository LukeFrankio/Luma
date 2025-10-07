# luma_core library
# Core functionality: types, math, logging, time, jobs (stub), memory (stub)

cmake_minimum_required(VERSION 4.1 FATAL_ERROR)

# Define library target
add_library(luma_core STATIC
    # Source files
    ${CMAKE_CURRENT_SOURCE_DIR}/logging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/time.cpp
)

# Set C++26 standard
target_compile_features(luma_core PUBLIC cxx_std_26)

# Public include directories
target_include_directories(luma_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(luma_core PUBLIC
    glm::glm
)

# Apply compiler warnings
include(CompilerWarnings)
set_luma_warnings(luma_core)



# Set library properties
set_target_properties(luma_core PROPERTIES
    OUTPUT_NAME "luma_core"
    POSITION_INDEPENDENT_CODE ON
    CXX_EXTENSIONS OFF
)

message(STATUS "luma_core library configured")
