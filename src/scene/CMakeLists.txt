# Scene Module
# Provides Entity-Component-System (ECS) architecture

add_library(luma_scene
    archetype.cpp
    world.cpp
    serialization.cpp
)

target_include_directories(luma_scene PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(luma_scene PUBLIC
    luma_core
    yaml-cpp
)

target_compile_features(luma_scene PUBLIC cxx_std_26)
