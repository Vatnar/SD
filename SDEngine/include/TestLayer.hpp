#pragma once
#include "Event.hpp"
#include "ShaderCompiler.hpp"
#include "Utils.hpp"
#include "nvrhi/nvrhi.h"
#include "nvrhi/utils.h"

class TestLayer : public Layer
{
public:
    struct ViewProjection
    {
        float viewProj[16];
    };

    TestLayer(nvrhi::DeviceHandle device) : mDevice(std::move(device))
    {
        mClearColor = nvrhi::Color(0.0f, 0.0f, 0.0f, 1.f);
    }

    void OnAttach() override
    {
        auto logger = spdlog::get("engine");

        // Create CommandList
        mCmdList = mDevice->createCommandList();

        // Load Texture
        mExampleTexture = CreateTexture(mDevice, "assets/textures/example.jpg");

        // ViewProjection Buffer
        nvrhi::BufferDesc constantBufferDesc;
        constantBufferDesc.byteSize         = sizeof(ViewProjection);
        constantBufferDesc.isConstantBuffer = true;
        constantBufferDesc.debugName        = "ViewProjectionConstantBuffer";
        constantBufferDesc.initialState     = nvrhi::ResourceStates::ConstantBuffer;
        constantBufferDesc.keepInitialState = true;
        constantBufferDesc.cpuAccess        = nvrhi::CpuAccessMode::None;
        mViewProjBuffer                     = mDevice->createBuffer(constantBufferDesc);

        // Vertex Buffer
        static const Vertex vertices[] = {
                {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}}, // TL
                { {0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}}, // TR
                { {-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}, // BL
                { {-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}}, // BL
                { {0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}}, // TR
                {  {0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}}  // BR
        };

        nvrhi::BufferDesc vertexBufferDesc;
        vertexBufferDesc.byteSize         = sizeof(vertices);
        vertexBufferDesc.isVertexBuffer   = true;
        vertexBufferDesc.debugName        = "VertexBuffer";
        vertexBufferDesc.initialState     = nvrhi::ResourceStates::VertexBuffer;
        vertexBufferDesc.keepInitialState = true;
        mVertexBuffer                     = mDevice->createBuffer(vertexBufferDesc);

        // Upload vertices immediately
        auto cmdList = mDevice->createCommandList();
        cmdList->open();
        cmdList->writeBuffer(mVertexBuffer, vertices, sizeof(vertices));
        cmdList->close();
        mDevice->executeCommandList(cmdList);

        // Sampler
        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
        samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
        samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
        mSampler             = mDevice->createSampler(samplerDesc);

        // Binding Layout
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(2));
        layoutDesc.bindingOffsets = nvrhi::VulkanBindingOffsets()
                                            .setShaderResourceOffset(0)
                                            .setSamplerOffset(0)
                                            .setConstantBufferOffset(0)
                                            .setUnorderedAccessViewOffset(0);

        mBindingLayout = mDevice->createBindingLayout(layoutDesc);

        // Binding Set
        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = {nvrhi::BindingSetItem::ConstantBuffer(0, mViewProjBuffer),
                                   nvrhi::BindingSetItem::Texture_SRV(1, mExampleTexture),
                                   nvrhi::BindingSetItem::Sampler(2, mSampler)};
        mBindingSet             = mDevice->createBindingSet(bindingSetDesc, mBindingLayout);

        // Shaders
        if (!initShaderCompiler())
            throw std::runtime_error("Failed to init shader compiler");

        std::vector<char> vertexSpv;
        if (!compileShader("assets/shaders/vertex.hlsl", vertexSpv, "vs_6_0"))
            throw std::runtime_error("Failed to compile vertex shader");

        std::vector<char> pixelSpv;
        if (!compileShader("assets/shaders/pixel.hlsl", pixelSpv, "ps_6_0"))
            throw std::runtime_error("Failed to compile pixel shader");

        mVertexShader = mDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex),
                                              vertexSpv.data(),
                                              vertexSpv.size());
        mPixelShader  = mDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel),
                                             pixelSpv.data(),
                                             pixelSpv.size());

        // Input Layout
        nvrhi::VertexAttributeDesc attributes[] = {nvrhi::VertexAttributeDesc()
                                                           .setName("POSITION")
                                                           .setFormat(nvrhi::Format::RGB32_FLOAT)
                                                           .setOffset(0)
                                                           .setElementStride(sizeof(Vertex)),
                                                   nvrhi::VertexAttributeDesc()
                                                           .setName("TEXCOORD")
                                                           .setFormat(nvrhi::Format::RG32_FLOAT)
                                                           .setOffset(sizeof(float) * 3)
                                                           .setElementStride(sizeof(Vertex))};

        mInputLayout = mDevice->createInputLayout(attributes, 2, mVertexShader);
    }

    // Called when swapchain is recreated or initialized
    void UpdatePipeline(const nvrhi::FramebufferInfo& fbInfo)
    {
        auto renderState  = nvrhi::RenderState{};
        auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
                                    .setRenderState(renderState)
                                    .setInputLayout(mInputLayout)
                                    .setVertexShader(mVertexShader)
                                    .setPixelShader(mPixelShader)
                                    .addBindingLayout(mBindingLayout);

        pipelineDesc.renderState.rasterState.setCullMode(nvrhi::RasterCullMode::None);
        pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
        pipelineDesc.primType                                      = nvrhi::PrimitiveType::TriangleList;

        mGraphicsPipeline = mDevice->createGraphicsPipeline(pipelineDesc, fbInfo);
    }

    void SetFramebuffer(nvrhi::FramebufferHandle fb) { mFramebuffer = fb; }

    void OnRender() override
    {
        if (!mCmdList || !mFramebuffer || !mGraphicsPipeline)
            return;

        mCmdList->open();

        // Update ViewProjection
        {
            ViewProjection vp{};
            std::fill(std::begin(vp.viewProj), std::end(vp.viewProj), 0.0f);

            const auto& fbInfo = mFramebuffer->getFramebufferInfo();
            float       aspect = static_cast<float>(fbInfo.width) / static_cast<float>(fbInfo.height);
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

            mCmdList->writeBuffer(mViewProjBuffer, &vp, sizeof(vp));
        }

        // Clear
        nvrhi::utils::ClearColorAttachment(mCmdList, mFramebuffer, 0, mClearColor);
        mCmdList->clearDepthStencilTexture(mFramebuffer->getDesc().depthAttachment.texture,
                                           nvrhi::AllSubresources,
                                           true,
                                           1.0f,
                                           false,
                                           0);

        // Draw
        auto graphicsState =
                nvrhi::GraphicsState()
                        .setPipeline(mGraphicsPipeline)
                        .setFramebuffer(mFramebuffer)
                        .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(
                                nvrhi::Viewport(static_cast<float>(mFramebuffer->getFramebufferInfo().width),
                                                static_cast<float>(mFramebuffer->getFramebufferInfo().height))))
                        .addBindingSet(mBindingSet)
                        .addVertexBuffer(nvrhi::VertexBufferBinding().setBuffer(mVertexBuffer).setSlot(0).setOffset(0));

        mCmdList->setGraphicsState(graphicsState);
        mCmdList->draw(nvrhi::DrawArguments().setVertexCount(6).setInstanceCount(1));

        mCmdList->close();
        mDevice->executeCommandList(mCmdList);
    }

    void OnEvent(Event& e) override
    {
        switch (e.category())
        {
            case EventCategory::Window:
                break;
            case EventCategory::Mouse:
                break;
            case EventCategory::Key:
                break;
            case EventCategory::KeyPressed:
            {
                auto pressed = dynamic_cast<KeyPressedEvent&>(e);
                if (pressed.key == GLFW_KEY_SPACE)
                {
                    mClearColor = nvrhi::Color(1.0f, 0.0f, 0.0f, 1.0f);
                    e.Handled   = true;
                }
            }
            break;
            case EventCategory::KeyReleased:
            {
                auto released = dynamic_cast<KeyReleasedEvent&>(e);
                if (released.key == GLFW_KEY_SPACE)
                {
                    mClearColor = nvrhi::Color(0.0f, 0.0f, 0.0f, 1.0f);
                    e.Handled   = true;
                }
            }
            break;
            case EventCategory::Engine:
                break;
        }
    }

private:
    nvrhi::DeviceHandle           mDevice;
    nvrhi::CommandListHandle      mCmdList;
    nvrhi::FramebufferHandle      mFramebuffer;
    nvrhi::TextureHandle          mExampleTexture;
    nvrhi::BufferHandle           mViewProjBuffer, mVertexBuffer;
    nvrhi::SamplerHandle          mSampler;
    nvrhi::BindingLayoutHandle    mBindingLayout;
    nvrhi::BindingSetHandle       mBindingSet;
    nvrhi::ShaderHandle           mVertexShader, mPixelShader;
    nvrhi::InputLayoutHandle      mInputLayout;
    nvrhi::GraphicsPipelineHandle mGraphicsPipeline;

    nvrhi::Color mClearColor;
};
