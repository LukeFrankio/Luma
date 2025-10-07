# luma_editor module
# ImGui integration for debug UI, settings, profiler, scene hierarchy

add_library(luma_editor STATIC
    imgui_context.cpp
)

target_include_directories(luma_editor
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(luma_editor
    PUBLIC
        luma_core
        luma_vulkan
        luma_input
        imgui
        Vulkan::Vulkan
)

# Apply compiler warnings and sanitizers
if(COMMAND apply_compiler_warnings)
    apply_compiler_warnings(luma_editor)
endif()

message(STATUS "luma_editor module configured")
