# Build configuration for render module tests
# @author LukeFrankio
# @date 2025-10-12

# SDF tests (header-only, just needs linking)
add_executable(test_sdf
    test_sdf.cpp
)

target_link_libraries(test_sdf
    PRIVATE
        luma_render
        luma_core
        GTest::gtest
        GTest::gtest_main
)

# Register with CTest
add_test(NAME test_sdf COMMAND test_sdf)
