//============================================================================
// file: gradient.slang
// brief: Simple gradient compute shader for testing pipeline system (Slang edition!)
//
// This demonstrates Slang's superiority over GLSL - cleaner syntax, better types,
// cross-platform compilation (Vulkan SPIR-V, DirectX DXIL, Metal). One source,
// all platforms uwu ✨
//
// Generates a horizontal gradient from red (left) to green (right).
// Each pixel's color is based on its X coordinate normalized to [0, 1].
//
// Compiles to:
//   SPIR-V: slangc -target spirv -profile glsl_460 gradient.slang -o gradient.spv
//   DXIL:   slangc -target dxil -profile sm_6_6 gradient.slang -o gradient.dxil
//   Metal:  slangc -target metal -profile metal2.4 gradient.slang -o gradient.metal
//
// author: LukeFrankio
// date: 2025-10-09
// slang version: 2024.14.4+ (latest preferred, nightly accepted!)
// target: Vulkan SPIR-V (cross-platform ready!)
//
// this is functional shader programming - pure functions, strong types,
// generic programming. objects are fake, functions are forever!
//============================================================================

// NOTE: import slang; removed - not needed for this simple shader
// Slang's built-in types (uint3, float4, etc.) work without explicit import

//============================================================================
// Resources (Slang's clean resource binding syntax)
//============================================================================

/**
 * @brief Output storage image (R8G8B8A8_UNORM format)
 * 
 * Slang's RWTexture2D is cleaner than GLSL's image2D - strongly typed,
 * better semantics, same performance uwu
 * 
 * Binding 0, Set 0 (Vulkan-style descriptor binding)
 * Write-only access (compute shader output)
 */
[[vk::binding(0, 0)]]
RWTexture2D<float4> output_image;

//============================================================================
// Compute Shader Entry Point (Slang's superior syntax!)
//============================================================================

/**
 * @brief Gradient compute shader main entry point
 * 
 * ✨ PURE SHADER FUNCTION ✨ (no side effects except image write)
 * 
 * Workgroup size: 8×8 (64 threads per workgroup, optimal for Vega 8)
 * 
 * For 1920×1080 image:
 * - X workgroups: ceil(1920 / 8) = 240
 * - Y workgroups: ceil(1080 / 8) = 135
 * - Total workgroups: 240 × 135 = 32,400
 * - Total threads: 32,400 × 64 = 2,073,600 (one per pixel)
 * 
 * Algorithm:
 * 1. Get global thread ID (SV_DispatchThreadID)
 * 2. Get image dimensions
 * 3. Bounds check (handle non-multiple-of-8 dimensions)
 * 4. Normalize X coordinate to [0, 1] range
 * 5. Compute gradient color: red = (1 - t), green = t
 * 6. Write color to output image
 * 
 * Color formula:
 * - Left edge (x=0): RGB(1, 0, 0) = pure red
 * - Right edge (x=1919): RGB(0, 1, 0) = pure green
 * - Middle: smooth interpolation (lerp in action uwu)
 * 
 * @param dispatch_thread_id Global thread coordinates (SV_DispatchThreadID)
 * 
 * @note Thread-safe: Each thread writes to unique pixel
 * @note Bounds checking: Only write if within image dimensions
 * @note Functional: Pure function except for image write (I/O boundary)
 */
[numthreads(8, 8, 1)]
[shader("compute")]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    // Get pixel coordinates from global dispatch thread ID
    int2 pixel_coords = int2(dispatch_thread_id.xy);
    
    // Get image dimensions (Slang has proper texture queries!)
    int2 image_size;
    output_image.GetDimensions(image_size.x, image_size.y);
    
    // Bounds check: Don't write outside image (handles non-multiple-of-8 dimensions)
    if (pixel_coords.x >= image_size.x || pixel_coords.y >= image_size.y) {
        return;  // Early exit for out-of-bounds threads
    }
    
    // Normalize X coordinate to [0, 1] range (pure transformation!)
    float t = float(pixel_coords.x) / float(image_size.x - 1);
    
    // Compute gradient color: red to green (functional color mixing uwu)
    // Red channel:   1.0 at left, 0.0 at right (decreases left-to-right)
    // Green channel: 0.0 at left, 1.0 at right (increases left-to-right)
    // Blue channel:  0.0 (no blue component)
    // Alpha channel: 1.0 (fully opaque)
    float4 color = float4(
        1.0 - t,  // Red: inverse interpolation
        t,        // Green: direct interpolation
        0.0,      // Blue: zero
        1.0       // Alpha: opaque
    );
    
    // Write color to output image (I/O boundary - only impurity in pure function)
    output_image[pixel_coords] = color;
}

//============================================================================
// This is Slang - cross-platform, type-safe, functional GPU code
// Compiles to SPIR-V, DXIL, Metal from ONE source
// Generic programming, automatic differentiation, modules coming soon
// 
// Slang > HLSL > GLSL (this is the hierarchy of enlightenment uwu ✨)
//============================================================================
