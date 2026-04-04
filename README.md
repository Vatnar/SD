# SD - Shader Debugger

[![Docs](https://img.shields.io/badge/docs-gh--pages-blue)](https://vatnar.github.io/SD/annotated.html)

### Features

* (WIP) Real-time shader debugging
* (WIP) Vulkan-based rendering engine

### Architecture & Design

* (WIP)

---

[![License: PolyForm Shield](https://img.shields.io/badge/License-PolyForm%20Shield%201.0.0-5D6D7E)](https://polyformproject.org/licenses/shield/1.0.0)
This project is licensed under the **PolyForm Shield License 1.0.0**. (See `LICENSE` and `NOTICE`)

---

## Engine TODO (Roadmap)

### 🟢 Phase 1: Command System (In Progress)

- [x] **EntityHandle**: Implement handle system for deferred entity creation
- [x] **Command Base Class**: Command interface with serialization support
- [x] **CommandQueue**: Queue with Apply() method (thread-safety deferred)
- [x] **Entity Commands**: CreateEntity, DestroyEntity, AddComponent, RemoveComponent
- [x] **Scene Integration**: Add command queue to Scene with AddCommand/ApplyCommands
- [x] **Serialization**: Serialize/deserialize commands for networking + file saving
- [ ] **Testing**: Write unit tests for CommandQueue and Scene
- [ ] **File Serialization**: Serialize command queue to file and reload

### 🟡 Phase 2: Asset Management

- [ ] **AssetManager Core**: Centralized asset loading/management
- [ ] **Asset Handles**: Handle<T> system for type-safe asset references
- [ ] **Texture Loading**: Load textures from files
- [ ] **Mesh Loading**: Load meshes from files
- [ ] **Material System**: Material assets referencing textures/shaders

### 🔵 Phase 3: Graphics Pipeline

- [ ] **Render System**: ECS-based render system querying Renderable components
- [ ] **Generic Render Graph**: Modular, swappable render passes
- [ ] **View-Specific Renderers**: Different render configs per view (Wireframe, Lit, etc.)
- [ ] **Asset Integration**: Connect AssetManager with RenderSystem

### 🛠️ Developer Workflow

- [x] **DLL Hot-Reloading Support**: Enhancing the `Application` and `View` persistence to allow game logic swaps
  without state loss.
- [ ] **Editor Tooling**: Building out more `GuiToolLayers` for scene inspection and entity manipulation.

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