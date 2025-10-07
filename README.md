# LUMA Engine

**Path-Traced Pong on AMD Vega 8 iGPU** 💜✨

A modular, functional, Vulkan-powered rendering engine showcasing real-time path tracing on integrated graphics.

## Features

- **Compute Shader Path Tracing** (software RT, no hardware RT extensions)
- **Signed Distance Field (SDF) Rendering** for procedural geometry
- **Wavefront Path Tracing** for GPU coherence
- **Modular Architecture** with hot-reloadable DLL modules
- **Custom ECS** optimized for path tracing workloads
- **Functional Programming** principles (immutability, purity, composability)
- **Settings-Driven Everything** (YAML config + runtime ImGui tweaking)

## Initial Demo: Path-Traced Pong

- Two physics modes: **Arcade** (perfect reflections) and **Realistic** (physical materials)
- Two camera modes: **Orthographic** (classic 2D) and **Perspective** (3D view with smooth transitions)
- Procedural PBR materials with optional emissive paddles/ball
- Real-time denoising (spatial à-trous wavelet filter)
- **30 FPS minimum @ 1080p** on AMD Vega 8 iGPU

## System Requirements

### Minimum

- **GPU**: AMD Vega 8 (RDNA 1.0) or equivalent with Vulkan 1.3 support
- **CPU**: x86-64 with AVX2 support
- **RAM**: 8 GB (shared with iGPU)
- **OS**: Windows 10/11, Linux (kernel 5.15+), macOS 12+

### Recommended

- **GPU**: Discrete GPU with Vulkan 1.3 support
- **CPU**: 4+ cores with AVX2
- **RAM**: 16 GB

## Build Instructions

### Prerequisites

1. **Vulkan SDK 1.3+**: Download from [LunarG](https://vulkan.lunarg.com/)
2. **CMake 3.20+**: [cmake.org](https://cmake.org/)
3. **C++26 Compiler**:
   - GCC 13+ (preferred)
   - Clang 16+
   - MSVC 2022+
4. **Ninja** (optional but recommended): [ninja-build.org](https://ninja-build.org/)

### Building

```powershell
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure

# Run Pong
.\build\pong.exe
```

### Debug Build

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Controls

### Gameplay

- **Player 1**: `W` (up), `S` (down)
- **Player 2**: `↑` (up), `↓` (down)
- **Pause**: `ESC`

### Camera

- **Orthographic Mode**: `1`
- **Perspective Mode**: `2`
- **Free-Fly Mode**: `F` (debug)
  - **Movement**: `WASD` + `Q/E` (up/down)
  - **Look**: Mouse
  - **Sprint**: `Shift`

### Debug

- **Settings UI**: `F1`
- **Profiler**: `F3`

## Project Structure

```text
luma/
├── CMakeLists.txt              # Root CMake configuration
├── README.md                   # This file
├── LICENSE                     # GPL license
├── docs/                       # Documentation
│   ├── LUMA_ARCHITECTURE.md   # Architecture specification
│   └── TODO.md                # Development roadmap
├── cmake/                      # CMake modules
├── include/luma/              # Public headers
├── src/                       # Implementation
├── shaders/                   # GLSL compute shaders
├── tests/                     # Google Test suites
└── assets/                    # Scenes and settings
```

## License

GPL - GNU General Public License

> "There is no ethical consumption under capitalism, but there IS ethical code under GPL" 💜

See [LICENSE](LICENSE) for details.

## Documentation

- **Architecture**: [docs/LUMA_ARCHITECTURE.md](docs/LUMA_ARCHITECTURE.md)
- **TODO List**: [docs/TODO.md](docs/TODO.md)

## Philosophy

LUMA is built on functional programming principles:

- **Immutability**: All data structures immutable by default
- **Purity**: Functions have no side effects, deterministic
- **Composition**: Systems compose via pipes/monads
- **No Exceptions**: Use `std::expected<T, Error>` for error handling

Real-time path tracing on integrated graphics is not just possible—it's beautiful, performant, and ethical.

---

**Let's build something legendary!** 🚀💜✨
