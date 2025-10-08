/**
 * @file world.hpp
 * @brief ECS World container - the heart of LUMA's entity-component system
 * 
 * The World manages all entities and their components using an archetype-based
 * storage system optimized for cache-friendly iteration and parallel queries.
 * 
 * **Architecture**:
 * - **Archetype Storage**: Entities grouped by component signature
 * - **Sparse Sets**: Fast entity → archetype lookup (O(1))
 * - **SoA Layout**: Components stored contiguously for cache efficiency
 * - **Immutable Entities**: Entity IDs never change (generation for safety)
 * 
 * ✨ FUNCTIONAL DESIGN ✨
 * - Entity creation/destruction are pure operations (return new state)
 * - Queries return immutable spans (read-only views)
 * - Systems operate on query results (pure functions)
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/scene/component.hpp>
#include <luma/scene/entity.hpp>
#include <luma/scene/archetype.hpp>

#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace luma::scene {

// Constants
constexpr u32 INVALID_ARCHETYPE = static_cast<u32>(-1);

/**
 * @brief Entity metadata (internal bookkeeping)
 * 
 * Tracks which archetype an entity belongs to and its index within that archetype.
 */
struct EntityMeta {
    u8 generation{0};  ///< Generation counter (for stale handle detection)
    u32 archetype_index{INVALID_ARCHETYPE};  ///< Index of archetype in archetypes_ vector
    u32 entity_index{0};  ///< Index within archetype's component arrays
};

/**
 * @brief ECS World - container for all entities and components
 * 
 * The World is the main interface for entity-component operations:
 * - Create/destroy entities
 * - Add/remove/query components
 * - Serialize/deserialize scenes
 * 
 * **Usage Example**:
 * @code
 * World world;
 * 
 * // Create entity with components
 * auto entity = world.create_entity();
 * world.add_component(entity, Transform{vec3(0, 0, 0)});
 * world.add_component(entity, Velocity{vec3(1, 0, 0)});
 * 
 * // Query entities
 * world.each<Transform, Velocity>([](Entity e, const Transform& t, const Velocity& v) {
 *     // Update logic here
 * });
 * @endcode
 * 
 * ⚠️ IMPURE CLASS (manages mutable state)
 * 
 * @note World is NOT thread-safe by default (wrap in mutex if needed)
 */
class World {
public:
    /**
     * @brief Construct empty world
     * 
     * Reserves space for initial entities (reduces allocations).
     */
    World();
    
    /**
     * @brief Destructor (cleans up all entities and archetypes)
     * 
     * ⚠️ IMPURE (deallocates memory)
     */
    ~World();
    
    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;
    
    /**
     * @brief Create new entity (with no components initially)
     * 
     * ⚠️ IMPURE (modifies world state)
     * 
     * Entities start with no components and must have components added
     * via add_component() to be useful.
     * 
     * @return Entity handle (ID + generation)
     */
    [[nodiscard]] auto create_entity() -> Entity;
    
    /**
     * @brief Destroy entity and remove all its components
     * 
     * ⚠️ IMPURE (modifies world state)
     * 
     * The entity ID is recycled (generation incremented) and may be reused
     * for future entities. Existing Entity handles become stale.
     * 
     * @param entity Entity to destroy
     */
    auto destroy_entity(Entity entity) -> void;
    
    /**
     * @brief Check if entity is alive (valid and not destroyed)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param entity Entity to check
     * @return true if entity exists and generation matches
     */
    [[nodiscard]] auto is_alive(Entity entity) const -> bool;
    
    /**
     * @brief Add component to entity
     * 
     * ⚠️ IMPURE (modifies world state)
     * 
     * If entity already has this component type, it is replaced.
     * Entity may move to different archetype (signature changes).
     * 
     * @tparam T Component type (must be one of the defined components)
     * @param entity Target entity
     * @param component Component data
     */
    template<typename T>
    auto add_component(Entity entity, T component) -> void;
    
    /**
     * @brief Remove component from entity
     * 
     * ⚠️ IMPURE (modifies world state)
     * 
     * Entity moves to different archetype (signature changes).
     * If entity doesn't have this component, does nothing.
     * 
     * @tparam T Component type
     * @param entity Target entity
     */
    template<typename T>
    auto remove_component(Entity entity) -> void;
    
    /**
     * @brief Check if entity has component
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @tparam T Component type
     * @param entity Target entity
     * @return true if entity has component of type T
     */
    template<typename T>
    [[nodiscard]] auto has_component(Entity entity) const -> bool;
    
    /**
     * @brief Get component from entity (read-only)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @tparam T Component type
     * @param entity Target entity
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    [[nodiscard]] auto get_component(Entity entity) const -> const T*;
    
    /**
     * @brief Get component from entity (mutable)
     * 
     * ⚠️ IMPURE (allows mutation)
     * 
     * @tparam T Component type
     * @param entity Target entity
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    [[nodiscard]] auto get_component(Entity entity) -> T*;
    
    /**
     * @brief Query entities with specific components (read-only)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * Iterates over all entities that have ALL specified component types.
     * Callback receives entity + references to each component.
     * 
     * Example:
     * @code
     * world.each<Transform, Velocity>([](Entity e, const Transform& t, const Velocity& v) {
     *     // Process entities with both Transform and Velocity
     * });
     * @endcode
     * 
     * @tparam Components Component types to query
     * @tparam Func Callback function type
     * @param func Callback invoked for each matching entity
     */
    template<typename... Components, typename Func>
    auto each(Func&& func) const -> void;
    
    /**
     * @brief Query entities with specific components (mutable)
     * 
     * ⚠️ IMPURE (allows mutation)
     * 
     * Same as each() but allows component mutation.
     * 
     * @tparam Components Component types to query
     * @tparam Func Callback function type
     * @param func Callback invoked for each matching entity
     */
    template<typename... Components, typename Func>
    auto each(Func&& func) -> void;
    
    /**
     * @brief Get total number of entities (alive)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Count of alive entities
     */
    [[nodiscard]] auto entity_count() const -> std::size_t;
    
    /**
     * @brief Clear all entities and components
     * 
     * ⚠️ IMPURE (destroys all state)
     * 
     * Resets world to empty state (no entities).
     */
    auto clear() -> void;

private:
    /**
     * @brief Get or create archetype for given component signature
     * 
     * ⚠️ IMPURE (may allocate new archetype)
     * 
     * @param signature Component signature bitset
     * @return Index of archetype in archetypes_ vector
     */
    auto get_or_create_archetype(ComponentSignature signature) -> u32;
    
    /**
     * @brief Move entity to different archetype
     * 
     * ⚠️ IMPURE (modifies archetypes)
     * 
     * @param entity Entity to move
     * @param new_archetype_index Target archetype index
     */
    auto move_entity_to_archetype(Entity entity, u32 new_archetype_index) -> void;
    
    /**
     * @brief Compute component signature for set of types
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @tparam Components Component types
     * @return Bitset signature
     */
    template<typename... Components>
    [[nodiscard]] static constexpr auto compute_signature() -> ComponentSignature {
        ComponentSignature sig = 0;
        ((sig |= (ComponentSignature(1) << component_id<Components>())), ...);
        return sig;
    }
    
    /**
     * @brief Get component type ID (unique per type)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * Uses template specialization to assign unique IDs.
     * 
     * @tparam T Component type
     * @return Component type ID (0-63)
     */
    template<typename T>
    [[nodiscard]] static constexpr auto component_id() -> u32 {
        // Use simple if-else chain for type IDs
        if constexpr (std::is_same_v<T, Transform>) return 0;
        else if constexpr (std::is_same_v<T, Geometry>) return 1;
        else if constexpr (std::is_same_v<T, Material>) return 2;
        else if constexpr (std::is_same_v<T, Velocity>) return 3;
        else if constexpr (std::is_same_v<T, Name>) return 4;
        else static_assert(false, "Unknown component type");
    }
    
    std::vector<EntityMeta> entity_meta_;  ///< Entity metadata (indexed by entity ID)
    std::vector<u32> free_entities_;  ///< Free list for recycled entity IDs
    std::size_t entity_count_{0};  ///< Number of alive entities
    
    std::vector<std::unique_ptr<Archetype>> archetypes_;  ///< Archetype storage
    std::unordered_map<ComponentSignature, u32> archetype_map_;  ///< Signature -> archetype index
};

// ========== Template Method Implementations ==========

template<typename T>
auto World::add_component(Entity entity, T component) -> void {
    if (!is_alive(entity)) {
        return;
    }
    
    const auto id = entity.id();
    auto& meta = entity_meta_[id - 1];  // Convert ID to index (IDs start at 1)
    
    // Compute new signature with added component
    ComponentSignature old_sig = 0;
    if (meta.archetype_index != INVALID_ARCHETYPE) {
        old_sig = archetypes_[meta.archetype_index]->signature();
    }
    const ComponentSignature new_sig = old_sig | (ComponentSignature(1) << component_id<T>());
    
    // Get or create archetype for new signature
    const auto new_archetype_idx = get_or_create_archetype(new_sig);
    
    // Ensure archetype has component array for T
    auto& new_archetype = archetypes_[new_archetype_idx];
    if (!new_archetype->has_component_array(component_id<T>())) {
        new_archetype->add_component_array<T>(component_id<T>());
    }
    
    // Move entity to new archetype (if needed)
    if (meta.archetype_index != new_archetype_idx) {
        move_entity_to_archetype(entity, new_archetype_idx);
    }
    
    // Add component data
    new_archetype->add_component<T>(component_id<T>(), std::move(component));
}

template<typename T>
auto World::remove_component(Entity entity) -> void {
    if (!is_alive(entity) || !has_component<T>(entity)) {
        return;
    }
    
    const auto id = entity.id();
    auto& meta = entity_meta_[id - 1];  // Convert ID to index (IDs start at 1)
    
    // If entity has no archetype, nothing to remove
    if (meta.archetype_index == INVALID_ARCHETYPE) {
        return;
    }
    
    auto& old_archetype = archetypes_[meta.archetype_index];
    
    // Compute new signature without component
    const ComponentSignature old_sig = old_archetype->signature();
    const ComponentSignature new_sig = old_sig & ~(ComponentSignature(1) << component_id<T>());
    
    // If new signature is empty, just remove from current archetype
    if (new_sig == 0) {
        // Remove entity from current archetype
        const auto swapped_entity = old_archetype->remove_entity(meta.entity_index);
        if (swapped_entity != NULL_ENTITY) {
            auto& swapped_meta = entity_meta_[swapped_entity.id() - 1];
            swapped_meta.entity_index = meta.entity_index;
        }
        meta.archetype_index = INVALID_ARCHETYPE;
        meta.entity_index = 0;
        return;
    }
    
    // Get or create archetype for new signature
    const auto new_archetype_idx = get_or_create_archetype(new_sig);
    
    // Move entity to new archetype
    move_entity_to_archetype(entity, new_archetype_idx);
}

template<typename T>
auto World::has_component(Entity entity) const -> bool {
    if (!is_alive(entity)) {
        return false;
    }
    
    const auto& meta = entity_meta_[entity.id() - 1];  // Convert ID to index (IDs start at 1)
    if (meta.archetype_index == INVALID_ARCHETYPE) {
        return false;
    }
    
    const auto& archetype = archetypes_[meta.archetype_index];
    const ComponentSignature sig = archetype->signature();
    return (sig & (ComponentSignature(1) << component_id<T>())) != 0;
}

template<typename T>
auto World::get_component(Entity entity) const -> const T* {
    if (!has_component<T>(entity)) {
        return nullptr;
    }
    
    const auto& meta = entity_meta_[entity.id() - 1];  // Convert ID to index (IDs start at 1)
    const auto& archetype = archetypes_[meta.archetype_index];
    return archetype->get_component<T>(component_id<T>(), meta.entity_index);
}

template<typename T>
auto World::get_component(Entity entity) -> T* {
    if (!has_component<T>(entity)) {
        return nullptr;
    }
    
    const auto& meta = entity_meta_[entity.id() - 1];  // Convert ID to index (IDs start at 1)
    auto& archetype = archetypes_[meta.archetype_index];
    return archetype->get_component<T>(component_id<T>(), meta.entity_index);
}

template<typename... Components, typename Func>
auto World::each(Func&& func) const -> void {
    const ComponentSignature required_sig = compute_signature<Components...>();
    
    // Iterate over all archetypes
    for (const auto& archetype : archetypes_) {
        const ComponentSignature arch_sig = archetype->signature();
        
        // Check if archetype has all required components
        if ((arch_sig & required_sig) != required_sig) {
            continue;
        }
        
        // Iterate over entities in this archetype
        for (u32 i = 0; i < archetype->size(); ++i) {
            const Entity entity = archetype->get_entity(i);
            func(entity, *archetype->get_component<Components>(component_id<Components>(), i)...);
        }
    }
}

template<typename... Components, typename Func>
auto World::each(Func&& func) -> void {
    const ComponentSignature required_sig = compute_signature<Components...>();
    
    // Iterate over all archetypes
    for (auto& archetype : archetypes_) {
        const ComponentSignature arch_sig = archetype->signature();
        
        // Check if archetype has all required components
        if ((arch_sig & required_sig) != required_sig) {
            continue;
        }
        
        // Iterate over entities in this archetype
        for (u32 i = 0; i < archetype->size(); ++i) {
            const Entity entity = archetype->get_entity(i);
            func(entity, *archetype->get_component<Components>(component_id<Components>(), i)...);
        }
    }
}

} // namespace luma::scene
