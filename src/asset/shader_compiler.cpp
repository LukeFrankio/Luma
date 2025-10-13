// shader_compiler.cpp - Slang shader compilation implementation
// Slang > GLSL - the SUPERIOR shader language uwu ✨

#include <luma/asset/shader_compiler.hpp>
#include <luma/core/logging.hpp>

#include <slang.h>

#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>

namespace luma::asset {

// Convert our ShaderStage enum to Slang's stage (currently unused - CLI tool doesn't need it)
[[maybe_unused]] static auto to_slang_stage(ShaderStage stage) -> SlangStage {
    switch (stage) {
    case ShaderStage::VERTEX: return SLANG_STAGE_VERTEX;
    case ShaderStage::FRAGMENT: return SLANG_STAGE_FRAGMENT;
    case ShaderStage::COMPUTE: return SLANG_STAGE_COMPUTE;
    case ShaderStage::GEOMETRY: return SLANG_STAGE_GEOMETRY;
    case ShaderStage::TESS_CONTROL: return SLANG_STAGE_HULL;
    case ShaderStage::TESS_EVALUATION: return SLANG_STAGE_DOMAIN;
    }
    return SLANG_STAGE_COMPUTE; // fallback
}

ShaderCompiler::ShaderCompiler(std::filesystem::path shader_dir,
                                 std::filesystem::path cache_dir)
    : shader_dir_(std::move(shader_dir)), cache_dir_(std::move(cache_dir)),
      compiler_impl_(nullptr) {
    // Create cache directory if it doesn't exist
    if (!std::filesystem::exists(cache_dir_)) {
        std::filesystem::create_directories(cache_dir_);
        LOG_INFO("Created shader cache directory: {}", cache_dir_.string());
    }
    
    // NOTE: We don't create a Slang session here anymore.
    // Using slangc CLI tool as workaround for API initialization issues.
    // The compiler_impl_ is kept as nullptr for now.
    
    LOG_INFO("Slang shader compiler initialized (using CLI tool)");
}

ShaderCompiler::~ShaderCompiler() {
    // No cleanup needed when using CLI tool
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
    LOG_INFO("Compiling shader with Slang: {}", shader_path);

    // Read source file
    auto source_result = read_file(source_path);
    if (!source_result) {
        return std::unexpected(source_result.error());
    }
    const auto& source = source_result.value();

    // Compile Slang/GLSL to SPIR-V
    auto spirv_result = compile_slang(source, stage, shader_path);
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
        LOG_INFO("Cached SPIR-V: {}", cache_path.string());
    } else {
        LOG_WARN("Failed to write shader cache: {}", cache_path.string());
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

    // Slang extension (all stages)
    if (ext == ".slang") {
        // For .slang files, we need to parse for entry point attributes
        // For now, default to compute (can be enhanced later)
        // TODO: Parse [shader("compute")] attribute from source
        return ShaderStage::COMPUTE;
    }
    
    // GLSL extensions (Slang supports GLSL input!)
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
        // Default to compute for unknown .spv files
        return ShaderStage::COMPUTE;
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

auto ShaderCompiler::compile_slang(std::string_view source, ShaderStage /*stage*/,
                                    std::string_view /*filename*/)
    -> std::expected<std::vector<u32>, ShaderError> {
    // WORKAROUND: Use slangc command-line compiler instead of API
    // The Slang C++ API has issues with session creation that cause hangs.
    // CLI tool works perfectly, so we'll use that until API is fixed.
    //
    // TODO: Investigate proper Slang API initialization sequence
    // - Might need specific module loading order
    // - Session creation hangs even with correct structure sizes
    // - Could be related to COM interface threading model
    
    // Write source to temporary file
    const auto temp_shader = cache_dir_ / "temp_shader.slang";
    std::ofstream temp_file(temp_shader);
    if (!temp_file) {
        LOG_ERROR("Failed to create temporary shader file");
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }
    temp_file << source;
    temp_file.close();
    
    // Output SPIR-V path
    const auto temp_spirv = cache_dir_ / "temp_shader.spv";
    
    // Find slangc executable (it's in the Slang bin directory)
    // We know it exists because we fetched Slang prebuilt
    std::filesystem::path slangc_path;
    
    // Try to find slangc in common locations (work backwards from cache dir)
    // Cache dir is typically build/shaders_cache
    // Slang is at build/_deps/slang-src/bin/slangc.exe
    auto search_base = cache_dir_.parent_path(); // build/
    
    std::vector<std::filesystem::path> possible_paths = {
        search_base / "_deps/slang-src/bin/slangc.exe",  // From build dir
        search_base.parent_path() / "build/_deps/slang-src/bin/slangc.exe",  // From project root
        std::filesystem::current_path() / "_deps/slang-src/bin/slangc.exe",  // From CWD
        "slangc.exe",  // In PATH (unlikely but worth trying)
    };
    
    for (const auto& path : possible_paths) {
        if (std::filesystem::exists(path)) {
            slangc_path = std::filesystem::absolute(path);
            break;
        }
    }
    
    if (slangc_path.empty()) {
        LOG_ERROR("Could not find slangc executable! Searched:");
        for (const auto& path : possible_paths) {
            LOG_ERROR("  - {}", path.string());
        }
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }
    
    LOG_INFO("Found slangc at: {}", slangc_path.string());
    
    // Build command: slangc -target spirv -profile glsl_460 -I<shader_dir> input.slang -o output.spv
    std::string command = slangc_path.string() + 
        " -target spirv -profile glsl_460" +
        " -I\"" + shader_dir_.string() + "\"" +
        " \"" + temp_shader.string() + 
        "\" -o \"" + temp_spirv.string() + "\" 2>&1";
    
    LOG_INFO("Compiling with Slang CLI: {}", command);
    
    // Execute command
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        LOG_ERROR("Failed to execute slangc");
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }
    
    // Read output
    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int exit_code = _pclose(pipe);
    
    if (exit_code != 0) {
        LOG_ERROR("Slang compilation failed:\n{}", output);
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }
    
    if (!output.empty()) {
        LOG_INFO("Slang compiler output:\n{}", output);
    }
    
    // Read compiled SPIR-V
    std::ifstream spirv_file(temp_spirv, std::ios::binary);
    if (!spirv_file) {
        LOG_ERROR("Failed to read compiled SPIR-V");
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }
    
    spirv_file.seekg(0, std::ios::end);
    const auto file_size = spirv_file.tellg();
    spirv_file.seekg(0, std::ios::beg);
    
    const auto spirv_size = static_cast<std::size_t>(file_size) / sizeof(u32);
    std::vector<u32> spirv(static_cast<std::size_t>(spirv_size));
    
    spirv_file.read(reinterpret_cast<char*>(spirv.data()), file_size);
    
    if (!spirv_file) {
        LOG_ERROR("Failed to read SPIR-V data");
        spirv_file.close();  // Close before returning
        return std::unexpected(ShaderError::COMPILATION_FAILED);
    }
    
    spirv_file.close();  // IMPORTANT: Close before trying to delete!
    
    LOG_INFO("Slang compilation successful: {} SPIR-V words ({} bytes)",
             spirv.size(), spirv.size() * 4);
    
    // Clean up temp files
    std::error_code ec;
    std::filesystem::remove(temp_shader, ec);
    if (ec) {
        LOG_WARN("Failed to remove temp shader file: {}", ec.message());
    }
    std::filesystem::remove(temp_spirv, ec);
    if (ec) {
        LOG_WARN("Failed to remove temp SPIR-V file: {}", ec.message());
    }
    
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
