#include "Core/SDImGuiContext.hpp"

#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"
#include "Utils/Utils.hpp"
#include "Application.hpp"

namespace SD {

SDImGuiContext::~SDImGuiContext() {
  Shutdown();
}

void SDImGuiContext::Init(Window& window, VulkanWindow& vw) {
  if (mVulkanInitialized)
    return;

  // 1. Create Context
  IMGUI_CHECKVERSION();
  mContext = ImGui::CreateContext();
  ImGui::SetCurrentContext(mContext);

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigDockingTransparentPayload = true;

  ImGui::StyleColorsDark();

  VulkanContext& ctx = vw.GetVulkanContext();
  CreateDescriptorPool(ctx);

  // 2. Init GLFW
  ImGui_ImplGlfw_InitForVulkan(window.GetNativeHandle(), true);

  CreateCompatibleRenderPass(ctx, vw.GetSurfaceFormat().format);

  // 3. Renderer Init (Vulkan)
  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = *ctx.GetInstance();
  init_info.PhysicalDevice = ctx.GetPhysicalDevice();
  init_info.Device = *ctx.GetVulkanDevice();
  init_info.QueueFamily = ctx.GetGraphicsFamilyIndex();
  init_info.Queue = ctx.GetGraphicsQueue();
  init_info.DescriptorPool = *mDescriptorPool;
  init_info.MinImageCount = 2;
  init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;

  init_info.PipelineInfoMain.RenderPass = *mRenderPass;
  init_info.PipelineInfoMain.Subpass = 0;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  // Create default sampler for textures
  vk::SamplerCreateInfo samplerInfo{};
  samplerInfo.magFilter = vk::Filter::eLinear;
  samplerInfo.minFilter = vk::Filter::eLinear;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = vk::CompareOp::eAlways;
  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

  mDefaultSampler = CheckVulkanResVal(ctx.GetVulkanDevice()->createSamplerUnique(samplerInfo),
                                      "Failed to create ImGui sampler");

  mVulkanInitialized = true;
}

void SDImGuiContext::Shutdown() {
  if (mVulkanInitialized) {
    ImGui::SetCurrentContext(mContext);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    mVulkanInitialized = false;
  }

  if (mContext) {
    ImGui::DestroyContext(mContext);
    mContext = nullptr;
  }

  mRenderPass.reset();
  mDescriptorPool.reset();
}

void SDImGuiContext::BeginFrame() {
  ImGui::SetCurrentContext(mContext);
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void SDImGuiContext::EndFrame() {
  ImGui::Render();
}

void SDImGuiContext::RenderDrawData(vk::CommandBuffer cmd) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void SDImGuiContext::UpdatePlatformWindows() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

void SDImGuiContext::BeginDockSpace(const std::string& title) {
  static bool dockspaceOpen = true;
  static bool opt_fullscreen_persistant = true;
  bool opt_fullscreen = opt_fullscreen_persistant;
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  }

  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin(title.c_str(), &dockspaceOpen, window_flags);
  ImGui::PopStyleVar();

  if (opt_fullscreen)
    ImGui::PopStyleVar(2);

  // --- Menu Bar ---
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Project", "Ctrl+N")) {}
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {}
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) { Application::Get().Close(); }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Toggle Viewport")) {}
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

void SDImGuiContext::EndDockSpace() {
  ImGui::End();
}

void SDImGuiContext::CreateDescriptorPool(VulkanContext& ctx) {
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

  mDescriptorPool = CheckVulkanResVal(ctx.GetVulkanDevice()->createDescriptorPoolUnique(pool_info),
                                      "Failed to create ImGui descriptor pool");
}

void SDImGuiContext::CreateCompatibleRenderPass(VulkanContext& ctx, vk::Format format) {
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

  mRenderPass = CheckVulkanResVal(ctx.GetVulkanDevice()->createRenderPassUnique(info),
                                  "Failed to create ImGui RenderPass");
}

VkDescriptorSet SDImGuiContext::CreateTextureFromView(VkImageView view, VkImageLayout layout) {
  return ImGui_ImplVulkan_AddTexture(*mDefaultSampler, view, (VkImageLayout)layout);
}

void SDImGuiContext::RemoveTexture(VkDescriptorSet descriptorSet) {
  ImGui_ImplVulkan_RemoveTexture(descriptorSet);
}

} // namespace SD
