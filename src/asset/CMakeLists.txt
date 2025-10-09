# luma_asset - Asset loading and shader compilation

add_library(luma_asset STATIC
    shader_compiler.cpp
)

target_include_directories(luma_asset
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(luma_asset
    PUBLIC
        luma_core
        ${Vulkan_LIBRARIES}
    PRIVATE
        slang  # Link against Slang (the SUPERIOR shader compiler uwu)
)

# Include Vulkan headers
target_include_directories(luma_asset PRIVATE ${Vulkan_INCLUDE_DIRS})

# Apply compiler warnings
include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
set_luma_warnings(luma_asset)

# Apply sanitizers in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(${CMAKE_SOURCE_DIR}/cmake/Sanitizers.cmake)
    enable_luma_sanitizers(luma_asset)
endif()
