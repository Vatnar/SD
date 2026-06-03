# SD Coding Style Guide

## 1. Naming Conventions

### 1.1 General Entity Naming

| Category                                                               | Case       | Prefix / Suffix       | Example                                     |
|------------------------------------------------------------------------|------------|-----------------------|---------------------------------------------|
| Classes, structs, unions, enums                                        | PascalCase | —                     | `RenderPass`, `Entity`, `WindowMode`        |
| Type aliases (`using` / `typedef`)                                     | PascalCase | _                     | `ViewResult_t`, `ComponentId`               |
| Template parameters                                                    | PascalCase | —                     | `TComponent`, `TAlloc`                      |
| Concepts                                                               | PascalCase | —                     | `Renderable`, `Serializable`                |
| Functions, methods                                                     | snake_case | —                     | `push_layer()`, `get_name()`                |
| Local variables, parameters                                            | snake_case | —                     | `window_width`, `dt`                        |
| Namespaces                                                             | snake_case | —                     | `namespace sd { ... }`                      |
| Compile-time constants (`constexpr`, `consteval`, enum values, macros) | UPPER_CASE | —                     | `MAX_LAYERS`, `StatusCode::OK`, `SD_ASSERT` |
| Boolean getters / predicates                                           | snake_case | `is_`, `has_`, `can_` | `is_running()`, `has_component()`           |
| Interface / abstract base classes                                      | PascalCase | `I` prefix            | `IRenderer`, `ILayer`                       |
| C function-pointer typedefs                                            | PascalCase | `Fn` suffix           | `OnLoadFn`, `GetApiFn`                      |

**Notes**

- `PascalCase` = UpperCamelCase (`MyClassName`).
- `snake_case` = lower-case with underscores (`my_variable_name`).
- `UPPER_CASE` = ALL_CAPS with underscores (`MAX_VALUE`).

### 1.2 Member & Storage-Class Naming

| Category                          | Case       | Prefix | Example          |
|-----------------------------------|------------|--------|------------------|
| Class / struct non-static members | snake_case | `m_`   | `m_window_width` |
| Class / struct static members     | snake_case | `s_`   | `s_instance`     |
| Global variables (any scope)      | snake_case | `g_`   | `g_application`  |

### 1.3 Files

- **C++ headers with a primary exported type**: PascalCase matching that type (`Application.hpp`, `EntityManager.hpp`).
- **C++ sources with a primary exported type**: PascalCase matching the header (`Application.cpp`).
- **C++ files without a primary exported type** (entry points, free-function utilities, implementation-detail files):
  `snake_case` (`main.cpp`, `string_utils.cpp`, `shader_compiler.cpp`).
- **C headers**: snake_case with `.h` extension (`game.h`).

---

## 2. Header Guards

Use `#pragma once` in every header.

---

## 3. Include Order

1. **Matching header** — the `.hpp` that corresponds to the current `.cpp` file.
2. **Standard library** — C++ standard headers and C/POSIX system headers (angle brackets, no vendor directory).
3. **Third-party library** — external dependencies (angle brackets, usually have a directory path).
4. **Project headers** — engine-internal headers (quoted includes).

Example:

```cpp
#include "SD/core/Vulkan/VulkanRenderer.hpp"  // 1. matching header

#include <algorithm>                        // 2. standard library
#include <vector>

#include "SD/core/vulkan/vulkan_config.hpp"                // 3. third-party
#include <spdlog/spdlog.h>

#include "SD/core/Base.hpp"                    // 4. project headers
#include "SD/core/Logging.hpp"
```

---

## 4. Formatting Rules

The project uses `.clang-format` (Google base style with tweaks). Key settings are:

| Setting                            | Value                         |
|------------------------------------|-------------------------------|
| Column limit                       | `100`                         |
| Indent width                       | `2` spaces                    |
| Tab width                          | `2`                           |
| Use tabs                           | Never                         |
| Brace style                        | Attach (K&R)                  |
| Pointer / reference alignment      | Left (`int* ptr`, `int& ref`) |
| Qualifier alignment                | Left (`const int* x`)         |
| Short functions on a single line   | Inline only                   |
| Always break template declarations | Yes                           |

---

## 5. Language-Specific Rules

### 5.1 C++

- Prefer `using` over `typedef` for type aliases.
- Use `nullptr` instead of `NULL` or `0`.
- Use `override` and `final` explicitly.
- Mark constructors `explicit` when they accept a single argument.
- Prefer `enum class` over plain `enum`.

### 5.2 C API Boundary

- C structs are still **types**, therefore use `PascalCase`.
- C typedefs for function pointers keep the `Fn` suffix and use `PascalCase`.
- This style applies only to the current C-compatible headers; future non-C replacements will follow normal C++ rules.