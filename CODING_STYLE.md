# SD Coding Style Guide

This document defines the naming conventions, formatting rules, and general code style for the SD engine. The goal is
consistency across the entire codebase.

All rules are enforced by `.clang-format` and `.clang-tidy` where technically possible. Any deviation must be justified
with a code-review comment.

---

## 1. Naming Conventions

### 1.1 General Entity Naming

| Category                                                               | Case       | Prefix / Suffix       | Example                                     |
|------------------------------------------------------------------------|------------|-----------------------|---------------------------------------------|
| Classes, structs, unions, enums                                        | PascalCase | —                     | `RenderPass`, `Entity`, `WindowMode`        |
| Type aliases (`using` / `typedef`)                                     | PascalCase | `_t` optional         | `ViewResult_t`, `ComponentId`               |
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

**Notes**

- The prefix is *always* required, even for private members.
- Static local variables inside functions should also use the `s_` prefix.

### 1.3 Files

- **C++ headers**: PascalCase matching the primary exported type (`Application.hpp`, `EntityManager.hpp`).
- **C++ sources**: PascalCase matching the header (`Application.cpp`).
- **C headers**: snake_case with `.h` extension (`game.h`).

---

## 2. Header Guards

Use `#pragma once` in every header. Do **not** use traditional `#ifndef` / `#define` / `#endif` guards.

```cpp
#pragma once

// ... header contents ...
```

---

## 3. Include Order

Includes must be grouped and ordered as follows, separated by blank lines:

1. **Matching header** — the `.hpp` that corresponds to the current `.cpp` file.
2. **Standard library** — C++ standard headers and C/POSIX system headers (angle brackets, no vendor directory).
3. **Third-party library** — external dependencies (angle brackets, usually have a directory path).
4. **Project headers** — engine-internal headers (quoted includes).

Example:

```cpp
#include "Core/Vulkan/VulkanRenderer.hpp"  // 1. matching header

#include <algorithm>                        // 2. standard library
#include <vector>

#include <vulkan/vulkan.hpp>                // 3. third-party
#include <spdlog/spdlog.h>

#include "Core/Base.hpp"                    // 4. project headers
#include "Core/Logging.hpp"
```

**Rationale**: The matching header first validates that it is self-contained. Separating standard, third-party, and
project headers makes it immediately obvious where a dependency comes from and prevents accidental project→third-party
circular includes.

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