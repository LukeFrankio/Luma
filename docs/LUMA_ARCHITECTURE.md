# LUMA Engine - Complete Architecture Specification

**Version:** 1.0.0  
**Date:** October 7, 2025  
**Target Platform:** AMD Vega 8 (RDNA 1.0) @ 1920x1080  
**License:** GPL (remember: there is no ethical consumption under capitalism but there IS ethical code under GPL uwu)  
**Development Timeline:** ~1 month with AI assistance

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Core Philosophy](#2-core-philosophy)
3. [Hardware Specifications](#3-hardware-specifications)
4. [Module Architecture](#4-module-architecture)
5. [Rendering System](#5-rendering-system)
6. [Scene Representation](#6-scene-representation)
7. [Physics System](#7-physics-system)
8. [Settings System](#8-settings-system)
9. [Asset Pipeline](#9-asset-pipeline)
10. [Threading Model](#10-threading-model)
11. [Memory Management](#11-memory-management)
12. [Error Handling](#12-error-handling)
13. [Build System](#13-build-system)
14. [Performance Targets](#14-performance-targets)
15. [Development Roadmap](#15-development-roadmap)
16. [Code Style](#16-code-style)

---

## 1. Executive Summary

**LUMA** (all caps) is a modular, functional, Vulkan-powered rendering engine designed for extreme flexibility and performance. Initial milestone: path-traced Pong game showcasing real-time ray tracing on integrated graphics.

### Key Features

- **Compute shader path tracing** (software RT, no hardware RT extensions required)
- **Signed Distance Field (SDF) rendering** for procedural geometry
- **Wavefront path tracing** for GPU coherence
- **Modular architecture** with hot-reloadable DLL modules
- **Custom ECS** optimized for path tracing workloads
- **Functional programming principles** (immutability, purity, composability)
- **Settings-driven everything** (YAML config + runtime ImGui tweaking)
- **SDF-based physics** (unified rendering + collision detection)

### Initial Demo: Path-Traced Pong

- Two physics modes: **Arcade** (perfect reflections) and **Realistic** (physical materials)
- Two camera modes: **Orthographic** (classic 2D) and **Perspective** (3D view with lerp transitions)
- Procedural PBR materials with optional emissive paddles/ball
- Real-time denoising (spatial à-trous wavelet filter)
- 30 FPS minimum @ 1080p on AMD Vega 8 iGPU

---

## 2. Core Philosophy

### Functional Programming First

- **Immutability**: All data structures immutable by default (const members, no setters)
- **Purity**: Functions have no side effects, deterministic, referentially transparent
- **Composition**: Systems as pure functions that compose via pipes/monads
- **No Exceptions**: Use `std::expected<T, Error>` (C++26) for error handling
- **Constexpr Everything**: All math/arithmetic operations `constexpr` where possible

### Design Principles

1. **Modular**: Core engine + optional modules (DLLs) with stable C ABI
2. **Simple but Powerful**: Start minimal, scale to complex features
3. **Data-Oriented**: ECS with cache-friendly layouts, SoA over AoS
4. **GPU-First**: Minimize CPU work, maximize GPU compute utilization
5. **Developer Experience**: Hot-reload everything (shaders, modules, assets, settings)
6. **Performance**: 30 FPS minimum on iGPU, unlocked framerate, adaptive quality

---

## 3. Hardware Specifications

### Target Hardware

- **GPU**: AMD Radeon Vega 8 (integrated)
  - Architecture: RDNA 1.0
  - Vulkan: 1.3 (core features, spotty extension support)
  - Compute Units: 8 CUs
  - Wavefront Size: 64 threads (AMD terminology)
  - Local Data Share (LDS): 16 KB per CU
  - Memory: Shared system RAM (host-visible, unified memory)
  
- **CPU**: x86-64 with AVX2 support
  - Multi-threading: Unlimited cores/threads (dynamic job distribution)
  - SIMD: AVX2 for CPU-side work (BVH build, etc.)

- **Display**: 1920×1080 native resolution
  - Internal render resolution: Scalable via settings (dynamic res scale)

### Vulkan Feature Requirements

**Core 1.3 Features** (confirmed available on Vega 8):

- `shaderFloat64` ✅
- `shaderInt64` ✅
- `shaderStorageImageReadWithoutFormat` ✅
- `shaderStorageImageWriteWithoutFormat` ✅
- `fragmentStoresAndAtomics` ✅
- `geometryShader` ✅ (for rasterization fallback/UI)
- `tessellationShader` ✅ (future SDF tessellation)
- `samplerAnisotropy` ✅

**Extensions Used**:

- Core Vulkan 1.3 only (maximum portability)
- Optional AMD-specific optimizations (shader ballot, wave intrinsics) disabled by default

---

## 4. Module Architecture

### Core Modules (Mandatory)

All projects using LUMA require these:

1. **Core** (`luma_core.dll`)
   - Logging (structured, severity levels)
   - Math library (GLM wrapper, custom SIMD types)
   - Threading (job system with work-stealing)
   - Memory allocators (linear, pool, VMA wrapper)
   - Error types (`Result<T, Error>`, error codes)
   - Time/profiling utilities

2. **Vulkan Backend** (`luma_vulkan.dll`)
   - Device/queue/instance management
   - Command buffer abstraction (functional wrapper)
   - Memory management (VMA integration)
   - Swapchain + presentation
   - Pipeline cache + hot-reload
   - Synchronization primitives (fences, semaphores, barriers)

3. **Scene** (`luma_scene.dll`)
   - Custom ECS (archetype storage, queries as pure functions)
   - Components: Transform, Geometry, Material, RigidBody, Collider
   - Scene graph (optional, for hierarchies)
   - Serialization (YAML save/load)

4. **Asset** (`luma_asset.dll`)
   - Asset loading (glTF, procedural generation)
   - Hot-reload (file watcher, async reloading)
   - Resource handle system (ID + generation counter for stale detection)
   - Shader compilation (GLSL → SPIR-V via shaderc, disk caching)

5. **Input** (`luma_input.dll`)
   - Keyboard/mouse input (event-based)
   - Input mapping system (rebindable controls)
   - Window library integration (GLFW)

6. **Audio** (`luma_audio.dll`)
   - Future: OpenAL integration
   - MVP: Basic sound effects for Pong

### Additional Modules (Project-Specific)

Required for path-traced Pong:

1. **PathTracer** (`luma_pathtracer.dll`)
   - BVH builder (CPU-side, incremental rebuild)
   - Compute shader path tracer (wavefront architecture)
   - Camera ray generation
   - Material evaluation (PBR: diffuse, metallic, roughness, emissive)
   - Denoising (spatial à-trous wavelet filter)
   - Accumulation + tonemapping

2. **Physics** (`luma_physics.dll`)
   - Custom SDF-based collision detection
   - Two modes: Arcade (perfect reflections) vs Realistic (physical response)
   - Broadphase: None (Pong has <10 objects)
   - Narrowphase: SDF distance queries

3. **Settings** (`luma_settings.dll`)
   - Generic settings framework (builder API + YAML override)
   - ImGui integration (automatic UI generation from settings schema)
   - Categories: Rendering, Performance, Graphics, Gameplay, Debug
   - Presets: Low/Medium/High/Ultra (defined later)

4. **Editor** (`luma_editor.dll`)
   - ImGui docking layout
   - Scene hierarchy view
   - Component inspector
   - Performance graphs (FPS, frame time, GPU time, trace time, denoise time, upscale time)
   - Render target visualization (G-buffer views: albedo, normal, depth)
   - Debug overlays (BVH wireframe, ray paths, normals)

### Module System Details

**DLL Loading**:

- Auto-discovery: Scan `modules/` folder for `luma_*.dll`
- Explicit load order: Core → Vulkan → (others in dependency order)
- Hot-reload: File watcher detects changes, reloads DLL, re-initializes
- Version checking: Reject incompatible ABI versions

**Module Interface (C ABI for stability)**:

```c
// Stable C interface
extern "C" {
  // Called on module load
  ModuleInfo* luma_module_init(EngineContext* ctx);
  
  // Called on module unload
  void luma_module_shutdown(ModuleInfo* info);
  
  // Called per frame (optional)
  void luma_module_update(ModuleInfo* info, float delta_time);
  
  // Module metadata
  const char* luma_module_name();
  uint32_t luma_module_version();  // semantic version (major.minor.patch)
}
```

**Communication Between Modules**:

- Event system: Async, type-safe events posted to queue
- Direct function calls: For performance-critical paths (via function pointers in `EngineContext`)
- Shared state: Minimal, prefer passing data explicitly

---

## 5. Rendering System

### Path Tracing Architecture

**Implementation Approach**: Software ray tracing via **Vulkan Compute Shaders**

- No hardware RT extensions (VK_KHR_ray_tracing_pipeline not used)
- Manual BVH traversal in compute shader
- Portable across all Vulkan 1.3 devices

**Algorithm**: Wavefront Path Tracing

- **Wavefront**: Rays grouped by coherence (material, depth, state)
- **Material Sorting**: Sort rays by material ID per bounce (coherent shading)
- **Persistent Rays**: Single buffer with compaction (remove terminated rays)
- **Megakernel**: All logic in one compute shader (ray gen → intersection → shading loop)

**Compute Shader Layout** (optimized for Vega 8):

- Workgroup size: **128 threads** (2 wavefronts of 64)
- 2D dispatch for image: **16×16 tiles** = 256 threads per workgroup (adjust for power-of-2)
- Actually: **8×16 tiles** = 128 threads (matches wavefront)

### BVH (Bounding Volume Hierarchy)

**Structure**: BVH2 (binary tree, 2 children per node)

- **Build Algorithm**: SAH (Surface Area Heuristic) binned builder
- **CPU-side**: Build on CPU, upload to GPU buffer
- **Rebuild Strategy**: Incremental
  - Rebuild only changed subtrees (moving ball/paddle)
  - Full rebuild fallback if too many changes
- **Layout**: Breadth-first (better for GPU cache coherency)
- **Traversal**: Stackless (dynamic, no stack overflow risk)
  - Track parent pointers for backtracking
  - Slightly more memory but eliminates stack limits

**Storage**:

```glsl
// GPU buffer layout
struct BVHNode {
  vec3 aabb_min;      // 12 bytes
  uint left_child;    // 4 bytes (0xFFFFFFFF = leaf)
  vec3 aabb_max;      // 12 bytes
  uint right_child;   // 4 bytes
  // 32 bytes per node (cache-line friendly)
};
```

### Path Tracing Algorithms (User-Selectable)

**Unidirectional Path Tracing** (default for MVP):

- Camera → light path only
- Simple, fast, works well for direct + indirect lighting
- 1-4 SPP (samples per pixel) per frame with progressive accumulation

**Bidirectional Path Tracing** (future):

- Camera path + light path, connect in middle
- Better for caustics and complex lighting
- Heavier compute cost

**Settings Exposed**:

- Algorithm choice: Unidirectional / Bidirectional
- Max bounces: 1-16 (default: 4)
- Samples per pixel per frame: 1-8 (default: 2)
- Russian roulette: Enable/disable (terminate low-contribution rays early)

### Denoising

**Spatial Denoiser** (MVP): À-trous wavelet filter

- 5×5 kernel, multiple passes (e.g., 3-5 passes)
- Edge-stopping functions: Normal, depth, albedo
- Compute shader implementation (fast, stays on GPU)

**Temporal Accumulation** (future):

- Accumulate samples across frames (if camera/scene static)
- Motion vectors for reprojection (if camera/scene moving)
- Exponential moving average: `result = mix(history, current, α)`

**AI Denoiser** (future):

- NVIDIA NRD or Intel OIDN integration
- Optional, adds external dependency

**Settings Exposed**:

- Denoising method: None / Spatial / Temporal / AI
- Spatial filter strength: 0.0-1.0
- Temporal blend factor: 0.0-1.0

### Material System

**PBR (Physically-Based Rendering)**:

- **Metallic-Roughness** workflow (glTF 2.0 compatible)
- Parameters: Base color, metallic, roughness, emissive, IOR
- Procedural materials: Defined in code (no textures for Pong MVP)

**Material Types** (all supported, user-selectable):

1. **Diffuse/Lambertian**: Matte surfaces (walls)
2. **Metallic**: Metals with roughness (paddles)
3. **Dielectric**: Glass/plastic with IOR (ball, optional)
4. **Emissive**: Lights (paddles, ball, optional)
5. **Mirror**: Perfect reflections (arcade mode aesthetic)

**For Pong**:

- Paddles: Metallic or Emissive (user toggle)
- Ball: Metallic or Emissive (user toggle)
- Walls: Diffuse (off-white for pristine clean aesthetic)
- Background: Procedural gradient or solid color

**Material Definition** (hybrid approach):

1. **Code-defined base**: C++ structs for built-in materials
2. **Data-driven parameters**: YAML override for tweaking
3. **Future: Shader graphs**: Node-based material editor

**Example YAML**:

```yaml
materials:
  paddle:
    type: metallic
    base_color: [1.0, 1.0, 1.0]
    metallic: 1.0
    roughness: 0.1
    emissive: [0.0, 0.0, 0.0]  # disabled by default
  ball:
    type: metallic
    base_color: [1.0, 1.0, 1.0]
    metallic: 1.0
    roughness: 0.05
    emissive: [1.0, 1.0, 1.0]  # enabled by default
```

### Camera System

**Camera Modes**:

1. **Orthographic**: Classic 2D Pong view (top-down or side)
2. **Perspective**: 3D view at 45° angle (cinematic)
3. **Free-fly** (debug): WASD + mouse camera

**Transition**:

- Lerp projection matrix over time (e.g., 0.5 seconds)
- Keep game state consistent (paddle positions stay same in world space)
- Smooth interpolation function: `smoothstep` or custom ease curve

**Controls**:

- Fixed camera (default): No player control
- Free camera (debug): WASD for movement, mouse for rotation

**Implementation**:

- Camera generates rays in compute shader (ray generation kernel)
- Orthographic: Parallel rays
- Perspective: Rays from focal point through pixel

### Lighting

**Emissive Geometry as Lights**:

- No explicit light objects (area lights are just emissive meshes)
- Paddles/ball can be emissive (user toggle)
- Environment lighting: Procedural sky or solid color

**Light Sampling**:

- Multiple Importance Sampling (MIS) for efficiency
- Explicitly sample emissive surfaces (if few lights)
- BRDF sampling (for indirect lighting)

### Post-Processing

**Tonemapping**: ACES filmic (photographic look)  
**Gamma Correction**: sRGB (2.2 gamma)  
**Bloom** (future): Kawase blur on bright pixels  
**TAA** (Temporal Anti-Aliasing, future): Accumulate with jitter

---

## 6. Scene Representation

### Entity Component System (ECS)

**Architecture**: Fully custom, optimized for path tracing

- **Archetype storage**: Group entities by component signature (cache-friendly)
- **Sparse sets**: Fast entity lookup
- **Queries as pure functions**: Return views over component arrays
- **Immutable entities**: Entity creation returns new ID, no in-place mutation

**Core Components**:

```cpp
namespace luma::ecs {
  struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
  };
  
  struct Geometry {
    GeometryType type;  // SDF_SPHERE, SDF_BOX, SDF_PLANE, MESH (future)
    union {
      struct { float radius; } sphere;
      struct { glm::vec3 extents; float rounding; } box;
      struct { glm::vec3 normal; float distance; } plane;
    };
  };
  
  struct Material {
    glm::vec3 base_color;
    float metallic;
    float roughness;
    glm::vec3 emissive;
    float ior;  // index of refraction
  };
  
  struct RigidBody {
    glm::vec3 velocity;
    glm::vec3 angular_velocity;
    float mass;
    float restitution;  // bounciness (0-1)
  };
  
  struct Collider {
    // SDF-based: reuse Geometry component for collision shape
    // Additional collision properties:
    bool is_trigger;  // no physics response, just events
  };
  
  struct Paddle {
    uint32_t player_id;  // 0 or 1
    float speed;         // movement speed
  };
  
  struct Ball {
    float base_speed;    // reset speed
    float current_speed; // can accelerate
  };
}
```

**Systems as Pure Functions**:

```cpp
namespace luma::systems {
  // Pure function: takes components, returns updated components
  auto update_physics(
    std::span<const RigidBody> bodies,
    std::span<const Transform> transforms,
    float delta_time
  ) -> std::vector<Transform>;
  
  // Query-based system
  auto render_system(World& world) -> RenderCommands {
    auto query = world.query<Transform, Geometry, Material>();
    // Build render commands from query results
  };
}
```

### Scene Graph (Optional)

**Hierarchy**: Optional parent-child relationships

- Most Pong entities are root-level (no hierarchy needed)
- Future: Use for complex models (child meshes)

**Transform Propagation**:

- World transform = parent_world * local_transform
- Computed on demand (pure function)

### Serialization

**Format**: YAML (human-readable, easy to edit)
**Saved Data**: Entities + components, scene settings, material definitions
**Hot-Reload**: Watch scene file, reload on change

**Example Scene File**:

```yaml
scene:
  name: "Pong"
  entities:
    - name: "Left Paddle"
      components:
        - Transform: {position: [-5, 0, 0], rotation: [0,0,0,1], scale: [1,1,1]}
        - Geometry: {type: sdf_box, extents: [0.5, 2, 0.5], rounding: 0.1}
        - Material: {base_color: [1,1,1], metallic: 1, roughness: 0.1}
        - RigidBody: {velocity: [0,0,0], mass: 1}
        - Paddle: {player_id: 0, speed: 5}
    
    - name: "Ball"
      components:
        - Transform: {position: [0, 0, 0]}
        - Geometry: {type: sdf_sphere, radius: 0.3}
        - Material: {base_color: [1,1,1], metallic: 1, roughness: 0.05, emissive: [1,1,1]}
        - RigidBody: {velocity: [3, 2, 0], mass: 0.1}
        - Ball: {base_speed: 5, current_speed: 5}
```

---

## 7. Physics System

### SDF-Based Collision Detection

**Unified Rendering + Physics**:

- Use same SDF functions for both rendering and collision
- Distance queries are exact (no approximation needed)
- Collision normal = gradient of SDF

**Algorithm**:

1. **Broadphase**: None (Pong has <10 entities, test all pairs)
2. **Narrowphase**: SDF distance query
   - If `distance(A, B) < 0`, collision detected
   - Penetration depth = `-distance`
   - Collision normal = `normalize(gradient(sdf))`

**SDF Functions** (GLSL + C++ mirror):

```glsl
float sdf_sphere(vec3 p, float radius) {
  return length(p) - radius;
}

float sdf_box(vec3 p, vec3 extents, float rounding) {
  vec3 q = abs(p) - extents;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - rounding;
}

float sdf_plane(vec3 p, vec3 normal, float distance) {
  return dot(p, normal) + distance;
}
```

### Physics Modes

**Arcade Mode** (default):

- Perfect reflections: `v_out = reflect(v_in, normal)`
- Constant speed: Normalize velocity after reflection, scale to constant
- No spin, no friction
- Slight acceleration on paddle hit (optional)

**Realistic Mode**:

- Physical material properties: restitution (bounciness), friction
- Spin on ball from paddle velocity: `angular_vel += cross(paddle_vel, collision_point)`
- Energy loss: Velocity *= restitution on collision
- More unpredictable, challenging gameplay

**Runtime Toggle**: Switch mode via settings (if/else in collision response function)

### Collision Response

```cpp
namespace luma::physics {
  // Pure function: compute new velocities after collision
  struct CollisionResult {
    glm::vec3 velocity_a;
    glm::vec3 velocity_b;
    glm::vec3 angular_velocity_a;
    glm::vec3 angular_velocity_b;
  };
  
  auto resolve_collision_arcade(
    const RigidBody& body_a,
    const RigidBody& body_b,
    const glm::vec3& normal,
    float penetration
  ) -> CollisionResult;
  
  auto resolve_collision_realistic(
    const RigidBody& body_a,
    const RigidBody& body_b,
    const Material& mat_a,
    const Material& mat_b,
    const glm::vec3& normal,
    const glm::vec3& contact_point,
    float penetration
  ) -> CollisionResult;
}
```

---

## 8. Settings System

### Architecture

**Hybrid Approach**: Builder API in code + YAML override

- Code defines settings schema (types, ranges, defaults)
- YAML file overrides defaults (user customization)
- ImGui generates UI automatically from schema

**Settings Categories**:

1. **Rendering**: Path tracing algorithm, SPP, max bounces, denoising
2. **Performance**: Internal resolution scale, thread count, BVH quality
3. **Graphics**: Material toggles (emissive), camera mode, post-processing
4. **Gameplay**: Physics mode (arcade/realistic), difficulty
5. **Debug**: Show BVH, show normals, freeze frame, profiler overlay

### Settings Definition (Builder API)

```cpp
namespace luma::settings {
  struct SettingsSchema {
    // Builder API
    auto add_category(std::string_view name) -> CategoryBuilder&;
  };
  
  struct CategoryBuilder {
    auto add_int_slider(std::string_view name, int default_val, int min, int max) -> CategoryBuilder&;
    auto add_float_slider(std::string_view name, float default_val, float min, float max) -> CategoryBuilder&;
    auto add_bool_toggle(std::string_view name, bool default_val) -> CategoryBuilder&;
    auto add_enum_dropdown(std::string_view name, std::vector<std::string> options, int default_idx) -> CategoryBuilder&;
  };
  
  // Example usage
  auto create_render_settings() -> SettingsSchema {
    SettingsSchema schema;
    schema.add_category("Rendering")
      .add_enum_dropdown("algorithm", {"Unidirectional", "Bidirectional"}, 0)
      .add_int_slider("spp", 2, 1, 8)
      .add_int_slider("max_bounces", 4, 1, 16)
      .add_enum_dropdown("denoise", {"None", "Spatial", "Temporal", "AI"}, 1);
    
    schema.add_category("Graphics")
      .add_bool_toggle("emissive_paddles", false)
      .add_bool_toggle("emissive_ball", true)
      .add_enum_dropdown("camera_mode", {"Orthographic", "Perspective", "Free"}, 1);
    
    schema.add_category("Gameplay")
      .add_enum_dropdown("physics_mode", {"Arcade", "Realistic"}, 0);
    
    return schema;
  }
}
```

### YAML Override

**File**: `settings.yaml` (loaded at startup, hot-reloadable)

```yaml
rendering:
  algorithm: "Unidirectional"
  spp: 2
  max_bounces: 4
  denoise: "Spatial"

performance:
  internal_resolution_scale: 1.0  # 1.0 = native 1080p, 0.5 = 960x540 upscaled
  adaptive_spp: true              # reduce SPP if frame time > target
  adaptive_resolution: true       # reduce res scale if frame time > target

graphics:
  emissive_paddles: false
  emissive_ball: true
  camera_mode: "Perspective"
  bloom: false  # future

gameplay:
  physics_mode: "Arcade"

debug:
  show_bvh: false
  show_normals: false
  freeze_frame: false
  profiler_overlay: true
```

### ImGui Integration

**Auto-Generated UI**:

- Parse schema, generate ImGui widgets (sliders, checkboxes, dropdowns)
- Organized by category (collapsible headers)
- Live updates: Changes apply immediately (or next frame)
- Save button: Write current values back to YAML

**Example UI Code** (auto-generated):

```cpp
if (ImGui::CollapsingHeader("Rendering")) {
  ImGui::SliderInt("SPP", &settings.rendering.spp, 1, 8);
  ImGui::SliderInt("Max Bounces", &settings.rendering.max_bounces, 1, 16);
  ImGui::Combo("Denoise", &settings.rendering.denoise, "None\0Spatial\0Temporal\0AI\0");
}

if (ImGui::CollapsingHeader("Gameplay")) {
  ImGui::Combo("Physics Mode", &settings.gameplay.physics_mode, "Arcade\0Realistic\0");
}
```

---

## 9. Asset Pipeline

### Asset Types

**For MVP (Pong)**:

- **Geometry**: Procedural SDFs (sphere, box, plane)
- **Materials**: Code-defined PBR parameters
- **Shaders**: GLSL compute shaders (path tracer, denoiser, tonemapper)
- **Scenes**: YAML scene files

**Future**:

- **Meshes**: glTF 2.0 (triangles for non-SDF geometry)
- **Textures**: PNG/JPG (for albedo, normal, roughness maps)
- **Audio**: WAV/OGG (sound effects, music)

### Shader Compilation

**Pipeline**:

1. **Source**: GLSL shaders in `shaders/` folder
2. **Compile**: GLSL → SPIR-V (using `shaderc` library at runtime)
3. **Cache**: Save SPIR-V to disk (`shaders_cache/` folder) with hash
4. **Load**: Check cache first, recompile only if source changed
5. **Hot-Reload**: File watcher detects changes, triggers recompilation

**Compile Strategy**:

- **First run**: Compile all shaders, save to disk (one-time cost)
- **Subsequent runs**: Load from cache (instant)
- **Development mode**: Watch shader files, recompile on change
- **Release mode**: Embed SPIR-V in executable (no runtime compilation)

**Shader Lookup** (via command-line argument):

- Default: Load embedded shaders (release)
- With `--external-shaders` flag: Load from `shaders/` folder (dev)
- Debug builds: Default to external (easier iteration)

**Example Shaders**:

- `path_tracer.comp`: Main path tracing kernel
- `bvh_build.comp`: GPU BVH builder (future optimization)
- `denoise_spatial.comp`: Spatial denoiser
- `tonemap.comp`: Tonemapping + gamma correction
- `accumulate.comp`: Temporal accumulation

### Hot-Reload

**File Watcher**:

- **Windows**: `ReadDirectoryChangesW` API (async, low overhead)
- **Fallback**: Poll file modification times every N frames (N=60, check once per second)

**Reload Strategy**:

- Detect change → mark asset dirty
- Next frame: Reload asset (shader recompile, YAML reparse, etc.)
- GPU resources: Wait for idle (no in-flight commands), destroy old, create new

**Hot-Reloadable Assets**:

- Shaders ✅ (recompile, recreate pipelines)
- Scenes ✅ (reload entities, rebuild BVH)
- Settings ✅ (reload YAML, update runtime values)
- Modules ✅ (unload DLL, reload, re-init)

### Resource Handle System

**Handle Design** (detect stale references):

```cpp
struct ResourceHandle {
  uint32_t id;         // Index into resource array
  uint32_t generation; // Increment on delete (detect stale handles)
};

template<typename T>
class ResourcePool {
  std::vector<T> resources;
  std::vector<uint32_t> generations;
  
  auto get(ResourceHandle handle) -> std::optional<T&> {
    if (handle.generation == generations[handle.id]) {
      return resources[handle.id];
    }
    return std::nullopt;  // Stale handle
  }
};
```

---

## 10. Threading Model

### Job System Architecture

**Design**: Fully dynamic, work-stealing job system

- All threads in a shared pool (no dedicated threads)
- Jobs are pure functions with explicit dependencies
- Task graph: Directed acyclic graph (DAG) of jobs

**Thread Allocation**:

- Use all available CPU threads (detect via `std::thread::hardware_concurrency()`)
- No thread affinity (OS schedules freely)
- User override via settings (if fewer threads desired)

**Job Types**:

```cpp
namespace luma::jobs {
  struct Job {
    std::function<void()> work;            // Pure function
    std::vector<JobHandle> dependencies;   // Must complete before this job
    std::atomic<uint32_t> ref_count;       // For dependency tracking
  };
  
  struct JobHandle {
    uint32_t id;
    uint32_t generation;
  };
  
  class JobSystem {
    auto schedule(Job job) -> JobHandle;
    auto wait(JobHandle handle) -> void;
    auto wait_all() -> void;
  };
}
```

**Example Frame Graph**:

```text
Input → ECS Update → Physics → Build BVH → GPU Submit → Present
                      ↓                        ↑
                    Audio                  Wait Fence
```

### GPU Synchronization

**Vulkan Sync**:

- **Fences**: CPU-GPU sync (wait for frame completion)
- **Semaphores**: GPU-GPU sync (queue handoff, swapchain acquire/present)
- **Barriers**: Pipeline sync (memory dependencies between shader stages)

**Frame Pacing**:

- **Triple buffering**: 3 frames in flight (reduce CPU stalls)
- **Async compute**: Path tracing on compute queue, UI on graphics queue (parallel)
- **Backpressure**: If GPU too slow, CPU waits (avoid unbounded memory growth)

**Compute Queue Usage**:

- Vega 8 has separate compute queue (can run parallel to graphics)
- Path tracing on compute queue (long-running)
- UI rendering on graphics queue (rasterization, ImGui)
- Synchronize via semaphore when path tracing done (copy to swapchain)

---

## 11. Memory Management

### Allocators

**Linear Allocator** (frame-temp data):

- Fast: Bump pointer, no per-allocation overhead
- Reset every frame (no fragmentation)
- Use for: Temporary command buffers, intermediate data

**Pool Allocator** (fixed-size objects):

- Fast: Free-list of same-size blocks
- No fragmentation
- Use for: Entities, components, jobs

**VMA (Vulkan Memory Allocator)**:

- GPU memory management (buffers, images)
- Handles device-local, host-visible, staging memory
- Defragmentation (if needed)
- Even on iGPU (shared memory): VMA optimizes host-visible memory

**Ownership Model**:

- **Prefer unique ownership**: `std::unique_ptr`, move semantics
- **Shared ownership**: `std::shared_ptr` only when necessary (multiple owners)
- **Handle system**: For hot-reloadable assets (detect stale references)

### GPU Memory Strategy (iGPU Specifics)

**Unified Memory** (Vega 8 shares system RAM):

- Prefer `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`
- No explicit staging (CPU writes directly to GPU-visible memory)
- Use `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` (no manual flushing)

**Buffers**:

- **Uniform Buffers**: Small (<64KB), per-frame (camera, settings)
- **Storage Buffers**: Large (BVH, geometry, materials), static or dynamic
- **Staging**: Not needed on iGPU (can write directly)

**Memory Budget**:

- Assume 2-4 GB available (shared with OS/other apps)
- Monitor usage: Query `VkPhysicalDeviceMemoryBudgetPropertiesEXT` (if available)

---

## 12. Error Handling

### No Exceptions Philosophy

**Use `std::expected<T, Error>` (C++26)**:

```cpp
namespace luma {
  enum class Error {
    VulkanDeviceCreationFailed,
    ShaderCompilationFailed,
    FileNotFound,
    InvalidYAML,
    BVHBuildFailed,
    // ...
  };
  
  // Pure functions return Result
  auto load_shader(std::filesystem::path path) -> std::expected<ShaderModule, Error>;
  
  auto create_device(const DeviceConfig& config) -> std::expected<Device, Error>;
}
```

**Error Propagation** (monadic, functional):

```cpp
auto result = load_shader("path_tracer.comp")
  .and_then([](auto shader) { return create_pipeline(shader); })
  .and_then([](auto pipeline) { return render_frame(pipeline); });

if (!result) {
  log_error("Pipeline creation failed: {}", result.error());
}
```

**Logging**:

- Structured logging with severity levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
- Log to console + file (`logs/luma.log`)
- Include context: module name, timestamp, thread ID

**Assertions** (debug only):

```cpp
#ifdef LUMA_DEBUG
  #define LUMA_ASSERT(cond, msg) if (!(cond)) { log_fatal(msg); abort(); }
#else
  #define LUMA_ASSERT(cond, msg) ((void)0)
#endif
```

**Vulkan Validation Layers** (debug only):

- Enable `VK_LAYER_KHRONOS_validation` in debug builds
- Catch API misuse, sync issues, memory errors

---

## 13. Build System

### CMake Structure

**Minimum Version**: CMake 3.20+

**Project Layout**:

```text
luma/
├── CMakeLists.txt              # Root config
├── cmake/
│   ├── CompilerWarnings.cmake  # -Wall -Wextra -Werror
│   ├── Sanitizers.cmake        # ASan/UBSan in debug
│   └── FetchDependencies.cmake # FetchContent for deps
├── include/luma/               # Public headers
│   ├── core/
│   ├── render/
│   └── ...
├── src/                        # Implementation
│   ├── core/
│   ├── vulkan/
│   ├── pathtracer/
│   └── ...
├── shaders/                    # GLSL shaders
├── tests/                      # Google Test suites
├── docs/                       # Documentation (Doxygen + Markdown)
└── examples/
    └── pong/                   # Path-traced Pong game
```

**Build Targets**:

- `luma_core` (static lib or DLL)
- `luma_vulkan` (DLL)
- `luma_scene` (DLL)
- `luma_asset` (DLL)
- `luma_input` (DLL)
- `luma_audio` (DLL)
- `luma_pathtracer` (DLL)
- `luma_physics` (DLL)
- `luma_settings` (DLL)
- `luma_editor` (DLL)
- `pong` (executable, links all modules)
- `luma_tests` (executable, Google Test)

### Dependencies

**Fetch Strategy**: `find_package` with `FetchContent` fallback

- Try system-installed libraries first (vcpkg, system packages)
- If not found, fetch from GitHub via CMake

**Dependencies**:

1. **Vulkan SDK**: Required, must be installed manually (link against `Vulkan::Vulkan`)
2. **VMA (Vulkan Memory Allocator)**: FetchContent from GitHub (header-only)
3. **GLM**: FetchContent (header-only math library)
4. **ImGui**: FetchContent (docking branch for dockable windows)
5. **GLFW**: find_package or FetchContent (window/input)
6. **shaderc**: find_package (part of Vulkan SDK) or FetchContent
7. **yaml-cpp**: FetchContent (YAML parsing)
8. **Google Test**: FetchContent (unit testing)
9. **stb_image** (future): FetchContent (header-only, texture loading)

**Example CMake**:

```cmake
find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)

FetchContent_MakeAvailable(glm imgui)
```

### Compiler Flags

**All Platforms**:

- C++26: `-std=c++26` (GCC/Clang), `/std:c++latest` (MSVC)
- Warnings: `-Wall -Wextra -Werror -pedantic`
- Optimizations: `-O3` (release), `-O0 -g` (debug)

**Debug**:

- Sanitizers: `-fsanitize=address,undefined` (GCC/Clang)
- Assertions: `LUMA_DEBUG=1`
- Vulkan validation: Enabled

**Release**:

- LTO (Link-Time Optimization): `-flto`
- Strip symbols: `-s`
- No sanitizers, no validation layers

### Build Commands

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build (parallel)
cmake --build build --parallel

# Run tests
ctest --test-dir build

# Install (optional)
cmake --install build --prefix /usr/local
```

---

## 14. Performance Targets

### Frame Budget (30 FPS minimum)

**Total Frame Time**: 33.3 ms (30 FPS)

**Breakdown** (approximate, on Vega 8 @ 1080p medium settings):

- **Input**: <0.1 ms (polling, event processing)
- **ECS Update**: 0.5 ms (paddle movement, ball physics)
- **BVH Build**: 1-2 ms (incremental, only if scene changed)
- **Path Tracing**: 25 ms (largest cost, compute shader)
- **Denoising**: 3 ms (spatial à-trous filter)
- **Upscale** (if enabled): 1 ms (bilinear or Lanczos)
- **Tonemap + Present**: 1 ms (final image processing)
- **ImGui/UI**: 1 ms (if visible)

**Adaptive Quality**:

- **Adaptive SPP**: If frame time > 33ms, reduce SPP (e.g., 2 → 1)
- **Adaptive Resolution**: If frame time still > 33ms, reduce internal resolution scale (e.g., 1.0 → 0.75)
- **Fallback**: Minimum 0.5x scale, 1 SPP (still looks decent with denoising)

### Performance Profiling

**Built-in Profiler** (custom ImGui):

- **CPU Timers**: Per-system timings (via `std::chrono`)
- **GPU Timestamps**: Vulkan timestamp queries (16 per frame)
- **Memory Stats**: Allocations, GPU usage (via VMA stats)
- **Frame Graph**: ImGui line plot (FPS, frame time over last 120 frames)

**GPU Timestamp Queries** (breakdown):

1. BVH upload
2. Ray generation
3. BVH traversal + intersection
4. Material shading
5. Accumulation
6. Denoising (spatial filter)
7. Tonemapping
8. UI rendering
9. Total GPU time

**Tracy Integration** (optional):

- Deep profiling with flame graphs
- Requires Tracy client connection (external tool)
- Compile with `-DLUMA_TRACY=ON`

### Optimization Priorities

**Phase 1** (MVP):

- Correct implementation (functionality first)
- Basic profiling (identify bottlenecks)

**Phase 2** (Post-MVP):

- Wavefront path tracing optimization (coherent memory access)
- BVH quality tuning (SAH cost function tweaking)
- Compute shader optimization (reduce register pressure, increase occupancy)

**Phase 3** (Polish):

- SIMD optimizations (AVX2 for CPU-side BVH build)
- Async compute overlap (compute queue + graphics queue parallel)
- GPU-driven BVH build (move to compute shader for dynamic scenes)

---

## 15. Development Roadmap

### Phase 0: Foundation (Week 1)

**Goal**: Core infrastructure + basic Vulkan rendering

**Tasks**:

- [x] Project structure (CMakeLists.txt, folder layout)
- [ ] Core module: Logging, math (GLM), threading (job system stub)
- [ ] Vulkan backend: Instance, device, queues, swapchain
- [ ] VMA integration (GPU memory management)
- [ ] Window + input (GLFW integration)
- [ ] ImGui integration (basic UI overlay)
- [ ] Google Test setup (example test)
- [ ] Clang-format + clang-tidy config

**Milestone**: Window opens, ImGui renders, Vulkan validation layers enabled

---

### Phase 1: Compute Shaders + Scene (Week 2)

**Goal**: Compute shader dispatch + basic ECS

**Tasks**:

- [ ] Shader compilation pipeline (GLSL → SPIR-V, caching)
- [ ] Compute pipeline abstraction
- [ ] Simple compute shader (e.g., fill image with gradient)
- [ ] ECS implementation: Entities, components, queries
- [ ] Transform, Geometry (SDF), Material components
- [ ] Scene serialization (YAML save/load)
- [ ] Procedural geometry generation (SDF sphere, box, plane)

**Milestone**: Compute shader writes gradient to image, displays in window. ECS can create/query entities.

---

### Phase 2: BVH + Ray Tracing MVP (Week 3)

**Goal**: First path-traced image

**Tasks**:

- [ ] BVH builder (CPU, SAH binned, breadth-first layout)
- [ ] Upload BVH + geometry to GPU buffers
- [ ] Path tracer compute shader (megakernel):
  - [ ] Ray generation (camera → screen)
  - [ ] BVH traversal (stackless)
  - [ ] SDF intersection (sphere, box, plane)
  - [ ] Basic shading (Lambertian diffuse)
- [ ] Accumulation (progressive rendering)
- [ ] Tonemapping (ACES + gamma correction)

**Milestone**: Static scene (sphere + ground plane) rendered with path tracing, progressive accumulation.

---

### Phase 3: Materials + Lighting (Week 4)

**Goal**: PBR materials, emissive lighting

**Tasks**:

- [ ] Material system: Metallic-roughness PBR
- [ ] BRDF evaluation (GGX microfacet, Fresnel)
- [ ] Emissive materials (geometry as lights)
- [ ] Multiple importance sampling (MIS)
- [ ] Material hot-reload (YAML tweaking)

**Milestone**: Scene with metal sphere, diffuse plane, emissive area light. Realistic shading.

---

### Phase 4: Denoising + Camera (Week 5)

**Goal**: Real-time quality + camera controls

**Tasks**:

- [ ] Spatial denoiser (à-trous wavelet filter, compute shader)
- [ ] G-buffer: Albedo, normal, depth outputs
- [ ] Camera system: Orthographic + perspective
- [ ] Camera transition (lerp between modes)
- [ ] Free-fly camera (debug, WASD + mouse)

**Milestone**: Clean denoised image at 2 SPP. Camera can switch between ortho/perspective smoothly.

---

### Phase 5: Physics + Pong Gameplay (Week 6)

**Goal**: Playable Pong prototype

**Tasks**:

- [ ] Physics module: SDF collision detection
- [ ] Collision response: Arcade mode (perfect reflections)
- [ ] Collision response: Realistic mode (physical materials)
- [ ] Input mapping (keyboard controls for paddles)
- [ ] Paddle entity: Movement, constraints (stay in bounds)
- [ ] Ball entity: Velocity, collision, respawn
- [ ] Score tracking (simple counter)
- [ ] Game state machine (menu, playing, game over)

**Milestone**: Playable Pong with arcade physics. Ball bounces, paddles move, score increments.

---

### Phase 6: Settings System (Week 7)

**Goal**: Configurable everything

**Tasks**:

- [ ] Settings module: Builder API + YAML override
- [ ] ImGui auto-generation (widgets from schema)
- [ ] Settings categories: Rendering, Performance, Graphics, Gameplay, Debug
- [ ] Hot-reload YAML settings
- [ ] Adaptive quality (adaptive SPP, adaptive resolution)
- [ ] Presets (Low/Medium/High/Ultra)

**Milestone**: Sleek settings UI. User can tweak all parameters live. Settings save/load from YAML.

---

### Phase 7: Polish + Optimization (Week 8)

**Goal**: Performance + visual quality

**Tasks**:

- [ ] Wavefront path tracing (sort rays by material)
- [ ] Persistent ray buffers (compaction)
- [ ] BVH optimization (incremental rebuild)
- [ ] Profiler UI (CPU/GPU timings, frame graph)
- [ ] GPU timestamp queries (detailed breakdown)
- [ ] Bloom post-processing (Kawase blur)
- [ ] Audio integration (ball hit sound, paddle hit sound)

**Milestone**: 30+ FPS on Vega 8 @ 1080p medium settings. Polished visuals with bloom.

---

### Phase 8: Documentation + Release (Week 9+)

**Goal**: Shareable project

**Tasks**:

- [ ] README.md (overview, build instructions) [root]
- [ ] docs/ARCHITECTURE.md (this document, refined)
- [ ] docs/API.md (how to use LUMA for other projects)
- [ ] docs/BUILDING.md (detailed per-platform)
- [ ] docs/CONTRIBUTING.md (coding standards, PR process)
- [ ] Doxygen docs (generate HTML)
- [ ] CI/CD: GitHub Actions (build + test on push)
- [ ] Release build (embed shaders, strip symbols)
- [ ] Gameplay video (screen recording, upload)

**Milestone**: GitHub repo public, documentation complete, gameplay video showcasing path-traced Pong.

---

## 16. Code Style

### Naming Conventions

**Types**: `PascalCase`

```cpp
struct Transform;
class JobSystem;
enum class Error;
```

**Functions/Variables**: `snake_case`

```cpp
auto calculate_aabb(const Geometry& geom) -> AABB;
float delta_time = 0.016f;
```

**Constants**: `UPPER_SNAKE_CASE`

```cpp
constexpr float PI = 3.14159265359f;
constexpr uint32_t MAX_BOUNCES = 16;
```

**Namespaces**: `snake_case`

```cpp
namespace luma::render { ... }
namespace luma::physics { ... }
```

**Files**: `snake_case.h` / `snake_case.cpp`

```text
transform.h
job_system.h
path_tracer.cpp
```

### Code Structure

**Indentation**: 2 spaces (no tabs)

**Braces**: K&R style (opening brace on same line for functions, new line for control flow)

```cpp
auto foo() -> int {  // same line
  if (condition) {   // same line
    // ...
  }
}
```

**Line Length**: 100 characters maximum

**Headers**: `#pragma once` (no include guards)

```cpp
#pragma once

#include <vector>
#include "luma/core/types.h"
```

**Includes**: Sorted (standard, third-party, project)

```cpp
#include <vector>
#include <glm/glm.hpp>
#include "luma/core/math.h"
```

### Functional Style

**Immutability**: All data const by default

```cpp
struct Transform {
  const glm::vec3 position;
  const glm::quat rotation;
  const glm::vec3 scale;
};
```

**Pure Functions**: No side effects, return new values

```cpp
// Pure: returns new transform
auto translate(const Transform& t, glm::vec3 offset) -> Transform {
  return {t.position + offset, t.rotation, t.scale};
}

// Impure (avoid):
void translate(Transform& t, glm::vec3 offset) {
  t.position += offset;  // mutation!
}
```

**Constexpr**: Use everywhere possible

```cpp
constexpr auto square(float x) -> float {
  return x * x;
}
```

**Result Types**: No exceptions

```cpp
auto load_file(std::filesystem::path path) -> std::expected<std::string, Error>;
```

### Comments

**Doxygen**: Public API documentation

```cpp
/// @brief Builds a BVH from geometry
/// @param geometries Array of SDF geometries
/// @return BVH root node or error
auto build_bvh(std::span<const Geometry> geometries) -> std::expected<BVHNode, Error>;
```

**Inline Comments**: Explain "why", not "what"

```cpp
// Use breadth-first layout for better GPU cache coherency
std::vector<BVHNode> nodes = build_breadth_first(root);
```

### Testing

**Google Test**: Unit tests for all modules

```cpp
TEST(BVHTest, BuildFromGeometry) {
  std::vector<Geometry> geoms = { /* ... */ };
  auto result = build_bvh(geoms);
  ASSERT_TRUE(result.has_value());
  EXPECT_GT(result->node_count, 0);
}
```

**Property-Based**: Test invariants

```cpp
TEST(BVHTest, Monotonicity) {
  // BVH traversal should visit closer nodes first
  // Test with random rays, ensure distance is monotonic
}
```

---

## Appendix A: Vulkan Extensions Reference

**Core 1.3 Features Used** (available on Vega 8):

- `shaderFloat64`: 64-bit float in shaders (precision)
- `shaderInt64`: 64-bit int (large buffers)
- `shaderStorageImageReadWithoutFormat`: Read from storage images
- `shaderStorageImageWriteWithoutFormat`: Write to storage images
- `fragmentStoresAndAtomics`: Atomic ops in fragment shader (future)

**Extensions NOT Used** (for portability):

- `VK_KHR_ray_tracing_pipeline`: Hardware RT (not on Vega 8)
- `VK_KHR_acceleration_structure`: Hardware BVH (not on Vega 8)
- `VK_AMD_*`: AMD-specific extensions (optional future optimization)

---

## Appendix B: Performance Estimates

**Vega 8 Specs**:

- Compute Units: 8 CUs
- Stream Processors: 512 (8 CUs × 64 threads)
- Compute Performance: ~1.1 TFLOPS (FP32)
- Memory Bandwidth: ~40 GB/s (shared system RAM, dual-channel DDR4-2400)

**Path Tracing Cost** (per frame, 1920×1080, 2 SPP):

- Rays per frame: 1920 × 1080 × 2 = 4,147,200 rays
- Assume 4 bounces average: ~16.5 million ray-BVH tests
- BVH traversal: ~50 cycles per ray (estimate)
- Total cycles: ~825 million
- At 1.1 GHz GPU clock: ~0.75 ms per bounce
- 4 bounces: ~3 ms (optimistic)
- Shading + material eval: ~20 ms (bulk of time)
- **Total path tracing: ~25 ms** (realistic estimate)

**Denoising Cost** (1920×1080, 5×5 kernel, 3 passes):

- Pixels: 2,073,600
- Per-pixel cost: ~100 cycles (filter + edge-stopping)
- Total: ~207 million cycles → ~0.2 ms per pass
- 3 passes: ~0.6 ms (optimistic)
- **Realistic: ~3 ms** (memory bandwidth bound)

**Total Frame Time**: 25 (trace) + 3 (denoise) + 5 (other) = **33 ms → 30 FPS** ✅

---

## Appendix C: File Structure Summary

```text
luma/
├── CMakeLists.txt
├── README.md
├── LICENSE (GPL)
├── docs/
│   ├── LUMA_ARCHITECTURE.md (this file)
│   ├── TODO.md
│   ├── API.md (future)
│   ├── BUILDING.md (future)
│   └── CONTRIBUTING.md (future)
│
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   └── FetchDependencies.cmake
│
├── include/luma/
│   ├── core/
│   │   ├── types.h
│   │   ├── math.h
│   │   ├── logging.h
│   │   ├── jobs.h
│   │   └── memory.h
│   ├── vulkan/
│   │   ├── device.h
│   │   ├── swapchain.h
│   │   ├── command_buffer.h
│   │   └── pipeline.h
│   ├── scene/
│   │   ├── ecs.h
│   │   ├── components.h
│   │   └── serialization.h
│   ├── render/
│   │   ├── bvh.h
│   │   ├── path_tracer.h
│   │   ├── camera.h
│   │   └── material.h
│   ├── physics/
│   │   ├── collision.h
│   │   └── sdf.h
│   ├── settings/
│   │   └── settings.h
│   └── luma.h (master include)
│
├── src/
│   ├── core/
│   ├── vulkan/
│   ├── scene/
│   ├── render/
│   ├── physics/
│   ├── settings/
│   └── main.cpp (entry point for Pong)
│
├── shaders/
│   ├── path_tracer.comp
│   ├── denoise_spatial.comp
│   ├── tonemap.comp
│   └── common/
│       ├── sdf.glsl
│       ├── bvh.glsl
│       └── pbr.glsl
│
├── shaders_cache/
│   └── (SPIR-V cache, gitignored)
│
├── assets/
│   ├── scenes/
│   │   └── pong.yaml
│   └── settings/
│       └── default.yaml
│
├── tests/
│   ├── core/
│   ├── vulkan/
│   ├── scene/
│   └── render/
│
├── docs/
│   ├── LUMA_ARCHITECTURE.md (this file)
│   ├── TODO.md
│   ├── Doxyfile
│   ├── api/
│   └── guides/
│
└── examples/
    └── pong/
        └── (Pong-specific game logic)
```

---

## Appendix D: Key Technologies Summary

| Component | Technology | Rationale |
|-----------|-----------|-----------|
| **Language** | C++26 | Modern features (std::expected, constexpr, concepts) |
| **Graphics API** | Vulkan 1.3 | Compute shaders, portability, low overhead |
| **Ray Tracing** | Compute shader (software) | No RT extensions needed, works on iGPU |
| **BVH** | BVH2, SAH builder | Simple, fast, good quality |
| **ECS** | Custom, archetype-based | Optimized for path tracing (flat arrays) |
| **Math** | GLM | Industry standard, GLSL-compatible |
| **GPU Memory** | VMA | Handles unified memory on iGPU smartly |
| **Windowing** | GLFW | Lightweight, cross-platform |
| **UI** | ImGui (docking branch) | Immediate-mode, dockable windows |
| **Serialization** | yaml-cpp | Human-readable config/scenes |
| **Shader Compiler** | shaderc | GLSL → SPIR-V at runtime |
| **Testing** | Google Test | Industry standard |
| **Build** | CMake 3.20+ | Cross-platform, FetchContent |
| **License** | GPL | Ethical code sharing 💜 |

---

## Appendix E: Glossary

- **BVH**: Bounding Volume Hierarchy (spatial acceleration structure)
- **SAH**: Surface Area Heuristic (BVH build quality metric)
- **SDF**: Signed Distance Field (procedural geometry representation)
- **SPP**: Samples Per Pixel (path tracing quality metric)
- **ECS**: Entity Component System (data-oriented architecture)
- **PBR**: Physically-Based Rendering (realistic material model)
- **MIS**: Multiple Importance Sampling (variance reduction)
- **BRDF**: Bidirectional Reflectance Distribution Function (material shading)
- **VMA**: Vulkan Memory Allocator (GPU memory management)
- **ImGui**: Immediate-Mode Graphical User Interface
- **YAML**: YAML Ain't Markup Language (config file format)
- **GPL**: GNU General Public License (free software license)

---

## Final Notes

This architecture is designed to be:

1. **Functional**: Immutable data, pure functions, composable systems
2. **Modular**: Hot-reloadable DLLs, stable C ABI
3. **Performant**: GPU compute-first, wavefront path tracing, adaptive quality
4. **Developer-Friendly**: Hot-reload everything, ImGui tools, comprehensive profiling
5. **Ethical**: GPL-licensed, open-source by design

**LUMA** is not just an engine—it's a statement that real-time path tracing on integrated graphics is possible, beautiful, and ethical. Let's build something legendary. 💜✨

---

**Document Version**: 1.0.0  
**Last Updated**: October 7, 2025  
**Author**: AI Architect + Developer (that's you! uwu)  
**Status**: Ready for implementation  
