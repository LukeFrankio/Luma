# Sanitizers.cmake
# AddressSanitizer and UndefinedBehaviorSanitizer configuration for LUMA Engine

function(enable_luma_sanitizers target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Sanitizers only supported on GCC/Clang (compiler: ${CMAKE_CXX_COMPILER_ID})")
        return()
    endif()
    
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Sanitizers only enabled in Debug builds (current: ${CMAKE_BUILD_TYPE})")
        return()
    endif()
    
    set(SANITIZER_FLAGS "")
    
    # AddressSanitizer
    if(LUMA_ENABLE_ASAN)
        list(APPEND SANITIZER_FLAGS "address")
        message(STATUS "AddressSanitizer enabled for target: ${target}")
    endif()
    
    # UndefinedBehaviorSanitizer
    if(LUMA_ENABLE_UBSAN)
        list(APPEND SANITIZER_FLAGS "undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled for target: ${target}")
    endif()
    
    if(SANITIZER_FLAGS)
        # Join flags with comma
        string(REPLACE ";" "," SANITIZER_FLAGS_STR "${SANITIZER_FLAGS}")
        
        target_compile_options(${target} PRIVATE
            -fsanitize=${SANITIZER_FLAGS_STR}
            -fno-omit-frame-pointer
            -g
        )
        
        target_link_options(${target} PRIVATE
            -fsanitize=${SANITIZER_FLAGS_STR}
        )
        
        # Set environment variables for better error messages
        if(LUMA_ENABLE_ASAN)
            set_target_properties(${target} PROPERTIES
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:symbolize=1:halt_on_error=0"
            )
        endif()
        
        if(LUMA_ENABLE_UBSAN)
            set_target_properties(${target} PROPERTIES
                ENVIRONMENT "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0"
            )
        endif()
    endif()
endfunction()

# Apply sanitizers to all targets in a directory
function(enable_luma_sanitizers_for_all_targets)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        get_property(targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target ${targets})
            enable_luma_sanitizers(${target})
        endforeach()
    endif()
endfunction()

message(STATUS "Sanitizers module loaded (Debug builds only)")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "  - AddressSanitizer: ${LUMA_ENABLE_ASAN}")
    message(STATUS "  - UBSanitizer: ${LUMA_ENABLE_UBSAN}")
endif()
