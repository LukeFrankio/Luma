/**
 * @file serialization.hpp
 * @brief Scene serialization to/from YAML format
 * 
 * Provides functions for saving and loading ECS scenes to human-readable
 * YAML files. Supports all core components (Transform, Geometry, Material,
 * Velocity, Name) with automatic serialization.
 * 
 * ✨ FUNCTIONAL DESIGN ✨
 * - Pure functions where possible
 * - Immutable scene data (load returns new World)
 * - Error handling via std::expected (C++26)
 * - No global state
 * 
 * @author LukeFrankio
 * @date 2025-10-12
 * @version 1.0
 * 
 * @note Uses yaml-cpp library for YAML parsing
 * @note YAML files are hot-reloadable at runtime
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/scene/world.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace luma::scene {

/**
 * @enum SerializationError
 * @brief Error codes for serialization operations
 */
enum class SerializationError {
    FILE_NOT_FOUND,           ///< YAML file doesn't exist
    FILE_OPEN_FAILED,         ///< Cannot open file for reading/writing
    YAML_PARSE_ERROR,         ///< Invalid YAML syntax
    INVALID_COMPONENT_DATA,   ///< Component data is malformed
    MISSING_REQUIRED_FIELD,   ///< Required YAML field missing
    UNSUPPORTED_VERSION,      ///< Scene file version mismatch
};

/**
 * @brief Saves ECS world to YAML file
 * 
 * ✨ FUNCTIONAL (but has I/O side effects) ✨
 * 
 * Serializes all entities and their components to human-readable YAML.
 * Resulting file can be edited manually and hot-reloaded.
 * 
 * @param world ECS world to serialize
 * @param path Output file path (will be created/overwritten)
 * @return Success (void) or error code
 * 
 * @note File is created with UTF-8 encoding
 * @note Parent directories are created automatically if needed
 * 
 * example:
 * @code
 * World world;
 * auto entity = world.create_entity();
 * world.add_component(entity, Transform{});
 * 
 * auto result = save_scene(world, "scenes/my_scene.yaml");
 * if (!result) {
 *     log_error("Failed to save: {}", static_cast<int>(result.error()));
 * }
 * @endcode
 */
auto save_scene(
    const World& world, 
    const std::filesystem::path& path
) -> std::expected<void, SerializationError>;

/**
 * @brief Loads ECS world from YAML file
 * 
 * ✨ FUNCTIONAL (but has I/O side effects) ✨
 * 
 * Deserializes entities and components from YAML into a new World.
 * Existing World parameter is cleared and repopulated.
 * 
 * @param world World to populate (will be cleared first!)
 * @param path Input file path
 * @return Success (void) or error code
 * 
 * @pre File exists and contains valid YAML
 * @post world is cleared and repopulated with loaded data
 * 
 * @note All entities in World are destroyed before loading
 * @note Unknown component types are skipped with warning
 * 
 * example:
 * @code
 * World world;
 * auto result = load_scene(world, "scenes/pong.yaml");
 * if (!result) {
 *     log_error("Failed to load: {}", static_cast<int>(result.error()));
 *     return;
 * }
 * 
 * // World now contains entities from file
 * log_info("Loaded {} entities", world.entity_count());
 * @endcode
 */
auto load_scene(
    World& world, 
    const std::filesystem::path& path
) -> std::expected<void, SerializationError>;

/**
 * @brief Converts error code to human-readable string
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param error Error code
 * @return Error description
 * 
 * @complexity O(1)
 * 
 * example:
 * @code
 * auto result = load_scene(world, path);
 * if (!result) {
 *     log_error("Load failed: {}", error_to_string(result.error()));
 * }
 * @endcode
 */
constexpr auto error_to_string(SerializationError error) noexcept -> const char* {
    switch (error) {
        case SerializationError::FILE_NOT_FOUND:
            return "File not found";
        case SerializationError::FILE_OPEN_FAILED:
            return "Failed to open file";
        case SerializationError::YAML_PARSE_ERROR:
            return "Invalid YAML syntax";
        case SerializationError::INVALID_COMPONENT_DATA:
            return "Component data is malformed";
        case SerializationError::MISSING_REQUIRED_FIELD:
            return "Required field missing in YAML";
        case SerializationError::UNSUPPORTED_VERSION:
            return "Unsupported scene file version";
        default:
            return "Unknown error";
    }
}

}  // namespace luma::scene
