// test_shader_compiler.cpp - Unit tests for Slang shader compilation

#include <luma/asset/shader_compiler.hpp>
#include <luma/core/logging.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace luma::asset;

class ShaderCompilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set log level to ERROR to keep test output clean
        luma::Logger::instance().set_level(luma::LogLevel::ERROR);
        
        // Create temporary shader and cache directories
        shader_dir_ = std::filesystem::temp_directory_path() / "luma_test_shaders";
        cache_dir_ = std::filesystem::temp_directory_path() / "luma_test_cache";
        
        std::filesystem::create_directories(shader_dir_);
        std::filesystem::create_directories(cache_dir_);
        
        // Create a simple test shader (Slang syntax)
        const std::string test_shader_source = R"(
// Simple test compute shader in Slang
[[vk::binding(0, 0)]]
RWTexture2D<float4> output_image;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    output_image[dispatch_thread_id.xy] = float4(1.0, 0.0, 0.0, 1.0);
}
)";
        
        test_shader_path_ = shader_dir_ / "test.slang";
        std::ofstream file(test_shader_path_);
        file << test_shader_source;
        file.close();
        
        // Create a bad shader (invalid Slang)
        const std::string bad_shader_source = R"(
// Invalid Slang shader (syntax error)
THIS IS NOT VALID SLANG CODE!!!
void main() { not valid }
)";
        
        bad_shader_path_ = shader_dir_ / "bad.slang";
        std::ofstream bad_file(bad_shader_path_);
        bad_file << bad_shader_source;
        bad_file.close();
    }
    
    void TearDown() override {
        // Clean up temporary directories
        // Close any file handles first by resetting paths
        test_shader_path_.clear();
        bad_shader_path_.clear();
        
        // Remove cache directory first (less likely to have locks)
        std::error_code ec;
        std::filesystem::remove_all(cache_dir_, ec);
        std::filesystem::remove_all(shader_dir_, ec);
    }
    
    std::filesystem::path shader_dir_;
    std::filesystem::path cache_dir_;
    std::filesystem::path test_shader_path_;
    std::filesystem::path bad_shader_path_;
};

TEST_F(ShaderCompilerTest, CompileSimpleShader) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    auto result = compiler.compile("test.slang");
    
    ASSERT_TRUE(result.has_value());
    
    const auto& module = result.value();
    EXPECT_FALSE(module.spirv.empty());
    EXPECT_EQ(module.stage, ShaderStage::COMPUTE);
    EXPECT_GT(module.source_hash, 0u);
}

TEST_F(ShaderCompilerTest, CacheWorks) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    // First compile (cache miss)
    auto result1 = compiler.compile("test.slang");
    ASSERT_TRUE(result1.has_value());
    const auto spirv1 = result1.value().spirv;
    
    // Second compile (cache hit)
    auto result2 = compiler.compile("test.slang");
    ASSERT_TRUE(result2.has_value());
    const auto spirv2 = result2.value().spirv;
    
    // SPIR-V should be identical
    EXPECT_EQ(spirv1, spirv2);
    
    // Cache file should exist
    const auto cache_path = compiler.get_cache_path("test.slang");
    EXPECT_TRUE(std::filesystem::exists(cache_path));
}

TEST_F(ShaderCompilerTest, InvalidShaderFails) {
    // bad.slang already created in SetUp() with syntax errors
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    auto result = compiler.compile("bad.slang");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ShaderError::COMPILATION_FAILED);
}

TEST_F(ShaderCompilerTest, MissingShaderFails) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    auto result = compiler.compile("nonexistent.slang");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ShaderError::FILE_NOT_FOUND);
}

TEST_F(ShaderCompilerTest, ForceRecompileWorks) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    // First compile
    auto result1 = compiler.compile("test.slang");
    ASSERT_TRUE(result1.has_value());
    
    // Modify the shader (change output color from red to green)
    const std::string modified_shader = R"(
RWTexture2D<float4> output_image : register(u0);

[numthreads(8, 8, 1)]
[shader("compute")]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint2 pixel = dispatch_thread_id.xy;
    output_image[pixel] = float4(0.0, 1.0, 0.0, 1.0);  // Green instead of red
}
)";
    
    std::ofstream file(test_shader_path_);
    file << modified_shader;
    file.close();
    
    // Force recompile
    auto result2 = compiler.compile("test.slang", true);
    ASSERT_TRUE(result2.has_value());
    
    // Hash should be different
    EXPECT_NE(result1.value().source_hash, result2.value().source_hash);
}

TEST_F(ShaderCompilerTest, StageDeductionWorks) {
    // Create compute shader with .slang extension
    // Note: Slang primarily supports compute shaders in this integration
    const std::string compute_shader = R"(
RWTexture2D<float4> output_image : register(u0);

[numthreads(8, 8, 1)]
[shader("compute")]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint2 pixel = dispatch_thread_id.xy;
    output_image[pixel] = float4(1.0, 0.0, 0.0, 1.0);
}
)";
    
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    const auto path = shader_dir_ / "test.slang";
    std::ofstream file(path);
    file << compute_shader;
    file.close();
    
    auto result = compiler.compile("test.slang");
    ASSERT_TRUE(result.has_value()) << "Failed to compile test.slang";
    EXPECT_EQ(result.value().stage, ShaderStage::COMPUTE);
}
