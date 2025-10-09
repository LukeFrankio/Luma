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
    
    # Slang - Modern shader compiler (GLSL/HLSL/Slang → SPIR-V/DXIL/Metal)
    # The SUPERIOR shader language - cross-platform, generics, autodiff, modules!
    # Using PREBUILT binaries to avoid dependency conflicts and faster builds!
    # Building from source causes Vulkan::Headers conflicts and webgpu_dawn issues
    FetchContent_Declare(
        slang
        URL https://github.com/shader-slang/slang/releases/download/v2024.14.4/slang-2024.14.4-windows-x86_64.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    
    # Make all dependencies available
    message(STATUS "Making dependencies available (this may take a while on first run)...")
    
    # Fetch dependencies (use MakeAvailable for everything, manual setup after)
    FetchContent_MakeAvailable(
        glm
        vma
        glfw
        yaml-cpp
        slang
    )
    
    # Slang prebuilt binaries - manually create imported target (no CMakeLists.txt in prebuilt)
    if(NOT TARGET slang_prebuilt)
        message(STATUS "Slang prebuilt binaries at: ${slang_SOURCE_DIR}")
        
        # Create imported target for Slang prebuilt DLL
        add_library(slang_prebuilt SHARED IMPORTED GLOBAL)
        
        # Set library location based on platform
        if(WIN32)
            set_target_properties(slang_prebuilt PROPERTIES
                IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang.dll"
                IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang.lib"
                INTERFACE_INCLUDE_DIRECTORIES "${slang_SOURCE_DIR}/include"
            )
        else()
            message(FATAL_ERROR "Slang prebuilt binaries currently only configured for Windows")
        endif()
        
        # Create alias so we can use "slang" as target name
        add_library(slang ALIAS slang_prebuilt)
        
        message(STATUS "Slang imported target created (prebuilt DLL)")
        message(STATUS "  - DLL: ${slang_SOURCE_DIR}/bin/slang.dll")
        message(STATUS "  - LIB: ${slang_SOURCE_DIR}/lib/slang.lib")
        message(STATUS "  - Headers: ${slang_SOURCE_DIR}/include")
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
    message(STATUS "Slang: ${slang_SOURCE_DIR} (the SUPERIOR shader compiler uwu)")
    message(STATUS "==========================")
    message(STATUS "")
endfunction()
