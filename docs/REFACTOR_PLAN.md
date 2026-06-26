# Refactor Plan: ZII + Arena + No Virtual + No OOP

## Guiding Principles

- **ZII**: Every type must be valid when zero-initialized. `0` is always a valid, safe state (not necassarily the "correct" state — e.g. a texture may show a "not found" texture, but it won't crash).
- **Arena allocators**: Replace all `std::unique_ptr` with arena-allocated raw pointers. Single shared `engine_arena` for all persistent subsystems.
- **No virtual**: Type erasure via function pointer vtables (same pattern as existing `LayerNode`/`LayerList`).
- **No classes, minimal ctors/dtors**: Plain structs. Ctors only when ZII is insufficient (type needs non-zero setup). Dtors only for cleanup of non-arena resources (VkDevice, glfw, etc.). Otherwise, arena handles allocation. Third-party types (`vk::UniqueHandle`, quill sinks, etc.) kept as-is.
- **Keep current style**: Naming (`m_` prefix, `PascalCase` types, `snake_case` functions), file extensions (`.hpp`/`.cpp`), and formatting stay unchanged.

---

## Dependency Graph

```
Phase A (ZII) ──┐
                  ├──> Phase C (no virtual) ──> Phase D (cleanup)
Phase B (arena) ─┘
                        └──> Phase E (struct + ctor/dtor audit)
```

Phases A and B are independent (parallelizable). C depends on both. E depends on B. D is final cleanup.

---

## Phase A — ZII (Zero Initialization Idiom)

**Goal:** `= {}` is always valid for every engine type. No redundant constructors.

### A1 — Fix sentinel blockers

| File | Current | Change |
|------|---------|--------|
| `SD/include/SD/core/ecs/Command.hpp` | `EntityHandle::id = g_type_max<U32>` (0xFFFFFFFF) | Change to `0` as invalid sentinel |
| All files checking `handle.id == g_type_max<U32>` | Sentinel comparison | Change to `handle.id == 0` |
| `EntityManager::create()` | Allocates index starting at 0 | Start at `1`, reserve `0` as invalid |

### A2 — Non-zero defaults (inline member initializers only)

These are the only acceptable exception to ZII — non-zero defaults via inline initializers:

| File | Member | Change |
|------|--------|--------|
| `SD/include/SD/core/ecs/components.hpp` | `Renderable::color[4]` | inline `= {1.0f, 0.0f, 0.0f, 1.0f}` |
| `SD/include/SD/core/FrameTimer.hpp` | `m_fixed_time_step` | inline `= 1.0 / 60.0` |
| `SD/include/SD/core/View.hpp` | `m_color_format` | inline `= vk::Format::eR8G8B8A8Unorm` |
| `SD/include/SD/core/View.hpp` | `m_extent` | inline `= vk::Extent2D{1280, 720}` |

### A3 — Accept behavior changes from ZII

These members become `0`/`false`/`nullptr` by default. Their non-zero values must be set explicitly during init or attach:

| File | Member | ZII default | Fix |
|------|--------|-------------|-----|
| `SD/include/SD/core/Layer.hpp` | `is_active` | `false` | `on_attach()` sets it, or caller sets after push |
| `SD/include/SD/Application.hpp` | `is_running` | `false` | Set to `true` explicitly before `run()` |
| `SD/include/SD/Application.hpp` | `hot_reload_enabled` | `false` | Set in `app_init()` |
| `SD/include/SD/core/layers/EngineDebugLayer.hpp` | all visibility bools | `false` | Initialize in `on_attach()` or accept defaults |
| `SD/include/SD/core/logging.hpp` | `CategoryInfo::visible` | `false` | Accept default (opt-in to visible categories) |

### A4 — Remove redundant constructors

These constructors do nothing that zero-init doesn't already do — delete entirely:

| File | Constructor | Reason |
|------|-------------|--------|
| `SD/include/SD/core/Window.hpp` | `WindowBuilder() : m_desc({}) {}` | Redundant |
| `SD/include/SD/core/id_types.hpp` | `ViewId(uint32_t v = 0)` | Redundant |
| `SD/include/SD/core/id_types.hpp` | `WindowId(uint32_t v = 0)` | Redundant |
| `SD/include/SD/core/WindowManager.hpp` | `WindowProps() = default` + param ctor with `= {}` | Use aggregate init instead |
| `SD/include/SD/core/profiler.hpp` | `Profile()` default ctor | Redundant |
| All event structs (`events/*.hpp`) | Any ctor that just sets `= {}` | Remove, use aggregate init |

### A5 — Arena push helpers

```cpp
template<typename T>
T* arena_push(Arena* arena) {
  return arena->push_array<T>(1);  // zero-inits via push(..., true)
}

template<typename T>
T* arena_push_no_zero(Arena* arena) {
  return arena->push_array_no_zero<T>(1);  // no memset, for hot-path
}
```

---

## Phase B — Arena Allocators (Remove `std::unique_ptr`)

**Goal:** Zero `std::unique_ptr` in engine code. All persistent subsystems live in a single `engine_arena`.

### B1 — Application struct (replaces class)

```cpp
struct Application {
  bool              is_running;
  bool              hot_reload_enabled;
  bool              game_code_reload_paused;

  ApplicationSpec   spec;
  WindowManager*    window_manager;
  ViewManager*      view_manager;
  LayoutManager*    layout_manager;
  SceneManager      scene_manager;      // already a struct
  LayerList         global_layers;      // already arena-backed
  EventManager      app_event_manager;  // already arena-backed
  RuntimeStateManager* state_manager;
  FrameTimer        timer;
  GlfwContext*      glfw_ctx;
  VulkanContext*    vulkan_ctx;
  VulkanRenderer*   renderer;
  SDImGuiContext*   imgui_ctx;

  Arena*            engine_arena;       // owns all subsystems
  Arena*            frame_arena;        // cleared each frame

  std::atomic<bool> restart_requested;
  std::atomic<bool> shader_reload_requested;
};
```

### B2 — Application constructor/destructor

Application keeps its constructor and destructor (ZII doesn't cover VkDevice, glfwTerminate, etc.).
The constructor allocates the engine arena, pushes all subsystems with placement new.
The destructor calls explicit destructors for non-trivial subsystems, then `arena_release`.
See `Application.cpp` for the final implementation.

### B3 — Subsystem unique_ptr replacements

| File | Current | After |
|------|---------|-------|
| `SD/include/SD/core/WindowManager.hpp` | `unique_ptr<Window> logic`, `unique_ptr<VulkanWindow> render` | Raw `Window* logic`, `VulkanWindow* render`. Arena passed to `wm_init()` |
| `SD/include/SD/core/ViewManager.hpp` | `unordered_map<ViewId, unique_ptr<View>>` | `unordered_map<ViewId, View*>`, arena-allocate Views |
| `SD/include/SD/core/SceneManager.hpp` | `vector<unique_ptr<Scene>>` | `ArenaVec<Scene*>`, arena-allocate Scenes |
| `SD/include/SD/core/SDImGuiViewport.hpp` | `unique_ptr<VulkanFramebuffer>` | Raw `VulkanFramebuffer*` from arena |
| `SD/include/SD/core/Window.hpp` | `build()` returns `unique_ptr<Window>` | `build(Arena* arena)` returns `Window*` |

### B4 — Builder pattern fix

```cpp
// Before:
std::unique_ptr<Window> WindowBuilder::build() const;

// After:
Window* WindowBuilder::build(Arena* arena) const;
```

### B5 — Remove `#include <memory>`

Remove `<memory>` include from all engine headers:
- `SD/include/SD/Application.hpp`
- `SD/include/SD/core/Window.hpp`
- `SD/include/SD/core/WindowManager.hpp`
- `SD/include/SD/core/ViewManager.hpp`
- `SD/include/SD/core/View.hpp`
- `SD/include/SD/core/SceneManager.hpp`
- `SD/include/SD/core/SDImGuiViewport.hpp`
- `SD/include/SD/core/ecs/EntityManager.hpp`
- `SD/include/SD/core/logging.hpp`

Third-party code (quill logging sinks using `std::shared_ptr`) keeps `<memory>` isolated inside `SD/src/core/logging.cpp`.

---

## Phase C — No Virtual Functions ~> DONE

**Goal:** Zero virtual functions in engine code.

### C1 — View — remove virtual (no type-erased vtable needed)

Removed `virtual` from all `View` methods and destructor. No subclasses of `View` exist
in the codebase, so the full type-erased vtable from the original plan was unnecessary.
Simple `virtual` removal is sufficient — the vtable pattern can be added later if
custom View types are needed.

### C2 — Remove Serializable base class ~> DONE

- `SD/include/SD/utils/serialization.hpp`: Delete `Serializable` class entirely
- `HasSerialize` / `HasDeserialize` concepts + ADL free functions already handle everything
- Remove any remaining class that inherits `Serializable`

### C3 — Remove all virtual destructors ~> DONE

| File | Virtual | Replacement |
|------|---------|-------------|
| `SD/include/SD/Application.hpp` | `virtual ~Application()` | Non-virtual (arena handles cleanup) |
| `SD/include/SD/core/View.hpp` | `virtual ~View()` | Non-virtual (type-erased, no polymorphic deletion) |
| `SD/include/SD/utils/serialization.hpp` | `virtual ~Serializable()` | Deleted with class removal |

### C4 — ViewManager changes ~> DONE

- `create<T>()` allocates `T` from passed arena, sets up vtable, stores `View*`
- All dispatch goes through `ViewVTable` function pointers — no virtual calls

---

## Phase D — Include Cleanup ~> DONE

- Add `SD/arena.hpp` include to 4 headers that use `Arena*` (WindowManager, SDImGuiViewport, ViewManager, Window)
- Remove `<memory>` from View.hpp, logging.hpp, EntityManager.hpp (no longer needed)
- SandboxAppStandalone pre-existing link error unchanged (GameRenderLayer not linked)

---

## Phase E — Change `class` → `struct` + Ctor/Dtor Audit ~> DONE

**Goal:** No `class` keyword for engine types. Keep ctors where ZII is insufficient
(type needs non-zero setup). Keep dtors where cleanup of non-arena resources is needed.
Remove ctors/dtors that are now redundant after arena allocation.

### E1 — Change `class` to `struct` ~> DONE

All 27 engine headers converted. Zero `class` definitions remain in engine headers.
Forward declarations changed from `class Foo;` to `struct Foo;` to match.

- `class Foo { public:` → `struct Foo {`
- Move all members to public
- Remove `public:` / `private:` / `protected:` labels

### E2 — Ctor removal candidates (ZII-sufficient types)

These types are valid when zero-initialized — their constructors are redundant:

| File | Ctor | Reason |
|------|------|--------|
| `SD/include/SD/core/FrameTimer.hpp` | Parameter ctor | All members have ZII-safe defaults |
| `SD/include/SD/core/layers/PerformanceLayer.hpp` | Parameter ctor | Just copies args |
| `SD/include/SD/core/ViewManager.hpp` | Default ctor | Empty |
| `SD/include/SD/core/Scene.hpp` | Parameter ctor | Members are strings + ECS containers |
| `WindowManager.hpp` | Parameter ctor | Handled by arena + placement new |
| `EventManager.hpp` | Default ctor | Allocates its own arena; could take one from caller instead |

### E3 — Dtor retention

Keep dtors (they call explicit cleanup before arena_release):

- `Application::~Application()` — calls subsystem dtors + arena_release
- `GlfwContext::~GlfwContext()` — calls `glfwTerminate`
- `VulkanContext::~VulkanContext()` — waits for device, destroys VMA
- `VulkanRenderer::~VulkanRenderer()` — device wait + cleanup
- `SDImGuiContext::~SDImGuiContext()` — calls `ImGui::Shutdown`, `ImGui_ImplVulkan_Shutdown`
- `VulkanWindow::~VulkanWindow()` — destroys surface, swapchain, framebuffers
- `Window::~Window()` — calls `glfwDestroyWindow`
- `VulkanFramebuffer::~VulkanFramebuffer()` — destroys Vk objects
- `SDImGuiViewport::~SDImGuiViewport()` — removes ImGui texture, framebuffer cleanup

### E4 — Scene arena usage

`Scene` uses the shared `engine_arena` for entity pool data (already done in Phase B):
```cpp
explicit Scene(std::string name = "Untitled Scene", Arena* pool_arena = nullptr) :
  m_name(std::move(name)) {
  em.m_pool_arena = pool_arena;
}
```

---

## Summary

| Phase | What | Files | Key risk |
|-------|------|-------|----------|
| A | ZII — ctor removal, sentinel fix, defaults | ~25 | EntityHandle sentinel ripple |
| B | Arena — replace unique_ptrs | ~10 | Application init order |
| C | No virtual — View vtable, remove Serializable | ~8 | View dispatch correctness | **DONE** |
| D | Include cleanup | ~12 | Missing transitive includes | **DONE** |
| E | class→struct, ctor/dtor audit | ~15 | Keeping wrong ctor breaks ZII | **DONE** |
| **Total** | | **~55** | |

### Notes

- **Third-party types** (`vk::UniqueHandle`, quill `shared_ptr`, etc.) are kept as-is.
- **`std::function`** callbacks in Application (for close, events) kept for now — recognized overhead, can revisit later.
- **`std::atomic`** members stay as-is — they're ZII-compatible (zero-init = `false`).
- Single `engine_arena` for now. Can split into sub-arenas later if needed.
