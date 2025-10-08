// shader_compiler.hpp - GLSL to SPIR-V shader compilation with caching
// Part of the LUMA Engine asset pipeline

#pragma once

#include <luma/core/types.hpp>

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luma::asset {

/// Shader compilation error codes
enum class ShaderError {
    FILE_NOT_FOUND,      ///< Shader source file does not exist
    COMPILATION_FAILED,  ///< GLSL compilation failed (syntax/semantic error)
    CACHE_READ_FAILED,   ///< Failed to read cached SPIR-V
    CACHE_WRITE_FAILED,  ///< Failed to write SPIR-V to cache
    INVALID_STAGE,       ///< Unknown shader stage (not .vert, .frag, .comp, etc.)
    INVALID_SPIRV,       ///< Cached SPIR-V is invalid or corrupted
};

/// Shader stage deduced from file extension
enum class ShaderStage {
    VERTEX,          ///< Vertex shader (.vert)
    FRAGMENT,        ///< Fragment shader (.frag)
    COMPUTE,         ///< Compute shader (.comp)
    GEOMETRY,        ///< Geometry shader (.geom)
    TESS_CONTROL,    ///< Tessellation control shader (.tesc)
    TESS_EVALUATION, ///< Tessellation evaluation shader (.tese)
};

/// Compilation result: SPIR-V bytecode
struct ShaderModule {
    std::vector<u32> spirv;           ///< SPIR-V bytecode (32-bit words)
    ShaderStage stage;                ///< Shader stage
    std::filesystem::path source_path; ///< Original source file path
    u64 source_hash;                  ///< Hash of source file (for cache validation)
};

/// Compiler for GLSL shaders to SPIR-V, with disk caching and hot-reload support.
///
/// The shader compiler:
/// - Compiles GLSL to SPIR-V using shaderc
/// - Caches compiled SPIR-V to disk (hashed by source content)
/// - Automatically detects file changes for hot-reload
/// - Supports all Vulkan shader stages (compute, vertex, fragment, etc.)
///
/// Example usage:
/// @code
/// ShaderCompiler compiler("shaders", "shaders_cache");
/// auto result = compiler.compile("path_tracer.comp");
/// if (result) {
///     const auto& module = result.value();
///     // Create VkShaderModule from module.spirv
/// } else {
///     log_error("Shader compilation failed: {}", static_cast<int>(result.error()));
/// }
/// @endcode
class ShaderCompiler {
public:
    /// Construct a shader compiler with specified shader source and cache directories.
    ///
    /// @param shader_dir Directory containing GLSL shader source files
    /// @param cache_dir Directory for storing compiled SPIR-V bytecode
    ShaderCompiler(std::filesystem::path shader_dir, std::filesystem::path cache_dir);

    /// Destructor (non-copyable, non-movable for simplicity)
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    ShaderCompiler(ShaderCompiler&&) = delete;
    ShaderCompiler& operator=(ShaderCompiler&&) = delete;

    /// Compile a GLSL shader to SPIR-V, using cache if available and valid.
    ///
    /// This function:
    /// 1. Checks if cached SPIR-V exists and is valid (source hash matches)
    /// 2. If cache hit, loads SPIR-V from disk
    /// 3. If cache miss, compiles GLSL to SPIR-V and writes to cache
    /// 4. Returns ShaderModule with SPIR-V bytecode
    ///
    /// @param shader_path Relative path to shader file (e.g., "path_tracer.comp")
    /// @param force_recompile If true, ignore cache and recompile (for hot-reload)
    /// @return ShaderModule on success, ShaderError on failure
    [[nodiscard]] auto compile(std::string_view shader_path, bool force_recompile = false)
        -> std::expected<ShaderModule, ShaderError>;

    /// Load SPIR-V directly from a file (bypasses compilation, useful for pre-compiled shaders).
    ///
    /// @param spirv_path Path to .spv file
    /// @return ShaderModule on success, ShaderError on failure
    [[nodiscard]] auto load_spirv(const std::filesystem::path& spirv_path)
        -> std::expected<ShaderModule, ShaderError>;

    /// Get the cache file path for a given shader source file.
    ///
    /// @param shader_path Relative path to shader source file
    /// @return Absolute path to cached SPIR-V file (.spv)
    [[nodiscard]] auto get_cache_path(std::string_view shader_path) const
        -> std::filesystem::path;

    /// Check if a shader has been modified since last compilation (for hot-reload).
    ///
    /// @param shader_path Relative path to shader file
    /// @return True if shader source is newer than cached SPIR-V
    [[nodiscard]] auto is_outdated(std::string_view shader_path) const -> bool;

    /// Get the shader source directory.
    [[nodiscard]] auto shader_directory() const -> const std::filesystem::path& {
        return shader_dir_;
    }

    /// Get the shader cache directory.
    [[nodiscard]] auto cache_directory() const -> const std::filesystem::path& {
        return cache_dir_;
    }

private:
    /// Deduce shader stage from file extension.
    [[nodiscard]] static auto deduce_stage(const std::filesystem::path& path)
        -> std::expected<ShaderStage, ShaderError>;

    /// Compute hash of file contents (for cache validation).
    [[nodiscard]] static auto compute_file_hash(const std::filesystem::path& path) -> u64;

    /// Read file contents into string.
    [[nodiscard]] static auto read_file(const std::filesystem::path& path)
        -> std::expected<std::string, ShaderError>;

    /// Compile GLSL source to SPIR-V using shaderc.
    [[nodiscard]] auto compile_glsl(std::string_view source, ShaderStage stage,
                                     std::string_view filename)
        -> std::expected<std::vector<u32>, ShaderError>;

    /// Load cached SPIR-V from disk.
    [[nodiscard]] auto load_cached_spirv(const std::filesystem::path& cache_path)
        -> std::expected<std::vector<u32>, ShaderError>;

    /// Write SPIR-V to cache.
    auto write_cache(const std::filesystem::path& cache_path, std::span<const u32> spirv) -> bool;

    std::filesystem::path shader_dir_; ///< Shader source directory
    std::filesystem::path cache_dir_;  ///< SPIR-V cache directory
    void* compiler_impl_;              ///< Opaque pointer to shaderc compiler (pimpl)
};

} // namespace luma::asset
