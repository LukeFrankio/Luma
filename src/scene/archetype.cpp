/**
 * @file archetype.cpp
 * @brief Archetype implementation
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#include <luma/scene/archetype.hpp>

namespace luma::scene {

Archetype::Archetype(ComponentSignature signature)
    : signature_(signature) {}

auto Archetype::add_entity(Entity entity) -> u32 {
    const auto index = static_cast<u32>(entities_.size());
    entities_.push_back(entity);
    return index;
}

auto Archetype::remove_entity(u32 index) -> Entity {
    if (index >= entities_.size()) {
        return NULL_ENTITY;
    }
    
    // Get entity that will be swapped into this position
    const auto swapped_entity = (index < entities_.size() - 1) ? entities_.back() : NULL_ENTITY;
    
    // Swap-and-pop
    if (index < entities_.size() - 1) {
        entities_[index] = entities_.back();
    }
    entities_.pop_back();
    
    return swapped_entity;
}

} // namespace luma::scene
