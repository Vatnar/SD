# Unity Build Plan for SD Engine

## 1. Context

SD currently uses CMake's built-in unity build via presets (`CMAKE_UNITY_BUILD=ON` with
`CMAKE_UNITY_BUILD_BATCH_SIZE=8`, BATCH mode). Vendor exclusions already exist:

- `glfw` — whole target excluded (`UNITY_BUILD OFF`)
- `imgui` backends — per-file exclusion (`SKIP_UNITY_BUILD_INCLUSION`)

The project builds with C++26, `-fno-exceptions`, `-freflection`, and
`CMAKE_CXX_SCAN_FOR_MODULES OFF` (already set in all presets — mandatory for unity + C++20+).

## 2. The Problem

When multiple `.cpp` files are merged into a single translation unit, internal-linkage
entities collide:

| Pattern | Files affected | Unity issue |
|---|---|---|
| `static` at file scope | `logging.cpp`, `CommandQueue.cpp`, `hot_reloader.cpp`, `sandbox.cpp` | Same name → redefinition |
| Anonymous `namespace { ... }` | `ShaderCompiler.cpp`, `CommandQueue.cpp`, `sandbox.cpp`, `ecs_tests.cpp` | Same name → redefinition |
| `LOCAL_PERSIST` (`#define LOCAL_PERSIST static`) | `types.hpp` users | Expands to `static` → collides |

The `release-unity` preset exists but will fail to build if any two files in the same
batch share internal identifiers.

## 3. Solution: `FILE_INTERNAL` Macro Pattern

Adopt the pattern proven in `sd2`:

**CMake side** — set a per-target unique-ID property:

```cmake
set_target_properties(SD PROPERTIES
        UNITY_BUILD_UNIQUE_ID "FILE_INTERNAL"
)
```

This tells CMake to `#define FILE_INTERNAL` to a unique hash per source file when
unity is active. The macro is undefined/redefined between each `#include` in the
generated unity batch file.

**C++ side** — a force-included header provides the macro pair:

```cpp
// src/internal.hpp  (private, force-included)
#pragma once

#ifndef FILE_INTERNAL
// Non-unity: use a fixed name so FILE_INTERNAL:: qualified calls always resolve.
// Unity mode: CMake injects `-DFILE_INTERNAL=<unique_hash>` via UNITY_BUILD_UNIQUE_ID.
#define FILE_INTERNAL _file_local
#endif

#define FILE_INTERNAL_BEGIN namespace { namespace FILE_INTERNAL {
#define FILE_INTERNAL_END } }
```

Usage in `.cpp` files:

```cpp
// Before (breaks in unity):
static void helper() { ... }
// or:
namespace { void helper() { ... } }

// After (safe in unity):
FILE_INTERNAL_BEGIN
    void helper() { ... }
FILE_INTERNAL_END

// Call via:
FILE_INTERNAL::helper();
```

**Non-unity fallback:** When unity is off, `FILE_INTERNAL` is empty (`#ifndef`
fallback), so `FILE_INTERNAL_BEGIN` expands to `namespace { namespace {`. This is a
redundant nested anonymous namespace — harmless, compiles identically.

---

## 4. Infrastructure Changes

### 4.1 Create `SD/src/internal.hpp`

Force-included header with the `FILE_INTERNAL` macros. Also includes the export header
so `SD_EXPORT` is available everywhere (already generated via `GenerateExportHeader`).

### 4.2 Update `SD/CMakeLists.txt`

Add to the `SD` target:
```cmake
set_target_properties(SD PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        UNITY_BUILD_UNIQUE_ID "FILE_INTERNAL"     # <-- new
)

target_compile_options(SD PRIVATE
        "$<$<CXX_COMPILER_ID:GNU,Clang>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/internal.hpp>"
        "$<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/src/internal.hpp>"
)
```

### 4.3 Update other targets

Targets that participate in unity (i.e. whose `UNITY_BUILD` will be `ON` via
`CMAKE_UNITY_BUILD`) also need `UNITY_BUILD_UNIQUE_ID` and the force-include:

| Target | Type | Has static/anonymous ns? | Needs changes |
|---|---|---|---|
| `SD` | SHARED | Yes | Add `UNITY_BUILD_UNIQUE_ID`, force-include, migrate code |
| `HotReloadApp` | EXECUTABLE | Yes (heavy `static` usage) | Add `UNITY_BUILD_UNIQUE_ID`, force-include, migrate code |
| `SandboxApp` | SHARED | Yes (`sandbox.cpp`) | Add `UNITY_BUILD_UNIQUE_ID`, force-include, migrate code |
| `SandboxAppStandalone` | EXECUTABLE | Same source as SandboxApp | Same as SandboxApp |
| `SandboxGameObjects` | OBJECT | None (GameRenderLayer) | Force-include only |
| `SDGTest` | EXECUTABLE | Yes (`ecs_tests.cpp`) | Add `UNITY_BUILD_UNIQUE_ID`, force-include, migrate code |

Vendor targets (glfw excluded, imgui/stb/vma/fmt/...) — no changes needed.

### 4.4 `CMAKE_UNITY_BUILD` presets already exist

The `release-unity` preset is ready. No changes needed — just ensure targets compile.

---

## 5. File-by-File Migration

### 5.1 `SD/src/core/logging.cpp` — heavy `static` usage

All file-scope `static` declarations must move inside `FILE_INTERNAL_BEGIN/END`:

```cpp
// Replace:
static constexpr const char* HISTORY_FILE    = "debug_history.log";
static constexpr size_t      MAX_MEM_ENTRIES = 500;
static std::deque<LogEntry> g_log_history;
// ... all other static entities ...

FILE_INTERNAL_BEGIN

constexpr const char* HISTORY_FILE    = "debug_history.log";
constexpr size_t      MAX_MEM_ENTRIES = 500;
std::deque<LogEntry> g_log_history;
// ... everything else ...

FILE_INTERNAL_END
```

Callers use `FILE_INTERNAL::g_log_history`, `FILE_INTERNAL::write_entry_to_file(...)`,
etc. This is the largest migration — ~50 `static` declarations.

If `HISTORY_FILE` and `MAX_MEM_ENTRIES` are `constexpr`, they don't strictly need to be
in the namespace (internal linkage is automatic for `constexpr` at file scope), but
keeping everything together is clearest.

### 5.2 `SD/src/core/ShaderCompiler.cpp` — anonymous namespace

```cpp
// Replace:
namespace {
constexpr const char* k_shader_model = "6_0";
struct ExtMapping { ... };
constexpr std::array k_ext_to_profile = { ... };
// ...
} // namespace

FILE_INTERNAL_BEGIN
constexpr const char* k_shader_model = "6_0";
struct ExtMapping { ... };
constexpr std::array k_ext_to_profile = { ... };
// ...
FILE_INTERNAL_END
```

The anonymous namespace becomes the `FILE_INTERNAL` wrapper. Note: `constexpr`
globals already have internal linkage, but this keeps the pattern consistent.

### 5.3 `SD/src/core/ecs/CommandQueue.cpp` — anonymous namespace + `static`

```cpp
// Replace:
namespace {
struct CommandTypeRegistrar { ... };
static CommandTypeRegistrar s_registrar;
} // namespace

FILE_INTERNAL_BEGIN
struct CommandTypeRegistrar { ... };
CommandTypeRegistrar s_registrar;
FILE_INTERNAL_END
```

The `static` on `s_registrar` is redundant inside an anonymous namespace, and
redundant inside `FILE_INTERNAL` too.

### 5.4 `hot_reloader/src/hot_reloader.cpp` — heavy `static` globals

All `static` file-scope globals and functions:

```cpp
// Replace all:
static void*                           g_game_handle{nullptr};
static GameAPI                         g_game_api{};
// ... etc ...
static std::filesystem::path get_live_so_path(...) { ... }
// ... etc ...

FILE_INTERNAL_BEGIN
void*                           g_game_handle{nullptr};
GameAPI                         g_game_api{};
// ... etc ...
std::filesystem::path get_live_so_path(...) { ... }
// ... etc ...
FILE_INTERNAL_END
```

Usage site changes: qualify calls with `FILE_INTERNAL::` or just `get_live_so_path(...)`
(since unqualified lookup finds it within the enclosing `FILE_INTERNAL` namespace).

### 5.5 `sandbox_app/sandbox.cpp` — `static` function + anonymous namespace

```cpp
// Replace:
static void register_game_categories() { ... }

namespace {
vk::UniqueShaderModule create_shader_module(...) { ... }
} // namespace

FILE_INTERNAL_BEGIN
void register_game_categories() { ... }
vk::UniqueShaderModule create_shader_module(...) { ... }
FILE_INTERNAL_END
```

### 5.6 `sandbox_app/sandbox.cpp` — C-API `static` wrappers

**Do not put `extern "C"` functions inside `FILE_INTERNAL`.** An anonymous namespace
wrapping `extern "C"` is implementation-defined: GCC gives the function internal linkage
(the anonymous namespace wins), while Clang may preserve external C linkage. Since
these are loaded via `dlsym`, they must have external linkage.

Instead, drop `static` and rename to a file-unique name to avoid unity collisions:

```cpp
// Before:
extern "C" {
static void c_on_load(...) { ... }
static void c_on_update(...) { ... }
static void c_on_unload(...) { ... }
static void c_on_reload(...) { ... }
}

// After:
extern "C" {
// Unique names prevent collisions when files are merged in unity builds.
void sandbox_c_on_load(...) { ... }
void sandbox_c_on_update(...) { ... }
void sandbox_c_on_unload(...) { ... }
void sandbox_c_on_reload(...) { ... }
}
```

`get_game_api()` has the same concern — keep it outside `FILE_INTERNAL` and ensure
its name won't collide (or rename to `sandbox_get_game_api`).

### 5.7 `testing/tests/ecs_tests.cpp` — anonymous namespace

```cpp
// Replace:
namespace {
// test helpers
} // namespace

FILE_INTERNAL_BEGIN
// test helpers
FILE_INTERNAL_END
```

### 5.8 `SD/include/SD/core/types.hpp` — `LOCAL_PERSIST` macro

```cpp
// Current:
#define LOCAL_PERSIST static

// This is used inside functions for persistent local variables, e.g.:
//   LOCAL_PERSIST int counter = 0;
```

This is used for function-local `static` variables — these are per-function, not
per-file, so they don't collide in unity builds. **No change needed.**

---

## 6. File `internal.hpp` placement

Copy from `sd2` to `SD/src/internal.hpp`:

```cpp
#pragma once

#include <SD/generated/export.hpp>

#ifndef FILE_INTERNAL
// Non-unity: use a fixed name so FILE_INTERNAL:: qualified calls resolve.
// Unity mode: CMake injects -DFILE_INTERNAL=<unique_hash>.
#define FILE_INTERNAL _file_local
#endif

#define FILE_INTERNAL_BEGIN namespace { namespace FILE_INTERNAL {
#define FILE_INTERNAL_END } }
```

Force-include via `-include` on all SD-engine-related targets.

---

## 7. Edge Cases & Risks

### 7.1 `extern "C"` + `FILE_INTERNAL`

`extern "C"` and anonymous namespaces interact: a function inside
`namespace { extern "C" void f(); }` has implementation-defined linkage — GCC gives it
internal linkage (anonymous namespace wins), Clang may give external C linkage.
Since the hot-reload wrappers are loaded via `dlsym`, they **must not** be inside
`FILE_INTERNAL`. See §5.6 for the recommended approach (unique names outside
`FILE_INTERNAL`).

### 7.2 `constexpr` globals

`constexpr` at namespace scope has internal linkage implicitly. `FILE_INTERNAL` wrapping
adds another layer but is redundant — still correct.

### 7.3 Template instantiations

Templates are fine — ODR allows multiple definitions across TUs, and unity build
collapses them into one.

### 7.4 `LOCAL_PERSIST` (function-local `static`)

Safe — function-local static is unique per-function, not per-file. No change.

### 7.5 Header-only code

Headers are `#include`d as-is — no changes needed. Anonymous namespaces in inline
headers are an existing smell but also not a unity issue since the code is already
in each TU.

### 7.6 Precompiled headers + unity

`target_precompile_headers` is compatible with unity builds. CMake handles ordering:
PCH is compiled before unity batches. No changes needed.

### 7.7 Debug build performance

Unity builds disable some optimizations. The `release-unity` preset uses Release mode.
Debug builds should keep unity off (the `debug` preset does not set `CMAKE_UNITY_BUILD`).

### 7.8 Pre-existing `SandboxAppStandalone` link error

`SandboxAppStandalone` currently fails to link — it uses `GameRenderLayer` symbols but
doesn't link `SandboxGameObjects` (an OBJECT library). Fix by adding the dependency:
```cmake
target_link_libraries(SandboxAppStandalone PRIVATE SandboxGameObjects)
```
This is unrelated to unity but blocks the standalone build.

### 7.9 `CMAKE_CXX_SCAN_FOR_MODULES OFF`

Already in all presets. Without this, CMake 4.x scans for C++ modules when
C++20+ is enabled, which silently disables unity builds.

---

## 8. Implementation Order

```
1.  Create SD/src/internal.hpp                    — infrastructure
2.  Update SD/CMakeLists.txt                       — force-include + UNITY_BUILD_UNIQUE_ID
3.  Migrate SD/src/core/logging.cpp                — largest file, ~50 static decls
4.  Migrate SD/src/core/ShaderCompiler.cpp         — anonymous namespace
5.  Migrate SD/src/core/ecs/CommandQueue.cpp       — anonymous + static
6.  Build SD target in unity mode                  — verify, fix ODR violations
7.  Update hot_reloader/CMakeLists.txt             — force-include + UNITY_BUILD_UNIQUE_ID
8.  Migrate hot_reloader/src/hot_reloader.cpp      — heavy static globals
9.  Build HotReloadApp target in unity mode        — verify
10. Update sandbox_app/CMakeLists.txt              — force-include + UNITY_BUILD_UNIQUE_ID
11. Migrate sandbox_app/sandbox.cpp                — static + anonymous namespace
12. Build SandboxApp + SandboxAppStandalone        — verify
13. Update testing/CMakeLists.txt                  — force-include + UNITY_BUILD_UNIQUE_ID
14. Migrate testing/tests/ecs_tests.cpp            — anonymous namespace
15. Build SDGTest in unity mode                    — verify
16. Full unity build with all targets              — final verification
```

Steps 3–5 and 8–14 can be parallelized across files.

---

## 9. Verification

```bash
# Configure with unity
cmake --preset release-unity

# Build
cmake --build --preset release-unity-build

# Run tests
./build/release-unity/bin/SDGTest

# Build without unity (ensure no regression)
cmake --preset release
cmake --build --preset release-build
```

---

## 10. Future Considerations

- **GROUP mode**: The current preset uses BATCH mode (auto-grouping). If specific
  grouping is needed later (e.g. per-module batches), switch to `UNITY_BUILD_MODE GROUP`
  with explicit `UNITY_GROUP` properties.
- **Debug unity**: Could add a `debug-unity` preset for faster debug iteration.
- **`SD_EXPORT` in unified builds**: `GenerateExportHeader` works correctly with unity
  builds — the export macro is defined per translation unit as normal.
