#include "TestLayer.hpp"
vk::VertexInputBindingDescription TestLayer::Vertex::getBindingDescription()
{
    vk::VertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;
    return bindingDescription;
}
std::array<vk::VertexInputAttributeDescription, 2> TestLayer::Vertex::getAttributeDescriptions()
{
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset   = offsetof(Vertex, position);

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = vk::Format::eR32G32Sfloat;
    attributeDescriptions[1].offset   = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}
vk::CommandBuffer TestLayer::getCommandBuffer(uint32_t currentFrame) const
{
    return *mCommandBuffers[currentFrame];
}
void TestLayer::OnAttach()
{
    auto& device         = mVulkanCtx.GetVulkanDevice();
    auto& physicalDevice = mVulkanCtx.GetPhysicalDevice();
    auto  commandPool    = mVulkanCtx.GetCommandPool();
    auto  queue          = mVulkanCtx.GetGraphicsQueue();

    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateCommandBuffers();

    auto textureResult = CreateTexture(device.get(), physicalDevice, queue, commandPool, "assets/textures/example.jpg");
    if (textureResult)
    {
        mTexture = std::move(textureResult.value());
    }
    else
    {
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
    auto [stagingBuffer, stagingBufferMemory] =
            CreateBuffer(device.get(),
                         physicalDevice,
                         bufferSize,
                         vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void *data = device->mapMemory(*stagingBufferMemory, 0, bufferSize);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    device->unmapMemory(*stagingBufferMemory);

    // Vertex buffer
    auto [vb, vbMem]    = CreateBuffer(device.get(),
                                    physicalDevice,
                                    bufferSize,
                                    vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal);
    mVertexBuffer       = std::move(vb);
    mVertexBufferMemory = std::move(vbMem);

    SingleTimeCommand(device.get(),
                      queue,
                      commandPool,
                      [&](const vk::CommandBuffer& cmdBuffer)
                      {
                          vk::BufferCopy copyRegion(0, 0, bufferSize);
                          cmdBuffer.copyBuffer(*stagingBuffer, *mVertexBuffer, copyRegion);
                      });

    for (size_t i = 0; i < mVulkanCtx.GetMaxFramesInFlight(); i++)
    {
        vk::DeviceSize vpBufferSize = sizeof(ViewProjection);
        auto [ub, ubMem] =
                CreateBuffer(device.get(),
                             physicalDevice,
                             vpBufferSize,
                             vk::BufferUsageFlagBits::eUniformBuffer,
                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        mUniformBuffers.push_back(std::move(ub));
        mUniformBuffersMemory.push_back(std::move(ubMem));
        mUniformBuffersMapped.push_back(device->mapMemory(*mUniformBuffersMemory.back(), 0, vpBufferSize));
    }

    vk::DescriptorPoolSize poolSizes[] = {
            {vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight())},
            { vk::DescriptorType::eSampledImage, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight())},
            {      vk::DescriptorType::eSampler, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight())}
    };

    vk::DescriptorPoolCreateInfo poolInfo({}, static_cast<uint32_t>(mVulkanCtx.GetMaxFramesInFlight()), 3, poolSizes);
    mDescriptorPool = device->createDescriptorPoolUnique(poolInfo);

    std::vector<vk::DescriptorSetLayout> layouts(mVulkanCtx.GetMaxFramesInFlight(), *mDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo        allocInfo(*mDescriptorPool, mVulkanCtx.GetMaxFramesInFlight(), layouts.data());
    mDescriptorSets = device->allocateDescriptorSets(allocInfo);

    vk::SamplerCreateInfo samplerCreateInfo({},
                                            vk::Filter::eLinear,
                                            vk::Filter::eLinear,
                                            vk::SamplerMipmapMode::eLinear,
                                            vk::SamplerAddressMode::eRepeat,
                                            vk::SamplerAddressMode::eRepeat,
                                            vk::SamplerAddressMode::eRepeat,
                                            0.0f,
                                            VK_FALSE,
                                            0.0f,
                                            VK_FALSE,
                                            vk::CompareOp::eAlways,
                                            0.0f,
                                            0.0f,
                                            vk::BorderColor::eIntOpaqueBlack,
                                            VK_FALSE);
    mSampler = device->createSamplerUnique(samplerCreateInfo);

    for (size_t i = 0; i < mVulkanCtx.GetMaxFramesInFlight(); i++)
    {
        vk::DescriptorBufferInfo bufferInfo(*mUniformBuffers[i], 0, sizeof(ViewProjection));
        vk::DescriptorImageInfo  textureInfo({}, *mTexture.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo  samplerInfo(*mSampler, {}, {});

        std::array<vk::WriteDescriptorSet, 3> descriptorWrites{};

        descriptorWrites[0].dstSet          = mDescriptorSets[i];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = vk::DescriptorType::eUniformBuffer;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo     = &bufferInfo;

        descriptorWrites[1].dstSet          = mDescriptorSets[i];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &textureInfo;

        descriptorWrites[2].dstSet          = mDescriptorSets[i];
        descriptorWrites[2].dstBinding      = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType  = vk::DescriptorType::eSampler;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo      = &samplerInfo;

        device->updateDescriptorSets(descriptorWrites, nullptr);
    }

    CreateFramebuffers();
}
void TestLayer::OnDetach()
{
    auto& device = mVulkanCtx.GetVulkanDevice();
    device->waitIdle();
}
void TestLayer::UpdateUniformBuffer(uint32_t currentImage) const
{
    ViewProjection vp{};
    std::ranges::fill(vp.viewProj, 0.0f);

    const auto& extent = mVulkanCtx.GetSwapchainExtent();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    float       scaleX = 1.0f;
    float       scaleY = 1.0f;

    if (aspect > 1.0f)
        scaleX /= aspect;
    else
        scaleY *= aspect;

    vp.viewProj[0]  = scaleX;
    vp.viewProj[5]  = scaleY;
    vp.viewProj[10] = 1.0f;
    vp.viewProj[15] = 1.0f;

    memcpy(mUniformBuffersMapped[currentImage], &vp, sizeof(vp));
}
void TestLayer::RecordCommands(uint32_t imageIndex, uint32_t currentFrame)
{
    UpdateUniformBuffer(currentFrame);

    auto& cmdBuffer = mCommandBuffers[currentFrame];
    cmdBuffer->reset();

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmdBuffer->begin(beginInfo);

    std::array<vk::ClearValue, 1> clearValues{};
    clearValues[0].color = vk::ClearColorValue{mClearColor};

    vk::RenderPassBeginInfo renderPassInfo(*mRenderPass,
                                           *mFramebuffers[imageIndex],
                                           {
                                                   {0, 0},
                                                   mVulkanCtx.GetSwapchainExtent()
    },
                                           clearValues);

    cmdBuffer->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline);
    vk::DeviceSize offsets[] = {0};
    cmdBuffer->bindVertexBuffers(0, 1, &*mVertexBuffer, offsets);
    cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  *mPipelineLayout,
                                  0,
                                  mDescriptorSets[currentFrame],
                                  nullptr);
    cmdBuffer->draw(6, 1, 0, 0);
    cmdBuffer->endRenderPass();
    cmdBuffer->end();
}
void TestLayer::OnEvent(InputEvent& e)
{
    switch (e.category())
    {
        case InputEventCategory::KeyReleased:
        {
            if (const auto key = dynamic_cast<KeyReleasedEvent&>(e); GLFW_KEY_SPACE == key.key)
            {
                mClearColor = std::array{0.0f, 0.0f, 0.0f, 1.0f};
                e.Handled   = true;
            }
        }
        break;
        case InputEventCategory::MouseReleased:
        {
            if (const auto button = dynamic_cast<MouseReleasedEvent&>(e); GLFW_MOUSE_BUTTON_LEFT == button.button)
            {
                mClearColor = std::array{0.0f, 0.0f, 0.0f, 1.0f};
                e.Handled   = true;
            }
        }
        break;
        case InputEventCategory::MousePressed:
        {
            if (const auto button = dynamic_cast<MousePressedEvent&>(e); GLFW_MOUSE_BUTTON_LEFT == button.button)
            {
                mClearColor = std::array{0.0f, 1.0f, 0.0f, 1.0f};
                e.Handled   = true;
            }
        }
        break;
        case InputEventCategory::KeyPressed:
        {
            if (const auto key = dynamic_cast<KeyPressedEvent&>(e); GLFW_KEY_SPACE == key.key && false == key.repeat)
            {
                mClearColor = std::array{1.0f, 0.0f, 0.0f, 1.0f};
                e.Handled   = true;
            }
        }
        break;
        default:
            break;
    }
}
void TestLayer::OnSwapchainRecreated()
{
    mVulkanCtx.GetVulkanDevice()->waitIdle();
    mFramebuffers.clear();
    CreateFramebuffers();
    CreateGraphicsPipeline();
}
void TestLayer::CreateRenderPass()
{
    vk::AttachmentDescription colorAttachment({},
                                              mVulkanCtx.GetSurfaceFormat().format,
                                              vk::SampleCountFlagBits::e1,
                                              vk::AttachmentLoadOp::eClear,
                                              vk::AttachmentStoreOp::eStore,
                                              vk::AttachmentLoadOp::eDontCare,
                                              vk::AttachmentStoreOp::eDontCare,
                                              vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, colorAttachmentRef);

    vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL,
                                     0,
                                     vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                     vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                     {},
                                     vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass, dependency);
    mRenderPass = mVulkanCtx.GetVulkanDevice()->createRenderPassUnique(renderPassInfo);
}
void TestLayer::CreateDescriptorSetLayout()
{
    constexpr vk::DescriptorSetLayoutBinding uboLayoutBinding(0,
                                                              vk::DescriptorType::eUniformBuffer,
                                                              1,
                                                              vk::ShaderStageFlagBits::eVertex);
    constexpr vk::DescriptorSetLayoutBinding textureLayoutBinding(1,
                                                                  vk::DescriptorType::eSampledImage,
                                                                  1,
                                                                  vk::ShaderStageFlagBits::eFragment);
    constexpr vk::DescriptorSetLayoutBinding samplerLayoutBinding(2,
                                                                  vk::DescriptorType::eSampler,
                                                                  1,
                                                                  vk::ShaderStageFlagBits::eFragment);

    std::array                              bindings = {uboLayoutBinding, textureLayoutBinding, samplerLayoutBinding};
    const vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);

    mDescriptorSetLayout = mVulkanCtx.GetVulkanDevice()->createDescriptorSetLayoutUnique(layoutInfo);
}
void TestLayer::CreateGraphicsPipeline()
{
    auto& device = mVulkanCtx.GetVulkanDevice();

    ShaderCompiler compiler;

    // Compile Shaders
    std::vector<char> vertexSpv;
    if (!compiler.CompileShader("assets/shaders/vertex.hlsl", vertexSpv, "vs_6_0"))
        Engine::Abort("Failed to compile vertex shader");

    std::vector<char> pixelSpv;
    if (!compiler.CompileShader("assets/shaders/pixel.hlsl", pixelSpv, "ps_6_0"))
        Engine::Abort("Failed to compile pixel shader");

    auto vertShaderModule = CreateShaderModule(device.get(), vertexSpv);
    auto fragShaderModule = CreateShaderModule(device.get(), pixelSpv);

    vk::PipelineShaderStageCreateInfo shaderStages[] = {
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"),
            vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main")};

    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo   vertexInputInfo({}, bindingDescription, attributeDescriptions);
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    vk::Viewport                        viewport(0.0f,
                          0.0f,
                          static_cast<float>(mVulkanCtx.GetSwapchainExtent().width),
                          static_cast<float>(mVulkanCtx.GetSwapchainExtent().height),
                          0.0f,
                          1.0f);
    vk::Rect2D                          scissor({0, 0}, mVulkanCtx.GetSwapchainExtent());
    vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer({},
                                                        VK_FALSE,
                                                        VK_FALSE,
                                                        vk::PolygonMode::eFill,
                                                        vk::CullModeFlagBits::eNone,
                                                        vk::FrontFace::eClockwise,
                                                        VK_FALSE,
                                                        0.0f,
                                                        0.0f,
                                                        0.0f,
                                                        1.0f);
    vk::PipelineMultisampleStateCreateInfo   multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, *mDescriptorSetLayout);
    mPipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo({},
                                                shaderStages,
                                                &vertexInputInfo,
                                                &inputAssembly,
                                                nullptr,
                                                &viewportState,
                                                &rasterizer,
                                                &multisampling,
                                                nullptr,
                                                &colorBlending,
                                                nullptr,
                                                *mPipelineLayout,
                                                *mRenderPass,
                                                0);

    auto result = device->createGraphicsPipelineUnique(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
    {
        Engine::Abort("Failed to create graphics pipeline");
    }
    mPipeline = std::move(result.value);
}
void TestLayer::CreateFramebuffers()
{
    mFramebuffers.clear();
    const auto& imageViews = mVulkanCtx.GetSwapchainImageViews();
    const auto& extent     = mVulkanCtx.GetSwapchainExtent();

    for (const auto& view : imageViews)
    {
        vk::ImageView             attachments[] = {*view};
        vk::FramebufferCreateInfo framebufferInfo({}, *mRenderPass, attachments, extent.width, extent.height, 1);
        mFramebuffers.push_back(mVulkanCtx.GetVulkanDevice()->createFramebufferUnique(framebufferInfo));
    }
}
void TestLayer::CreateCommandBuffers()
{
    vk::CommandBufferAllocateInfo allocInfo(mVulkanCtx.GetCommandPool(),
                                            vk::CommandBufferLevel::ePrimary,
                                            mVulkanCtx.GetMaxFramesInFlight());
    mCommandBuffers = mVulkanCtx.GetVulkanDevice()->allocateCommandBuffersUnique(allocInfo);
}
