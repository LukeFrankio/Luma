# LUMA Engine - Complete Development TODO

**Project**: Path-Traced Pong on AMD Vega 8  
**Target**: 30 FPS @ 1920×1080  
**Timeline**: ~8 weeks  
**Status**: Phase 0 - In Progress (Foundation)

---

## Table of Contents

1. [Phase 0: Foundation (Week 1)](#phase-0-foundation-week-1)
2. [Phase 1: Compute Shaders + Scene (Week 2)](#phase-1-compute-shaders--scene-week-2)
3. [Phase 2: BVH + Ray Tracing MVP (Week 3)](#phase-2-bvh--ray-tracing-mvp-week-3)
4. [Phase 3: Materials + Lighting (Week 4)](#phase-3-materials--lighting-week-4)
5. [Phase 4: Denoising + Camera (Week 5)](#phase-4-denoising--camera-week-5)
6. [Phase 5: Physics + Pong Gameplay (Week 6)](#phase-5-physics--pong-gameplay-week-6)
7. [Phase 6: Settings System (Week 7)](#phase-6-settings-system-week-7)
8. [Phase 7: Polish + Optimization (Week 8)](#phase-7-polish--optimization-week-8)

---

## Phase 0: Foundation (Week 1)

**Goal**: Core infrastructure + basic Vulkan rendering  
**Milestone**: Window opens, ImGui renders, Vulkan validation layers enabled

### Project Structure

- [x] Create root `CMakeLists.txt`
  - [x] Set minimum CMake version to 4.1 (latest)
  - [x] Set C++26 standard
  - [x] Define project name and version
  - [x] Add subdirectories for modules (commented out, ready to uncomment)
- [x] Create folder structure:
  - [x] `include/luma/` (public headers)
  - [x] `src/` (implementation)
  - [x] `shaders/` (GLSL shaders)
  - [x] `tests/` (Google Test suites)
  - [x] `cmake/` (CMake modules)
  - [x] `assets/` (scenes, settings)
  - [x] `docs/` (documentation - LUMA_ARCHITECTURE.md and TODO.md already here)
- [x] Create `.gitignore`
  - [x] Ignore `build/`, `shaders_cache/`, `.vs/`, `.vscode/`
  - [x] Ignore OS-specific files (`.DS_Store`, `Thumbs.db`)
- [x] Create `README.md` (basic project overview)
- [x] Create `LICENSE` file (GPL)
- [x] Initialize git repository
  - [x] Initial commit with project structure

### CMake Configuration

- [x] Create `cmake/CompilerWarnings.cmake`
  - [x] Add `-Wall -Wextra -Werror -pedantic` for GCC/Clang
  - [x] Add `/W4 /WX` for MSVC
  - [x] Make warnings configurable (option to disable -Werror)
- [x] Create `cmake/Sanitizers.cmake`
  - [x] Add `-fsanitize=address,undefined` for debug builds
  - [x] Platform-specific handling (GCC/Clang only)
- [x] Create `cmake/FetchDependencies.cmake`
  - [x] Set up FetchContent for dependencies
  - [x] All dependencies configured (GLM, VMA, ImGui, GLFW, yaml-cpp, Google Test)
  - [x] Using Vulkan SDK's shaderc (glslc) instead of building from source
- [x] Create code quality tools
  - [x] `.clang-format` (LLVM style with C++26 settings)
  - [x] `.clang-tidy` (comprehensive static analysis)

### Core Module (`luma_core`)

- [x] Create `include/luma/core/types.hpp`
  - [x] Define basic types: `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`
  - [x] Define `Result<T>` as alias for `std::expected<T, Error>`
  - [x] Define `Error` struct with error codes and messages
  - [x] Define `ErrorCode` enum class with comprehensive error codes
  - [x] Define `NonCopyable` and `NonMovable` utility types
- [x] Create `include/luma/core/math.hpp`
  - [x] Include GLM headers
  - [x] Define common types: `vec2`, `vec3`, `vec4`, `mat4`, `quat`
  - [x] Add constexpr math helpers (clamp, lerp, smoothstep, smootherstep)
  - [x] Add mathematical constants (pi, e, sqrt2, deg_to_rad, etc.)
  - [x] Add utility functions (min, max, sign, approx_equal, approx_zero)
- [x] Create `include/luma/core/logging.hpp`
  - [x] Define log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
  - [x] Define log macros: `LOG_INFO()`, `LOG_ERROR()`, etc.
  - [x] Add format string support (std::format)
  - [x] Define Logger class with thread-safe singleton pattern
- [x] Create `src/core/logging.cpp`
  - [x] Implement console output (colored ANSI terminal output)
  - [x] Implement file output (`logs/luma.log`)
  - [x] Thread-safe logging with mutex
  - [x] Include timestamp, thread ID, severity level, source location
- [x] Create `include/luma/core/time.hpp`
  - [x] Define `Timer` class using `std::chrono::steady_clock`
  - [x] Add `delta_time()`, `elapsed()`, `tick()` functions
  - [x] Add `FPSCounter` class for FPS tracking
- [x] Create `src/core/time.cpp`
  - [x] Implement timer functionality with high-resolution clock
  - [x] Implement FPS counter with moving average
- [x] Create `include/luma/core/jobs.hpp`
  - [x] Define `Job` struct (function, data, atomic ref count, parent pointer, generation)
  - [x] Define `JobHandle` struct (ID, generation for stale handle detection)
  - [x] Define `JobSystem` class interface (work-stealing thread pool)
  - [x] Comprehensive Doxygen documentation with examples
- [x] Create `src/core/jobs.cpp`
  - [x] Implement work-stealing job system with std::deque-based queues
  - [x] Create thread pool based on `hardware_concurrency()`
  - [x] Implement job scheduling queue (LIFO for owner, FIFO for thieves)
  - [x] Implement dependency tracking via atomic reference counting
  - [x] Implement parallel_for helper for data-parallel tasks
  - [x] Fixed Job struct storage (using std::unique_ptr to handle std::atomic members)
- [x] Create `include/luma/core/memory.hpp`
  - [x] Define `LinearAllocator` class (bump pointer allocator)
  - [x] Define `PoolAllocator` class (fixed-size free-list allocator)
  - [x] Define allocation tracking helpers (AllocationTracker)
  - [x] Template helpers for type-safe allocation
  - [x] Comprehensive Doxygen documentation
- [x] Create `src/core/memory.cpp`
  - [x] Implement linear allocator (bump pointer, O(1) alloc, reset all at once)
  - [x] Implement pool allocator (free-list, O(1) alloc/dealloc)
  - [x] Add debug tracking (allocation count, size via std::atomic)
  - [x] Fixed Windows compatibility (std::malloc instead of std::aligned_alloc)
  - [x] Fixed error codes and return types (CORE_OUT_OF_MEMORY, std::unexpected wrappers)
- [x] Create `CMakeLists.txt` for core module
  - [x] Define `luma_core` target (static library)
  - [x] Added jobs.cpp and memory.cpp to source files
  - [x] Link GLM
  - [x] Set C++26 standard
  - [x] Apply compiler warnings and sanitizers
- [x] Build verification
  - [x] Zero compilation warnings (fixed GLM warnings with pragma directives)
  - [x] Zero compilation errors
  - [x] Library created: `libluma_core.a` (grown from 3.8 MB to 6.05 MB)
- [x] Create `CMakeLists.txt` for core module
  - [x] Define `luma_core` target (static library)
  - [x] Link GLM
  - [x] Set C++26 standard
  - [x] Apply compiler warnings and sanitizers

### Vulkan Backend Module (`luma_vulkan`)

- [x] Fetch Vulkan SDK
  - [x] Use `find_package(Vulkan REQUIRED)` in CMake
  - [x] Verify Vulkan 1.3 support (SDK 1.4.321 found)
- [x] Fetch VMA (Vulkan Memory Allocator)
  - [x] Use FetchContent to download from GitHub (v3.1.0)
  - [x] Include VMA header (single-header library)
- [x] Create `include/luma/vulkan/instance.hpp`
  - [x] Define `Instance` class with RAII semantics
  - [x] Add methods: `create()`, `handle()`, `validation_layers()`, `has_validation()`
  - [x] Comprehensive Doxygen documentation
- [x] Create `src/vulkan/instance.cpp`
  - [x] Implement instance creation with validation layers
  - [x] Enable `VK_LAYER_KHRONOS_validation` in debug
  - [x] Set up debug messenger callback
  - [x] Query available extensions
  - [x] Platform-specific surface extensions (Win32/Xlib)
- [x] Create `include/luma/vulkan/device.hpp`
  - [x] Define `Device` class with RAII semantics
  - [x] Define `QueueFamilyIndices` struct
  - [x] Add methods: `create()`, `handle()`, `physical_device()`, queue accessors
  - [x] Store queue families (graphics, compute, transfer, present)
- [x] Create `src/vulkan/device.cpp`
  - [x] Implement physical device selection (prefer discrete GPU, fallback to iGPU)
  - [x] Query device features (Vulkan 1.3 features)
  - [x] Create logical device with required features
  - [x] Get queue handles (graphics, compute, transfer, present queues)
  - [x] Device scoring algorithm for best GPU selection
- [x] Create `include/luma/vulkan/swapchain.hpp`
  - [x] Define `Swapchain` class with RAII semantics
  - [x] Define `SwapchainSupportDetails` struct
  - [x] Add methods: `create()`, `acquire_next_image()`, `present()`
- [x] Create `src/vulkan/swapchain.cpp`
  - [x] Query surface capabilities
  - [x] Select surface format (prefer SRGB)
  - [x] Select present mode (prefer MAILBOX, fallback to FIFO)
  - [x] Create swapchain with triple buffering
  - [x] Create image views for swapchain images
  - [x] Handle OUT_OF_DATE errors for window resize
- [x] Create `include/luma/vulkan/command_buffer.hpp`
  - [x] Define `CommandPool` class with RAII semantics
  - [x] Define `CommandBuffer` class (functional wrapper)
  - [x] Add methods: `begin()`, `end()`, `submit()`, `submit_and_wait()`
- [x] Create `src/vulkan/command_buffer.cpp`
  - [x] Create command pool with flags
  - [x] Allocate command buffers (single and multiple)
  - [x] Implement recording helpers
  - [x] Submit with semaphores and fences
- [x] Create `include/luma/vulkan/sync.hpp`
  - [x] Define `Fence` wrapper with RAII
  - [x] Define `Semaphore` wrapper with RAII
  - [x] Define barrier helpers (image, buffer, memory)
- [x] Create `src/vulkan/sync.cpp`
  - [x] Implement fence creation/waiting/reset
  - [x] Implement semaphore creation
  - [x] Implement pipeline barrier helpers
  - [x] Implement transition_image_layout helper
- [x] Create `include/luma/vulkan/memory.hpp`
  - [x] Define `Allocator` class (VMA wrapper)
  - [x] Define `Buffer` class (uses VMA) with template helpers
  - [x] Define `Image` class (uses VMA)
  - [x] Add memory allocation helpers (create_with_data, map_and_write, map_and_read)
- [x] Create `src/vulkan/memory.cpp`
  - [x] Initialize VMA allocator with Vulkan 1.3 support
  - [x] Implement buffer creation (uniform, storage, staging)
  - [x] Implement image creation (storage images for compute)
  - [x] Handle unified memory on iGPU (HOST_VISIBLE | DEVICE_LOCAL)
  - [x] Implement map/unmap, flush/invalidate operations
  - [x] Helper functions: copy_buffer, copy_buffer_to_image, copy_image_to_buffer
- [x] Create `CMakeLists.txt` for vulkan module
  - [x] Define `luma_vulkan` target (static library)
  - [x] Link Vulkan::Vulkan
  - [x] Link VMA (header-only)
  - [x] Link `luma_core`
  - [x] Apply compiler warnings and sanitizers
- [x] Build verification
  - [x] Zero compilation warnings
  - [x] Zero compilation errors
  - [x] Library created: `libluma_vulkan.a` (23.1 MB)

### Window + Input Module (`luma_input`)

- [x] Fetch GLFW
  - [x] Use FetchContent (configured in FetchDependencies.cmake)
- [x] Create `include/luma/input/window.hpp`
  - [x] Define `Window` class with RAII semantics
  - [x] Add methods: `create()`, `should_close()`, `poll_events()`
  - [x] Add window query methods: `width()`, `height()`, `framebuffer_width()`, `framebuffer_height()`
  - [x] Add `is_minimized()`, `wait_while_minimized()`
  - [x] Add `set_resize_callback()` for framebuffer resize events
  - [x] Add `create_surface()` for Vulkan surface creation
  - [x] Comprehensive Doxygen documentation
- [x] Create `src/input/window.cpp`
  - [x] Initialize GLFW (with automatic cleanup via atexit)
  - [x] Create window with Vulkan surface support
  - [x] Set up framebuffer resize callback forwarding
  - [x] Implement window query functions (width, height, minimize state)
  - [x] Implement Vulkan surface creation via glfwCreateWindowSurface
- [x] Create `include/luma/input/input.hpp`
  - [x] Define `Input` class with functional query interface
  - [x] Add keyboard query methods: `is_key_pressed()`, `is_key_just_pressed()`, `is_key_just_released()`
  - [x] Add mouse button query methods: `is_mouse_button_pressed()`, etc.
  - [x] Add mouse position/delta methods: `mouse_position()`, `mouse_delta()`, `mouse_scroll()`
  - [x] Add `set_cursor_mode()` for cursor capture (FPS mode)
  - [x] Comprehensive Doxygen documentation
- [x] Create `src/input/input.cpp`
  - [x] Implement GLFW input polling (keyboard + mouse)
  - [x] Store current and previous frame state (for edge detection)
  - [x] Handle mouse movement and delta calculation
  - [x] Implement scroll callback (accumulate scroll events over frame)
  - [x] Implement edge detection (just-pressed/just-released queries)
- [x] Create `CMakeLists.txt` for input module
  - [x] Define `luma_input` target (static library)
  - [x] Link GLFW
  - [x] Link `luma_core` (for types, logging, math)
  - [x] Link Vulkan::Vulkan (for VkSurfaceKHR)
  - [x] Apply compiler warnings and sanitizers
- [x] Build verification
  - [x] Zero compilation warnings
  - [x] Zero compilation errors
  - [x] Library created: `libluma_input.a` (3.6 MB)

### ImGui Integration

- [ ] Fetch ImGui
  - [ ] Use FetchContent (docking branch)
  - [ ] Include ImGui source files in build
- [ ] Create `include/luma/editor/imgui_context.h`
  - [ ] Define `ImGuiContext` class
  - [ ] Add methods: `init()`, `shutdown()`, `begin_frame()`, `end_frame()`, `render()`
- [ ] Create `src/editor/imgui_context.cpp`
  - [ ] Initialize ImGui
  - [ ] Set up ImGui GLFW backend
  - [ ] Set up ImGui Vulkan backend
  - [ ] Create descriptor pool for ImGui
  - [ ] Create render pass for ImGui
- [ ] Create basic ImGui test window
  - [ ] Display "Hello LUMA" text
  - [ ] Display FPS counter

### Google Test Setup

- [ ] Fetch Google Test
  - [ ] Use FetchContent
- [ ] Create `tests/CMakeLists.txt`
  - [ ] Define `luma_tests` target (executable)
  - [ ] Link GTest::gtest, GTest::gtest_main
  - [ ] Link all LUMA modules
- [ ] Create `tests/core/test_logging.cpp`
  - [ ] Test log output to string buffer
  - [ ] Verify log levels work correctly
- [ ] Create `tests/core/test_math.cpp`
  - [ ] Test vector operations (add, subtract, dot, cross)
  - [ ] Test matrix operations (multiply, transpose)
  - [ ] Test quaternion operations (multiply, slerp)
- [ ] Enable CTest
  - [ ] Add `enable_testing()` in root CMakeLists.txt
  - [ ] Configure test discovery

### Code Quality Tools

- [ ] Create `.clang-format` file
  - [ ] Set style: `BasedOnStyle: Google` or custom
  - [ ] Configure indent: 2 spaces
  - [ ] Configure line length: 100 chars
  - [ ] Configure brace style: K&R for functions
- [ ] Create `.clang-tidy` file
  - [ ] Enable modernize checks
  - [ ] Enable performance checks
  - [ ] Enable readability checks
  - [ ] Disable overly strict checks
- [ ] Test formatting
  - [ ] Run `clang-format` on all source files
  - [ ] Verify output matches style guide

### Build and Test

- [ ] Configure CMake
  - [ ] `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- [ ] Build all targets
  - [ ] `cmake --build build --parallel`
- [ ] Run tests
  - [ ] `ctest --test-dir build --output-on-failure`
- [ ] Verify window opens
  - [ ] Run main executable
  - [ ] Check Vulkan validation layers output
  - [ ] Verify ImGui test window appears

---

## Phase 1: Compute Shaders + Scene (Week 2)

**Goal**: Compute shader dispatch + basic ECS  
**Milestone**: Compute shader writes gradient to image, displays in window. ECS can create/query entities.

### Shader Compilation System

- [ ] Fetch shaderc
  - [ ] Use `find_package(shaderc)` (from Vulkan SDK) or FetchContent
- [ ] Create `include/luma/asset/shader_compiler.h`
  - [ ] Define `ShaderCompiler` class
  - [ ] Add methods: `compile_glsl()`, `load_spirv()`, `get_cache_path()`
- [ ] Create `src/asset/shader_compiler.cpp`
  - [ ] Initialize shaderc compiler
  - [ ] Implement GLSL → SPIR-V compilation
  - [ ] Compute hash of GLSL source
  - [ ] Check cache directory for existing SPIR-V
  - [ ] Save compiled SPIR-V to cache
  - [ ] Load SPIR-V from cache on subsequent runs
- [ ] Create cache directory structure
  - [ ] `shaders_cache/` folder (add to .gitignore)
- [ ] Test shader compilation
  - [ ] Write simple test shader (compute shader that returns value)
  - [ ] Compile to SPIR-V
  - [ ] Verify cache works

### Compute Pipeline Abstraction

- [ ] Create `include/luma/vulkan/pipeline.h`
  - [ ] Define `ComputePipeline` class
  - [ ] Add methods: `create()`, `destroy()`, `bind()`, `dispatch()`
- [ ] Create `src/vulkan/pipeline.cpp`
  - [ ] Create compute pipeline from SPIR-V
  - [ ] Create pipeline layout (descriptor sets, push constants)
  - [ ] Create descriptor set layout
  - [ ] Create descriptor pool
  - [ ] Allocate descriptor sets
- [ ] Create `include/luma/vulkan/descriptor.h`
  - [ ] Define `DescriptorSet` class
  - [ ] Add methods: `bind_buffer()`, `bind_image()`, `update()`
- [ ] Create `src/vulkan/descriptor.cpp`
  - [ ] Implement descriptor set updates
  - [ ] Bind buffers to descriptor sets
  - [ ] Bind images to descriptor sets

### Simple Gradient Compute Shader

- [ ] Create `shaders/gradient.comp`
  - [ ] Write GLSL compute shader
  - [ ] Use `layout(local_size_x = 8, local_size_y = 8) in;`
  - [ ] Write gradient to storage image (R8G8B8A8_UNORM)
  - [ ] Compute color based on `gl_GlobalInvocationID`
- [ ] Compile gradient shader
  - [ ] Use shader compiler to generate SPIR-V
- [ ] Create storage image
  - [ ] Create VkImage (1920×1080, R8G8B8A8_UNORM)
  - [ ] Create VkImageView
  - [ ] Allocate memory with VMA (DEVICE_LOCAL)
- [ ] Dispatch gradient shader
  - [ ] Bind compute pipeline
  - [ ] Bind descriptor set (storage image)
  - [ ] Dispatch (1920/8, 1080/8, 1) workgroups
  - [ ] Insert pipeline barrier (wait for compute to finish)
- [ ] Copy image to swapchain
  - [ ] Use `vkCmdBlitImage()` or render to quad
  - [ ] Present swapchain image
- [ ] Verify gradient appears in window

### Scene Module - ECS Foundation

- [ ] Create `include/luma/scene/entity.h`
  - [ ] Define `Entity` type (uint32_t ID + generation)
  - [ ] Define `EntityHandle` struct
- [ ] Create `include/luma/scene/component.h`
  - [ ] Define `Transform` component (position, rotation, scale)
  - [ ] Define `Geometry` component (SDF type, parameters)
  - [ ] Define `Material` component (PBR parameters)
- [ ] Create `include/luma/scene/world.h`
  - [ ] Define `World` class (ECS container)
  - [ ] Add methods: `create_entity()`, `destroy_entity()`, `add_component()`, `get_component()`, `query()`
- [ ] Create `src/scene/world.cpp`
  - [ ] Implement archetype storage (group entities by component signature)
  - [ ] Implement sparse set for entity lookup
  - [ ] Implement component storage (SoA layout)
  - [ ] Implement entity creation (return EntityHandle)
  - [ ] Implement component addition (move entity to correct archetype)
  - [ ] Implement queries (return span of components)
- [ ] Create `include/luma/scene/archetype.h`
  - [ ] Define `Archetype` class (stores entities with same components)
  - [ ] Define `ComponentArray<T>` (contiguous array of components)
- [ ] Create `src/scene/archetype.cpp`
  - [ ] Implement archetype storage
  - [ ] Implement entity addition to archetype
  - [ ] Implement entity removal from archetype
  - [ ] Implement component access by index
- [ ] Test ECS
  - [ ] Create test entities with Transform component
  - [ ] Query entities with Transform
  - [ ] Verify query returns correct components
  - [ ] Test entity destruction

### Procedural Geometry (SDF)

- [ ] Create `include/luma/render/sdf.h`
  - [ ] Define `SDFType` enum (SPHERE, BOX, PLANE)
  - [ ] Define `SDF` union struct (parameters for each type)
  - [ ] Add SDF evaluation functions (C++ versions)
- [ ] Create `src/render/sdf.cpp`
  - [ ] Implement `sdf_sphere()` (distance from point to sphere)
  - [ ] Implement `sdf_box()` (distance from point to box with rounding)
  - [ ] Implement `sdf_plane()` (distance from point to plane)
- [ ] Create GLSL SDF library
  - [ ] Create `shaders/common/sdf.glsl`
  - [ ] Port C++ SDF functions to GLSL
  - [ ] Add SDF operations (union, intersection, subtraction)
- [ ] Test SDF evaluation
  - [ ] Create test entities with SDF geometry
  - [ ] Evaluate SDF at various points
  - [ ] Verify distances are correct

### Scene Serialization (YAML)

- [ ] Fetch yaml-cpp
  - [ ] Use FetchContent
- [ ] Create `include/luma/scene/serialization.h`
  - [ ] Define `SceneSerializer` class
  - [ ] Add methods: `save_scene()`, `load_scene()`
- [ ] Create `src/scene/serialization.cpp`
  - [ ] Implement YAML serialization for entities
  - [ ] Serialize Transform component (position, rotation, scale as arrays)
  - [ ] Serialize Geometry component (type as string, parameters as nested map)
  - [ ] Serialize Material component (all PBR parameters)
  - [ ] Implement YAML deserialization
  - [ ] Create entities from YAML data
  - [ ] Add components to entities
- [ ] Create example scene file
  - [ ] Create `assets/scenes/test_scene.yaml`
  - [ ] Add sphere entity
  - [ ] Add box entity
  - [ ] Add plane entity (ground)
- [ ] Test scene loading
  - [ ] Load test scene from YAML
  - [ ] Verify entities created correctly
  - [ ] Verify components match YAML data

### Integration Test

- [ ] Load scene from YAML
- [ ] Upload geometry to GPU (prepare for next phase)
- [ ] Display gradient in window (from compute shader)
- [ ] Verify ImGui can show scene hierarchy (list of entities)

---

## Phase 2: BVH + Ray Tracing MVP (Week 3)

**Goal**: First path-traced image  
**Milestone**: Static scene (sphere + ground plane) rendered with path tracing, progressive accumulation.

### BVH Builder (CPU-Side)

- [ ] Create `include/luma/render/bvh.h`
  - [ ] Define `BVHNode` struct (aabb_min, aabb_max, left_child, right_child)
  - [ ] Define `AABB` struct (min, max points)
  - [ ] Define `BVHBuilder` class
  - [ ] Add methods: `build()`, `rebuild_subtree()`
- [ ] Create `src/render/bvh.cpp`
  - [ ] Implement AABB calculation from geometry
  - [ ] Implement SAH (Surface Area Heuristic) cost function
  - [ ] Implement binned SAH builder:
    - [ ] Partition primitives into bins
    - [ ] Compute SAH cost for each split candidate
    - [ ] Choose best split (minimum cost)
    - [ ] Recursively build subtrees
  - [ ] Convert binary tree to breadth-first array layout
  - [ ] Implement `rebuild_subtree()` for incremental updates
- [ ] Implement AABB helpers
  - [ ] `aabb_union()` (merge two AABBs)
  - [ ] `aabb_surface_area()` (for SAH)
  - [ ] `aabb_intersect_ray()` (for traversal testing)
- [ ] Test BVH construction
  - [ ] Create test scene with spheres
  - [ ] Build BVH
  - [ ] Verify tree structure (no overlap, correct hierarchy)
  - [ ] Verify breadth-first layout

### BVH Upload to GPU

- [ ] Create GPU buffer for BVH nodes
  - [ ] Allocate storage buffer (DEVICE_LOCAL | HOST_VISIBLE on iGPU)
  - [ ] Copy BVH nodes from CPU to GPU
  - [ ] Create descriptor for BVH buffer
- [ ] Create GPU buffer for geometry data
  - [ ] Store SDF parameters (sphere radius, box extents, etc.)
  - [ ] Create descriptor for geometry buffer
- [ ] Create GPU buffer for materials
  - [ ] Store PBR parameters (base_color, metallic, roughness, emissive)
  - [ ] Create descriptor for material buffer
- [ ] Bind buffers to compute shader
  - [ ] Update descriptor sets
  - [ ] Verify data uploaded correctly (readback test)

### Path Tracer Compute Shader (Megakernel)

- [ ] Create `shaders/path_tracer.comp`
  - [ ] Set local_size to 8×16 (128 threads, 2 wavefronts)
  - [ ] Define uniform buffer for camera data
  - [ ] Define storage buffer for BVH
  - [ ] Define storage buffer for geometry
  - [ ] Define storage buffer for materials
  - [ ] Define storage image for output (RGBA32F)
- [ ] Implement ray generation
  - [ ] Compute ray from pixel coords
  - [ ] Handle orthographic vs perspective projection
  - [ ] Generate primary ray per pixel
- [ ] Implement BVH traversal (stackless)
  - [ ] Include `shaders/common/bvh.glsl` helper
  - [ ] Traverse BVH using parent pointers
  - [ ] Test AABB intersection
  - [ ] Find closest hit
- [ ] Implement SDF intersection
  - [ ] Include `shaders/common/sdf.glsl`
  - [ ] Evaluate SDF at ray position
  - [ ] Use sphere tracing (march along ray until hit)
  - [ ] Return hit point, normal, material ID
- [ ] Implement basic shading (Lambertian diffuse)
  - [ ] Compute cosine-weighted hemisphere sample
  - [ ] Bounce ray in random direction
  - [ ] Accumulate color (multiply by albedo, scale by cosine)
  - [ ] Handle Russian roulette for path termination
- [ ] Implement multi-bounce path tracing
  - [ ] Loop for N bounces (default 4)
  - [ ] Accumulate throughput (color attenuation)
  - [ ] Break if ray misses (environment contribution)

### Camera System

- [ ] Create `include/luma/render/camera.h`
  - [ ] Define `Camera` class
  - [ ] Define `CameraMode` enum (ORTHOGRAPHIC, PERSPECTIVE, FREE_FLY)
  - [ ] Add methods: `get_projection_matrix()`, `get_view_matrix()`, `generate_ray()`
- [ ] Create `src/render/camera.cpp`
  - [ ] Implement orthographic projection
  - [ ] Implement perspective projection
  - [ ] Implement ray generation from screen coords
  - [ ] Update camera uniform buffer
- [ ] Create camera uniform buffer
  - [ ] Define `CameraUniforms` struct (projection, view, position, forward, etc.)
  - [ ] Allocate uniform buffer (HOST_VISIBLE)
  - [ ] Update buffer per frame

### Accumulation (Progressive Rendering)

- [ ] Create accumulation buffer
  - [ ] Allocate RGBA32F image (same size as output)
  - [ ] Initialize to zero
- [ ] Modify path tracer shader
  - [ ] Read previous accumulation value
  - [ ] Add current sample
  - [ ] Write average to output image
  - [ ] Store new accumulation value
- [ ] Track sample count
  - [ ] Increment per frame
  - [ ] Reset on camera movement or scene change

### Tonemapping + Gamma Correction

- [ ] Create `shaders/tonemap.comp`
  - [ ] Read HDR image (RGBA32F)
  - [ ] Apply ACES filmic tonemapping
  - [ ] Apply gamma correction (sRGB, 2.2 gamma)
  - [ ] Write LDR image (RGBA8_SRGB)
- [ ] Create tonemap pipeline
  - [ ] Compile shader to SPIR-V
  - [ ] Create compute pipeline
  - [ ] Bind HDR input and LDR output images
- [ ] Dispatch tonemap shader
  - [ ] Run after path tracer
  - [ ] Copy LDR image to swapchain

### Integration

- [ ] Load test scene (sphere + ground plane)
- [ ] Build BVH from scene geometry
- [ ] Upload BVH + geometry + materials to GPU
- [ ] Dispatch path tracer compute shader
- [ ] Accumulate samples over multiple frames
- [ ] Dispatch tonemap shader
- [ ] Display result in window
- [ ] Verify static scene renders correctly
- [ ] Verify accumulation improves quality over time

---

## Phase 3: Materials + Lighting (Week 4)

**Goal**: PBR materials, emissive lighting  
**Milestone**: Scene with metal sphere, diffuse plane, emissive area light. Realistic shading.

### PBR Material System

- [ ] Expand `Material` component
  - [ ] Add `type` field (DIFFUSE, METALLIC, DIELECTRIC, EMISSIVE, MIRROR)
  - [ ] Add IOR (index of refraction) for dielectric
- [ ] Update material buffer on GPU
  - [ ] Add material type field
  - [ ] Add IOR field
- [ ] Create `shaders/common/pbr.glsl`
  - [ ] Implement Fresnel (Schlick approximation)
  - [ ] Implement GGX microfacet distribution
  - [ ] Implement Smith G term (geometry function)
  - [ ] Implement Disney BRDF evaluation

### BRDF Evaluation

- [ ] Implement metallic BRDF
  - [ ] Sample specular lobe (GGX distribution)
  - [ ] Evaluate Fresnel for metals (use base_color as F0)
  - [ ] Compute importance-sampled direction
- [ ] Implement diffuse BRDF (Lambert)
  - [ ] Cosine-weighted hemisphere sampling
  - [ ] Simple diffuse evaluation (albedo / π)
- [ ] Implement dielectric BRDF
  - [ ] Compute Fresnel (reflection vs refraction)
  - [ ] Sample reflection or refraction based on Fresnel
  - [ ] Handle total internal reflection
- [ ] Implement mirror BRDF
  - [ ] Perfect specular reflection
  - [ ] Simple reflection direction calculation
- [ ] Update path tracer shader
  - [ ] Call appropriate BRDF based on material type
  - [ ] Compute sample direction
  - [ ] Evaluate BRDF contribution

### Emissive Materials

- [ ] Add emissive evaluation to shader
  - [ ] Check if material is emissive
  - [ ] Add emissive contribution to path radiance
  - [ ] Scale by emissive color/intensity
- [ ] Implement explicit light sampling (NEE - Next Event Estimation)
  - [ ] Build light list (entities with emissive materials)
  - [ ] Sample random light from list
  - [ ] Cast shadow ray to light
  - [ ] Add light contribution if not occluded

### Multiple Importance Sampling (MIS)

- [ ] Implement MIS weight calculation
  - [ ] Power heuristic (balance between BRDF and light sampling)
  - [ ] Combine BRDF sample and light sample
- [ ] Update path tracer shader
  - [ ] Sample both BRDF and light
  - [ ] Compute MIS weights
  - [ ] Combine contributions with weights
  - [ ] Reduce variance in lighting

### Material Hot-Reload

- [ ] Implement YAML material definitions
  - [ ] Extend scene format to include material overrides
  - [ ] Load materials from YAML
- [ ] Watch material YAML file
  - [ ] Use file watcher (from Phase 1)
  - [ ] Detect changes to material files
- [ ] Reload materials on change
  - [ ] Reparse YAML
  - [ ] Update material buffer on GPU
  - [ ] Reset accumulation

### Test Scene

- [ ] Create `assets/scenes/pbr_test.yaml`
  - [ ] Add metal sphere (metallic = 1.0, roughness = 0.2)
  - [ ] Add diffuse sphere (metallic = 0.0, roughness = 1.0)
  - [ ] Add emissive quad (area light)
  - [ ] Add ground plane (diffuse)
- [ ] Load and render test scene
- [ ] Verify PBR shading looks realistic
- [ ] Verify emissive geometry lights the scene
- [ ] Verify MIS reduces noise

---

## Phase 4: Denoising + Camera (Week 5)

**Goal**: Real-time quality + camera controls  
**Milestone**: Clean denoised image at 2 SPP. Camera can switch between ortho/perspective smoothly.

### G-Buffer Generation

- [ ] Extend path tracer shader outputs
  - [ ] Output albedo (base color) to separate image
  - [ ] Output normal (world-space) to separate image
  - [ ] Output depth (distance from camera) to separate image
- [ ] Create G-buffer images
  - [ ] Allocate albedo image (RGBA8)
  - [ ] Allocate normal image (RGBA16F)
  - [ ] Allocate depth image (R32F)
- [ ] Update path tracer to write G-buffer
  - [ ] Write primary hit albedo
  - [ ] Write primary hit normal
  - [ ] Write primary hit depth

### Spatial Denoiser (À-Trous Wavelet Filter)

- [ ] Create `shaders/denoise_spatial.comp`
  - [ ] Read noisy input image
  - [ ] Read G-buffer (albedo, normal, depth)
  - [ ] Implement 5×5 à-trous kernel
  - [ ] Compute edge-stopping weights:
    - [ ] Normal similarity
    - [ ] Depth similarity
    - [ ] Luminance similarity
  - [ ] Weighted average of neighboring pixels
  - [ ] Write denoised output
- [ ] Implement multi-pass denoising
  - [ ] Create ping-pong buffers (double buffering)
  - [ ] Run 3-5 passes with increasing kernel size
  - [ ] Each pass reads from previous output
- [ ] Compile denoiser shader
  - [ ] Create compute pipeline
  - [ ] Bind G-buffer images
  - [ ] Bind input/output images
- [ ] Dispatch denoiser
  - [ ] Run after path tracer
  - [ ] Run before tonemapping
- [ ] Test denoising
  - [ ] Render noisy image (1-2 SPP)
  - [ ] Apply denoiser
  - [ ] Verify quality improvement

### Camera Modes Implementation

- [ ] Implement orthographic camera
  - [ ] Set orthographic projection matrix
  - [ ] Generate parallel rays
  - [ ] Update camera uniform buffer
- [ ] Implement perspective camera
  - [ ] Set perspective projection matrix (FOV, aspect, near, far)
  - [ ] Generate rays from camera position through pixels
  - [ ] Update camera uniform buffer
- [ ] Implement free-fly camera (debug)
  - [ ] WASD for movement
  - [ ] Mouse for rotation
  - [ ] Update view matrix each frame
  - [ ] Handle input in camera update function

### Camera Transition (Lerp)

- [ ] Create camera transition system
  - [ ] Store source and target camera states
  - [ ] Interpolate projection matrix over time
  - [ ] Use smoothstep for easing
- [ ] Implement transition trigger
  - [ ] Detect camera mode change (user input)
  - [ ] Start transition (duration = 0.5 seconds)
  - [ ] Lerp from current to target over time
- [ ] Update path tracer during transition
  - [ ] Use interpolated camera matrix
  - [ ] Reset accumulation during transition
  - [ ] Resume accumulation after transition complete

### Camera Controls

- [ ] Add keyboard bindings
  - [ ] `1` key: Switch to orthographic
  - [ ] `2` key: Switch to perspective
  - [ ] `F` key: Toggle free-fly mode
- [ ] Add mouse controls (free-fly mode)
  - [ ] Capture mouse cursor
  - [ ] Track mouse delta
  - [ ] Update camera rotation
- [ ] Add WASD controls (free-fly mode)
  - [ ] W/S: Move forward/backward
  - [ ] A/D: Strafe left/right
  - [ ] Q/E: Move up/down (optional)
  - [ ] Speed multiplier with Shift

### Phase 4 Integration

- [ ] Render test scene at 2 SPP
- [ ] Apply spatial denoiser
- [ ] Display denoised result
- [ ] Switch between orthographic and perspective cameras
- [ ] Verify smooth transition
- [ ] Enable free-fly camera
- [ ] Verify WASD + mouse controls work
- [ ] Check denoised quality is acceptable

---

## Phase 5: Physics + Pong Gameplay (Week 6)

**Goal**: Playable Pong prototype  
**Milestone**: Playable Pong with arcade physics. Ball bounces, paddles move, score increments.

### Physics Module - SDF Collision Detection

- [ ] Create `include/luma/physics/collision.h`
  - [ ] Define `CollisionInfo` struct (normal, penetration depth, contact point)
  - [ ] Define collision query functions
- [ ] Create `src/physics/collision.cpp`
  - [ ] Implement SDF-based collision test
  - [ ] Evaluate SDF at entity positions
  - [ ] Detect collision (distance < 0)
  - [ ] Compute penetration depth (-distance)
  - [ ] Compute collision normal (gradient of SDF)
- [ ] Implement broadphase
  - [ ] For Pong: None needed (<10 entities)
  - [ ] Test all pairs (O(n²) acceptable)
- [ ] Implement narrowphase
  - [ ] Use SDF distance queries
  - [ ] Return collision info if collision detected

### Collision Response - Arcade Mode

- [ ] Create `include/luma/physics/response.h`
  - [ ] Define `PhysicsMode` enum (ARCADE, REALISTIC)
  - [ ] Define collision response functions
- [ ] Create `src/physics/response.cpp` (arcade mode)
  - [ ] Implement perfect reflection: `v_out = reflect(v_in, normal)`
  - [ ] Normalize velocity to maintain constant speed
  - [ ] No friction, no spin
  - [ ] Optional: Slight acceleration on paddle hit
- [ ] Apply collision response
  - [ ] Update entity velocity component
  - [ ] Separate entities (push apart by penetration depth)

### Collision Response - Realistic Mode

- [ ] Implement realistic physics response
  - [ ] Compute impulse based on restitution
  - [ ] Apply impulse to velocities: `v += j * normal / mass`
  - [ ] Compute angular velocity from contact point
  - [ ] Apply friction (tangential impulse)
- [ ] Add spin to ball
  - [ ] Store angular velocity in RigidBody component
  - [ ] Affect trajectory based on spin
- [ ] Make response mode toggleable
  - [ ] Store current mode in settings
  - [ ] Branch in collision response function

### Paddle Entity and Logic

- [ ] Create `Paddle` component (already defined in Phase 2)
  - [ ] Fields: player_id, speed
- [ ] Implement paddle movement system
  - [ ] Query entities with Paddle + Transform + RigidBody
  - [ ] Read input (W/S for player 0, Up/Down for player 1)
  - [ ] Update velocity based on input
  - [ ] Apply speed from Paddle component
- [ ] Implement paddle constraints
  - [ ] Clamp paddle position to playfield bounds
  - [ ] Prevent paddles from overlapping with walls
- [ ] Add to scene
  - [ ] Create left paddle entity (player 0)
  - [ ] Create right paddle entity (player 1)
  - [ ] Set paddle geometry (SDF box, vertical)
  - [ ] Set paddle material (metallic or emissive)

### Ball Entity and Logic

- [ ] Create `Ball` component (already defined in Phase 2)
  - [ ] Fields: base_speed, current_speed
- [ ] Implement ball physics system
  - [ ] Query entities with Ball + Transform + RigidBody
  - [ ] Update position based on velocity
  - [ ] Handle ball-paddle collision
  - [ ] Handle ball-wall collision
  - [ ] Handle ball leaving playfield (score point)
- [ ] Implement ball respawn
  - [ ] Detect ball out of bounds (left or right)
  - [ ] Award point to opponent
  - [ ] Reset ball to center
  - [ ] Reset ball velocity (random angle)
- [ ] Add to scene
  - [ ] Create ball entity
  - [ ] Set ball geometry (SDF sphere)
  - [ ] Set ball material (metallic or emissive)
  - [ ] Set initial velocity

### Wall Entities

- [ ] Create wall entities (top and bottom)
  - [ ] Top wall: SDF plane or box
  - [ ] Bottom wall: SDF plane or box
  - [ ] Set wall material (diffuse, off-white)
- [ ] Add walls to scene
  - [ ] Position walls at playfield boundaries
- [ ] Handle ball-wall collision
  - [ ] Reflect ball velocity
  - [ ] Play sound effect (future)

### Score Tracking

- [ ] Create score state
  - [ ] Store player 0 score
  - [ ] Store player 1 score
- [ ] Increment score on goal
  - [ ] Detect ball leaving left side (player 1 scores)
  - [ ] Detect ball leaving right side (player 0 scores)
  - [ ] Update score state
- [ ] Display score
  - [ ] Use ImGui to draw score text
  - [ ] Position score display at top of screen
  - [ ] Update score text each frame

### Game State Machine

- [ ] Define game states
  - [ ] MENU: Show start menu
  - [ ] PLAYING: Active gameplay
  - [ ] PAUSED: Game paused
  - [ ] GAME_OVER: End screen (first to N points wins)
- [ ] Implement state transitions
  - [ ] MENU → PLAYING: On start button press
  - [ ] PLAYING ↔ PAUSED: On pause key (ESC)
  - [ ] PLAYING → GAME_OVER: When player reaches winning score
  - [ ] GAME_OVER → MENU: On return to menu
- [ ] Update logic per state
  - [ ] MENU: Show start button, accept input
  - [ ] PLAYING: Run physics, input, rendering
  - [ ] PAUSED: Freeze game, show pause menu
  - [ ] GAME_OVER: Show winner, show restart button
- [ ] Render per state
  - [ ] MENU: Render menu UI
  - [ ] PLAYING: Render scene normally
  - [ ] PAUSED: Render scene dimmed + pause overlay
  - [ ] GAME_OVER: Render winner text + restart button

### Input Mapping

- [ ] Create input mapping system
  - [ ] Define actions (PADDLE_UP, PADDLE_DOWN, PAUSE, etc.)
  - [ ] Map keys to actions (configurable)
- [ ] Default bindings
  - [ ] Player 0: W (up), S (down)
  - [ ] Player 1: Up arrow (up), Down arrow (down)
  - [ ] ESC: Pause
  - [ ] 1/2: Camera mode switch
- [ ] Apply input to paddle movement
  - [ ] Read action state (is PADDLE_UP pressed?)
  - [ ] Update paddle velocity accordingly

### Pong Scene File

- [ ] Create `assets/scenes/pong.yaml`
  - [ ] Define left paddle entity
  - [ ] Define right paddle entity
  - [ ] Define ball entity
  - [ ] Define top wall entity
  - [ ] Define bottom wall entity
  - [ ] Define camera settings (orthographic by default)
  - [ ] Define material settings (emissive ball enabled)
- [ ] Load Pong scene on startup
- [ ] Verify all entities created correctly

### Phase 5 Integration

- [ ] Start game in MENU state
- [ ] Press start to enter PLAYING state
- [ ] Move paddles with W/S and Up/Down
- [ ] Verify ball bounces off paddles
- [ ] Verify ball bounces off walls
- [ ] Verify score increments on goal
- [ ] Verify ball respawns after goal
- [ ] Verify game transitions to GAME_OVER when player wins
- [ ] Verify pause works (ESC key)
- [ ] Test camera switching (orthographic ↔ perspective)
- [ ] Verify gameplay is smooth at 30+ FPS

---

## Phase 6: Settings System (Week 7)

**Goal**: Configurable everything  
**Milestone**: Sleek settings UI. User can tweak all parameters live. Settings save/load from YAML.

### Settings Module - Builder API

- [ ] Create `include/luma/settings/settings.h`
  - [ ] Define `SettingsSchema` class
  - [ ] Define `CategoryBuilder` class
  - [ ] Define setting types: IntSlider, FloatSlider, BoolToggle, EnumDropdown
- [ ] Create `src/settings/settings.cpp`
  - [ ] Implement `add_category()` method
  - [ ] Implement `add_int_slider()` method
  - [ ] Implement `add_float_slider()` method
  - [ ] Implement `add_bool_toggle()` method
  - [ ] Implement `add_enum_dropdown()` method
  - [ ] Store schema in hierarchical structure (categories → settings)
- [ ] Define default settings
  - [ ] Rendering category: algorithm, SPP, max_bounces, denoise
  - [ ] Performance category: resolution_scale, adaptive_spp, adaptive_resolution
  - [ ] Graphics category: emissive_paddles, emissive_ball, camera_mode, bloom
  - [ ] Gameplay category: physics_mode, winning_score
  - [ ] Debug category: show_bvh, show_normals, freeze_frame, profiler

### YAML Override System

- [ ] Create `assets/settings/default.yaml`
  - [ ] Define default values for all settings
  - [ ] Organize by category
  - [ ] Add comments for clarity
- [ ] Implement YAML loading
  - [ ] Parse YAML file
  - [ ] Match keys to settings in schema
  - [ ] Override default values
  - [ ] Warn on unknown keys
- [ ] Implement YAML saving
  - [ ] Serialize current settings to YAML
  - [ ] Write to file (`settings.yaml`)
  - [ ] Preserve comments and formatting (if possible)
- [ ] Hot-reload settings
  - [ ] Watch `settings.yaml` with file watcher
  - [ ] Reload on change
  - [ ] Apply new values immediately

### ImGui Auto-Generation

- [ ] Create `include/luma/editor/settings_ui.h`
  - [ ] Define `SettingsUI` class
  - [ ] Add method: `render()` (generates ImGui widgets from schema)
- [ ] Create `src/editor/settings_ui.cpp`
  - [ ] Iterate over categories in schema
  - [ ] Create collapsible header per category
  - [ ] Generate widget per setting:
    - [ ] IntSlider → `ImGui::SliderInt()`
    - [ ] FloatSlider → `ImGui::SliderFloat()`
    - [ ] BoolToggle → `ImGui::Checkbox()`
    - [ ] EnumDropdown → `ImGui::Combo()`
  - [ ] Bind widget to settings value (read/write)
  - [ ] Add "Save" button to write settings to YAML
- [ ] Add settings window to ImGui
  - [ ] Create "Settings" menu item
  - [ ] Open settings window on click
  - [ ] Render settings UI in window

### Adaptive Quality System

- [ ] Implement frame time measurement
  - [ ] Track frame time per frame (via Timer)
  - [ ] Compute rolling average (last N frames)
- [ ] Implement adaptive SPP
  - [ ] If average frame time > target (33ms):
    - [ ] Reduce SPP (e.g., 2 → 1)
  - [ ] If average frame time < target and SPP reduced:
    - [ ] Increase SPP back to desired value
- [ ] Implement adaptive resolution
  - [ ] If frame time still > target after reducing SPP:
    - [ ] Reduce internal resolution scale (e.g., 1.0 → 0.75)
  - [ ] If frame time < target and resolution reduced:
    - [ ] Increase resolution scale
  - [ ] Clamp to minimum (0.5x) and maximum (1.0x)
- [ ] Update path tracer to use current resolution scale
  - [ ] Scale dispatch dimensions
  - [ ] Upscale to native resolution (bilinear or Lanczos)

### Settings Categories Implementation

- [ ] Rendering settings
  - [ ] Apply algorithm choice (unidirectional/bidirectional)
  - [ ] Apply SPP to path tracer
  - [ ] Apply max bounces to path tracer
  - [ ] Apply denoise method (none/spatial/temporal/AI)
- [ ] Performance settings
  - [ ] Apply resolution scale to path tracer dispatch
  - [ ] Enable/disable adaptive SPP
  - [ ] Enable/disable adaptive resolution
- [ ] Graphics settings
  - [ ] Toggle emissive paddles (update material on entities)
  - [ ] Toggle emissive ball (update material on entity)
  - [ ] Switch camera mode (orthographic/perspective/free-fly)
  - [ ] Enable/disable bloom (future)
- [ ] Gameplay settings
  - [ ] Switch physics mode (arcade/realistic)
  - [ ] Set winning score (game over condition)
- [ ] Debug settings
  - [ ] Toggle BVH visualization (wireframe overlay)
  - [ ] Toggle normal visualization (debug view)
  - [ ] Toggle freeze frame (pause rendering)
  - [ ] Toggle profiler overlay (show timings)

### Presets System

- [ ] Define preset profiles
  - [ ] Low: 1 SPP, 2 bounces, 0.75x resolution, spatial denoise
  - [ ] Medium: 2 SPP, 4 bounces, 1.0x resolution, spatial denoise
  - [ ] High: 4 SPP, 8 bounces, 1.0x resolution, spatial denoise
  - [ ] Ultra: 8 SPP, 16 bounces, 1.0x resolution, temporal denoise (if implemented)
- [ ] Add preset dropdown to settings UI
  - [ ] List presets in combo box
  - [ ] On selection, apply preset values to settings
- [ ] Save current settings as custom preset (optional)

### Phase 6 Integration

- [ ] Open settings window
- [ ] Verify all categories appear
- [ ] Tweak rendering settings (SPP, bounces)
- [ ] Verify changes apply immediately
- [ ] Switch physics mode (arcade ↔ realistic)
- [ ] Verify physics behavior changes
- [ ] Toggle emissive materials
- [ ] Verify materials update in real-time
- [ ] Save settings to YAML
- [ ] Reload settings from YAML
- [ ] Verify saved settings persist
- [ ] Test adaptive quality (artificially increase load)
- [ ] Verify SPP/resolution reduce automatically
- [ ] Test presets (Low/Medium/High)
- [ ] Verify preset values apply correctly

---

## Phase 7: Polish + Optimization (Week 8)

**Goal**: Performance + visual quality  
**Milestone**: 30+ FPS on Vega 8 @ 1080p medium settings. Polished visuals with bloom.

### Wavefront Path Tracing Optimization

- [ ] Modify path tracer shader for wavefront architecture
  - [ ] Store persistent rays in buffer (position, direction, throughput, state)
  - [ ] Compact ray buffer (remove terminated rays)
  - [ ] Sort rays by material ID per bounce
- [ ] Implement ray compaction
  - [ ] Use prefix sum to compute compacted indices
  - [ ] Write only active rays to compacted buffer
- [ ] Implement ray sorting
  - [ ] Use radix sort or bitonic sort on GPU
  - [ ] Sort by material ID (coherent shading)
- [ ] Update path tracer dispatch
  - [ ] Dispatch only for active rays
  - [ ] Reduce workgroup count as rays terminate
- [ ] Test wavefront optimization
  - [ ] Profile before and after
  - [ ] Verify performance improvement (better GPU utilization)

### BVH Optimization (Incremental Rebuild)

- [ ] Detect changed entities
  - [ ] Track which entities moved since last frame
  - [ ] Mark affected BVH nodes as dirty
- [ ] Implement incremental rebuild
  - [ ] Rebuild only dirty subtrees
  - [ ] Keep unchanged subtrees intact
  - [ ] Merge updated subtrees back into tree
- [ ] Update BVH upload
  - [ ] Upload only changed nodes (partial buffer update)
  - [ ] Use mapped memory for fast updates on iGPU
- [ ] Test incremental rebuild
  - [ ] Profile rebuild time (should be <2ms)
  - [ ] Verify BVH remains correct after updates

### Profiler UI (CPU/GPU Timings)

- [ ] Create profiler data structure
  - [ ] Store per-system CPU timings
  - [ ] Store per-stage GPU timings (via timestamp queries)
  - [ ] Store frame times (last 120 frames for graph)
- [ ] Implement CPU profiling
  - [ ] Wrap systems with timing scopes
  - [ ] Record time per system (input, ECS, physics, BVH, etc.)
- [ ] Implement GPU profiling
  - [ ] Insert timestamp queries in command buffer
  - [ ] Query timestamps for each shader dispatch:
    - [ ] BVH upload
    - [ ] Ray generation
    - [ ] BVH traversal
    - [ ] Material shading
    - [ ] Accumulation
    - [ ] Denoising
    - [ ] Tonemapping
  - [ ] Compute delta times (in milliseconds)
- [ ] Create profiler ImGui window
  - [ ] Display CPU timings (bar chart or list)
  - [ ] Display GPU timings (bar chart or list)
  - [ ] Display frame graph (line plot of FPS over time)
  - [ ] Display memory stats (allocations, GPU usage)
- [ ] Add profiler toggle
  - [ ] Keybind to show/hide profiler (F3 or tilde)
  - [ ] Add to debug settings (profiler_overlay)

### Bloom Post-Processing

- [ ] Create brightness extraction shader
  - [ ] Read HDR image
  - [ ] Extract pixels brighter than threshold (e.g., luminance > 1.0)
  - [ ] Write to brightness buffer
- [ ] Create Kawase blur shader
  - [ ] Implement multi-pass Kawase blur
  - [ ] Downsample and blur (5 passes typical)
  - [ ] Upsample and combine
- [ ] Combine bloom with final image
  - [ ] Add blurred bright pixels to final image
  - [ ] Control bloom intensity with setting
- [ ] Compile bloom shaders
  - [ ] Create compute pipelines
  - [ ] Allocate intermediate buffers (downsampled images)
- [ ] Integrate bloom into render pipeline
  - [ ] Run after path tracing, before tonemapping
  - [ ] Make bloom optional (enable/disable in settings)
- [ ] Test bloom effect
  - [ ] Render scene with emissive materials
  - [ ] Verify bloom glows around bright objects
  - [ ] Adjust bloom intensity setting

### Audio Integration (Basic)

- [ ] Choose audio library (OpenAL or miniaudio)
  - [ ] Add to FetchContent (if not using system lib)
- [ ] Create `include/luma/audio/audio.h`
  - [ ] Define `AudioSystem` class
  - [ ] Add methods: `init()`, `shutdown()`, `play_sound()`, `set_volume()`
- [ ] Create `src/audio/audio.cpp`
  - [ ] Initialize audio library
  - [ ] Load sound files (WAV format)
  - [ ] Play sounds (non-blocking)
  - [ ] Clean up on shutdown
- [ ] Add sound effects
  - [ ] Ball hit paddle sound
  - [ ] Ball hit wall sound
  - [ ] Goal scored sound (optional)
- [ ] Trigger sounds on events
  - [ ] Play paddle hit sound on ball-paddle collision
  - [ ] Play wall hit sound on ball-wall collision
  - [ ] Play goal sound on score change
- [ ] Add volume control to settings
  - [ ] Master volume slider (0.0 - 1.0)
  - [ ] Apply volume to audio system
- [ ] Test audio
  - [ ] Play game
  - [ ] Verify sounds trigger correctly
  - [ ] Adjust volume in settings

### Compute Shader Optimization

- [ ] Profile path tracer shader
  - [ ] Use Vulkan profiling tools (RenderDoc, Radeon GPU Profiler)
  - [ ] Identify bottlenecks (memory bandwidth, ALU utilization, etc.)
- [ ] Reduce register pressure
  - [ ] Minimize local variables in shader
  - [ ] Reuse variables where possible
  - [ ] Avoid deep function call stacks
- [ ] Increase occupancy
  - [ ] Reduce shared memory usage (LDS)
  - [ ] Reduce register count per thread
  - [ ] Balance workgroup size (8×16 = 128 threads optimal for Vega 8)
- [ ] Optimize memory access patterns
  - [ ] Ensure coalesced reads/writes (adjacent threads access adjacent memory)
  - [ ] Use texture cache for random access (if applicable)
- [ ] Test optimizations
  - [ ] Profile after each change
  - [ ] Verify performance improves
  - [ ] Ensure correctness (compare output images)

### Final Polish

- [ ] Adjust default settings for optimal quality/performance
  - [ ] SPP: 2 (good balance)
  - [ ] Max bounces: 4 (sufficient for Pong)
  - [ ] Resolution scale: 1.0 (native)
  - [ ] Denoise: Spatial (fast and effective)
- [ ] Tune denoiser parameters
  - [ ] Filter strength
  - [ ] Edge-stopping weights (normal, depth, luminance)
- [ ] Tweak material parameters
  - [ ] Paddle metallic/roughness values
  - [ ] Ball emissive intensity
  - [ ] Wall diffuse color (off-white, not pure white)
- [ ] Improve visual consistency
  - [ ] Ensure emissive materials look good in both orthographic and perspective
  - [ ] Adjust camera positions/angles for cinematic views
- [ ] Add visual feedback
  - [ ] Highlight paddle on input (subtle glow or color shift)
  - [ ] Trail effect on ball (optional, if performance allows)

### Phase 7 Integration

- [ ] Run full game session
- [ ] Monitor FPS (should be 30+ on Vega 8 @ 1080p medium settings)
- [ ] Check profiler overlay (verify timings match budget)
- [ ] Test wavefront optimization (verify improved GPU utilization)
- [ ] Test incremental BVH rebuild (verify fast updates)
- [ ] Play audio (verify sounds trigger correctly)
- [ ] Enable bloom (verify visual quality)
- [ ] Switch settings (Low/Medium/High presets)
- [ ] Verify each preset maintains 30 FPS
- [ ] Verify visual quality is acceptable across all presets

---

## Final Deliverables Checklist

### Code Quality

- [ ] All code compiles without warnings (-Werror)
- [ ] All tests pass (Google Test)
- [ ] Code formatted with clang-format
- [ ] No memory leaks (verify with sanitizers)
- [ ] Vulkan validation layers report no errors

### Documentation

- [ ] README.md up to date [root]
  - [ ] Project overview
  - [ ] Build instructions
  - [ ] Controls/usage guide
  - [ ] System requirements
  - [ ] License information
- [ ] docs/LUMA_ARCHITECTURE.md finalized
- [ ] Code comments complete (Doxygen style)
- [ ] Settings documented (YAML comments)

### Performance

- [ ] 30 FPS minimum @ 1080p on AMD Vega 8 (medium settings)
- [ ] Adaptive quality maintains framerate
- [ ] No stuttering or frame drops
- [ ] Profiler shows balanced CPU/GPU usage

### Gameplay

- [ ] Pong is fully playable
- [ ] Controls responsive (paddle movement smooth)
- [ ] Physics feel good (arcade and realistic modes)
- [ ] Score tracking works correctly
- [ ] Game over triggers at winning score
- [ ] Menu/pause/game states transition smoothly

### Visuals

- [ ] Path-traced rendering looks correct
- [ ] Materials (diffuse, metallic, emissive) look realistic
- [ ] Denoising produces clean images at low SPP
- [ ] Camera transitions smooth (ortho ↔ perspective)
- [ ] Bloom adds polish (if enabled)

### Settings

- [ ] All settings functional
- [ ] Settings UI intuitive (ImGui)
- [ ] Settings save/load from YAML
- [ ] Hot-reload works for shaders, settings, scenes
- [ ] Presets (Low/Medium/High) work correctly

### Audio

- [ ] Sound effects trigger on events
- [ ] Volume control works
- [ ] No audio glitches or pops

---

## Stretch Goals (Beyond Pong MVP)

**If time permits after completing Phase 7:**

### Temporal Denoising

- [ ] Implement temporal accumulation with motion vectors
- [ ] Add reprojection for moving objects
- [ ] Blend history with current frame (exponential moving average)
- [ ] Test temporal stability (no flickering)

### AI Denoiser (Optional)

- [ ] Integrate NVIDIA NRD or Intel OIDN
- [ ] Add as optional dependency
- [ ] Compare quality vs spatial denoiser
- [ ] Measure performance cost

### Advanced Camera Features

- [ ] Depth of field (bokeh effect)
- [ ] Motion blur (for fast-moving ball)
- [ ] Camera shake on collision (game feel)

### Additional Game Modes

- [ ] Single-player vs AI (simple paddle AI)
- [ ] Two-player local multiplayer (already supported)
- [ ] Power-ups (speed boost, larger paddle, multi-ball)

### Enhanced Visuals

- [ ] Particle effects (sparks on collision)
- [ ] Trail effect on ball (motion blur-like)
- [ ] Screen-space reflections (for floor)
- [ ] Improved environment lighting (HDRI)

### Extended Physics

- [ ] Ball spin affecting trajectory (Magnus effect)
- [ ] Paddle angle affecting ball direction
- [ ] Variable ball speed (increases over time)

### Menus and UI

- [ ] Main menu with options (start game, settings, quit)
- [ ] In-game pause menu
- [ ] End screen with stats (longest rally, etc.)
- [ ] Controller support (gamepad)

---

## Notes

- **Focus on MVP**: Complete Phases 0-7 first before considering stretch goals
- **Profile Early**: Use profiler from Phase 7 throughout development
- **Test Incrementally**: Verify each phase works before moving to next
- **Commit Often**: Make git commits after each major feature
- **Document as You Go**: Add comments and documentation while code is fresh
- **Ask for Help**: If stuck, consult Vulkan specs, examples, or community

---

Let's build something legendary! 💜✨
