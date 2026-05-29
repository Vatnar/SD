// TODO(docs): Add file-level Doxygen header
//   - @file VulkanWindow.hpp
//   - @brief Vulkan swapchain and per-window rendering resources
//   - Relationship to Window (GLFW) and VulkanContext
//   - Frame synchronization model
#pragma once
#include "Core/Events/window/WindowEvents.hpp"
#include "Core/LayerList.hpp"
#include "Core/Vulkan/VulkanConfig.hpp"
#include "Core/Window.hpp"
#include "VulkanContext.hpp"

namespace sd {
// TODO(docs): Document FrameSync and SwapchainSync structs
//   - Purpose: Synchronization primitives for frame rendering
//   - imageAcquired: GPU signal when swapchain image is ready
//   - inFlight: CPU-GPU sync for command buffer reuse
//   - renderComplete: GPU signal when rendering done
struct FrameSync {
  vk::UniqueSemaphore image_acquired;
  vk::UniqueFence in_flight;
};

struct SwapchainSync {
  vk::UniqueSemaphore render_complete;
};

// TODO(docs): Document VulkanWindow class thoroughly
//   - Purpose: Manages Vulkan swapchain and per-window resources
//   - Swapchain creation and recreation
//   - Frame synchronization (MAX_FRAMES_IN_FLIGHT)
//   - Command buffer management
//   - Minimization handling
//   - Integration with LayerList for swapchain recreation
class VulkanWindow {
public:
  uint32_t current_frame = 0;
  uint32_t current_image_index = 0;
  explicit VulkanWindow(Window& window, VulkanContext& vulkan_ctx);
  ~VulkanWindow();
  void recreate_swapchain(LayerList& layers);
  [[nodiscard]] const Window& get_window() const;


  // TODO: a lot of htese we prolly dont need
  FrameSync& get_frame_sync();
  SwapchainSync& get_swapchain_sync(uint32_t image_index);

  vk::UniqueSwapchainKHR& get_swapchain();
  vk::SwapchainCreateInfoKHR& get_swapchain_create_info();
  [[nodiscard]] const std::vector<vk::Image>& get_swapchain_images() const;
  vk::SurfaceFormatKHR& get_surface_format();
  vk::Extent2D& get_swapchain_extent();
  [[nodiscard]] const std::vector<vk::UniqueImageView>& get_swapchain_image_views() const;

  [[nodiscard]] const std::vector<vk::UniqueFramebuffer>& get_framebuffers() const;

  [[nodiscard]] vk::RenderPass get_render_pass() const;

  std::expected<uint32_t, vk::Result> get_vulkan_images(vk::UniqueSemaphore& image_acquired);
  vk::Result present_image(uint32_t image_index);

  void rebuild_per_image_sync();

  [[nodiscard]] vk::CommandPool get_command_pool() const;

  void resize(int witdh, int height);

  bool is_minimized() const { return m_is_minimized; }
  bool is_framebuffer_resized() const { return m_is_framebuffer_resized; }
  void reset_framebuffer_resized() { m_is_framebuffer_resized = false; }

  vk::CommandBuffer get_current_command_buffer() const { return *m_command_buffers[current_frame]; }
  VulkanContext& get_vulkan_context() { return m_vulkan_ctx; }

private:
  void create_swapchain();
  void create_render_pass(); // can be shared, but yeah...

  void create_command_pool();

  void create_swapchain_dependent_resources();
  void create_framebuffers();

  VulkanContext& m_vulkan_ctx;
  vk::Device& m_device;


  Window& m_window;

  vk::UniqueSurfaceKHR m_surface;

  vk::UniqueSwapchainKHR m_swapchain;
  vk::SwapchainCreateInfoKHR m_swapchain_create_info;

  std::vector<vk::Image> m_swapchain_images;
  std::vector<vk::UniqueImageView> m_swapchain_image_views;
  vk::Extent2D m_swapchain_extent;
  vk::SurfaceFormatKHR m_surface_format;
  vk::SurfaceCapabilitiesKHR m_surface_capabilities;

  vk::UniqueRenderPass m_render_pass;

  vk::UniqueCommandPool m_command_pool;
  std::vector<vk::UniqueCommandBuffer> m_command_buffers;

  std::vector<vk::UniqueFramebuffer> m_framebuffers;

  std::vector<FrameSync> m_frame_syncs;
  std::vector<SwapchainSync> m_swapchain_syncs;

  bool m_is_minimized{};
  bool m_is_framebuffer_resized{};
};
} // namespace SD
