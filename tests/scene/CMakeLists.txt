# Scene Module Tests

add_executable(test_ecs
    test_ecs.cpp
)

target_link_libraries(test_ecs PRIVATE
    luma_scene
    luma_core
    GTest::gtest_main
)

# Register test with CTest
add_test(NAME Scene.ECS COMMAND test_ecs)

# Set working directory for test (needed for shader paths)
set_tests_properties(Scene.ECS PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
