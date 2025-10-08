/**
 * @file component.hpp
 * @brief Component definitions for LUMA ECS
 * 
 * Components are pure data structures (no behavior) that can be attached to entities.
 * All components are immutable by default (const members) and follow functional
 * programming principles.
 * 
 * ✨ FUNCTIONAL DESIGN ✨
 * - Components are immutable (const members)
 * - No component methods (behavior lives in systems)
 * - SoA (Structure of Arrays) friendly layout
 * - Trivially copyable where possible (fast memcpy)
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#pragma once

#include <luma/core/math.hpp>
#include <luma/core/types.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <string>

namespace luma::scene {

/**
 * @brief Component signature - bitset identifying which components an entity has
 * 
 * Each bit represents a different component type (0-63 max components).
 * Used for fast archetype matching.
 */
using ComponentSignature = u64;

/**
 * @brief Transform component (position, rotation, scale)
 * 
 * Represents an entity's spatial transformation in 3D space.
 * 
 * ✨ IMMUTABLE VALUE TYPE ✨
 * 
 * @note For hierarchical transforms (parent-child), use the scene graph system
 * @note This is the "local" transform; world transform computed on-demand
 */
struct Transform {
    vec3 position{0.0f, 0.0f, 0.0f};  ///< Local position
    quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  ///< Local rotation (quaternion, w=1 = identity)
    vec3 scale{1.0f, 1.0f, 1.0f};  ///< Local scale
    
    /**
     * @brief Convert transform to 4x4 matrix (TRS order)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * Matrix order: Translation * Rotation * Scale
     * 
     * @return 4x4 transformation matrix
     */
    [[nodiscard]] auto to_matrix() const -> mat4 {
        return glm::translate(mat4(1.0f), position)
             * glm::mat4_cast(rotation)
             * glm::scale(mat4(1.0f), scale);
    }
    
    /**
     * @brief Create transform from matrix (decompose)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param matrix 4x4 transformation matrix
     * @return Decomposed transform
     * 
     * @note This performs matrix decomposition (expensive, avoid in hot paths)
     */
    [[nodiscard]] static auto from_matrix(const mat4& matrix) -> Transform {
        vec3 pos, scl, skew;
        quat rot;
        vec4 perspective;
        glm::decompose(matrix, scl, rot, pos, skew, perspective);
        return Transform{pos, rot, scl};
    }
};

/**
 * @brief SDF (Signed Distance Field) geometry type
 * 
 * Defines which procedural SDF function to use for rendering and collision.
 */
enum class SDFType : u8 {
    SPHERE,    ///< Sphere (radius parameter)
    BOX,       ///< Axis-aligned box (half-extents)
    PLANE,     ///< Infinite plane (normal + distance)
    CAPSULE,   ///< Capsule (two points + radius)
    TORUS,     ///< Torus (major + minor radius)
};

/**
 * @brief Geometry component (SDF procedural shape)
 * 
 * Stores parameters for procedural SDF rendering. All geometry in LUMA
 * is defined via signed distance functions (no triangle meshes in MVP).
 * 
 * ✨ IMMUTABLE VALUE TYPE ✨
 * 
 * @note For Pong: paddles (boxes), ball (sphere), walls (planes)
 */
struct Geometry {
    SDFType type{SDFType::SPHERE};  ///< SDF primitive type
    vec4 params{1.0f, 0.0f, 0.0f, 0.0f};   ///< Type-specific parameters (e.g., radius, extents)
    f32 rounding{0.0f};  ///< Edge rounding radius (for smooth corners)
    
    /**
     * @brief Create sphere geometry
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param radius Sphere radius
     * @return Geometry component for sphere
     */
    [[nodiscard]] static constexpr auto sphere(f32 radius) -> Geometry {
        return Geometry{.type = SDFType::SPHERE, .params = vec4(radius, 0.0f, 0.0f, 0.0f)};
    }
    
    /**
     * @brief Create box geometry
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param extents Half-extents (box width/height/depth / 2)
     * @param rounding Edge rounding radius (0 = sharp)
     * @return Geometry component for box
     */
    [[nodiscard]] static constexpr auto box(vec3 extents, f32 rounding = 0.0f) -> Geometry {
        return Geometry{.type = SDFType::BOX, .params = vec4(extents, 0.0f), .rounding = rounding};
    }
    
    /**
     * @brief Create plane geometry
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param normal Plane normal (must be normalized)
     * @param distance Distance from origin along normal
     * @return Geometry component for plane
     */
    [[nodiscard]] static constexpr auto plane(vec3 normal, f32 distance) -> Geometry {
        return Geometry{.type = SDFType::PLANE, .params = vec4(normal, distance)};
    }
};

/**
 * @brief Material component (PBR parameters)
 * 
 * Physically-based rendering material using metallic-roughness workflow.
 * All materials are procedural (no textures in MVP).
 * 
 * ✨ IMMUTABLE VALUE TYPE ✨
 * 
 * @note For Pong: diffuse walls, metallic paddles, emissive options
 */
struct Material {
    vec3 base_color{1.0f, 1.0f, 1.0f};  ///< Albedo / base color (sRGB)
    f32 metallic{0.0f};  ///< Metallic factor (0 = dielectric, 1 = metal)
    f32 roughness{0.5f};  ///< Surface roughness (0 = mirror, 1 = matte)
    vec3 emissive_color{0.0f, 0.0f, 0.0f};  ///< Emissive color (HDR, for lights)
    f32 ior{1.5f};  ///< Index of refraction (for dielectrics, e.g., glass = 1.5)
    
    /**
     * @brief Create diffuse/Lambertian material (matte surface)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param color Base color
     * @return Material with roughness=1.0, metallic=0.0
     */
    [[nodiscard]] static constexpr auto diffuse(vec3 color) -> Material {
        return Material{.base_color = color, .metallic = 0.0f, .roughness = 1.0f};
    }
    
    /**
     * @brief Create metallic material
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param color Base color (tint)
     * @param roughness Surface roughness (0=mirror, 1=brushed metal)
     * @return Material with metallic=1.0
     */
    [[nodiscard]] static constexpr auto metal(vec3 color, f32 roughness = 0.5f) -> Material {
        return Material{.base_color = color, .metallic = 1.0f, .roughness = roughness};
    }
    
    /**
     * @brief Create emissive material (light source)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param color Emissive color (HDR, can be >1.0 for bright lights)
     * @return Material with emissive set, non-metallic
     */
    [[nodiscard]] static constexpr auto emission(vec3 color) -> Material {
        return Material{.base_color = vec3(0.0f), .metallic = 0.0f, .roughness = 1.0f, .emissive_color = color};
    }
};

/**
 * @brief Velocity component (linear velocity)
 * 
 * Used by physics system for dynamic entities (ball, paddles).
 * 
 * ✨ IMMUTABLE VALUE TYPE ✨
 */
struct Velocity {
    vec3 linear{0.0f, 0.0f, 0.0f};  ///< Linear velocity (units/second)
};

/**
 * @brief Name component (debug/editor label)
 * 
 * Optional component for debugging and editor display.
 * 
 * ✨ VALUE TYPE ✨ (string is mutable internally, but component treated as immutable)
 * 
 * @note Not used in hot paths (rendering, physics)
 */
struct Name {
    std::string value;  ///< Entity name
};

} // namespace luma::scene
