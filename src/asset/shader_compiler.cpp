// shader_compiler.cpp - GLSL to SPIR-V shader compilation implementation

#include <luma/asset/shader_compiler.hpp>
#include <luma/core/logging.hpp>

#include <shaderc/shaderc.hpp>

#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>

namespace luma::asset {

// Convert our ShaderStage enum to shaderc's shader kind
static auto to_shaderc_kind(ShaderStage stage) -> shaderc_shader_kind {
    switch (stage) {
    case ShaderStage::VERTEX: return shaderc_vertex_shader;
    case ShaderStage::FRAGMENT: return shaderc_fragment_shader;
    case ShaderStage::COMPUTE: return shaderc_compute_shader;
    case ShaderStage::GEOMETRY: return shaderc_geometry_shader;
    case ShaderStage::TESS_CONTROL: return shaderc_tess_control_shader;
    case ShaderStage::TESS_EVALUATION: return shaderc_tess_evaluation_shader;
    }
    return shaderc_compute_shader; // fallback
}

ShaderCompiler::ShaderCompiler(std::filesystem::path shader_dir,
                                 std::filesystem::path cache_dir)
    : shader_dir_(std::move(shader_dir)), cache_dir_(std::move(cache_dir)),
      compiler_impl_(new shaderc::Compiler()) {
    // Create cache directory if it doesn't exist
    if (!std::filesystem::exists(cache_dir_)) {
        std::filesystem::create_directories(cache_dir_);
        LOG_INFO("Created shader cache directory: {}", cache_dir_.string());
    }
}

ShaderCompiler::~ShaderCompiler() {
    delete static_cast<shaderc::Compiler*>(compiler_impl_);
}

auto ShaderCompiler::compile(std::string_view shader_path, bool force_recompile)
    -> std::expected<ShaderModule, ShaderError> {
    const auto source_path = shader_dir_ / shader_path;

    // Check if source file exists
    if (!std::filesystem::exists(source_path)) {
        LOG_ERROR("Shader file not found: {}", source_path.string());
        return std::unexpected(ShaderError::FILE_NOT_FOUND);
    }

    // Deduce shader stage from extension
    auto stage_result = deduce_stage(source_path);
    if (!stage_result) {
        return std::unexpected(stage_result.error());
    }
    const auto stage = stage_result.value();

    // Compute source file hash
    const auto source_hash = compute_file_hash(source_path);

    // Check cache unless force recompile
    const auto cache_path = get_cache_path(shader_path);
    if (!force_recompile && std::filesystem::exists(cache_path)) {
        // Read cached hash (stored as first 8 bytes of cache file)
        std::ifstream cache_file(cache_path, std::ios::binary);
        if (cache_file) {
            u64 cached_hash = 0;
            cache_file.read(reinterpret_cast<char*>(&cached_hash), sizeof(cached_hash));
            if (cached_hash == source_hash) {
                // Cache hit! Load SPIR-V
                auto spirv_result = load_cached_spirv(cache_path);
                if (spirv_result) {
                    LOG_DEBUG("Shader cache hit: {}", shader_path);
                    return ShaderModule{
                        .spirv = std::move(spirv_result.value()),
                        .stage = stage,
                        .source_path = source_path,
                        .source_hash = source_hash,
                    };
                }
            }
        }
    }

    // Cache miss or force recompile - compile from source
    LOG_INFO("Compiling shader: {}", shader_path);

    // Read source file
    auto source_result = read_file(source_path);
    if (!source_result) {
        return std::unexpected(source_result.error());
    }
    const auto& source = source_result.value();

    // Compile GLSL to SPIR-V
    auto spirv_result = compile_glsl(source, stage, shader_path);
    if (!spirv_result) {
        return std::unexpected(spirv_result.error());
    }
    auto spirv = std::move(spirv_result.value());

    // Write to cache (hash + spirv)
    std::ofstream cache_file(cache_path, std::ios::binary);
    if (cache_file) {
        cache_file.write(reinterpret_cast<const char*>(&source_hash), sizeof(source_hash));
        cache_file.write(reinterpret_cast<const char*>(spirv.data()),
                          static_cast<std::streamsize>(spirv.size() * sizeof(u32)));
        if (cache_file) {
            LOG_DEBUG("Cached compiled shader: {}", cache_path.string());
        } else {
            LOG_WARN("Failed to write shader cache: {}", cache_path.string());
        }
    }

    return ShaderModule{
        .spirv = std::move(spirv),
        .stage = stage,
        .source_path = source_path,
        .source_hash = source_hash,
    };
}

auto ShaderCompiler::load_spirv(const std::filesystem::path& spirv_path)
    -> std::expected<ShaderModule, ShaderError> {
    if (!std::filesystem::exists(spirv_path)) {
        LOG_ERROR("SPIR-V file not found: {}", spirv_path.string());
        return std::unexpected(ShaderError::FILE_NOT_FOUND);
    }

    // Deduce stage from path
    auto stage_result = deduce_stage(spirv_path);
    if (!stage_result) {
        return std::unexpected(stage_result.error());
    }

    // Load SPIR-V from file
    auto spirv_result = load_cached_spirv(spirv_path);
    if (!spirv_result) {
        return std::unexpected(spirv_result.error());
    }

    return ShaderModule{
        .spirv = std::move(spirv_result.value()),
        .stage = stage_result.value(),
        .source_path = spirv_path,
        .source_hash = 0, // no source hash for direct load
    };
}

auto ShaderCompiler::get_cache_path(std::string_view shader_path) const
    -> std::filesystem::path {
    auto cache_file = std::filesystem::path(shader_path);
    cache_file.replace_extension(".spv");
    return cache_dir_ / cache_file.filename();
}

auto ShaderCompiler::is_outdated(std::string_view shader_path) const -> bool {
    const auto source_path = shader_dir_ / shader_path;
    const auto cache_path = get_cache_path(shader_path);

    if (!std::filesystem::exists(source_path)) {
        return false; // source doesn't exist
    }
    if (!std::filesystem::exists(cache_path)) {
        return true; // no cache
    }

    // Compare modification times
    const auto source_time = std::filesystem::last_write_time(source_path);
    const auto cache_time = std::filesystem::last_write_time(cache_path);
    return source_time > cache_time;
}

auto ShaderCompiler::deduce_stage(const std::filesystem::path& path)
    -> std::expected<ShaderStage, ShaderError> {
    const auto ext = path.extension().string();

    if (ext == ".vert") return ShaderStage::VERTEX;
    if (ext == ".frag") return ShaderStage::FRAGMENT;
    if (ext == ".comp") return ShaderStage::COMPUTE;
    if (ext == ".geom") return ShaderStage::GEOMETRY;
    if (ext == ".tesc") return ShaderStage::TESS_CONTROL;
    if (ext == ".tese") return ShaderStage::TESS_EVALUATION;
    if (ext == ".spv") {
        // For .spv files, try to deduce from the stem (e.g., "shader.comp.spv" → COMPUTE)
        auto stem = path.stem().extension().string();
        if (stem == ".vert") return ShaderStage::VERTEX;
        if (stem == ".frag") return ShaderStage::FRAGMENT;
        if (stem == ".comp") return ShaderStage::COMPUTE;
        if (stem == ".geom") return ShaderStage::GEOMETRY;
        if (stem == ".tesc") return ShaderStage::TESS_CONTROL;
        if (stem == ".tese") return ShaderStage::TESS_EVALUATION;
    }

    LOG_ERROR("Unknown shader stage for file: {}", path.string());
    return std::unexpected(ShaderError::INVALID_STAGE);
}

auto ShaderCompiler::compute_file_hash(const std::filesystem::path& path) -> u64 {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return 0;
    }

    // Simple hash: FNV-1a
    u64 hash = 14695981039346656037ULL;
    constexpr u64 fnv_prime = 1099511628211ULL;

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        const auto bytes_read = file.gcount();
        for (std::streamsize i = 0; i < bytes_read; ++i) {
            hash ^= static_cast<u64>(static_cast<unsigned char>(buffer[i]));
            hash *= fnv_prime;
        }
    }

    return hash;
}

auto ShaderCompiler::read_file(const std::filesystem::path& path)
    -> std::expected<std::string, ShaderError> {
    std::ifstream file(path, std::ios::in);
    if (!file) {
        LOG_ERROR("Failed to read file: {}", path.string());
        return std::unexpected(ShaderError::FILE_NOT_FOUND);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto ShaderCompiler::compile_glsl(std::string_view source, ShaderStage stage,
                                    std::string_view filename)
    -> std::expected<std::vector<u32>, ShaderError> {
    auto* compiler = static_cast<shaderc::Compiler*>(compiler_impl_);

    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);

    // Compile
    const auto result = compiler->CompileGlslToSpv(
        source.data(), source.size(), to_shaderc_kind(stage), filename.data(), options);

    // Check for errors
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        LOG_ERROR("Shader compilation failed for {}: {}", filename,
                        result.GetErrorMessage());
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }

    // Extract SPIR-V
    std::vector<u32> spirv(result.cbegin(), result.cend());
    LOG_INFO("Successfully compiled {} ({} bytes SPIR-V)", filename,
                   spirv.size() * sizeof(u32));

    return spirv;
}

auto ShaderCompiler::load_cached_spirv(const std::filesystem::path& cache_path)
    -> std::expected<std::vector<u32>, ShaderError> {
    std::ifstream file(cache_path, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open cached SPIR-V: {}", cache_path.string());
        return std::unexpected(ShaderError::CACHE_READ_FAILED);
    }

    // Skip hash (first 8 bytes)
    file.seekg(sizeof(u64), std::ios::beg);

    // Read SPIR-V
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    file.seekg(sizeof(u64), std::ios::beg);

    const auto spirv_size = (file_size - static_cast<std::streamoff>(sizeof(u64))) / static_cast<std::streamoff>(sizeof(u32));
    std::vector<u32> spirv(static_cast<size_t>(spirv_size));

    const auto bytes_to_read = static_cast<std::streamsize>(static_cast<size_t>(spirv_size) * sizeof(u32));
    file.read(reinterpret_cast<char*>(spirv.data()), bytes_to_read);

    if (!file) {
        LOG_ERROR("Failed to read cached SPIR-V: {}", cache_path.string());
        return std::unexpected(ShaderError::CACHE_READ_FAILED);
    }

    return spirv;
}

auto ShaderCompiler::write_cache(const std::filesystem::path& cache_path,
                                   std::span<const u32> spirv) -> bool {
    std::ofstream file(cache_path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(spirv.data()),
                static_cast<std::streamsize>(spirv.size() * sizeof(u32)));
    return static_cast<bool>(file);
}

} // namespace luma::asset
