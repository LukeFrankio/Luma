// test_shader_compiler.cpp - Unit tests for GLSL shader compilation

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
        
        // Create a simple test shader
        const std::string test_shader_source = R"(
#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 0, rgba8) uniform writeonly image2D output_image;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    imageStore(output_image, pixel, vec4(1.0, 0.0, 0.0, 1.0));
}
)";
        
        test_shader_path_ = shader_dir_ / "test.comp";
        std::ofstream file(test_shader_path_);
        file << test_shader_source;
        file.close();
    }
    
    void TearDown() override {
        // Clean up temporary directories
        std::filesystem::remove_all(shader_dir_);
        std::filesystem::remove_all(cache_dir_);
    }
    
    std::filesystem::path shader_dir_;
    std::filesystem::path cache_dir_;
    std::filesystem::path test_shader_path_;
};

TEST_F(ShaderCompilerTest, CompileSimpleShader) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    auto result = compiler.compile("test.comp");
    
    ASSERT_TRUE(result.has_value());
    
    const auto& module = result.value();
    EXPECT_FALSE(module.spirv.empty());
    EXPECT_EQ(module.stage, ShaderStage::COMPUTE);
    EXPECT_GT(module.source_hash, 0u);
}

TEST_F(ShaderCompilerTest, CacheWorks) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    // First compile (cache miss)
    auto result1 = compiler.compile("test.comp");
    ASSERT_TRUE(result1.has_value());
    const auto spirv1 = result1.value().spirv;
    
    // Second compile (cache hit)
    auto result2 = compiler.compile("test.comp");
    ASSERT_TRUE(result2.has_value());
    const auto spirv2 = result2.value().spirv;
    
    // SPIR-V should be identical
    EXPECT_EQ(spirv1, spirv2);
    
    // Cache file should exist
    const auto cache_path = compiler.get_cache_path("test.comp");
    EXPECT_TRUE(std::filesystem::exists(cache_path));
}

TEST_F(ShaderCompilerTest, InvalidShaderFails) {
    // Create a shader with syntax errors
    const std::string bad_shader = R"(
#version 460
THIS IS NOT VALID GLSL CODE
)";
    
    const auto bad_shader_path = shader_dir_ / "bad.comp";
    std::ofstream file(bad_shader_path);
    file << bad_shader;
    file.close();
    
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    auto result = compiler.compile("bad.comp");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ShaderError::COMPILATION_FAILED);
}

TEST_F(ShaderCompilerTest, MissingShaderFails) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    auto result = compiler.compile("nonexistent.comp");
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ShaderError::FILE_NOT_FOUND);
}

TEST_F(ShaderCompilerTest, ForceRecompileWorks) {
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    // First compile
    auto result1 = compiler.compile("test.comp");
    ASSERT_TRUE(result1.has_value());
    
    // Modify the shader (change source)
    const std::string modified_shader = R"(
#version 460
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(binding = 0, rgba8) uniform writeonly image2D output_image;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    imageStore(output_image, pixel, vec4(0.0, 1.0, 0.0, 1.0));
}
)";
    
    std::ofstream file(test_shader_path_);
    file << modified_shader;
    file.close();
    
    // Force recompile
    auto result2 = compiler.compile("test.comp", true);
    ASSERT_TRUE(result2.has_value());
    
    // Hash should be different
    EXPECT_NE(result1.value().source_hash, result2.value().source_hash);
}

TEST_F(ShaderCompilerTest, StageDeductionWorks) {
    // Create shaders with different extensions
    const std::string shader_source = "#version 460\nvoid main() {}";
    
    struct TestCase {
        const char* filename;
        ShaderStage expected_stage;
    };
    
    const TestCase test_cases[] = {
        {"test.vert", ShaderStage::VERTEX},
        {"test.frag", ShaderStage::FRAGMENT},
        {"test.comp", ShaderStage::COMPUTE},
        {"test.geom", ShaderStage::GEOMETRY},
        {"test.tesc", ShaderStage::TESS_CONTROL},
        {"test.tese", ShaderStage::TESS_EVALUATION},
    };
    
    ShaderCompiler compiler(shader_dir_, cache_dir_);
    
    for (const auto& test : test_cases) {
        const auto path = shader_dir_ / test.filename;
        std::ofstream file(path);
        file << shader_source;
        file.close();
        
        auto result = compiler.compile(test.filename);
        ASSERT_TRUE(result.has_value()) << "Failed to compile " << test.filename;
        EXPECT_EQ(result.value().stage, test.expected_stage);
    }
}
