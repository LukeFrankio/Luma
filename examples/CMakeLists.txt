# Examples CMakeLists.txt
# Executable demonstrations of Luma Engine features

# Gradient Visualizer: Compute shader example
add_executable(gradient_visualizer
    gradient_visualizer.cpp
)

target_link_libraries(gradient_visualizer
    PRIVATE
        luma_core
        luma_vulkan
        luma_asset
        Vulkan::Vulkan
)

target_include_directories(gradient_visualizer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Vulkan_INCLUDE_DIRS}
)

# Apply strict compiler warnings
if(MSVC)
    target_compile_options(gradient_visualizer PRIVATE /W4)
else()
    target_compile_options(gradient_visualizer PRIVATE
        -Wall -Wextra -pedantic -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
        -Wpedantic -Wconversion -Wsign-conversion -Wnull-dereference
        -Wdouble-promotion -Wformat=2 -Wmisleading-indentation
        -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast
    )
    if(LUMA_WARNINGS_AS_ERRORS)
        target_compile_options(gradient_visualizer PRIVATE -Werror)
    endif()
endif()

message(STATUS "Added example: gradient_visualizer")
