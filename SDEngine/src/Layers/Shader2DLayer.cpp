#include "Layers/Shader2DLayer.hpp"
vk::VertexInputBindingDescription Shader2DLayer::Vertex::getBindingDescription() {
  vk::VertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = vk::VertexInputRate::eVertex;
  return bindingDescription;
}
std::array<vk::VertexInputAttributeDescription, 2>
Shader2DLayer::Vertex::getAttributeDescriptions() {
  std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
  attributeDescriptions[0].offset = offsetof(Vertex, position);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = vk::Format::eR32G32Sfloat;
  attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

  return attributeDescriptions;
}

vk::CommandBuffer Shader2DLayer::GetCommandBuffer(uint32_t currentFrame) {
  return *mCommandBuffers[currentFrame];
}

void Shader2DLayer::OnAttach() {
  auto& device = mVulkanCtx.GetVulkanDevice();
  auto& physicalDevice = mVulkanCtx.GetPhysicalDevice();
  auto commandPool = mVulkanCtx.GetCommandPool();
  auto queue = mVulkanCtx.GetGraphicsQueue();

  CreateRenderPass();
  CreateDescriptorSetLayout();
  CreateGraphicsPipeline();
  CreateCommandBuffers();

  auto textureResult = CreateTexture(device.get(), physicalDevice, queue, commandPool,
                                     "assets/textures/example.jpg");
  if (textureResult) {
    mTexture = std::move(textureResult.value());
  } else {
    Engine::Abort(textureResult.error());
  }

  const std::vector<Vertex> vertices = {
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
      { {0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
      {  {0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
      {  {0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
      { {-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}}
  };

  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  // Staging buffer
  auto [stagingBuffer, stagingBufferMemory] = CreateBuffer(
      device.get(), physicalDevice, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data = CheckVulkanResVal(device->mapMemory(*stagingBufferMemory, 0, bufferSize),
                                 "Failed to map memory: ");
  memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
  device->unmapMemory(*stagingBufferMemory);

  // Vertex buffer
  auto [vb, vbMem] =
      CreateBuffer(device.get(), physicalDevice, bufferSize,
                   vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);
  mVertexBuffer = std::move(vb);
  mVertexBufferMemory = std::move(vbMem);

  SingleTimeCommand(device.get(), queue, commandPool, [&](const vk::CommandBuffer& cmdBuffer) {
    vk::BufferCopy copyRegion(0, 0, bufferSize);
    cmdBuffer.copyBuffer(*stagingBuffer, *mVertexBuffer, copyRegion);
  });

  for (size_t i = 0; i < mVulkanCtx.GetMaxFramesInFlight(); i++) {
    vk::DeviceSize vpBufferSize = sizeof(ViewProjection);
    auto [ub, ubMem] = CreateBuffer(
        device.get(), physicalDevice, vpBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    mUniformBuffers.push_back(std::move(ub));
    mUniformBuffersMemory.push_back(std::move(ubMem));
    mUniformBuffersMapped.push_back(
        CheckVulkanResVal(device->mapMemory(*mUniformBuffersMemory.back(), 0, vpBufferSize),
                          "Failed to map memory: "));
  }

  vk::DescriptorPoolSize poolSizes[] = {
      {vk::DescriptorType::eUniformBuffer,
       static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight())                                    },
      { vk::DescriptorType::eSampledImage, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight())},
      {      vk::DescriptorType::eSampler, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight())}
  };

  vk::DescriptorPoolCreateInfo poolInfo(
      {}, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight()), 3, poolSizes);
  mDescriptorPool = CheckVulkanResVal(device->createDescriptorPoolUnique(poolInfo),
                                      "Failed to create unique descriptor pool");

  std::vector<vk::DescriptorSetLayout> layouts(mVulkanCtx.GetMaxFramesInFlight(),
                                               *mDescriptorSetLayout);
  vk::DescriptorSetAllocateInfo allocInfo(*mDescriptorPool, mVulkanCtx.GetMaxFramesInFlight(),
                                          layouts.data());
  mDescriptorSets = CheckVulkanResVal(device->allocateDescriptorSets(allocInfo),
                                      "Failed to allocate descriptor sets: ");

  vk::SamplerCreateInfo samplerCreateInfo(
      {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
      vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
      vk::SamplerAddressMode::eRepeat, 0.0f, VK_FALSE, 0.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f,
      0.0f, vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
  mSampler = CheckVulkanResVal(device->createSamplerUnique(samplerCreateInfo),
                               "Failed to create unique sampler");

  for (size_t i = 0; i < mVulkanCtx.GetMaxFramesInFlight(); i++) {
    vk::DescriptorBufferInfo bufferInfo(*mUniformBuffers[i], 0, sizeof(ViewProjection));
    vk::DescriptorImageInfo textureInfo({}, *mTexture.imageView,
                                        vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo samplerInfo(*mSampler, {}, {});

    std::array<vk::WriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0].dstSet = mDescriptorSets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].dstSet = mDescriptorSets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &textureInfo;

    descriptorWrites[2].dstSet = mDescriptorSets[i];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = vk::DescriptorType::eSampler;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &samplerInfo;

    device->updateDescriptorSets(descriptorWrites, nullptr);
  }

  CreateFramebuffers();
}
void Shader2DLayer::OnDetach() {
  auto& device = mVulkanCtx.GetVulkanDevice();
  CheckVulkanRes(device->waitIdle(), "Failed to wait for device to idle");
}
void Shader2DLayer::UpdateUniformBuffer(uint32_t currentImage) const {
  ViewProjection vp{};
  std::ranges::fill(vp.proj, 0.0f);
  std::ranges::fill(vp.model, 0.0f);
  std::ranges::fill(vp.view, 0.0f);

  const auto& extent = mVulkanCtx.GetSwapchainExtent();
  const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
  float scaleX = 1.0f;
  float scaleY = 1.0f;

  if (aspect > 1.0f)
    scaleX /= aspect;
  else
    scaleY *= aspect;

  // clang-format off
  vp.proj = {
    scaleX, 0.f,    0.f, 0.f,
    0.f,    scaleY, 0.f, 0.f,
    0.f,    0.f,    1.f, 0.f,
    0.f,    0.f,    0.f, 1.f,
  };
  vp.model = {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f,
  };
  vp.view = {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f,
  };
  // clang-format on


  memcpy(mUniformBuffersMapped[currentImage], &vp, sizeof(vp));
}
void Shader2DLayer::RecordCommands(uint32_t imageIndex, uint32_t currentFrame) {
  UpdateUniformBuffer(currentFrame);

  auto& cmdBuffer = mCommandBuffers[currentFrame];
  CheckVulkanRes(cmdBuffer->reset(), "Failed to reset commandbuffer");

  constexpr vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  CheckVulkanRes(cmdBuffer->begin(beginInfo), "Failed to begin commandbuffer");

  std::array<vk::ClearValue, 1> clearValues{};
  clearValues[0].color = vk::ClearColorValue{mClearColor};

  vk::RenderPassBeginInfo renderPassInfo(*mRenderPass, *mFramebuffers[imageIndex],
                                         {
                                             {0, 0},
                                             mVulkanCtx.GetSwapchainExtent()
  },
                                         clearValues);

  cmdBuffer->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
  cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline);
  vk::DeviceSize offsets[] = {0};
  cmdBuffer->bindVertexBuffers(0, 1, &*mVertexBuffer, offsets);
  cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mPipelineLayout, 0,
                                mDescriptorSets[currentFrame], nullptr);
  cmdBuffer->draw(6, 1, 0, 0);
  cmdBuffer->endRenderPass();
  CheckVulkanRes(cmdBuffer->end(), "Failed to end commandbuffer");
}
void Shader2DLayer::OnEvent(InputEvent& e) {
  switch (e.category()) {
    case InputEventCategory::KeyReleased: {
      if (const auto key = dynamic_cast<KeyReleasedEvent&>(e); GLFW_KEY_SPACE == key.key) {
        mClearColor = std::array{0.0f, 0.0f, 0.0f, 1.0f};
        e.Handled = true;
      }
    } break;
    case InputEventCategory::MouseReleased: {
      if (const auto button = dynamic_cast<MouseReleasedEvent&>(e);
          GLFW_MOUSE_BUTTON_LEFT == button.button) {
        mClearColor = std::array{0.0f, 0.0f, 0.0f, 1.0f};
        e.Handled = true;
      }
    } break;
    case InputEventCategory::MousePressed: {
      if (const auto button = dynamic_cast<MousePressedEvent&>(e);
          GLFW_MOUSE_BUTTON_LEFT == button.button) {
        mClearColor = std::array{0.0f, 1.0f, 0.0f, 1.0f};
        e.Handled = true;
      }
    } break;
    case InputEventCategory::KeyPressed: {
      const auto key = dynamic_cast<KeyPressedEvent&>(e);
      if (key.key == GLFW_KEY_SPACE && key.repeat == false) {
        mClearColor = std::array{1.0f, 0.0f, 0.0f, 1.0f};
        e.Handled = true;
      }
      if (key.key == GLFW_KEY_LEFT && key.repeat == false) {
        SetPixelSource("assets/shaders/changeTestPixel.hlsl");
        e.Handled = true;
      }
      if (key.key == GLFW_KEY_RIGHT && key.repeat == false) {
        SetPixelSource("assets/shaders/pixel.hlsl");
        e.Handled = true;
      }
    } break;
    default:
      break;
  }
}
void Shader2DLayer::OnSwapchainRecreated() {
  CheckVulkanRes(mVulkanCtx.GetVulkanDevice()->waitIdle(), "Failed to wait for vulkan device");
  mFramebuffers.clear();
  CreateFramebuffers();
  CreateGraphicsPipeline();
}

void Shader2DLayer::OnUpdate(float dt) {
  Layer::OnUpdate(dt);
  const auto logger = spdlog::get("engine");

  const std::filesystem::file_time_type lastWriteVertex =
      std::filesystem::last_write_time(mVertexSource);
  const std::filesystem::file_time_type lastWritePixel =
      std::filesystem::last_write_time(mPixelSource);

  if (lastWritePixel != mLastWritePixel || lastWriteVertex != mLastWriteVertex)
    mNewShaderSource = true;


  if (mNewShaderSource) {
    mNewShaderSource = false;
    using clock = std::chrono::steady_clock;

    const auto before = clock::now();
    CreateGraphicsPipeline();
    const auto after = clock::now();

    const auto duration = after - before;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    logger->info("Time to change shaders: {}ms", ms);
  }

  mLastWriteVertex = lastWriteVertex;
  mLastWritePixel = lastWritePixel;
}

void Shader2DLayer::CreateRenderPass() {
  vk::AttachmentDescription colorAttachment(
      {}, mVulkanCtx.GetSurfaceFormat().format, vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal);

  vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef);

  vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, {},
                                   vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass, dependency);
  mRenderPass =
      CheckVulkanResVal(mVulkanCtx.GetVulkanDevice()->createRenderPassUnique(renderPassInfo),
                        "Failed to create unique render pass");
}
void Shader2DLayer::CreateDescriptorSetLayout() {
  constexpr vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer,
                                                            1, vk::ShaderStageFlagBits::eVertex);
  constexpr vk::DescriptorSetLayoutBinding textureLayoutBinding(
      1, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment);
  constexpr vk::DescriptorSetLayoutBinding samplerLayoutBinding(2, vk::DescriptorType::eSampler, 1,
                                                                vk::ShaderStageFlagBits::eFragment);

  std::array bindings = {uboLayoutBinding, textureLayoutBinding, samplerLayoutBinding};
  const vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);

  mDescriptorSetLayout =
      CheckVulkanResVal(mVulkanCtx.GetVulkanDevice()->createDescriptorSetLayoutUnique(layoutInfo),
                        "Failed to create unique descriptorsetlayout: ");
}
bool Shader2DLayer::CreateGraphicsPipeline() {
  auto& device = mVulkanCtx.GetVulkanDevice();

  auto logger = spdlog::get("engine");
  ShaderCompiler compiler;

  // compile shaders
  // TODO: Make recompile and shit
  std::vector<char> vertexSpv;
  if (!compiler.CompileShader(mVertexSource, vertexSpv, "vs_6_0")) {
    logger->info("Compilation of Vertex shader %s failed", mVertexSource);
    return false;
  }

  std::vector<char> pixelSpv;
  if (!compiler.CompileShader(mPixelSource, pixelSpv, "ps_6_0")) {
    logger->info("Compilation of Pixel shader %s failed", mPixelSource);
    return false;
  }


  auto vertShaderModule = CreateShaderModule(device.get(), vertexSpv);
  auto fragShaderModule = CreateShaderModule(device.get(), pixelSpv);

  // define shader stages
  vk::PipelineShaderStageCreateInfo shaderStages[] = {
      vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule,

                                        "main"),
      vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule,
                                        "main")};

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, bindingDescription,
                                                         attributeDescriptions);

  // group vertices into primitives for the vertex shader
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList,
                                                         VK_FALSE);

  vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(mVulkanCtx.GetSwapchainExtent().width),
                        static_cast<float>(mVulkanCtx.GetSwapchainExtent().height), 0.0f, 1.0f);

  // TODO: Should this be dynamic? STUDY: Dynamic vs hard-coded scissor extent
  vk::Rect2D scissor({0, 0}, mVulkanCtx.GetSwapchainExtent());
  vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

  // draw solid triangles,
  // TODO: Discard backfacing triangles when not in debug mode (eBack + ECW)
  vk::PipelineRasterizationStateCreateInfo rasterizer(
      {}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
      vk::FrontFace::eClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

  // TODO: Enable MSAA later, e4 or something, probably make it dynamic
  vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

  // NOTE: We dont need blending for this layer
  vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
  colorBlendAttachment.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1,
                                                      &colorBlendAttachment);

  // define shader inputs
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, *mDescriptorSetLayout);
  mPipelineLayout = CheckVulkanResVal(device->createPipelineLayoutUnique(pipelineLayoutInfo),
                                      "Failed to create pipeline-layout unique: ");

  vk::GraphicsPipelineCreateInfo pipelineInfo(
      {}, shaderStages, &vertexInputInfo, &inputAssembly, nullptr, &viewportState, &rasterizer,
      &multisampling, nullptr, &colorBlending, nullptr, *mPipelineLayout, *mRenderPass, 0);

  auto result = device->createGraphicsPipelineUnique(nullptr, pipelineInfo);
  if (result.result != vk::Result::eSuccess) {
    // TODO: Figure out how we do errors and wrap our own errors
    logger->error("Failed to create grapchis pipeline");
    return false;
  }
  mPipeline = std::move(result.value);
  return true;
}
void Shader2DLayer::CreateFramebuffers() {
  mFramebuffers.clear();
  const auto& imageViews = mVulkanCtx.GetSwapchainImageViews();
  const auto& extent = mVulkanCtx.GetSwapchainExtent();

  for (const auto& view : imageViews) {
    vk::ImageView attachments[] = {*view};
    vk::FramebufferCreateInfo framebufferInfo({}, *mRenderPass, attachments, extent.width,
                                              extent.height, 1);
    mFramebuffers.push_back(
        CheckVulkanResVal(mVulkanCtx.GetVulkanDevice()->createFramebufferUnique(framebufferInfo),
                          "Failed to create unique framebuffer: "));
  }
}
void Shader2DLayer::CreateCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo(mVulkanCtx.GetCommandPool(),
                                          vk::CommandBufferLevel::ePrimary,
                                          mVulkanCtx.GetMaxFramesInFlight());
  mCommandBuffers =
      CheckVulkanResVal(mVulkanCtx.GetVulkanDevice()->allocateCommandBuffersUnique(allocInfo),
                        "Failed to allocate unique commandbuffers");
}
