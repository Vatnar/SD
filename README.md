# SD - Shader Debugger

### Features

* (WIP) Real-time shader debugging
* (WIP) Vulkan-based rendering engine

### Architecture & Design

* (WIP)

---

[![License: PolyForm Shield](https://img.shields.io/badge/License-PolyForm%20Shield%201.0.0-5D6D7E)](https://polyformproject.org/licenses/shield/1.0.0)
This project is licensed under the **PolyForm Shield License 1.0.0**. (See `LICENSE` and `NOTICE`)

---

## Engine TODO ()

- **Result wrapping and handling**
- **Optimize Frame Allocation Strategy**
    - Replace per-frame stack allocations of `vk::CommandBuffer[]`, `vk::Semaphore[]`, and `vk::PipelineStageFlags[]`
      with persistent storage (e.g., `std::array` or small structs owned per-frame/per-batch).
    - Pass views/spans into submission code rather than rebuilding arrays every frame.

- **Implement Consistent GPU Error Handling**
    - **Unified Pattern:** Decide on a single strategy (e.g., `Engine::Abort` + logging for fatal errors, error
      codes/exceptions for recoverable ones).
    - **Uniform Application:** Apply this uniformly to `GetGraphicsQueue().submit(...)`, `PresentImage(...)`, and other
      critical Vulkan calls instead of selective checking.
    - **Strictness:** Maintain the same strictness as `waitForFences` (log + abort on impossible/UB-prone states) to
      prevent silent failures.

- **Refactor Submission Logic**
    - **Encapsulate Submission:** Move submission logic out of `main` into a "frame submission" or "render queue" struct
      that holds:
        - Command buffers to submit.
        - Wait semaphores and pipeline stages.
        - Signal semaphores and fences.
    - **High-Level API:** Expose a `SubmitFrame(...)` API on `VulkanContext` or a dedicated renderer class.

- **Abstract Command Recording**
    - **Hide Details:** Implement `layers.RecordCommands(imageIndex, currentFrame)` to let each layer register its own
      command buffers.
    - **Automate List Building:** Stop manually building the `cmdBuffers` list in `main`; let the engine provide the
      final list of buffers for submission.

- **Refine Event System Semantics**
    - **Lifetime Separation:** Separate "per-frame" events (auto-cleared each frame) from "longer-lived" or queued
      events.
    - **Typed Clearing:** Prefer `ClearType<T>()` for transient events instead of a global `Clear()` that wipes all
      types indiscriminately.

- **Reduce Global State**
    - **Encapsulate Globals:** Remove or encapsulate global variables like `VkPhysicalDevice vulkanPhysicalDevice`.
    - **Access Control:** Ensure access to device/physical-device handles always flows through well-defined
      engine/renderer/context objects.

- **Prepare for Library/App Split**
    - **Sketch Engine API:** Design a minimal API (`EngineConfig`, `Engine::Tick()`, `Engine::IsRunning()`) that
      encapsulates:
        - GLFW and window lifecycle.
        - Vulkan context and swapchain management.
        - Event pumping, updating, and rendering.
    - **Migration:** Gradually migrate logic from `main` into this API until `main` becomes a thin host.
