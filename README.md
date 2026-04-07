# SD

[![License](https://img.shields.io/github/license/vatnar/VLA)](LICENSE)
[![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://sd.vatnar.dev) 

SD is a game engine built for learning purposes, using Vulkan as the graphics API, GLFW for window management, and ImGui for UI.

### Features

- **Vulkan Renderer** — Modern GPU graphics with custom pipeline and shader management
- **ECS Architecture** — Entity Component System with sparse storage for efficient component queries
- **Command Queue** — Command-based ECS mutations for deterministic, serializable state changes (useful for networking/rollback)
- **Serialization** — Save/load component and ECS state to binary files
- **Hot Reloading** — Reload game code without restarting the engine
- **Multi-Window/View System** — Multiple render targets and windows with layered rendering
- **Event System** — Typed event dispatch for input, window, and application events
- **Custom Linear Algebra** — VLA library for matrices, vectors, and quaternions
- **Layer System** — Global and per-window layer stacks for flexible rendering pipelines

### Tech Stack

| Layer | Technology |
|-------|------------|
| Graphics | Vulkan |
| Window | GLFW |
| UI | ImGui |
| Math | VLA (custom) |

### License

This engine is licensed under the [MIT License](LICENSE).  
You may use it to build and sell games, and modify it to fit your needs.

If you want to redistribute, OEM, or sell a product based on this engine itself (e.g., as a hosted engine, SDK, or bundled engine), please contact for a commercial‑engine license.

---

## Hot Reloading

SD supports hot reloading of game code for fast iteration. The system watches your compiled game `.so` and reloads automatically when you rebuild.

### Quick Start

1. **Run** the `HotReload` configuration in CLion (or build and run `HotReloadApp` from `build/bin/`)
2. **Edit** your game code in `SandboxApp/`
3. **Build** (⌘F9) to rebuild the game library - it will automatically reload

### Configuration

**Important:** Configuration paths (`game-so-dir`, `build-dir`) are resolved relative to your **current working directory** (where you run `HotReloadApp`), not relative to the config file location.

Configuration is loaded in priority order (later overrides earlier):

1. **CLI arguments** (highest priority)
2. **Environment variables**
3. **Config file** (`hotreload.toml` in current working directory)
4. **Engine defaults** (`SD/config/engine.toml`)

#### Config File

Place a `hotreload.toml` in your working directory (where you run `HotReloadApp`):

```toml
[hotreload]
# Path to game shared library, relative to your WORKING DIRECTORY
# Examples:
#   libMyGame.so        = ./libMyGame.so
#   ./libMyGame.so      = ./libMyGame.so
#   lib/libMyGame.so    = ./lib/libMyGame.so
#   ../lib/libMyGame.so = sibling lib/ directory
game-so-path = "libMyGame.so"

# Window settings
app-name = "My Game"
window-width = 1920
window-height = 1080
```

#### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `SD_GAME_SO_PATH` | Path to game library (relative to PWD) | `libSandboxApp.so` |
| `SD_APP_NAME` | Window title | `Sandbox` |
| `SD_BUILD_DIR` | CMake build directory | `../build` |
| `SD_WINDOW_WIDTH` | Window width | `1280` |
| `SD_WINDOW_HEIGHT` | Window height | `720` |

#### CLI Arguments

```
--game-so-path <path>   Path to game library
--app-name <name>      Window title
--build-dir <path>     CMake build directory
--window-width <int>   Window width
--window-height <int>  Window height
```

### For Engine Users

To use the hot reloader with your own game:

1. Build your game code as a shared library (e.g., `libmygame.so`)
2. Create a `hotreload.toml` config file in your working directory
3. Set `game-so-path` to the path of your `.so` (relative to your working directory)
4. Run `HotReloadApp` from that working directory

Example:
```toml
# hotreload.toml (in your project root)
[hotreload]
game-so-path = "./build/bin/libmygame.so"
```

The hot reloader will load your game library and watch for rebuilds.
