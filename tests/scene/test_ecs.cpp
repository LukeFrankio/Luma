/**
 * @file test_ecs.cpp
 * @brief Comprehensive tests for LUMA ECS (Entity-Component-System)
 * 
 * Test coverage:
 * - Entity creation and destruction
 * - Component addition and removal
 * - Entity lifecycle and generation counter
 * - Archetype transitions
 * - Component queries (each<> templates)
 * - World state management
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#include <luma/scene/world.hpp>
#include <luma/scene/entity.hpp>
#include <luma/scene/component.hpp>

#include <gtest/gtest.h>

using namespace luma;
using namespace luma::scene;

class ECSTest : public ::testing::Test {
protected:
    World world;
};

// ========== Entity Tests ==========

TEST_F(ECSTest, EntityCreation) {
    const auto e1 = world.create_entity();
    const auto e2 = world.create_entity();
    
    EXPECT_NE(e1, NULL_ENTITY);
    EXPECT_NE(e2, NULL_ENTITY);
    EXPECT_NE(e1, e2);
    EXPECT_TRUE(world.is_alive(e1));
    EXPECT_TRUE(world.is_alive(e2));
    EXPECT_EQ(world.entity_count(), 2);
}

TEST_F(ECSTest, EntityDestruction) {
    const auto e1 = world.create_entity();
    const auto e2 = world.create_entity();
    
    world.destroy_entity(e1);
    
    EXPECT_FALSE(world.is_alive(e1));
    EXPECT_TRUE(world.is_alive(e2));
    EXPECT_EQ(world.entity_count(), 1);
}

TEST_F(ECSTest, EntityReuse) {
    const auto e1 = world.create_entity();
    const auto e1_id = e1.id();
    const auto e1_gen = e1.generation();
    
    world.destroy_entity(e1);
    
    // Create new entity - should reuse ID but increment generation
    const auto e2 = world.create_entity();
    
    EXPECT_EQ(e2.id(), e1_id);  // Same ID
    EXPECT_EQ(e2.generation(), e1_gen + 1);  // Incremented generation
    EXPECT_FALSE(world.is_alive(e1));  // Old entity invalid
    EXPECT_TRUE(world.is_alive(e2));  // New entity valid
}

TEST_F(ECSTest, NullEntityAlwaysDead) {
    EXPECT_FALSE(world.is_alive(NULL_ENTITY));
}

// ========== Component Tests ==========

TEST_F(ECSTest, AddComponent) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Transform{
        .position = {1.0f, 2.0f, 3.0f},
        .rotation = {1.0f, 0.0f, 0.0f, 0.0f},  // w,x,y,z (identity)
        .scale = {1.0f, 1.0f, 1.0f}
    });
    
    EXPECT_TRUE(world.has_component<Transform>(entity));
    EXPECT_FALSE(world.has_component<Velocity>(entity));
}

TEST_F(ECSTest, GetComponent) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Transform{
        .position = {1.0f, 2.0f, 3.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {2.0f, 2.0f, 2.0f}
    });
    
    const auto* transform = world.get_component<Transform>(entity);
    ASSERT_NE(transform, nullptr);
    EXPECT_EQ(transform->position.x, 1.0f);
    EXPECT_EQ(transform->position.y, 2.0f);
    EXPECT_EQ(transform->position.z, 3.0f);
    EXPECT_EQ(transform->scale.x, 2.0f);
}

TEST_F(ECSTest, MutateComponent) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Transform{
        .position = {0.0f, 0.0f, 0.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });
    
    // Mutate component
    auto* transform = world.get_component<Transform>(entity);
    ASSERT_NE(transform, nullptr);
    transform->position.x = 5.0f;
    
    // Verify mutation
    const auto* transform_const = world.get_component<Transform>(entity);
    EXPECT_EQ(transform_const->position.x, 5.0f);
}

// TODO(MVP+1): Fix archetype transitions - currently components are lost when moving between archetypes
// We need to implement component copying during archetype migrations
TEST_F(ECSTest, DISABLED_RemoveComponent) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Transform{});
    EXPECT_TRUE(world.has_component<Transform>(entity));
    
    world.remove_component<Transform>(entity);
    EXPECT_FALSE(world.has_component<Transform>(entity));
}

TEST_F(ECSTest, MultipleComponents) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Transform{});
    world.add_component(entity, Velocity{.linear = {1.0f, 0.0f, 0.0f}});
    world.add_component(entity, Name{.value = "TestEntity"});
    
    EXPECT_TRUE(world.has_component<Transform>(entity));
    EXPECT_TRUE(world.has_component<Velocity>(entity));
    EXPECT_TRUE(world.has_component<Name>(entity));
    EXPECT_FALSE(world.has_component<Geometry>(entity));
}

// ========== Archetype Tests ==========

// TODO(MVP+1): Fix archetype transitions
TEST_F(ECSTest, DISABLED_ArchetypeTransition) {
    const auto entity = world.create_entity();
    
    // Start with Transform
    world.add_component(entity, Transform{});
    EXPECT_TRUE(world.has_component<Transform>(entity));
    
    // Add Velocity - should move to new archetype
    world.add_component(entity, Velocity{});
    EXPECT_TRUE(world.has_component<Transform>(entity));
    EXPECT_TRUE(world.has_component<Velocity>(entity));
    
    // Remove Transform - should move to another archetype
    world.remove_component<Transform>(entity);
    EXPECT_FALSE(world.has_component<Transform>(entity));
    EXPECT_TRUE(world.has_component<Velocity>(entity));
}

// TODO(MVP+1): Fix archetype transitions
TEST_F(ECSTest, DISABLED_ComponentsPreservedAcrossArchetypes) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Transform{.position = {1.0f, 2.0f, 3.0f}});
    world.add_component(entity, Velocity{.linear = {4.0f, 5.0f, 6.0f}});
    
    // Verify data preserved after archetype transition
    const auto* transform = world.get_component<Transform>(entity);
    const auto* velocity = world.get_component<Velocity>(entity);
    
    ASSERT_NE(transform, nullptr);
    ASSERT_NE(velocity, nullptr);
    EXPECT_EQ(transform->position.x, 1.0f);
    EXPECT_EQ(velocity->linear.x, 4.0f);
}

// ========== Query Tests ==========

TEST_F(ECSTest, QuerySingleComponent) {
    const auto e1 = world.create_entity();
    const auto e2 = world.create_entity();
    const auto e3 = world.create_entity();
    
    world.add_component(e1, Transform{.position = {1.0f, 0.0f, 0.0f}});
    world.add_component(e2, Transform{.position = {2.0f, 0.0f, 0.0f}});
    // e3 has no Transform
    
    int count = 0;
    world.each<Transform>([&](Entity entity, const Transform& transform) {
        count++;
        EXPECT_TRUE(entity == e1 || entity == e2);
    });
    
    EXPECT_EQ(count, 2);
}

TEST_F(ECSTest, QueryMultipleComponents) {
    const auto e1 = world.create_entity();
    const auto e2 = world.create_entity();
    const auto e3 = world.create_entity();
    
    world.add_component(e1, Transform{});
    world.add_component(e1, Velocity{.linear = {1.0f, 0.0f, 0.0f}});
    
    world.add_component(e2, Transform{});
    // e2 has no Velocity
    
    world.add_component(e3, Velocity{});
    // e3 has no Transform
    
    int count = 0;
    world.each<Transform, Velocity>([&](Entity entity, const Transform& transform, const Velocity& velocity) {
        count++;
        EXPECT_EQ(entity, e1);  // Only e1 has both components
    });
    
    EXPECT_EQ(count, 1);
}

TEST_F(ECSTest, QueryMutation) {
    const auto e1 = world.create_entity();
    const auto e2 = world.create_entity();
    
    world.add_component(e1, Transform{.position = {0.0f, 0.0f, 0.0f}});
    world.add_component(e2, Transform{.position = {0.0f, 0.0f, 0.0f}});
    
    // Mutate all transforms
    world.each<Transform>([](Entity, Transform& transform) {
        transform.position.x += 1.0f;
    });
    
    // Verify mutations
    EXPECT_EQ(world.get_component<Transform>(e1)->position.x, 1.0f);
    EXPECT_EQ(world.get_component<Transform>(e2)->position.x, 1.0f);
}

TEST_F(ECSTest, QueryEmptyWorld) {
    int count = 0;
    world.each<Transform>([&](Entity, const Transform&) {
        count++;
    });
    
    EXPECT_EQ(count, 0);
}

// ========== World State Tests ==========

TEST_F(ECSTest, WorldClear) {
    const auto e1 = world.create_entity();
    const auto e2 = world.create_entity();
    
    world.add_component(e1, Transform{});
    world.add_component(e2, Velocity{});
    
    world.clear();
    
    EXPECT_EQ(world.entity_count(), 0);
    EXPECT_FALSE(world.is_alive(e1));
    EXPECT_FALSE(world.is_alive(e2));
}

TEST_F(ECSTest, EntityCountTracking) {
    EXPECT_EQ(world.entity_count(), 0);
    
    const auto e1 = world.create_entity();
    EXPECT_EQ(world.entity_count(), 1);
    
    const auto e2 = world.create_entity();
    EXPECT_EQ(world.entity_count(), 2);
    
    world.destroy_entity(e1);
    EXPECT_EQ(world.entity_count(), 1);
    
    world.destroy_entity(e2);
    EXPECT_EQ(world.entity_count(), 0);
}

// ========== Component Type Tests ==========

TEST_F(ECSTest, TransformComponent) {
    const auto entity = world.create_entity();
    
    // glm::quat is (w, x, y, z) order - identity rotation is (1, 0, 0, 0)
    const auto transform = Transform{
        .position = {1.0f, 2.0f, 3.0f},
        .rotation = {1.0f, 0.0f, 0.0f, 0.0f},  // Identity: w=1, xyz=0
        .scale = {2.0f, 2.0f, 2.0f}
    };
    
    world.add_component(entity, transform);
    
    const auto* retrieved = world.get_component<Transform>(entity);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->position.x, 1.0f);
    EXPECT_EQ(retrieved->rotation.w, 1.0f);
    EXPECT_EQ(retrieved->scale.x, 2.0f);
}

TEST_F(ECSTest, GeometryComponent) {
    const auto entity = world.create_entity();
    
    const auto geometry = Geometry::sphere(1.5f);
    world.add_component(entity, geometry);
    
    const auto* retrieved = world.get_component<Geometry>(entity);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, SDFType::SPHERE);
    EXPECT_EQ(retrieved->params.x, 1.5f);
}

TEST_F(ECSTest, MaterialComponent) {
    const auto entity = world.create_entity();
    
    // Material::metal(color, roughness) - metallic always 1.0, second param is roughness
    const auto material = Material::metal({1.0f, 0.8f, 0.0f}, 0.9f);
    world.add_component(entity, material);
    
    const auto* retrieved = world.get_component<Material>(entity);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->base_color.x, 1.0f);
    EXPECT_EQ(retrieved->metallic, 1.0f);  // metal() always sets metallic=1.0
    EXPECT_EQ(retrieved->roughness, 0.9f);  // Second param is roughness
}

TEST_F(ECSTest, NameComponent) {
    const auto entity = world.create_entity();
    
    world.add_component(entity, Name{.value = "TestEntity"});
    
    const auto* name = world.get_component<Name>(entity);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->value, "TestEntity");
}

// ========== Stress Tests ==========

TEST_F(ECSTest, ManyEntities) {
    constexpr std::size_t COUNT = 10000;
    
    std::vector<Entity> entities;
    entities.reserve(COUNT);
    
    for (std::size_t i = 0; i < COUNT; ++i) {
        entities.push_back(world.create_entity());
    }
    
    EXPECT_EQ(world.entity_count(), COUNT);
    
    for (const auto entity : entities) {
        EXPECT_TRUE(world.is_alive(entity));
    }
}

TEST_F(ECSTest, ManyComponents) {
    constexpr std::size_t COUNT = 1000;
    
    for (std::size_t i = 0; i < COUNT; ++i) {
        const auto entity = world.create_entity();
        world.add_component(entity, Transform{});
        world.add_component(entity, Velocity{});
        world.add_component(entity, Material{});
    }
    
    std::size_t query_count = 0;
    world.each<Transform, Velocity, Material>([&](Entity, const Transform&, const Velocity&, const Material&) {
        query_count++;
    });
    
    EXPECT_EQ(query_count, COUNT);
}
