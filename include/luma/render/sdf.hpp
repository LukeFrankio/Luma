/**
 * @file sdf.hpp
 * @brief Signed Distance Field (SDF) geometry functions
 * 
 * Provides C++ implementations of SDF primitives and operations
 * that mirror the Slang shader implementations. These functions
 * are used for both CPU-side physics and GPU rendering.
 * 
 * ✨ ALL FUNCTIONS ARE PURE ✨
 * - Deterministic (same inputs = same outputs)
 * - No side effects
 * - Thread-safe by design
 * - Compile-time evaluable where possible
 * 
 * @author LukeFrankio
 * @date 2025-10-12
 * @version 1.0
 * 
 * @note Slang versions of these functions exist in shaders/common/sdf.slang
 * @note Uses GLM for vector math (glm::vec3, glm::length, etc.)
 */

#pragma once

#include <luma/core/math.hpp>
#include <luma/core/types.hpp>

namespace luma::render {

/**
 * @enum SDFType
 * @brief Types of SDF primitives supported
 * 
 * Used to tag geometry components with their shape type.
 * Each type has associated parameters defined in the SDF union.
 */
enum class SDFType : u32 {
    SPHERE,   ///< Sphere primitive (radius)
    BOX,      ///< Box primitive (extents, rounding)
    PLANE,    ///< Infinite plane primitive (normal, distance)
};

/**
 * @brief Computes signed distance from point to sphere
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Distance is negative inside sphere, positive outside.
 * 
 * @param p Point to evaluate (world space)
 * @param radius Sphere radius (must be positive)
 * @return Signed distance (negative = inside, positive = outside)
 * 
 * @pre radius > 0.0f
 * @post result == length(p) - radius
 * 
 * @complexity O(1)
 * @performance ~5 FLOPS (square root dominates)
 * 
 * example:
 * @code
 * auto dist = sdf_sphere(glm::vec3(1.0f, 0.0f, 0.0f), 0.5f);
 * // dist = 0.5f (point is 0.5 units outside sphere)
 * @endcode
 */
constexpr auto sdf_sphere(const glm::vec3& p, f32 radius) noexcept -> f32 {
    return glm::length(p) - radius;
}

/**
 * @brief Computes signed distance from point to rounded box
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Box is centered at origin with given half-extents.
 * Rounding parameter creates smooth edges (like rounded corners).
 * 
 * @param p Point to evaluate (world space)
 * @param extents Half-extents of box (width/2, height/2, depth/2)
 * @param rounding Corner rounding radius (0 = sharp corners)
 * @return Signed distance (negative = inside, positive = outside)
 * 
 * @pre extents.x > 0 && extents.y > 0 && extents.z > 0
 * @pre rounding >= 0.0f
 * @post rounding == 0 implies sharp corners
 * 
 * @complexity O(1)
 * @performance ~15 FLOPS
 * 
 * example:
 * @code
 * auto extents = glm::vec3(1.0f, 2.0f, 0.5f);
 * auto dist = sdf_box(glm::vec3(0.0f), extents, 0.1f);
 * // dist < 0 (point at origin is inside box)
 * @endcode
 */
constexpr auto sdf_box(const glm::vec3& p, const glm::vec3& extents, f32 rounding) noexcept -> f32 {
    const auto q = glm::abs(p) - extents;
    return glm::length(glm::max(q, glm::vec3(0.0f))) 
           + glm::min(glm::max(q.x, glm::max(q.y, q.z)), 0.0f) 
           - rounding;
}

/**
 * @brief Computes signed distance from point to infinite plane
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Plane is defined by normal vector and distance from origin.
 * 
 * @param p Point to evaluate (world space)
 * @param normal Plane normal (must be normalized!)
 * @param distance Distance from origin along normal
 * @return Signed distance (negative = below plane, positive = above)
 * 
 * @pre glm::length(normal) ≈ 1.0f (normal must be normalized)
 * @post result == dot(p, normal) + distance
 * 
 * @complexity O(1)
 * @performance ~4 FLOPS
 * 
 * example:
 * @code
 * // Ground plane (XZ plane at y = 0)
 * auto normal = glm::vec3(0.0f, 1.0f, 0.0f);
 * auto dist = sdf_plane(glm::vec3(5.0f, 2.0f, 3.0f), normal, 0.0f);
 * // dist = 2.0f (point is 2 units above plane)
 * @endcode
 */
constexpr auto sdf_plane(const glm::vec3& p, const glm::vec3& normal, f32 distance) noexcept -> f32 {
    return glm::dot(p, normal) + distance;
}

/**
 * @brief Union of two SDFs (smooth minimum)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Returns minimum distance, effectively combining shapes.
 * 
 * @param d1 Distance to first shape
 * @param d2 Distance to second shape
 * @return Minimum distance (union of shapes)
 * 
 * @complexity O(1)
 * 
 * example:
 * @code
 * auto d1 = sdf_sphere(p, 1.0f);
 * auto d2 = sdf_box(p, glm::vec3(0.5f), 0.0f);
 * auto combined = sdf_union(d1, d2);  // sphere OR box
 * @endcode
 */
constexpr auto sdf_union(f32 d1, f32 d2) noexcept -> f32 {
    return glm::min(d1, d2);
}

/**
 * @brief Intersection of two SDFs (smooth maximum)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Returns maximum distance, effectively intersecting shapes.
 * 
 * @param d1 Distance to first shape
 * @param d2 Distance to second shape
 * @return Maximum distance (intersection of shapes)
 * 
 * @complexity O(1)
 */
constexpr auto sdf_intersection(f32 d1, f32 d2) noexcept -> f32 {
    return glm::max(d1, d2);
}

/**
 * @brief Subtraction of two SDFs
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Subtracts second shape from first (d1 - d2).
 * 
 * @param d1 Distance to base shape
 * @param d2 Distance to shape to subtract
 * @return Distance to subtracted shape
 * 
 * @complexity O(1)
 */
constexpr auto sdf_subtraction(f32 d1, f32 d2) noexcept -> f32 {
    return glm::max(d1, -d2);
}

/**
 * @brief Computes gradient of SDF (approximate normal)
 * 
 * ✨ MOSTLY PURE FUNCTION ✨
 * (uses finite differences, but deterministic)
 * 
 * Uses central differences to estimate normal at surface.
 * This is useful for collision normals in physics.
 * 
 * @tparam F Function type (auto-deduced)
 * @param sdf_func SDF evaluation function
 * @param p Point to evaluate gradient
 * @param epsilon Step size for finite differences
 * @return Approximate normal (normalized gradient)
 * 
 * @pre epsilon > 0.0f
 * @post glm::length(result) ≈ 1.0f
 * 
 * @complexity O(1) but evaluates sdf_func 6 times
 * @performance Dominated by 6 SDF evaluations
 * 
 * example:
 * @code
 * auto normal = sdf_gradient([](const glm::vec3& p) {
 *     return sdf_sphere(p, 1.0f);
 * }, glm::vec3(1.0f, 0.0f, 0.0f), 0.001f);
 * // normal ≈ (1, 0, 0) (pointing outward from sphere)
 * @endcode
 */
template<typename F>
constexpr auto sdf_gradient(F&& sdf_func, const glm::vec3& p, f32 epsilon = 0.001f) noexcept -> glm::vec3 {
    const auto dx = glm::vec3(epsilon, 0.0f, 0.0f);
    const auto dy = glm::vec3(0.0f, epsilon, 0.0f);
    const auto dz = glm::vec3(0.0f, 0.0f, epsilon);
    
    const auto grad_x = sdf_func(p + dx) - sdf_func(p - dx);
    const auto grad_y = sdf_func(p + dy) - sdf_func(p - dy);
    const auto grad_z = sdf_func(p + dz) - sdf_func(p - dz);
    
    return glm::normalize(glm::vec3(grad_x, grad_y, grad_z));
}

}  // namespace luma::render
