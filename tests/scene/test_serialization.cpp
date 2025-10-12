/**
 * @file test_serialization.cpp
 * @brief Tests for scene serialization (YAML save/load)
 * 
 * Validates scene persistence with all component types.
 * 
 * @author LukeFrankio
 * @date 2025-10-12
 */

#include <luma/scene/serialization.hpp>
#include <luma/scene/world.hpp>
#include <luma/scene/component.hpp>
#include <luma/core/math.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace luma;
using namespace luma::scene;

class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test files
        test_dir = std::filesystem::temp_directory_path() / "luma_serialization_tests";
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir);
    }

    std::filesystem::path test_dir;
};

// ========== Basic Save/Load Tests ==========

TEST_F(SerializationTest, SaveEmptyWorld) {
    World world;
    const auto path = test_dir / "empty.yaml";
    
    const auto result = save_scene(world, path);
    
    ASSERT_TRUE(result.has_value()) << "Save failed: " 
        << error_to_string(result.error());
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(SerializationTest, LoadNonExistentFile) {
    World world;
    const auto path = test_dir / "does_not_exist.yaml";
    
    const auto result = load_scene(world, path);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SerializationError::FILE_NOT_FOUND);
}

TEST_F(SerializationTest, RoundTripSingleEntity) {
    // Create world with one entity
    World world;
    const auto entity = world.create_entity();
    world.add_component<Transform>(entity, Transform{
        .position = glm::vec3(1.0f, 2.0f, 3.0f),
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f)
    });
    world.add_component<Name>(entity, Name{"TestEntity"});
    
    // Save
    const auto path = test_dir / "single_entity.yaml";
    ASSERT_TRUE(save_scene(world, path).has_value());
    
    // Load
    World loaded_world;
    ASSERT_TRUE(load_scene(loaded_world, path).has_value());
    
    // Verify
    EXPECT_EQ(loaded_world.entity_count(), 1u);
    
    // Verify Transform component
    bool found_entity = false;
    loaded_world.each<Transform>([&](Entity e, const Transform& t) {
        found_entity = true;
        EXPECT_EQ(t.position, glm::vec3(1.0f, 2.0f, 3.0f));
    });
    EXPECT_TRUE(found_entity);
}

// ========== Component Tests ==========

TEST_F(SerializationTest, SaveAndLoadGeometry) {
    World world;
    const auto entity = world.create_entity();
    world.add_component<Transform>(entity, Transform{});
    world.add_component<Geometry>(entity, Geometry::sphere(5.0f));
    
    const auto path = test_dir / "geometry.yaml";
    ASSERT_TRUE(save_scene(world, path).has_value());
    
    World loaded_world;
    ASSERT_TRUE(load_scene(loaded_world, path).has_value());
    
    bool found = false;
    loaded_world.each<Geometry>([&](Entity e, const Geometry& g) {
        found = true;
        EXPECT_EQ(g.type, SDFType::SPHERE);
        EXPECT_FLOAT_EQ(g.params.x, 5.0f);  // radius is in params.x
    });
    EXPECT_TRUE(found);
}

TEST_F(SerializationTest, SaveAndLoadMaterial) {
    World world;
    const auto entity = world.create_entity();
    world.add_component<Transform>(entity, Transform{});
    world.add_component<Material>(entity, Material{
        .base_color = glm::vec3(0.8f, 0.2f, 0.1f),
        .metallic = 0.9f,
        .roughness = 0.3f
    });
    
    const auto path = test_dir / "material.yaml";
    ASSERT_TRUE(save_scene(world, path).has_value());
    
    World loaded_world;
    ASSERT_TRUE(load_scene(loaded_world, path).has_value());
    
    bool found = false;
    loaded_world.each<Material>([&](Entity e, const Material& m) {
        found = true;
        EXPECT_EQ(m.base_color, glm::vec3(0.8f, 0.2f, 0.1f));
        EXPECT_FLOAT_EQ(m.metallic, 0.9f);
        EXPECT_FLOAT_EQ(m.roughness, 0.3f);
    });
    EXPECT_TRUE(found);
}

TEST_F(SerializationTest, SaveAndLoadVelocity) {
    World world;
    const auto entity = world.create_entity();
    world.add_component<Transform>(entity, Transform{});
    world.add_component<Velocity>(entity, Velocity{
        .linear = glm::vec3(5.0f, 10.0f, 15.0f)
    });
    
    const auto path = test_dir / "velocity.yaml";
    ASSERT_TRUE(save_scene(world, path).has_value());
    
    World loaded_world;
    ASSERT_TRUE(load_scene(loaded_world, path).has_value());
    
    bool found = false;
    loaded_world.each<Velocity>([&](Entity e, const Velocity& v) {
        found = true;
        EXPECT_EQ(v.linear, glm::vec3(5.0f, 10.0f, 15.0f));
    });
    EXPECT_TRUE(found);
}

// ========== Multi-Entity Tests ==========

TEST_F(SerializationTest, SaveAndLoadMultipleEntities) {
    World world;
    
    // Entity 1: Ball
    const auto ball = world.create_entity();
    world.add_component<Transform>(ball, Transform{
        .position = glm::vec3(0.0f, 5.0f, 0.0f)
    });
    world.add_component<Geometry>(ball, Geometry::sphere(1.0f));
    world.add_component<Name>(ball, Name{"Ball"});
    
    // Entity 2: Paddle
    const auto paddle = world.create_entity();
    world.add_component<Transform>(paddle, Transform{
        .position = glm::vec3(-10.0f, 0.0f, 0.0f)
    });
    world.add_component<Geometry>(paddle, Geometry::box(glm::vec3(0.5f, 3.0f, 0.5f), 0.1f));
    world.add_component<Name>(paddle, Name{"LeftPaddle"});
    
    const auto path = test_dir / "multi_entity.yaml";
    ASSERT_TRUE(save_scene(world, path).has_value());
    
    World loaded_world;
    ASSERT_TRUE(load_scene(loaded_world, path).has_value());
    
    EXPECT_EQ(loaded_world.entity_count(), 2u);
}

// ========== Error Handling Tests ==========

TEST_F(SerializationTest, LoadInvalidYAML) {
    const auto path = test_dir / "invalid.yaml";
    
    // Write invalid YAML
    std::ofstream file(path);
    file << "this is not valid yaml: [unclosed bracket\n";
    file.close();
    
    World world;
    const auto result = load_scene(world, path);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SerializationError::YAML_PARSE_ERROR);
}

TEST_F(SerializationTest, ErrorToStringReturnsValidStrings) {
    // error_to_string returns const char*, not std::string
    const char* file_not_found = error_to_string(SerializationError::FILE_NOT_FOUND);
    const char* file_open = error_to_string(SerializationError::FILE_OPEN_FAILED);
    const char* parse_error = error_to_string(SerializationError::YAML_PARSE_ERROR);
    
    EXPECT_NE(file_not_found, nullptr);
    EXPECT_NE(file_open, nullptr);
    EXPECT_NE(parse_error, nullptr);
    
    // Check that strings are not empty
    EXPECT_GT(std::strlen(file_not_found), 0u);
    EXPECT_GT(std::strlen(file_open), 0u);
    EXPECT_GT(std::strlen(parse_error), 0u);
}

// ========== Integration Tests ==========

TEST_F(SerializationTest, PongSceneRoundTrip) {
    // Create Pong scene from our example YAML structure
    World world;
    
    // Ball
    const auto ball = world.create_entity();
    world.add_component<Transform>(ball, Transform{
        .position = glm::vec3(0.0f, 0.0f, 0.0f)
    });
    world.add_component<Geometry>(ball, Geometry::sphere(0.5f));
    world.add_component<Material>(ball, Material{
        .base_color = glm::vec3(1.0f),
        .metallic = 0.0f,
        .roughness = 0.5f,
        .emissive_color = glm::vec3(0.1f)
    });
    world.add_component<Velocity>(ball, Velocity{
        .linear = glm::vec3(5.0f, 3.0f, 0.0f)
    });
    world.add_component<Name>(ball, Name{"Ball"});
    
    // Left Paddle
    const auto left_paddle = world.create_entity();
    world.add_component<Transform>(left_paddle, Transform{
        .position = glm::vec3(-15.0f, 0.0f, 0.0f)
    });
    world.add_component<Geometry>(left_paddle, Geometry::box(glm::vec3(0.5f, 4.0f, 0.5f), 0.2f));
    world.add_component<Material>(left_paddle, Material::metal(glm::vec3(0.2f, 0.6f, 1.0f), 0.3f));
    world.add_component<Name>(left_paddle, Name{"LeftPaddle"});
    
    // Right Paddle
    const auto right_paddle = world.create_entity();
    world.add_component<Transform>(right_paddle, Transform{
        .position = glm::vec3(15.0f, 0.0f, 0.0f)
    });
    world.add_component<Geometry>(right_paddle, Geometry::box(glm::vec3(0.5f, 4.0f, 0.5f), 0.2f));
    world.add_component<Material>(right_paddle, Material::metal(glm::vec3(1.0f, 0.2f, 0.2f), 0.3f));
    world.add_component<Name>(right_paddle, Name{"RightPaddle"});
    
    const auto path = test_dir / "pong_scene.yaml";
    ASSERT_TRUE(save_scene(world, path).has_value());
    
    World loaded_world;
    ASSERT_TRUE(load_scene(loaded_world, path).has_value());
    
    EXPECT_EQ(loaded_world.entity_count(), 3u);
    
    // Verify ball has velocity component
    bool ball_found = false;
    loaded_world.each<Name, Velocity>([&](Entity e, const Name& n, const Velocity& v) {
        if (n.value == "Ball") {
            ball_found = true;
            EXPECT_EQ(v.linear, glm::vec3(5.0f, 3.0f, 0.0f));
        }
    });
    EXPECT_TRUE(ball_found);
}
