//============================================================================
// file: sdf_renderer.slang
// brief: SDF scene renderer with ray marching (Slang edition!)
//
// This shader renders scenes composed of SDF primitives using ray marching.
// Supports sphere, box, and plane primitives with PBR materials.
//
// Algorithm:
// 1. Generate ray from pixel coordinates (orthographic camera)
// 2. Ray march through scene to find intersection
// 3. Compute shading (simple diffuse + ambient for now)
// 4. Output color to image
//
// author: LukeFrankio
// date: 2025-10-13
// slang version: 2024.14.4+
// target: Vulkan SPIR-V
//============================================================================

import common.sdf;  // Import SDF functions

//============================================================================
// Constants
//============================================================================

static const int MAX_STEPS = 128;         // Max ray marching steps
static const float MAX_DISTANCE = 100.0;  // Max ray distance
static const float EPSILON = 0.001;       // Surface threshold
static const float NORMAL_EPSILON = 0.01; // Normal calculation epsilon

//============================================================================
// Data Structures (GPU-side, matches CPU structures)
//============================================================================

/**
 * @brief SDF primitive type enumeration
 */
enum SDFType : uint {
    SPHERE = 0,
    BOX = 1,
    PLANE = 2,
};

/**
 * @brief Transform component (position, rotation, scale)
 * 
 * Layout matches C++ struct (std140 compatible)
 */
struct Transform {
    float3 position;
    float _pad0;
    float4 rotation;  // Quaternion (w, x, y, z)
    float3 scale;
    float _pad1;
};

/**
 * @brief Geometry component (SDF primitive)
 */
struct Geometry {
    uint type;        // SDFType enum value
    float3 params;    // Type-specific parameters
    float rounding;   // Rounding radius (for box)
    float3 _pad;
};

/**
 * @brief Material component (PBR parameters)
 */
struct Material {
    float3 base_color;
    float metallic;
    float roughness;
    float3 emissive_color;
};

/**
 * @brief Combined entity data
 */
struct EntityData {
    Transform transform;
    Geometry geometry;
    Material material;
};

/**
 * @brief Camera uniform data
 */
struct CameraData {
    float3 position;
    float _pad0;
    float3 forward;
    float _pad1;
    float3 up;
    float _pad2;
    float3 right;
    float _pad3;
    float2 view_size;  // View plane size (width, height)
    float near_plane;
    float far_plane;
};

//============================================================================
// Resources (Descriptor Set 0)
//============================================================================

[[vk::binding(0, 0)]]
RWTexture2D<float4> output_image;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraData> camera;

[[vk::binding(2, 0)]]
StructuredBuffer<EntityData> entities;

[[vk::push_constant]]
cbuffer PushConstants {
    uint entity_count;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

//============================================================================
// Utility Functions
//============================================================================

/**
 * @brief Apply quaternion rotation to vector
 * 
 * ✨ PURE FUNCTION ✨
 */
float3 quat_rotate(float4 q, float3 v) {
    float3 qvec = q.yzw;
    float3 uv = cross(qvec, v);
    float3 uuv = cross(qvec, uv);
    return v + ((uv * q.x) + uuv) * 2.0;
}

/**
 * @brief Evaluate SDF for a single entity
 * 
 * ✨ PURE FUNCTION ✨
 */
float evaluate_entity_sdf(float3 world_pos, EntityData entity) {
    // Transform world position to local space
    float3 local_pos = world_pos - entity.transform.position;
    
    // Apply inverse rotation (conjugate quaternion)
    float4 inv_quat = float4(entity.transform.rotation.x, -entity.transform.rotation.yzw);
    local_pos = quat_rotate(inv_quat, local_pos);
    
    // Apply inverse scale
    local_pos /= entity.transform.scale;
    
    // Evaluate SDF based on geometry type
    float dist = MAX_DISTANCE;
    
    if (entity.geometry.type == uint(SDFType::SPHERE)) {
        float radius = entity.geometry.params.x;
        dist = sdf_sphere(local_pos, radius);
    } else if (entity.geometry.type == uint(SDFType::BOX)) {
        float3 extents = entity.geometry.params;
        dist = sdf_box(local_pos, extents, entity.geometry.rounding);
    } else if (entity.geometry.type == uint(SDFType::PLANE)) {
        float3 normal = entity.geometry.params;
        float distance = entity.geometry.rounding;  // Repurpose rounding for plane distance
        dist = sdf_plane(local_pos, normal, distance);
    }
    
    // Apply scale to distance (approximate - good enough for simple cases)
    float min_scale = min(entity.transform.scale.x, min(entity.transform.scale.y, entity.transform.scale.z));
    return dist * min_scale;
}

/**
 * @brief Evaluate scene SDF (union of all entities)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Returns (distance, entity_index) of closest entity
 */
struct SDFResult {
    float distance;
    uint entity_index;
};

SDFResult evaluate_scene_sdf(float3 world_pos) {
    SDFResult result;
    result.distance = MAX_DISTANCE;
    result.entity_index = 0;
    
    for (uint i = 0; i < entity_count; i++) {
        float dist = evaluate_entity_sdf(world_pos, entities[i]);
        if (dist < result.distance) {
            result.distance = dist;
            result.entity_index = i;
        }
    }
    
    return result;
}

/**
 * @brief Compute surface normal using gradient
 * 
 * ✨ PURE FUNCTION ✨
 */
float3 compute_normal(float3 pos) {
    float3 normal = float3(
        evaluate_scene_sdf(pos + float3(NORMAL_EPSILON, 0.0, 0.0)).distance - 
        evaluate_scene_sdf(pos - float3(NORMAL_EPSILON, 0.0, 0.0)).distance,
        
        evaluate_scene_sdf(pos + float3(0.0, NORMAL_EPSILON, 0.0)).distance - 
        evaluate_scene_sdf(pos - float3(0.0, NORMAL_EPSILON, 0.0)).distance,
        
        evaluate_scene_sdf(pos + float3(0.0, 0.0, NORMAL_EPSILON)).distance - 
        evaluate_scene_sdf(pos - float3(0.0, 0.0, NORMAL_EPSILON)).distance
    );
    
    return normalize(normal);
}

/**
 * @brief Ray march through scene
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * Returns (hit, distance, entity_index)
 */
struct RayMarchResult {
    bool hit;
    float distance;
    uint entity_index;
};

RayMarchResult ray_march(float3 ray_origin, float3 ray_dir) {
    RayMarchResult result;
    result.hit = false;
    result.distance = 0.0;
    result.entity_index = 0;
    
    float t = 0.0;
    
    for (int i = 0; i < MAX_STEPS; i++) {
        float3 pos = ray_origin + ray_dir * t;
        SDFResult sdf = evaluate_scene_sdf(pos);
        
        if (sdf.distance < EPSILON) {
            // Hit!
            result.hit = true;
            result.distance = t;
            result.entity_index = sdf.entity_index;
            return result;
        }
        
        t += sdf.distance;
        
        if (t > MAX_DISTANCE) {
            // Missed
            return result;
        }
    }
    
    return result;
}

/**
 * @brief Simple diffuse shading
 * 
 * ✨ PURE FUNCTION ✨
 */
float3 shade_surface(float3 pos, float3 normal, Material material) {
    // Simple directional light (from above-left)
    float3 light_dir = normalize(float3(-0.5, 1.0, -0.3));
    
    // Lambertian diffuse
    float diffuse = max(dot(normal, light_dir), 0.0);
    
    // Ambient
    float ambient = 0.2;
    
    // Combine
    float3 color = material.base_color * (diffuse + ambient);
    
    // Add emissive
    color += material.emissive_color;
    
    return color;
}

//============================================================================
// Compute Shader Entry Point
//============================================================================

[numthreads(8, 8, 1)]
[shader("compute")]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    // Get pixel coordinates
    int2 pixel_coords = int2(dispatch_thread_id.xy);
    
    // Get image dimensions
    int2 image_size;
    output_image.GetDimensions(image_size.x, image_size.y);
    
    // Bounds check
    if (pixel_coords.x >= image_size.x || pixel_coords.y >= image_size.y) {
        return;
    }
    
    // Normalize pixel coordinates to [-1, 1]
    float2 uv = (float2(pixel_coords) + 0.5) / float2(image_size) * 2.0 - 1.0;
    uv.y = -uv.y;  // Flip Y (screen space vs world space)
    
    // Generate ray (orthographic projection)
    float3 ray_origin = camera.position + 
                        camera.right * uv.x * camera.view_size.x * 0.5 +
                        camera.up * uv.y * camera.view_size.y * 0.5;
    float3 ray_dir = camera.forward;
    
    // Ray march
    RayMarchResult march = ray_march(ray_origin, ray_dir);
    
    float3 color;
    
    if (march.hit) {
        // Hit surface - shade it
        float3 hit_pos = ray_origin + ray_dir * march.distance;
        float3 normal = compute_normal(hit_pos);
        Material material = entities[march.entity_index].material;
        
        color = shade_surface(hit_pos, normal, material);
    } else {
        // Miss - background color (dark blue)
        color = float3(0.05, 0.05, 0.15);
    }
    
    // Output color (with full alpha)
    output_image[pixel_coords] = float4(color, 1.0);
}

//============================================================================
// This shader demonstrates:
// - SDF scene rendering with ray marching
// - Transform hierarchies (position, rotation, scale)
// - Simple PBR-style materials
// - Orthographic camera projection
// - Functional shader programming (pure functions!)
//============================================================================
