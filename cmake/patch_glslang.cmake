# patch_glslang.cmake
# Patches glslang SpvBuilder.h to add missing #include <cstdint>

# Get the glslang source directory
set(GLSLANG_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/glslang-src")
set(SPV_BUILDER_H "${GLSLANG_SOURCE_DIR}/SPIRV/SpvBuilder.h")

if(EXISTS "${SPV_BUILDER_H}")
    # Read the file
    file(READ "${SPV_BUILDER_H}" FILE_CONTENTS)
    
    # Check if already patched
    string(FIND "${FILE_CONTENTS}" "#include <cstdint>" ALREADY_PATCHED)
    
    if(ALREADY_PATCHED EQUAL -1)
        # Find the location after the last #include before the namespace declaration
        # We'll insert after "#include <stack>"
        string(REPLACE "#include <stack>" "#include <stack>\n#include <cstdint>" FILE_CONTENTS "${FILE_CONTENTS}")
        
        # Write the patched file
        file(WRITE "${SPV_BUILDER_H}" "${FILE_CONTENTS}")
        
        message(STATUS "Successfully patched ${SPV_BUILDER_H}")
    else()
        message(STATUS "SpvBuilder.h already patched, skipping")
    endif()
else()
    message(WARNING "SpvBuilder.h not found at ${SPV_BUILDER_H}, patch may need to run later")
endif()
