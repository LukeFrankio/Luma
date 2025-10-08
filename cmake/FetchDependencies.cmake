# FetchDependencies.cmake
# External dependency management for LUMA Engine using FetchContent

include(FetchContent)

# Set FetchContent base directory
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps)

# Disable in-source builds
set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED OFF)

# Quiet fetch content by default
set(FETCHCONTENT_QUIET OFF)

function(fetch_dependencies)
    message(STATUS "Fetching external dependencies...")
    
    # GLM (OpenGL Mathematics) - Header-only math library
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )
    
    # Vulkan Memory Allocator (VMA) - Header-only GPU memory management
    FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.1.0
        GIT_SHALLOW TRUE
    )
    
    # ImGui (docking branch) - Immediate-mode GUI
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        GIT_SHALLOW TRUE
    )
    
    # GLFW - Window and input library
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    
    # yaml-cpp - YAML parsing library
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    
    # Google Test - Unit testing framework
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )
    
    # SPIRV-Headers (required by SPIRV-Tools)
    FetchContent_Declare(
        spirv-headers
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
        GIT_TAG vulkan-sdk-1.3.290.0
    )
    
    # SPIRV-Tools (required by shaderc)
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        spirv-tools
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
        GIT_TAG vulkan-sdk-1.3.290.0
    )
    
    # glslang (required by shaderc)
    FetchContent_Declare(
        glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG vulkan-sdk-1.3.290.0
    )
    
    # shaderc - GLSL/HLSL to SPIR-V compiler (built from source for MinGW compatibility)
    # Note: We build from source because Vulkan SDK's prebuilt libs are MSVC-only
    set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(SHADERC_ENABLE_SHARED_CRT OFF CACHE BOOL "" FORCE)
    
    # Disable spirv-tools tests and install
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
    
    # Disable glslang tests and install
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
    set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    
    # Fetch shaderc (dependencies will be provided by the above FetchContent declarations)
    FetchContent_Declare(
        shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        GIT_TAG v2024.3
    )
    
    # Make all dependencies available
    message(STATUS "Making dependencies available (this may take a while on first run)...")
    
    # Fetch dependencies in order (shaderc dependencies first)
    FetchContent_MakeAvailable(
        glm
        vma
        glfw
        yaml-cpp
        spirv-headers
        spirv-tools
        glslang
        shaderc
    )
    
    # Patch glslang SpvBuilder.h to add missing #include <cstdint>
    set(SPV_BUILDER_H "${glslang_SOURCE_DIR}/SPIRV/SpvBuilder.h")
    if(EXISTS "${SPV_BUILDER_H}")
        file(READ "${SPV_BUILDER_H}" FILE_CONTENTS)
        string(FIND "${FILE_CONTENTS}" "#include <cstdint>" ALREADY_PATCHED)
        if(ALREADY_PATCHED EQUAL -1)
            string(REPLACE "#include <stack>" "#include <stack>\n#include <cstdint>" FILE_CONTENTS "${FILE_CONTENTS}")
            file(WRITE "${SPV_BUILDER_H}" "${FILE_CONTENTS}")
            message(STATUS "Patched glslang SpvBuilder.h to add #include <cstdint>")
        endif()
    endif()
    
    if(LUMA_BUILD_TESTS)
        FetchContent_MakeAvailable(googletest)
    endif()
    
    # ImGui requires manual setup (no CMakeLists.txt)
    # Use FetchContent_MakeAvailable to avoid deprecation warning
    FetchContent_MakeAvailable(imgui)
    
    # Create ImGui library target manually
    if(NOT TARGET imgui)
        add_library(imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        )
        
        target_include_directories(imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )
        
        target_link_libraries(imgui PUBLIC
            glfw
            Vulkan::Vulkan
        )
        
        # Disable warnings for ImGui
        target_compile_options(imgui PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/w>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
        )
        
        message(STATUS "ImGui library target created")
    endif()
    
    message(STATUS "All dependencies fetched successfully")
    
    # Print dependency information
    message(STATUS "")
    message(STATUS "=== Dependency Summary ===")
    message(STATUS "GLM: ${glm_SOURCE_DIR}")
    message(STATUS "VMA: ${vma_SOURCE_DIR}")
    message(STATUS "ImGui: ${imgui_SOURCE_DIR}")
    message(STATUS "GLFW: ${glfw_SOURCE_DIR}")
    message(STATUS "yaml-cpp: ${yaml-cpp_SOURCE_DIR}")
    if(LUMA_BUILD_TESTS)
        message(STATUS "Google Test: ${googletest_SOURCE_DIR}")
    endif()
    message(STATUS "shaderc: ${shaderc_SOURCE_DIR} (built from source for MinGW)")
    message(STATUS "==========================")
    message(STATUS "")
endfunction()
