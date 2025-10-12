/**
 * @file sdf.slang
 * @brief Signed Distance Field (SDF) functions for GPU shaders
 * 
 * Slang implementations of SDF primitives that mirror the C++ versions
 * in include/luma/render/sdf.hpp. These functions are used for
 * ray marching and geometry evaluation on the GPU.
 * 
 * ✨ ALL FUNCTIONS ARE PURE ✨
 * - Deterministic output (same inputs = same outputs)
 * - No side effects or global state access
 * - GPU-friendly (no branches where possible)
 * 
 * @author LukeFrankio
 * @date 2025-10-12
 * @version 1.0
 * 
 * @note C++ versions exist in include/luma/render/sdf.hpp
 * @note Uses Slang float3 (equivalent to vec3 in GLSL)
 */

/**
 * @brief Computes signed distance from point to sphere
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param p Point to evaluate (world space)
 * @param radius Sphere radius (must be positive)
 * @return Signed distance (negative = inside, positive = outside)
 * 
 * example:
 * ```
 * float dist = sdf_sphere(float3(1.0, 0.0, 0.0), 0.5);
 * // dist = 0.5 (point is outside sphere)
 * ```
 */
float sdf_sphere(float3 p, float radius) {
    return length(p) - radius;
}

/**
 * @brief Computes signed distance from point to rounded box
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Box is centered at origin with given half-extents.
 * Rounding creates smooth edges (like rounded corners).
 * 
 * @param p Point to evaluate (world space)
 * @param extents Half-extents (width/2, height/2, depth/2)
 * @param rounding Corner rounding radius (0 = sharp)
 * @return Signed distance
 * 
 * example:
 * ```
 * float3 extents = float3(1.0, 2.0, 0.5);
 * float dist = sdf_box(float3(0.0), extents, 0.1);
 * ```
 */
float sdf_box(float3 p, float3 extents, float rounding) {
    float3 q = abs(p) - extents;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - rounding;
}

/**
 * @brief Computes signed distance from point to infinite plane
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param p Point to evaluate (world space)
 * @param normal Plane normal (must be normalized!)
 * @param distance Distance from origin along normal
 * @return Signed distance
 * 
 * example:
 * ```
 * // Ground plane at y = 0
 * float3 normal = float3(0.0, 1.0, 0.0);
 * float dist = sdf_plane(float3(5.0, 2.0, 3.0), normal, 0.0);
 * // dist = 2.0 (point is above plane)
 * ```
 */
float sdf_plane(float3 p, float3 normal, float distance) {
    return dot(p, normal) + distance;
}

/**
 * @brief Union of two SDFs (minimum)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param d1 Distance to first shape
 * @param d2 Distance to second shape
 * @return Minimum distance (union)
 */
float sdf_union(float d1, float d2) {
    return min(d1, d2);
}

/**
 * @brief Intersection of two SDFs (maximum)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param d1 Distance to first shape
 * @param d2 Distance to second shape
 * @return Maximum distance (intersection)
 */
float sdf_intersection(float d1, float d2) {
    return max(d1, d2);
}

/**
 * @brief Subtraction of two SDFs
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param d1 Distance to base shape
 * @param d2 Distance to shape to subtract
 * @return Distance to subtracted shape
 */
float sdf_subtraction(float d1, float d2) {
    return max(d1, -d2);
}

/**
 * @brief Smooth minimum (smooth union with blending)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Creates smooth transitions between shapes (no hard edges).
 * 
 * @param d1 Distance to first shape
 * @param d2 Distance to second shape
 * @param k Smoothness factor (larger = smoother, 0 = hard min)
 * @return Smoothly blended distance
 * 
 * example:
 * ```
 * float d1 = sdf_sphere(p, 1.0);
 * float d2 = sdf_sphere(p - float3(1.5, 0, 0), 1.0);
 * float smooth = sdf_smooth_union(d1, d2, 0.5);  // smooth blob
 * ```
 */
float sdf_smooth_union(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

/**
 * @brief Computes gradient of SDF (approximate normal)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Uses central differences to estimate surface normal.
 * This is used for shading and collision normals.
 * 
 * @param p Point to evaluate gradient
 * @param epsilon Step size for finite differences
 * @return Approximate normal (normalized)
 * 
 * @note This is a function template placeholder - actual usage
 *       requires passing the specific SDF function to evaluate
 * 
 * example (conceptual):
 * ```
 * float3 normal = normalize(float3(
 *     sdf(p + float3(eps, 0, 0)) - sdf(p - float3(eps, 0, 0)),
 *     sdf(p + float3(0, eps, 0)) - sdf(p - float3(0, eps, 0)),
 *     sdf(p + float3(0, 0, eps)) - sdf(p - float3(0, 0, eps))
 * ));
 * ```
 */
