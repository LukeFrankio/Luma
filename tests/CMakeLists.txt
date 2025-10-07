# ==============================================================================
# LUMA Engine Tests
# ==============================================================================
#
# Build configuration for test executables and Google Test suites
#
# @author LukeFrankio
# @date 2025-10-07

# ==============================================================================
# test_input executable (interactive test for window + input)
# ==============================================================================

add_executable(test_input
    test_input.cpp
)

target_link_libraries(test_input
    PRIVATE
        luma_core
        luma_input
)

# apply compiler warnings
if(COMMAND apply_compiler_warnings)
    apply_compiler_warnings(test_input)
endif()

# NOTE: Sanitizers disabled for test executables to avoid linking issues
# The libraries are built with sanitizers, which is sufficient for catching bugs
# if(COMMAND enable_luma_sanitizers)
#     enable_luma_sanitizers(test_input)
# endif()

message(STATUS "test_input executable configured (interactive window + input test)")

# ==============================================================================
# Google Test suites (TODO: add unit tests)
# ==============================================================================

# TODO: add unit tests for core module
# add_executable(luma_tests
#     core/test_logging.cpp
#     core/test_math.cpp
# )
# 
# target_link_libraries(luma_tests
#     PRIVATE
#         luma_core
#         GTest::gtest
#         GTest::gtest_main
# )
# 
# include(GoogleTest)
# gtest_discover_tests(luma_tests)
