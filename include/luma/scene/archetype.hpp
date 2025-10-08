/**
 * @file archetype.hpp
 * @brief Archetype storage for LUMA ECS
 * 
 * An archetype represents a unique combination of components. All entities
 * with the same component signature belong to the same archetype, enabling
 * cache-friendly iteration.
 * 
 * **Storage Strategy**: Structure of Arrays (SoA)
 * - Each component type has its own contiguous array
 * - Better cache locality than Array of Structures (AoS)
 * - Enables SIMD operations (future optimization)
 * 
 * ✨ FUNCTIONAL DESIGN ✨
 * - Archetype is immutable once created (signature fixed)
 * - Entity additions append to arrays (grow-only, no reallocation mid-frame)
 * - Entity removals swap-and-pop (maintains contiguous storage)
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/scene/component.hpp>
#include <luma/scene/entity.hpp>

#include <any>
#include <memory>
#include <unordered_map>
#include <vector>

namespace luma::scene {

/**
 * @brief Type-erased component array
 * 
 * Stores components of a single type in contiguous memory.
 * Uses std::any for type erasure (enables generic archetype storage).
 * 
 * ⚠️ IMPURE CLASS (manages mutable storage)
 */
class ComponentArray {
public:
    /**
     * @brief Construct empty component array
     * 
     * @tparam T Component type
     */
    template<typename T>
    static auto create() -> std::unique_ptr<ComponentArray> {
        auto array = std::make_unique<ComponentArray>();
        array->data_ = std::vector<T>();
        return array;
    }
    
    /**
     * @brief Add component to array
     * 
     * ⚠️ IMPURE (appends to vector)
     * 
     * @tparam T Component type
     * @param component Component data
     */
    template<typename T>
    auto push(T component) -> void {
        std::any_cast<std::vector<T>&>(data_).push_back(std::move(component));
    }
    
    /**
     * @brief Remove component at index (swap-and-pop)
     * 
     * ⚠️ IMPURE (modifies vector)
     * 
     * Swaps element at index with last element, then pops last.
     * This maintains contiguous storage but changes element order.
     * 
     * @tparam T Component type
     * @param index Index to remove
     */
    template<typename T>
    auto remove(u32 index) -> void {
        auto& vec = std::any_cast<std::vector<T>&>(data_);
        if (index < vec.size() - 1) {
            vec[index] = std::move(vec.back());
        }
        vec.pop_back();
    }
    
    /**
     * @brief Get component at index (read-only)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @tparam T Component type
     * @param index Index in array
     * @return Pointer to component (nullptr if out of bounds)
     */
    template<typename T>
    [[nodiscard]] auto get(u32 index) const -> const T* {
        const auto& vec = std::any_cast<const std::vector<T>&>(data_);
        return index < vec.size() ? &vec[index] : nullptr;
    }
    
    /**
     * @brief Get component at index (mutable)
     * 
     * ⚠️ IMPURE (allows mutation)
     * 
     * @tparam T Component type
     * @param index Index in array
     * @return Pointer to component (nullptr if out of bounds)
     */
    template<typename T>
    [[nodiscard]] auto get(u32 index) -> T* {
        auto& vec = std::any_cast<std::vector<T>&>(data_);
        return index < vec.size() ? &vec[index] : nullptr;
    }
    
    /**
     * @brief Get number of components in array
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @tparam T Component type
     * @return Component count
     */
    template<typename T>
    [[nodiscard]] auto size() const -> std::size_t {
        return std::any_cast<const std::vector<T>&>(data_).size();
    }

private:
    std::any data_;  ///< Type-erased component vector
};

/**
 * @brief Archetype - stores entities with identical component signatures
 * 
 * Each archetype has:
 * - **Entity list**: All entities in this archetype
 * - **Component arrays**: One per component type (SoA layout)
 * - **Signature**: Bitset identifying which components this archetype has
 * 
 * **Performance Characteristics**:
 * - Add entity: O(1) amortized (vector push_back)
 * - Remove entity: O(1) (swap-and-pop)
 * - Query components: O(n) iteration over contiguous arrays (cache-friendly)
 * 
 * ⚠️ IMPURE CLASS (manages mutable storage)
 * 
 * @note Archetypes are created by World, not directly by users
 */
class Archetype {
public:
    /**
     * @brief Construct archetype with component signature
     * 
     * @param signature Component signature bitset
     */
    explicit Archetype(ComponentSignature signature);
    
    /**
     * @brief Add entity to archetype
     * 
     * ⚠️ IMPURE (modifies entities_ vector)
     * 
     * @param entity Entity to add
     * @return Index of entity in archetype (used for component lookup)
     */
    [[nodiscard]] auto add_entity(Entity entity) -> u32;
    
    /**
     * @brief Remove entity from archetype (swap-and-pop)
     * 
     * ⚠️ IMPURE (modifies entities_ vector and component arrays)
     * 
     * Returns the entity that was swapped into the removed entity's position
     * (needed to update EntityMeta for that entity).
     * 
     * @param index Index of entity to remove
     * @return Entity that was swapped (NULL_ENTITY if index was last)
     */
    [[nodiscard]] auto remove_entity(u32 index) -> Entity;
    
    /**
     * @brief Check if archetype has component array for type T
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param type_id Component type ID
     * @return true if archetype has this component array
     */
    [[nodiscard]] auto has_component_array(u32 type_id) const -> bool {
        return components_.contains(type_id);
    }
    
    /**
     * @brief Add component array for type T
     * 
     * ⚠️ IMPURE (inserts into components_ map)
     * 
     * @tparam T Component type
     * @param type_id Component type ID
     */
    template<typename T>
    auto add_component_array(u32 type_id) -> void {
        components_[type_id] = ComponentArray::create<T>();
    }
    
    /**
     * @brief Add component to entity at index
     * 
     * ⚠️ IMPURE (appends to component array)
     * 
     * @tparam T Component type
     * @param type_id Component type ID
     * @param component Component data
     */
    template<typename T>
    auto add_component(u32 type_id, T component) -> void {
        if (auto it = components_.find(type_id); it != components_.end()) {
            it->second->push<T>(std::move(component));
        }
    }
    
    /**
     * @brief Remove component at entity index
     * 
     * ⚠️ IMPURE (removes from component array)
     * 
     * @tparam T Component type
     * @param type_id Component type ID
     * @param index Entity index in archetype
     */
    template<typename T>
    auto remove_component(u32 type_id, u32 index) -> void {
        if (auto it = components_.find(type_id); it != components_.end()) {
            it->second->remove<T>(index);
        }
    }
    
    /**
     * @brief Get component at entity index (read-only)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @tparam T Component type
     * @param type_id Component type ID
     * @param index Entity index in archetype
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    [[nodiscard]] auto get_component(u32 type_id, u32 index) const -> const T* {
        if (auto it = components_.find(type_id); it != components_.end()) {
            return it->second->get<T>(index);
        }
        return nullptr;
    }
    
    /**
     * @brief Get component at entity index (mutable)
     * 
     * ⚠️ IMPURE (allows mutation)
     * 
     * @tparam T Component type
     * @param type_id Component type ID
     * @param index Entity index in archetype
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    [[nodiscard]] auto get_component(u32 type_id, u32 index) -> T* {
        if (auto it = components_.find(type_id); it != components_.end()) {
            return it->second->get<T>(index);
        }
        return nullptr;
    }
    
    /**
     * @brief Get entity at index
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param index Entity index in archetype
     * @return Entity (NULL_ENTITY if out of bounds)
     */
    [[nodiscard]] auto get_entity(u32 index) const -> Entity {
        return index < entities_.size() ? entities_[index] : NULL_ENTITY;
    }
    
    /**
     * @brief Get number of entities in archetype
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Entity count
     */
    [[nodiscard]] auto size() const -> std::size_t {
        return entities_.size();
    }
    
    /**
     * @brief Get component signature
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Component signature bitset
     */
    [[nodiscard]] auto signature() const -> ComponentSignature {
        return signature_;
    }

private:
    ComponentSignature signature_;  ///< Component signature bitset
    std::vector<Entity> entities_;  ///< All entities in this archetype
    std::unordered_map<u32, std::unique_ptr<ComponentArray>> components_;  ///< Component arrays (one per type)
};

} // namespace luma::scene
