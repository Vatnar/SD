# SD

[![License](https://img.shields.io/github/license/vatnar/VLA)](LICENSE)
[![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://sd.vatnar.dev)

SD is a game engine built for learning purposes, using Vulkan as the graphics API and GLFW for window management. (Imgui
for Debug ui)

### Current Features

- vulkan rendering
- entity component system with sparse storage
- command based ecs mutation and serialization
- serialization of components and ecs state
- hot reloading, reload game code without restarting engine
- multiple render targets and windows
- custom linear algebra (VLA)
- global and per-view* layer stacks

### Tech Stack

- Vulkan: Graphics API
- GLFW: Window Management
- ImGui: debug ui
- VLA: math (predominantly linear algebra stuff)
- dxc: HLSL compilation to spirv
- spdlog: async logging
- stb: image loading
- tomlplusplus: toml parsing (subject for removal)
- VMA: vk memory alloc

### License

This engine is licensed under the [MIT License](LICENSE).  
You may use it to build and sell games, and modify it to fit your needs.

---

## Building

### Prerequisites

- CMake 3.25+
- C++23 compatible compiler (GCC 13+, Clang 16+) (Clang lacks some modern features)
- Vulkan SDK
- GLFW dependencies (X11 or Wayland development libraries (windows and mac is in the works))

### Quick Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target HotReloadApp -j$(nproc)
```

### Using ccache (Recommended)

[ccache](https://ccache.dev) dramatically speeds up incremental builds by caching compiled object files.

**Install:**

```bash
sudo apt install ccache    # Debian/Ubuntu
brew install ccache        # macOS
```

**Configure:**

ccache is automatically detected and enabled by CMake. To verify:

```bash
cmake -S . -B build
# Look for: -- ccache enabled: /usr/bin/ccache
```

## Build Flags

| Flag                                 | purpose                                                                               |
|:-------------------------------------|:--------------------------------------------------------------------------------------|
| `SD_DEBUG`                           | Build SD in debug configuration, enabling vulkan validation enables and more analysis |
| `SD_SUPRESS_VULKAN_INFO`             | Suppresses INFO from vulkan (when validation layers are enabled)                      |
| `SD_FAIL_ON_VULKAN_VALIDATION_ERROR` | Instantly abort on first vulkan validation error                                      |
| `ENGINE_LOG_LEVEL`                   | Specify what granularity of logs should be printed in the engine                      |

---

## Hot Reloading

SD supports hot reloading of game code for fast iteration. The system watches your compiled game `.so` and reloads
automatically when you rebuild. The packaged EngineDebugLayer will allow you to "pause" the hot reloading if needed.

### Quick Start

1. Run the `HotReloadApp` executable.
2. Edit and build your game as a shared library.
3. The changes will be detected and reload while keeping state

### Configuration

#### Config File

The hotreloader requires a `hotreload.toml` file in your working directory (where you run `HotReloadApp`):

```toml
[hotreload]
#   libMyGame.so        = ./libMyGame.so
#   lib/libMyGame.so    = ./lib/libMyGame.so
#   ../lib/libMyGame.so = sibling lib/ directory
game-so-path = "../libMyGame.so"

# Window settings
app-name = "My Game"
window-width = 1920
window-height = 1080
```