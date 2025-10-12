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
    
    Archetype* old_archetype = nullptr;
    u32 old_entity_index = 0;
    
    // Get old archetype info before removing entity
    if (meta.archetype_index != INVALID_ARCHETYPE) {
        old_archetype = archetypes_[meta.archetype_index].get();
        old_entity_index = meta.entity_index;
    }
    
    // Get new archetype
    auto& new_archetype = archetypes_[new_archetype_index];
    
    // Add entity to new archetype first (to get the new index)
    const auto new_index = new_archetype->add_entity(entity);
    
    // Copy component data from old to new archetype (before removing from old)
    if (old_archetype) {
        copy_components_to_archetype(entity, old_archetype, old_entity_index, new_archetype.get());
    }
    
    // Remove from old archetype (after copying data)
    if (old_archetype) {
        const auto swapped_entity = old_archetype->remove_entity(old_entity_index);
        
        // Update swapped entity's metadata
        if (swapped_entity != NULL_ENTITY) {
            auto& swapped_meta = entity_meta_[swapped_entity.id() - 1];  // Convert ID to index
            swapped_meta.entity_index = old_entity_index;
        }
    }
    
    // Update metadata
    meta.archetype_index = new_archetype_index;
    meta.entity_index = new_index;
}

auto World::copy_components_to_archetype(
    Entity entity,
    Archetype* old_archetype,
    u32 old_index,
    Archetype* new_archetype
) -> void {
    const auto old_sig = old_archetype->signature();
    const auto new_sig = new_archetype->signature();
    
    // For each component type, check if it exists in both archetypes
    // If so, copy the data from old to new
    
    // Helper lambda to copy a component if it exists in both archetypes
    auto copy_if_exists = [&]<typename T>(u32 comp_id) {
        const ComponentSignature comp_bit = ComponentSignature(1) << comp_id;
        
        // Check if component exists in both old and new archetypes
        if ((old_sig & comp_bit) && (new_sig & comp_bit)) {
            // Get component from old archetype
            if (const auto* old_comp = old_archetype->get_component<T>(comp_id, old_index)) {
                // Ensure new archetype has component array
                if (!new_archetype->has_component_array(comp_id)) {
                    new_archetype->add_component_array<T>(comp_id);
                }
                
                // Copy component data to new archetype
                new_archetype->add_component<T>(comp_id, *old_comp);
            }
        }
    };
    
    // Copy all known component types
    // Note: Component IDs are assigned in order of first registration
    copy_if_exists.template operator()<Transform>(World::component_id<Transform>());
    copy_if_exists.template operator()<Geometry>(World::component_id<Geometry>());
    copy_if_exists.template operator()<Material>(World::component_id<Material>());
    copy_if_exists.template operator()<Velocity>(World::component_id<Velocity>());
    copy_if_exists.template operator()<Name>(World::component_id<Name>());
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
