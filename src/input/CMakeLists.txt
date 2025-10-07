# ==============================================================================
# luma_input library (window + keyboard + mouse input)
# ==============================================================================
#
# Build configuration for window and input module (GLFW + functional input uwu)
# 
# builds the luma_input module as a static library. this module provides GLFW
# window management and keyboard/mouse input polling with a functional interface.
# 
# @author LukeFrankio
# @date 2025-10-07

add_library(luma_input STATIC
    window.cpp
    input.cpp
)

# set target properties
set_target_properties(luma_input PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# public include directories (for consumers of luma_input)
target_include_directories(luma_input PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# link dependencies
target_link_libraries(luma_input
    PUBLIC
        luma_core         # for types, logging, math
        luma_vulkan       # for Instance (surface creation)
        glfw              # GLFW windowing library
        Vulkan::Vulkan    # for VkSurfaceKHR
)

# apply compiler warnings and sanitizers
if(COMMAND apply_compiler_warnings)
    apply_compiler_warnings(luma_input)
endif()



# log successful configuration
message(STATUS "luma_input library configured (GLFW window + functional input)")
message(STATUS "  - Sources: window.cpp, input.cpp")
message(STATUS "  - Links: luma_core, luma_vulkan, glfw, Vulkan::Vulkan")
message(STATUS "  - C++ standard: C++26 (latest features enabled)")
