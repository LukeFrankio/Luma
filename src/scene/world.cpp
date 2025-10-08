/**
 * @file world.cpp
 * @brief World implementation
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#include <luma/scene/world.hpp>
#include <luma/scene/archetype.hpp>

#include <algorithm>

namespace luma::scene {

// Component type registry (global)
// Maps component types to unique IDs
namespace {
    static u32 g_next_component_id = 0;
    
    template<typename T>
    auto get_component_id() -> u32 {
        static const u32 id = g_next_component_id++;
        return id;
    }
}

World::World() = default;
World::~World() = default;

auto World::create_entity() -> Entity {
    // Try to reuse destroyed entity ID
    if (!free_entities_.empty()) {
        const auto id = free_entities_.back();
        free_entities_.pop_back();
        
        // Get current generation (already incremented when entity was destroyed)
        auto& meta = entity_meta_[id - 1];  // Convert ID to index (IDs start at 1)
        meta.archetype_index = INVALID_ARCHETYPE;
        meta.entity_index = 0;
        entity_count_++;
        
        return Entity::create(id, meta.generation);
    }
    
    // Create new entity (IDs start at 1, 0 is reserved for NULL_ENTITY)
    // Vector index = ID - 1 (so entity ID 1 is at index 0)
    const auto id = static_cast<u32>(entity_meta_.size() + 1);
    entity_meta_.push_back(EntityMeta{0, INVALID_ARCHETYPE, 0});
    entity_count_++;
    
    return Entity::create(id, 0);
}

auto World::destroy_entity(Entity entity) -> void {
    if (!is_alive(entity)) {
        return;
    }
    
    const auto id = entity.id();
    auto& meta = entity_meta_[id - 1];  // Convert ID to index (IDs start at 1)
    
    // Remove from archetype (if in one)
    if (meta.archetype_index != INVALID_ARCHETYPE) {
        auto& archetype = archetypes_[meta.archetype_index];
        const auto swapped_entity = archetype->remove_entity(meta.entity_index);
        
        // Update swapped entity's metadata
        if (swapped_entity != NULL_ENTITY) {
            auto& swapped_meta = entity_meta_[swapped_entity.id() - 1];  // Convert ID to index
            swapped_meta.entity_index = meta.entity_index;
        }
    }
    
    // Mark entity as destroyed (increment generation to invalidate old handles)
    meta.generation++;
    meta.archetype_index = INVALID_ARCHETYPE;
    free_entities_.push_back(id);
    entity_count_--;
}

auto World::is_alive(Entity entity) const -> bool {
    if (entity == NULL_ENTITY || entity.id() == 0) {
        return false;
    }
    
    const auto id = entity.id();
    if (id > entity_meta_.size()) {  // ID starts at 1, so > not >=
        return false;
    }
    
    // Check generation matches (validates this specific entity version)
    return entity_meta_[id - 1].generation == entity.generation();  // Convert ID to index
}

auto World::entity_count() const -> std::size_t {
    return entity_count_;
}

auto World::clear() -> void {
    archetypes_.clear();
    archetype_map_.clear();
    entity_meta_.clear();
    free_entities_.clear();
    entity_count_ = 0;
}

auto World::get_or_create_archetype(ComponentSignature signature) -> u32 {
    // Check if archetype already exists
    if (auto it = archetype_map_.find(signature); it != archetype_map_.end()) {
        return it->second;
    }
    
    // Create new archetype
    const auto index = static_cast<u32>(archetypes_.size());
    archetypes_.push_back(std::make_unique<Archetype>(signature));
    archetype_map_[signature] = index;
    
    return index;
}

auto World::move_entity_to_archetype(Entity entity, u32 new_archetype_index) -> void {
    if (!is_alive(entity)) {
        return;
    }
    
    const auto id = entity.id();
    auto& meta = entity_meta_[id - 1];  // Convert ID to index (IDs start at 1)
    
    // Remove from old archetype (if in one)
    if (meta.archetype_index != INVALID_ARCHETYPE) {
        auto& old_archetype = archetypes_[meta.archetype_index];
        const auto swapped_entity = old_archetype->remove_entity(meta.entity_index);
        
        // Update swapped entity's metadata
        if (swapped_entity != NULL_ENTITY) {
            auto& swapped_meta = entity_meta_[swapped_entity.id() - 1];  // Convert ID to index
            swapped_meta.entity_index = meta.entity_index;
        }
    }
    
    // Add to new archetype
    auto& new_archetype = archetypes_[new_archetype_index];
    const auto new_index = new_archetype->add_entity(entity);
    
    // Update metadata
    meta.archetype_index = new_archetype_index;
    meta.entity_index = new_index;
}

// Template instantiations for all component types
template auto World::add_component(Entity, Transform) -> void;
template auto World::add_component(Entity, Geometry) -> void;
template auto World::add_component(Entity, Material) -> void;
template auto World::add_component(Entity, Velocity) -> void;
template auto World::add_component(Entity, Name) -> void;

template auto World::remove_component<Transform>(Entity) -> void;
template auto World::remove_component<Geometry>(Entity) -> void;
template auto World::remove_component<Material>(Entity) -> void;
template auto World::remove_component<Velocity>(Entity) -> void;
template auto World::remove_component<Name>(Entity) -> void;

template auto World::has_component<Transform>(Entity) const -> bool;
template auto World::has_component<Geometry>(Entity) const -> bool;
template auto World::has_component<Material>(Entity) const -> bool;
template auto World::has_component<Velocity>(Entity) const -> bool;
template auto World::has_component<Name>(Entity) const -> bool;

template auto World::get_component<Transform>(Entity) const -> const Transform*;
template auto World::get_component<Geometry>(Entity) const -> const Geometry*;
template auto World::get_component<Material>(Entity) const -> const Material*;
template auto World::get_component<Velocity>(Entity) const -> const Velocity*;
template auto World::get_component<Name>(Entity) const -> const Name*;

template auto World::get_component<Transform>(Entity) -> Transform*;
template auto World::get_component<Geometry>(Entity) -> Geometry*;
template auto World::get_component<Material>(Entity) -> Material*;
template auto World::get_component<Velocity>(Entity) -> Velocity*;
template auto World::get_component<Name>(Entity) -> Name*;

} // namespace luma::scene
