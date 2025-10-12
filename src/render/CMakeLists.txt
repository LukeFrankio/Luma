# Render Module
# Provides rendering utilities (SDF, BVH, path tracing infrastructure)
# Currently header-only (SDF functions)

add_library(luma_render INTERFACE)

target_include_directories(luma_render INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(luma_render INTERFACE
    luma_core
)

target_compile_features(luma_render INTERFACE cxx_std_26)

# Note: SDF functions are constexpr header-only currently
# Future additions: BVH builder, camera system, material system
