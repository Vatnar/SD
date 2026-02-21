#include "Layers/Shader2DLayer.hpp"

#include "Core/Events/window/KeyboardEvents.hpp"
#include "Core/Events/window/MouseEvents.hpp"


std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory>
CreateBuffer2(const vk::Device& device, const vk::PhysicalDevice& physicalDevice,
              vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);

  vk::UniqueBuffer buffer =
      CheckVulkanResVal(device.createBufferUnique(bufferInfo), "Failed to create unique buffer");

  vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(*buffer);

  vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
  u32 memoryTypeIndex = -1;
  for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memRequirements.memoryTypeBits & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      memoryTypeIndex = i;
      break;
    }
  }
  if (memoryTypeIndex == -1) {
    Abort("failed to find suitable memory type!");
  }

  vk::MemoryAllocateInfo allocInfo(memRequirements.size, memoryTypeIndex);

  vk::UniqueDeviceMemory bufferMemory =
      CheckVulkanResVal(device.allocateMemoryUnique(allocInfo), "Failed to allocate unique memory");

  CheckVulkanRes(device.bindBufferMemory(*buffer, *bufferMemory, 0),
                 "Failed to bind buffer memory");

  return {std::move(buffer), std::move(bufferMemory)};
}

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

Shader2DLayer::Shader2DLayer(Scene& scene, VulkanContext& vulkanCtx, VulkanWindow& window,
                             const std::vector<std::string>& texturePaths) :
  Layer(scene), mVulkanCtx(vulkanCtx), mWindow(window), mTexturePaths(texturePaths),
  mClearColor({0.0f, 0.0f, 0.0f, 1.0f}) {
}


void Shader2DLayer::OnAttach() {
  auto& device = mVulkanCtx.GetVulkanDevice();
  auto& physicalDevice = mVulkanCtx.GetPhysicalDevice();
  auto commandPool = mWindow.GetCommandPool();
  auto queue = mVulkanCtx.GetGraphicsQueue();

  CreateCompatibleRenderPass();
  CreateDescriptorSetLayout();
  CreateGraphicsPipeline();

  // Load texture (using the first path for now)
  std::string texturePath =
      mTexturePaths.empty() ? "assets/textures/example.jpg" : mTexturePaths[0];
  auto textureResult = CreateTexture(device.get(), physicalDevice, queue, commandPool, texturePath);
  if (textureResult) {
    mTexture = std::move(textureResult.value());
  } else {
    Abort(textureResult.error());
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
  auto [stagingBuffer, stagingBufferMemory] = CreateBuffer2(
      device.get(), physicalDevice, bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data = CheckVulkanResVal(device->mapMemory(*stagingBufferMemory, 0, bufferSize),
                                 "Failed to map memory: ");
  memcpy(data, vertices.data(), static_cast<usize>(bufferSize));
  device->unmapMemory(*stagingBufferMemory);

  // Vertex buffer
  auto [vb, vbMem] =
      CreateBuffer2(device.get(), physicalDevice, bufferSize,
                    vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);
  mVertexBuffer = std::move(vb);
  mVertexBufferMemory = std::move(vbMem);

  auto res =
      SingleTimeCommand(device.get(), queue, commandPool, [&](const vk::CommandBuffer& cmdBuffer) {
        vk::BufferCopy copyRegion(0, 0, bufferSize);
        cmdBuffer.copyBuffer(*stagingBuffer, *mVertexBuffer, copyRegion);
      });

  if (!res) {
    Abort(res.error());
  }

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize vpBufferSize = sizeof(ViewProjection);
    auto [ub, ubMem] = CreateBuffer2(
        device.get(), physicalDevice, vpBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    mUniformBuffers.push_back(std::move(ub));
    mUniformBuffersMemory.push_back(std::move(ubMem));
    mUniformBuffersMapped.push_back(
        CheckVulkanResVal(device->mapMemory(*mUniformBuffersMemory.back(), 0, vpBufferSize),
                          "Failed to map memory: "));
  }

  vk::DescriptorPoolSize poolSizes[] = {
      {vk::DescriptorType::eUniformBuffer, static_cast<u32>(MAX_FRAMES_IN_FLIGHT)},
      { vk::DescriptorType::eSampledImage, static_cast<u32>(MAX_FRAMES_IN_FLIGHT)},
      {      vk::DescriptorType::eSampler, static_cast<u32>(MAX_FRAMES_IN_FLIGHT)}
  };

  vk::DescriptorPoolCreateInfo poolInfo({}, static_cast<u32>(MAX_FRAMES_IN_FLIGHT), 3, poolSizes);
  mDescriptorPool = CheckVulkanResVal(device->createDescriptorPoolUnique(poolInfo),
                                      "Failed to create unique descriptor pool");

  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *mDescriptorSetLayout);
  vk::DescriptorSetAllocateInfo allocInfo(*mDescriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data());
  mDescriptorSets = CheckVulkanResVal(device->allocateDescriptorSets(allocInfo),
                                      "Failed to allocate descriptor sets: ");

  vk::SamplerCreateInfo samplerCreateInfo(
      {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear,
      vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat,
      vk::SamplerAddressMode::eRepeat, 0.0f, VK_FALSE, 0.0f, VK_FALSE, vk::CompareOp::eAlways, 0.0f,
      0.0f, vk::BorderColor::eIntOpaqueBlack, VK_FALSE);
  mSampler = CheckVulkanResVal(device->createSamplerUnique(samplerCreateInfo),
                               "Failed to create unique sampler");

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
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
}
void Shader2DLayer::OnDetach() {
  auto& device = mVulkanCtx.GetVulkanDevice();
  CheckVulkanRes(device->waitIdle(), "Failed to wait for device to idle");
}
void Shader2DLayer::UpdateUniformBuffer(u32 currentImage) const {
  ViewProjection vp{};
  std::ranges::fill(vp.proj, 0.0f);
  std::ranges::fill(vp.model, 0.0f);
  std::ranges::fill(vp.view, 0.0f);

  const auto& extent = mWindow.GetSwapchainExtent();
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
void Shader2DLayer::OnRender(vk::CommandBuffer cmd) {
  // Hack: calculate frame index
  static u32 frameIndex = 0;
  frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  UpdateUniformBuffer(frameIndex);


  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline);
  vk::DeviceSize offsets[] = {0};
  cmd.bindVertexBuffers(0, 1, &*mVertexBuffer, offsets);
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mPipelineLayout, 0,
                         mDescriptorSets[frameIndex], nullptr);
  cmd.draw(6, 1, 0, 0);
}
void Shader2DLayer::OnEvent(Event& e) {
  switch (e.GetEventType()) {
    case EventType::KeyReleased: {
      if (const auto key = dynamic_cast<KeyReleasedEvent&>(e); GLFW_KEY_SPACE == key.key) {
        mClearColor = std::array{0.0f, 0.0f, 0.0f, 1.0f};
        e.isHandled = true;
      }
    } break;
    case EventType::MouseReleased: {
      if (const auto button = dynamic_cast<MouseReleasedEvent&>(e);
          GLFW_MOUSE_BUTTON_LEFT == button.button) {
        mClearColor = std::array{0.0f, 0.0f, 0.0f, 1.0f};
        e.isHandled = true;
      }
    } break;
    case EventType::MousePressed: {
      if (const auto button = dynamic_cast<MousePressedEvent&>(e);
          GLFW_MOUSE_BUTTON_LEFT == button.button) {
        mClearColor = std::array{0.0f, 1.0f, 0.0f, 1.0f};
        e.isHandled = true;
      }
    } break;
    case EventType::KeyPressed: {
      if (const auto key = dynamic_cast<KeyPressedEvent&>(e);
          GLFW_KEY_SPACE == key.key && false == key.repeat) {
        mClearColor = std::array{1.0f, 0.0f, 0.0f, 1.0f};
        e.isHandled = true;
      } else if (key.key == GLFW_KEY_RIGHT && !key.repeat) {
        mCurrentShaderIndex = (mCurrentShaderIndex + 1) % mShaderPaths.size();
        CreateGraphicsPipeline();
        e.isHandled = true;
      } else if (key.key == GLFW_KEY_LEFT && !key.repeat) {
        mCurrentShaderIndex =
            (mCurrentShaderIndex == 0) ? mShaderPaths.size() - 1 : mCurrentShaderIndex - 1;
        CreateGraphicsPipeline();
        e.isHandled = true;
      }
    } break;
    default:
      break;
  }
}
void Shader2DLayer::OnSwapchainRecreated() {
  CheckVulkanRes(mVulkanCtx.GetVulkanDevice()->waitIdle(), "Failed to wait for vulkan device");
  CreateCompatibleRenderPass();
  CreateGraphicsPipeline();
}
void Shader2DLayer::CreateCompatibleRenderPass() {
  vk::AttachmentDescription colorAttachment(
      {}, mVulkanCtx.GetSurfaceFormat().format, vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR);

  vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

  vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef);

  vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass);
  mCompatibleRenderPass =
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
void Shader2DLayer::CreateGraphicsPipeline() {
  auto& device = mVulkanCtx.GetVulkanDevice();

  ShaderCompiler compiler;

  // compile shaders
  // TODO: Use a pipeline cache to speed up pipeline creation
  std::vector<char> vertexSpv;
  if (!compiler.CompileShader("assets/shaders/vertex.hlsl", vertexSpv, "vs_6_0"))
    Abort("Failed to compile vertex shader");

  std::vector<char> pixelSpv;
  if (!compiler.CompileShader(mShaderPaths[mCurrentShaderIndex], pixelSpv, "ps_6_0"))
    Abort("Failed to compile pixel shader");


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

  // Dynamic viewport/scissor
  vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);
  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

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

  vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &vertexInputInfo, &inputAssembly,
                                              nullptr, &viewportState, &rasterizer, &multisampling,
                                              nullptr, &colorBlending, &dynamicState,
                                              *mPipelineLayout, *mCompatibleRenderPass, 0);

  auto result = device->createGraphicsPipelineUnique(nullptr, pipelineInfo);
  if (result.result != vk::Result::eSuccess) {
    // TODO: Figure out how we do errors and wrap our own errors
    Abort("Failed to create graphics pipeline");
  }
  mPipeline = std::move(result.value);
}
