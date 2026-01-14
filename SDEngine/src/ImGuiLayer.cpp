#include "ImGuiLayer.hpp"

#include "Utils.hpp"


ImGuiLayer::ImGuiLayer(VulkanContext& vulkanCtx, Window& window) :
  mVulkanCtx(vulkanCtx), mWindow(window) {
}

ImGuiLayer::~ImGuiLayer() {
}

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
    mDescriptorPool = mVulkanCtx.GetVulkanDevice()->createDescriptorPoolUnique(pool_info);
  }

  {
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
    mRenderPass = mVulkanCtx.GetVulkanDevice()->createRenderPassUnique(info);
  }

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance = *mVulkanCtx.GetInstance();
  init_info.PhysicalDevice = mVulkanCtx.GetPhysicalDevice();
  init_info.Device = *mVulkanCtx.GetVulkanDevice();
  init_info.QueueFamily = mVulkanCtx.GetGraphicsFamilyIndex();
  init_info.Queue = mVulkanCtx.GetGraphicsQueue();
  init_info.DescriptorPool = *mDescriptorPool;
  init_info.PipelineInfoMain.RenderPass = *mRenderPass;
  init_info.MinImageCount = 2;
  init_info.ImageCount = 2;
  ImGui_ImplVulkan_Init(&init_info);

  {
    vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                       mVulkanCtx.GetGraphicsFamilyIndex());
    mCommandPool = mVulkanCtx.GetVulkanDevice()->createCommandPoolUnique(poolInfo);

    vk::CommandBufferAllocateInfo allocInfo(*mCommandPool, vk::CommandBufferLevel::ePrimary,
                                            mVulkanCtx.GetMaxFramesInFlight());
    mCommandBuffers = mVulkanCtx.GetVulkanDevice()->allocateCommandBuffersUnique(allocInfo);
  }

  CreateSwapchainResources();
}

void ImGuiLayer::OnDetach() {
  mVulkanCtx.GetVulkanDevice()->waitIdle();
  DestroySwapchainResources();
  mCommandBuffers.clear();
  mCommandPool.reset();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  mRenderPass.reset();
  mDescriptorPool.reset();
}

void ImGuiLayer::Begin() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}


void ImGuiLayer::RecordCommands(uint32_t imageIndex, uint32_t currentFrame) {
  ImGui::Render();

  auto& cmdBuffer = mCommandBuffers[currentFrame];
  cmdBuffer->reset();
  cmdBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

  std::array<vk::ClearValue, 1> clearValues = {
      vk::ClearColorValue{std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f}}};

  vk::RenderPassBeginInfo rpInfo(*mRenderPass, *mFramebuffers[imageIndex],
                                 {
                                     {0, 0},
                                     mVulkanCtx.GetSwapchainExtent()
  },
                                 static_cast<uint32_t>(clearValues.size()), clearValues.data());

  cmdBuffer->beginRenderPass(rpInfo, vk::SubpassContents::eInline);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmdBuffer);
  cmdBuffer->endRenderPass();
  cmdBuffer->end();
}

void ImGuiLayer::End() {
}


void ImGuiLayer::OnSwapchainRecreated() {
  mVulkanCtx.GetVulkanDevice()->waitIdle();
  DestroySwapchainResources();
  CreateSwapchainResources();
}

void ImGuiLayer::CreateSwapchainResources() {
  const auto& swapchainExtent = mVulkanCtx.GetSwapchainExtent();
  const auto& imageViews = mVulkanCtx.GetSwapchainImageViews();

  for (const auto& view : imageViews) {
    vk::ImageView attachments[] = {*view};
    vk::FramebufferCreateInfo fbInfo({}, *mRenderPass, 1, attachments, swapchainExtent.width,
                                     swapchainExtent.height, 1);
    mFramebuffers.push_back(mVulkanCtx.GetVulkanDevice()->createFramebufferUnique(fbInfo));
  }
}

void ImGuiLayer::DestroySwapchainResources() {
  mFramebuffers.clear();
}
