#include "Layers/ImGuiLayer.hpp"

#include "Utils/Utils.hpp"


ImGuiLayer::ImGuiLayer(Scene& scene, VulkanContext& vulkanCtx, Window& window) :
  Layer(scene), mVulkanCtx(vulkanCtx), mWindow(window) {
}

ImGuiLayer::~ImGuiLayer() = default;

void ImGuiLayer::OnAttach() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(mWindow.GetNativeHandle(), true);

  {
    // TODO: This descriptor pool is huge and may be wasteful. Consider a more dynamic or
    // specialized pool for ImGui.
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
    mDescriptorPool =
        CheckVulkanResVal(mVulkanCtx.GetVulkanDevice()->createDescriptorPoolUnique(pool_info),
                          "Failed to create unique descriptor pool");
  }

  CreateCompatibleRenderPass();

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = *mVulkanCtx.GetInstance();
  init_info.PhysicalDevice = mVulkanCtx.GetPhysicalDevice();
  init_info.Device = *mVulkanCtx.GetVulkanDevice();
  init_info.QueueFamily = mVulkanCtx.GetGraphicsFamilyIndex();
  init_info.Queue = mVulkanCtx.GetGraphicsQueue();
  init_info.DescriptorPool = *mDescriptorPool;
  init_info.PipelineInfoMain.RenderPass = *mCompatibleRenderPass;
  init_info.MinImageCount = 2; // TODO: Query sync interval/min image count
  init_info.ImageCount = MAX_FRAMES_IN_FLIGHT; // This should match swapchain image count
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  ImGui_ImplVulkan_Init(&init_info);


  // Upload fonts (this records commands to the queue)

}

void ImGuiLayer::OnDetach() {
  CheckVulkanRes(mVulkanCtx.GetVulkanDevice()->waitIdle(), "Failed to wait for vulkan device ");
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  mCompatibleRenderPass.reset();
  mDescriptorPool.reset();
}

void ImGuiLayer::Begin() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}


void ImGuiLayer::OnUpdate(float dt) {
  Begin();
}

void ImGuiLayer::OnRender(vk::CommandBuffer& cmd) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGuiLayer::End() {
  // Traditionally End() might call Render(), but since we render in OnRender (inside renderpass), we do it there.
}


void ImGuiLayer::OnSwapchainRecreated() {
  CheckVulkanRes(mVulkanCtx.GetVulkanDevice()->waitIdle(), "Failed to wait for vulkan device");
  // ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount); // If count changed
  // RenderPass might need to be compatible still.
  // Since we use dynamic rendering or compatible render pass, if format changes, we might need to update.
  // ImGui_ImplVulkan_Init caches the RenderPass. If the swapchain format changes, the RenderPass passed to Init
  // might become incompatible if ImGui relies on it for pipeline creation.
  // WE should probably re-init ImGui_ImplVulkan or at least check if we need to.
  // For now, assuming format doesn't change often or remains compatible (B8G8R8A8_SRGB usually).
}

void ImGuiLayer::CreateCompatibleRenderPass() {
  // TODO: This render pass is specific to ImGui. Consider making it more generic or using dynamic
  // rendering.
  vk::AttachmentDescription attachment(
      {}, mVulkanCtx.GetSurfaceFormat().format, vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR);

  vk::AttachmentReference color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal);
  vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1,
                                 &color_attachment);

  vk::SubpassDependency dependency(
      VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo info({}, 1, &attachment, 1, &subpass, 1, &dependency);
  mCompatibleRenderPass = CheckVulkanResVal(mVulkanCtx.GetVulkanDevice()->createRenderPassUnique(info),
                                  "Failed to create unique render pass: ");
}
