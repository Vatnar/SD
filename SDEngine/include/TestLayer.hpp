#pragma once
#include "Layer.hpp"
#include "Event.hpp"
#include "ShaderCompiler.hpp"
#include "Utils.hpp"
#include "VulkanContext.hpp"

#include <vulkan/vulkan.hpp>

class TestLayer : public Layer
{
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    struct ViewProjection
    {
        float viewProj[16];
    };

    struct Vertex
    {
        float position[3];
        float texCoord[2];

        static vk::VertexInputBindingDescription getBindingDescription()
        {
            vk::VertexInputBindingDescription bindingDescription{};
            bindingDescription.binding   = 0;
            bindingDescription.stride    = sizeof(Vertex);
            bindingDescription.inputRate = vk::VertexInputRate::eVertex;
            return bindingDescription;
        }

        static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
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
    };

    TestLayer(VulkanContext& vulkanCtx) : mVulkanCtx(vulkanCtx)
    {
        mClearColor = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
    }

    void OnAttach() override
    {
        auto& device         = mVulkanCtx.GetVulkanDevice();
        auto& physicalDevice = mVulkanCtx.GetPhysicalDevice();
        auto  commandPool    = mVulkanCtx.GetCommandPool();
        auto  queue          = mVulkanCtx.GetGraphicsQueue();

        CreateRenderPass();

        CreateDescriptorSetLayout();

        CreateGraphicsPipeline();

        CreateCommandBuffers();

        auto textureResult =
                CreateTexture(device.get(), physicalDevice, queue, commandPool, "assets/textures/example.jpg");
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
        memcpy(data, vertices.data(), (size_t) bufferSize);
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

        vk::DeviceSize vpBufferSize = sizeof(ViewProjection);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
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
                {vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                { vk::DescriptorType::eSampledImage, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
                {      vk::DescriptorType::eSampler, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)}
        };

        // Max sets: MAX_FRAMES_IN_FLIGHT
        // Pool sizes count should satisfy all sets.
        vk::DescriptorPoolCreateInfo poolInfo({}, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT), 3, poolSizes);
        mDescriptorPool = device->createDescriptorPoolUnique(poolInfo);

        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *mDescriptorSetLayout);
        vk::DescriptorSetAllocateInfo        allocInfo(*mDescriptorPool, MAX_FRAMES_IN_FLIGHT, layouts.data());
        mDescriptorSets = device->allocateDescriptorSets(allocInfo);

        vk::SamplerCreateInfo samplerInfo({},
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
        mSampler = device->createSamplerUnique(samplerInfo);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo(*mUniformBuffers[i], 0, sizeof(ViewProjection));
            vk::DescriptorImageInfo  textureInfo({}, *mTexture.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
            vk::DescriptorImageInfo  samplerInfo(*mSampler, {}, {});

            std::array<vk::WriteDescriptorSet, 3> descriptorWrites{};

            // Binding 0: Uniform Buffer
            descriptorWrites[0].dstSet          = mDescriptorSets[i];
            descriptorWrites[0].dstBinding      = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType  = vk::DescriptorType::eUniformBuffer;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo     = &bufferInfo;

            // Binding 1: Texture (Sampled Image)
            descriptorWrites[1].dstSet          = mDescriptorSets[i];
            descriptorWrites[1].dstBinding      = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType  = vk::DescriptorType::eSampledImage;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo      = &textureInfo;

            // Binding 2: Sampler
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

    void OnDetach() override
    {
        auto& device = mVulkanCtx.GetVulkanDevice();
        device->waitIdle();
    }

    void UpdateUniformBuffer(uint32_t currentImage)
    {
        ViewProjection vp{};
        std::fill(std::begin(vp.viewProj), std::end(vp.viewProj), 0.0f);

        const auto& extent = mVulkanCtx.GetSwapchainExtent();
        float       aspect = extent.width / (float) extent.height;
        float       scaleX = 1.0f;
        float       scaleY = 1.0f;

        if (aspect > 1.0f)
            scaleX /= aspect;
        else
            scaleY *= aspect;

        // Identity matrix with scaling for aspect ratio fix
        vp.viewProj[0]  = scaleX;
        vp.viewProj[5]  = scaleY;
        vp.viewProj[10] = 1.0f;
        vp.viewProj[15] = 1.0f;

        memcpy(mUniformBuffersMapped[currentImage], &vp, sizeof(vp));
    }

    void Render(uint32_t      imageIndex,
                vk::Semaphore waitSemaphore,
                vk::Semaphore signalSemaphore,
                vk::Fence     signalFence,
                uint32_t      currentFrame)
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

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo         submitInfo;
        submitInfo.setWaitSemaphores(waitSemaphore);
        submitInfo.setWaitDstStageMask(waitStage);
        submitInfo.setCommandBuffers(*cmdBuffer);
        submitInfo.setSignalSemaphores(signalSemaphore);

        mVulkanCtx.GetGraphicsQueue().submit(submitInfo, signalFence);
    }

    void OnRender() override {}


    void OnEvent(Event& e) override
    {
        switch (e.category())
        {
            case EventCategory::KeyReleased:
            {
                auto key = dynamic_cast<KeyReleasedEvent&>(e);
                if (GLFW_KEY_SPACE == key.key)
                {
                    mClearColor = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
                    e.Handled   = true;
                }
            }
            break;
            case EventCategory::MouseReleased:
            {
                auto button = dynamic_cast<MouseReleasedEvent&>(e);
                if (GLFW_MOUSE_BUTTON_LEFT == button.button)
                {
                    mClearColor = std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f};
                    e.Handled   = true;
                }
            }
            break;
            case EventCategory::MousePressed:
            {
                auto button = dynamic_cast<MousePressedEvent&>(e);
                if (GLFW_MOUSE_BUTTON_LEFT == button.button)
                {
                    mClearColor = std::array<float, 4>{0.0f, 1.0f, 0.0f, 1.0f};
                    e.Handled   = true;
                }
            }
            break;
            case EventCategory::KeyPressed:
            {
                auto key = dynamic_cast<KeyPressedEvent&>(e);
                if (GLFW_KEY_SPACE == key.key && false == key.repeat)
                {
                    mClearColor = std::array<float, 4>{1.0f, 0.0f, 0.0f, 1.0f};
                    e.Handled   = true;
                }
            }
            break;
            default:
                break;
        }
    }

    void OnSwapchainRecreated()
    {
        mFramebuffers.clear();
        CreateFramebuffers();
        CreateGraphicsPipeline();
    }

private:
    void CreateRenderPass()
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

    void CreateDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding(0,
                                                        vk::DescriptorType::eUniformBuffer,
                                                        1,
                                                        vk::ShaderStageFlagBits::eVertex);
        vk::DescriptorSetLayoutBinding textureLayoutBinding(1,
                                                            vk::DescriptorType::eSampledImage,
                                                            1,
                                                            vk::ShaderStageFlagBits::eFragment);
        vk::DescriptorSetLayoutBinding samplerLayoutBinding(2,
                                                            vk::DescriptorType::eSampler,
                                                            1,
                                                            vk::ShaderStageFlagBits::eFragment);

        std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {uboLayoutBinding,
                                                                  textureLayoutBinding,
                                                                  samplerLayoutBinding};
        vk::DescriptorSetLayoutCreateInfo             layoutInfo({}, bindings);

        mDescriptorSetLayout = mVulkanCtx.GetVulkanDevice()->createDescriptorSetLayoutUnique(layoutInfo);
    }

    void CreateGraphicsPipeline()
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
                              (float) mVulkanCtx.GetSwapchainExtent().width,
                              (float) mVulkanCtx.GetSwapchainExtent().height,
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

    void CreateFramebuffers()
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

    void CreateCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo(mVulkanCtx.GetCommandPool(),
                                                vk::CommandBufferLevel::ePrimary,
                                                MAX_FRAMES_IN_FLIGHT);
        mCommandBuffers = mVulkanCtx.GetVulkanDevice()->allocateCommandBuffersUnique(allocInfo);
    }

    VulkanContext& mVulkanCtx;

    vk::UniqueRenderPass               mRenderPass;
    vk::UniqueDescriptorSetLayout      mDescriptorSetLayout;
    vk::UniquePipelineLayout           mPipelineLayout;
    vk::UniquePipeline                 mPipeline;
    std::vector<vk::UniqueFramebuffer> mFramebuffers;

    vk::UniqueDescriptorPool       mDescriptorPool;
    std::vector<vk::DescriptorSet> mDescriptorSets;

    std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

    vk::UniqueBuffer       mVertexBuffer;
    vk::UniqueDeviceMemory mVertexBufferMemory;

    std::vector<vk::UniqueBuffer>       mUniformBuffers;
    std::vector<vk::UniqueDeviceMemory> mUniformBuffersMemory;
    std::vector<void *>                 mUniformBuffersMapped;

    Texture           mTexture;
    vk::UniqueSampler mSampler;

    std::array<float, 4> mClearColor;
};
