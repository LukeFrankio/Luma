/**
 * @file entity.hpp
 * @brief Entity system for LUMA ECS
 * 
 * Entities are lightweight identifiers (ID + generation) that reference
 * collections of components. This design follows the "Entity-Component-System"
 * pattern optimized for cache-friendly, data-oriented design.
 * 
 * ✨ FUNCTIONAL DESIGN ✨
 * - Entities are immutable once created (ID never changes)
 * - Generation counter detects stale handles (use-after-free safety)
 * - No direct entity mutation (operate through World/System functions)
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#pragma once

#include <luma/core/types.hpp>

#include <compare>
#include <functional>

namespace luma::scene {

/**
 * @brief Unique identifier for an entity
 * 
 * Entities are composed of:
 * - **ID (lower 24 bits)**: Index into entity storage (max 16 million entities)
 * - **Generation (upper 8 bits)**: Incremented when entity destroyed (detect stale handles)
 * 
 * This 32-bit design enables:
 * - Compact representation (fits in register, cache-friendly)
 * - Fast comparison (single integer compare)
 * - Stale handle detection (prevent use-after-free)
 * 
 * ✨ IMMUTABLE VALUE TYPE ✨
 * 
 * @note Entities are created by World, never constructed directly by users
 */
struct Entity {
    u32 value;  ///< Packed ID (24 bits) + generation (8 bits)
    
    /**
     * @brief Construct null entity (ID=0, generation=0)
     * 
     * ✨ PURE FUNCTION ✨ (no side effects, always returns same value)
     */
    constexpr Entity() noexcept : value(0) {}
    
    /**
     * @brief Construct entity from packed value
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param val Packed ID + generation
     */
    explicit constexpr Entity(u32 val) noexcept : value(val) {}
    
    /**
     * @brief Construct entity from ID and generation
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param id Entity ID (24 bits max)
     * @param gen Generation counter (8 bits max)
     */
    constexpr Entity(u32 id, u8 gen) noexcept 
        : value((id & 0x00FFFFFF) | (static_cast<u32>(gen) << 24)) 
    {}
    
    /**
     * @brief Static factory method to create entity
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param id Entity ID (24 bits max)
     * @param gen Generation counter (8 bits max)
     * @return New Entity with specified ID and generation
     */
    [[nodiscard]] static constexpr auto create(u32 id, u8 gen) noexcept -> Entity {
        return Entity(id, gen);
    }
    
    /**
     * @brief Extract entity ID (index into storage)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Entity ID (0 to 16,777,215)
     */
    [[nodiscard]] constexpr auto id() const noexcept -> u32 {
        return value & 0x00FFFFFF;
    }
    
    /**
     * @brief Extract generation counter
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Generation (0 to 255)
     */
    [[nodiscard]] constexpr auto generation() const noexcept -> u8 {
        return static_cast<u8>(value >> 24);
    }
    
    /**
     * @brief Check if entity is valid (non-null)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if entity ID != 0
     */
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return id() != 0;
    }
    
    /**
     * @brief Check if entity is null (default-constructed)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if entity == Entity()
     */
    [[nodiscard]] constexpr auto is_null() const noexcept -> bool {
        return value == 0;
    }
    
    /**
     * @brief Three-way comparison (enables ==, !=, <, >, <=, >=)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * Entities are ordered first by ID, then by generation.
     * This enables use in std::map, std::set, sorting, etc.
     */
    [[nodiscard]] constexpr auto operator<=>(const Entity& other) const noexcept = default;
};

/**
 * @brief Null entity constant
 * 
 * Use this to represent "no entity" or "invalid entity".
 * 
 * Example:
 * @code
 * Entity parent = get_parent(entity);
 * if (parent == NULL_ENTITY) {
 *     // Entity has no parent
 * }
 * @endcode
 */
inline constexpr Entity NULL_ENTITY = Entity();

} // namespace luma::scene

/**
 * @brief Hash function for Entity (enables use in std::unordered_map)
 * 
 * ✨ PURE FUNCTION ✨
 */
template<>
struct std::hash<luma::scene::Entity> {
    [[nodiscard]] auto operator()(const luma::scene::Entity& entity) const noexcept -> std::size_t {
        return std::hash<luma::u32>{}(entity.value);
    }
};
