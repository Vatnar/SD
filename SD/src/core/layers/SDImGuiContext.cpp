#include "core/SDImGuiContext.hpp"

#include "Application.hpp"
#include "imgui_impl_glfw.h"

namespace sd {

SDImGuiContext::~SDImGuiContext() {
  shutdown();
}

void SDImGuiContext::init(Window& window, VulkanWindow& vw) {
  if (m_is_vulkan_initialized)
    return;

  // 1. Create Context
  IMGUI_CHECKVERSION();
  m_context = ImGui::CreateContext();
  ImGui::SetCurrentContext(m_context);

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigDockingTransparentPayload = true;

  // Set stable INI filename for layout persistence
  // Stored in project root (gitignored)
  io.IniFilename = "imgui.ini";

  ImGui::StyleColorsDark();

  VulkanContext& ctx = vw.get_vulkan_context();
  create_descriptor_pool(ctx);

  // 2. Init GLFW
  ImGui_ImplGlfw_InitForVulkan(window.get_native_handle(), true);

  create_compatible_render_pass(ctx, vw.get_surface_format().format);

  // 3. Renderer Init (Vulkan)
  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance       = *ctx.get_instance();
  init_info.PhysicalDevice = ctx.get_physical_device();
  init_info.Device         = *ctx.get_vulkan_device();
  init_info.QueueFamily    = ctx.get_graphics_family_index();
  init_info.Queue          = ctx.get_graphics_queue();
  init_info.DescriptorPool = *m_descriptor_pool;
  init_info.MinImageCount  = 2;
  init_info.ImageCount     = g_max_frames_in_flight;

  init_info.PipelineInfoMain.RenderPass  = *m_render_pass;
  init_info.PipelineInfoMain.Subpass     = 0;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  // Create default sampler for textures
  vk::SamplerCreateInfo sampler_info{};
  sampler_info.magFilter               = vk::Filter::eLinear;
  sampler_info.minFilter               = vk::Filter::eLinear;
  sampler_info.addressModeU            = vk::SamplerAddressMode::eRepeat;
  sampler_info.addressModeV            = vk::SamplerAddressMode::eRepeat;
  sampler_info.addressModeW            = vk::SamplerAddressMode::eRepeat;
  sampler_info.anisotropyEnable        = VK_FALSE;
  sampler_info.maxAnisotropy           = 1.0f;
  sampler_info.borderColor             = vk::BorderColor::eIntOpaqueBlack;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable           = VK_FALSE;
  sampler_info.compareOp               = vk::CompareOp::eAlways;
  sampler_info.mipmapMode              = vk::SamplerMipmapMode::eLinear;

  m_default_sampler = check_vulkan_res_val(
      ctx.get_vulkan_device()->createSamplerUnique(sampler_info), "Failed to create ImGui sampler");

  m_is_vulkan_initialized = true;
}

void SDImGuiContext::shutdown() {
  if (m_is_vulkan_initialized) {
    ImGui::SetCurrentContext(m_context);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    m_is_vulkan_initialized = false;
  }

  if (m_context) {
    ImGui::DestroyContext(m_context);
    m_context = nullptr;
  }

  m_render_pass.reset();
  m_descriptor_pool.reset();
}

void SDImGuiContext::begin_frame() {
  ImGui::SetCurrentContext(m_context);
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void SDImGuiContext::end_frame() {
  ImGui::Render();
}

void SDImGuiContext::render_draw_data(vk::CommandBuffer cmd) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void SDImGuiContext::update_platform_windows() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

void SDImGuiContext::begin_dock_space(const std::string& title) {
  static bool               dockspace_open            = true;
  static bool               opt_fullscreen_persistant = true;
  bool                      opt_fullscreen            = opt_fullscreen_persistant;
  static ImGuiDockNodeFlags dockspace_flags           = ImGuiDockNodeFlags_PassthruCentralNode;

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  }

  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin(title.c_str(), &dockspace_open, window_flags);
  ImGui::PopStyleVar();

  if (opt_fullscreen)
    ImGui::PopStyleVar(2);

  // --- Menu Bar ---
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Project", "Ctrl+N")) {
      }
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        m_callbacks.close_app();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Toggle Viewport")) {
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  // --- DockSpace ---
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  }
}

void SDImGuiContext::end_dock_space() {
  ImGui::End();
}

void SDImGuiContext::create_descriptor_pool(VulkanContext& ctx) {
  vk::DescriptorPoolSize pool_sizes[] = {
      {             vk::DescriptorType::eSampler, 1000},
      {vk::DescriptorType::eCombinedImageSampler, 1000},
      {        vk::DescriptorType::eSampledImage, 1000},
      {        vk::DescriptorType::eStorageImage, 1000},
      {  vk::DescriptorType::eUniformTexelBuffer, 1000},
      {  vk::DescriptorType::eStorageTexelBuffer, 1000},
      {       vk::DescriptorType::eUniformBuffer, 1000},
      {       vk::DescriptorType::eStorageBuffer, 1000},
      {vk::DescriptorType::eUniformBufferDynamic, 1000},
      {vk::DescriptorType::eStorageBufferDynamic, 1000},
      {     vk::DescriptorType::eInputAttachment, 1000}
  };

  vk::DescriptorPoolCreateInfo pool_info(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                         1000 * std::size(pool_sizes), std::size(pool_sizes),
                                         pool_sizes);

  m_descriptor_pool =
      check_vulkan_res_val(ctx.get_vulkan_device()->createDescriptorPoolUnique(pool_info),
                           "Failed to create ImGui descriptor pool");
}

void SDImGuiContext::create_compatible_render_pass(VulkanContext& ctx, vk::Format format) {
  vk::AttachmentDescription attachment{};
  attachment.setFormat(format)
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eLoad) // Load existing content (Overlay)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal) // Already drawn to by Scene
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);           // Ready for swap

  vk::AttachmentReference color_ref(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass{};
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
      .setColorAttachmentCount(1)
      .setPColorAttachments(&color_ref);

  vk::SubpassDependency dependency{};
  dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
      .setDstSubpass(0)
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
      .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo info({}, 1, &attachment, 1, &subpass, 1, &dependency);

  m_render_pass = check_vulkan_res_val(ctx.get_vulkan_device()->createRenderPassUnique(info),
                                       "Failed to create ImGui RenderPass");
}

VkDescriptorSet SDImGuiContext::create_texture_from_view(VkImageView view, VkImageLayout layout) {
  return ImGui_ImplVulkan_AddTexture(*m_default_sampler, view, (VkImageLayout)layout);
}

void SDImGuiContext::remove_texture(VkDescriptorSet descriptor_set) {
  ImGui_ImplVulkan_RemoveTexture(descriptor_set);
}

} // namespace sd
