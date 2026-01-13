Engine TODO (Next Steps)
Replace per-frame stack allocations of vk::CommandBuffer[], vk::Semaphore[], and vk::PipelineStageFlags[] with persistent storage (e.g. std::array or small structs owned per-frame or per-batch), and pass views/spans into submission code rather than rebuilding arrays every frame.

Design and implement a consistent error-handling strategy for GPU operations:

Decide on a unified pattern (e.g. Engine::Abort + logging for fatal errors, error codes or exceptions for recoverable ones).

Apply it uniformly to GetGraphicsQueue().submit(...), PresentImage(...), and other critical Vulkan calls, instead of selectively checking only some calls.

Keep the same level of strictness as with waitForFences (log + abort on impossible/UB-prone states), to avoid silent failures and inconsistent behavior.

Move submission logic out of main into an engine-owned abstraction:

Introduce a small “frame submission” or “render queue” struct that encapsulates:

Command buffers to submit.

Wait semaphores and pipeline stages.

Signal semaphores and fences.

Expose a higher-level SubmitFrame(...) API on VulkanContext or a dedicated renderer class.

Hide layer-specific recording details from main:

Add layers.RecordCommands(imageIndex, currentFrame) and let each layer register its own command buffers with the engine.

Stop manually building the cmdBuffers list in main; instead, let the engine provide the final list of command buffers to submit for the frame.

Refine event lifetime semantics:

Separate “per-frame” events (auto-cleared each frame) from “longer-lived” or queued events.

Prefer ClearType<T>() for transient events instead of a global Clear() that wipes all event types.

Reduce global state:

Remove or encapsulate the global VkPhysicalDevice vulkanPhysicalDevice (and any similar globals).

Ensure device/physical-device access always flows through well-defined engine/renderer/context objects.

Prepare for library/app split:

Sketch a minimal Engine API (EngineConfig, Engine::Tick(), Engine::IsRunning()) that encapsulates:

GLFW and window lifecycle.

Vulkan context and swapchain management.

Event pumping, updating, and rendering.

Gradually migrate logic from main into this API until main becomes a thin host for the engine.
