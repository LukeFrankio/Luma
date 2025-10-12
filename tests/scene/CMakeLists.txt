# Scene Module Tests

add_executable(test_ecs
    test_ecs.cpp
)

target_link_libraries(test_ecs PRIVATE
    luma_scene
    luma_core
    GTest::gtest_main
)

# Serialization tests
add_executable(test_serialization
    test_serialization.cpp
)

target_link_libraries(test_serialization PRIVATE
    luma_scene
    luma_core
    GTest::gtest_main
)

# Register tests with CTest
add_test(NAME Scene.ECS COMMAND test_ecs)
add_test(NAME Scene.Serialization COMMAND test_serialization)

# Set working directory for tests (needed for shader paths)
set_tests_properties(Scene.ECS PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_tests_properties(Scene.Serialization PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
