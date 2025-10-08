# luma_vulkan library
# Vulkan backend implementation with VMA memory management

add_library(luma_vulkan STATIC
    # Instance and device management
    instance.cpp
    device.cpp
    
    # Swapchain and presentation
    swapchain.cpp
    
    # Command buffers
    command_buffer.cpp
    
    # Synchronization
    sync.cpp
    
    # Memory management (VMA)
    memory.cpp
    
    # Compute pipelines and descriptors
    pipeline.cpp
    descriptor.cpp
)

target_include_directories(luma_vulkan PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(luma_vulkan PUBLIC
    luma_core
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
)

# Apply compiler warnings
set_luma_warnings(luma_vulkan)



# Set target properties
set_target_properties(luma_vulkan PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
