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

### 🟢 Immediate Focus: Modular Infrastructure

- [ ] **Handle-Based Asset Management**: Implementation of a centralized `AssetManager` with `Handle<T>` semantics for
  textures, meshes, and shaders.
- [x] **ECS System Orchestration**: Evolving `Scene` to host logic/render systems that operate on the `EntityManager`.
- [ ] **Command-Based Mutations**: Routing scene changes through a `CommandBus` to support Undo/Redo and future
  networking.

### 🔵 Rendering & Pipeline

- [ ] **Generic Render Graph**: Decoupling the rendering pipeline into modular, swappable passes.
- [ ] **View-Specific Renderers**: Allowing different `Views` to use different render configurations (e.g. Wireframe vs
  Lit).

### 🛠️ Developer Workflow

- [ ] **DLL Hot-Reloading Support**: Enhancing the `Application` and `View` persistence to allow game logic swaps
  without state loss.
- [ ] **Editor Tooling**: Building out more `GuiToolLayers` for scene inspection and entity manipulation.