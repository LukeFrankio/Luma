# CompilerWarnings.cmake
# Comprehensive compiler warning configuration for LUMA Engine

function(set_luma_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            # Enable all warnings
            -Wall
            -Wextra
            -pedantic
            
            # Additional warnings
            -Wshadow                    # Warn about variable shadowing
            -Wnon-virtual-dtor         # Warn about non-virtual destructors
            -Wold-style-cast           # Warn about C-style casts
            -Wcast-align               # Warn about alignment issues
            -Wunused                   # Warn about unused variables
            -Woverloaded-virtual       # Warn about overloaded virtuals
            -Wpedantic                 # Strict ISO C++ compliance
            -Wconversion               # Warn about implicit conversions
            -Wsign-conversion          # Warn about sign conversions
            -Wnull-dereference         # Warn about null dereferences
            -Wdouble-promotion         # Warn about float to double promotion
            -Wformat=2                 # Extra format string checks
            
            # GCC-specific warnings
            $<$<CXX_COMPILER_ID:GNU>:
                -Wmisleading-indentation
                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            >
            
            # Clang-specific warnings
            $<$<CXX_COMPILER_ID:Clang>:
                -Wmost
                -Wextra
            >
        )
        
        # Treat warnings as errors if enabled
        if(LUMA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE
            /W4                        # Warning level 4
            /permissive-               # Strict standards conformance
            /w14242                    # Conversion warning
            /w14254                    # Operator conversion warning
            /w14263                    # Member function override
            /w14265                    # Virtual destructor warning
            /w14287                    # Unsigned/negative comparison
            /w14289                    # Loop variable usage outside loop
            /w14296                    # Expression always true/false
            /w14311                    # Pointer truncation
            /w14545                    # Missing argument to comma
            /w14546                    # Function call before comma
            /w14547                    # Operator precedence
            /w14549                    # Operator has no effect
            /w14555                    # Expression has no effect
            /w14619                    # Pragma warning
            /w14640                    # Thread-unsafe static member init
            /w14826                    # Conversion sign extension
            /w14905                    # Wide string literal cast
            /w14906                    # String literal cast
            /w14928                    # Illegal copy-initialization
        )
        
        # Treat warnings as errors if enabled
        if(LUMA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()

# Apply warnings to all targets in a directory
function(set_luma_warnings_for_all_targets)
    get_property(targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
        set_luma_warnings(${target})
    endforeach()
endfunction()

message(STATUS "Compiler warnings module loaded (LUMA_WARNINGS_AS_ERRORS=${LUMA_WARNINGS_AS_ERRORS})")
