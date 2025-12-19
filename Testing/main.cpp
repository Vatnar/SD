#include "nvrhi/utils.h"
#include "nvrhi/validation.h"


#include <nvrhi/vulkan.h>

const bool enableValidation = true;


const char g_VertexShader[] = "";
const char g_PixelShader[]  = "";

struct Vertex
{
    float position[3];
    float texCoord[2];
};


int main()
{

    const char *deviceExtensions[] = {
            "VK_KHR_acceleration_structure",
            "VK_KHR_deferred_host_operations",
            "VK_KHR_ray_tracing_pipeline",

    };


    // NOTE: Creating NVRHI device
    nvrhi::vulkan::DeviceDesc deviceDesc;
    deviceDesc.errorCB             = g_MessageCallback;
    deviceDesc.physicalDevice      = vulkanPhysicalDevice;
    deviceDesc.device              = vulkanDevice;
    deviceDesc.graphicsQueue       = vulkanGraphicsQueue;
    deviceDesc.graphicsQueueIndex  = vulkanGraphicsQueueIndex;
    deviceDesc.deviceExtensions    = deviceExtensions;
    deviceDesc.numDeviceExtensions = std::size(deviceExtensions);

    nvrhi::DeviceHandle nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);


    // NOTE: VALIDATION:
    if (enableValidation)
    {
        nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);
        nvrhiDevice                              = nvrhiValidationLayer;
    }

    // NOTE: Create swap chain textures
    auto textureDesc = nvrhi::TextureDesc()
                               .setDimension(nvrhi::TextureDimension::Texture2D)
                               .setFormat(nvrhi::Format::RGBA8_UNORM)
                               .setWidth(swapChainWidth)
                               .setHeight(swapChainHeight)
                               .setIsRenderTarget(true)
                               .setDebugName("Swap Chain Image");

    nvrhi::TextureHandle swapChainTexture =
            nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image,
                                                      nativeTextureOrImage,
                                                      textureDesc);

    auto framebufferDesc                 = nvrhi::FramebufferDesc().addColorAttachment(swapChainTexture);
    nvrhi::FramebufferHandle framebuffer = nvrhiDevice->createFramebuffer(framebufferDesc);


    // NOTE: Graphics Pipeline
    nvrhi::ShaderHandle vertexShader =
            nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Vertex),
                                      g_VertexShader,
                                      sizeof(g_VertexShader));

    nvrhi::VertexAttributeDesc attributes[] = {
            nvrhi::VertexAttributeDesc()
                    .setName("POSITION")
                    .setFormat(nvrhi::Format::RGB32_FLOAT)
                    .setOffset(offsetof(Vertex, position))
                    .setElementStride(sizeof(Vertex)),
            nvrhi::VertexAttributeDesc()
                    .setName("TEXCOORD")
                    .setFormat(nvrhi::Format::RG32_FLOAT)
                    .setOffset(offsetof(Vertex, texCoord))
                    .setElementStride(sizeof(Vertex)),
    };

    nvrhi::InputLayoutHandle inputLayout =
            nvrhiDevice->createInputLayout(attributes, uint32_t(std::size(attributes)), vertexShader);

    nvrhi::ShaderHandle pixelShader =
            nvrhiDevice->createShader(nvrhi::ShaderDesc().setShaderType(nvrhi::ShaderType::Pixel),
                                      g_PixelShader,
                                      sizeof(g_PixelShader));


    auto framebufferInfo = framebuffer->getFramebufferInfo();

    // NOTE: Binding layout
    auto layoutDesc =
            nvrhi::BindingLayoutDesc()
                    .setVisibility(nvrhi::ShaderType::All)
                    .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))             // texture at t0
                    .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // constants at b0

    nvrhi::BindingLayoutHandle bindingLayout = nvrhiDevice->createBindingLayout(layoutDesc);

    // NOTE: Pipeline creation
    auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
                                .setInputLayout(inputLayout)
                                .setVertexShader(vertexShader)
                                .setPixelShader(pixelShader)
                                .addBindingLayout(bindingLayout);

    nvrhi::GraphicsPipelineHandle graphicsPipline =
            nvrhiDevice->createGraphicsPipeline(pipelineDesc, framebufferInfo);

    // NOTE: Creating the resources

    // CVB, constant volatile buffer
    auto constantBufferDesc =
            nvrhi::BufferDesc()
                    .setByteSize(sizeof(float) * 16) // stores one matrix
                    .setIsConstantBuffer(true)
                    .setIsVolatile(true)
                    .setMaxVersions(16); // number of automatic versions, only necessary on VULKAN

    nvrhi::BufferHandle constantBuffer = nvrhiDevice->createBuffer(constantBufferDesc);

    // vertex buffer
    static const Vertex g_Vertices[] = {
            //  position          texCoord
            {{0.f, 0.f, 0.f}, {0.f, 0.f}},
            {{1.f, 0.f, 0.f}, {1.f, 0.f}},
            // ...
    };

    auto vertexBufferDesc = nvrhi::BufferDesc()
                                    .setByteSize(sizeof(g_Vertices))
                                    .setIsVertexBuffer(true)
                                    .enableAutomaticStateTracking(nvrhi::ResourceStates::VertexBuffer)
                                    .setDebugName("Vertex Buffer");

    nvrhi::BufferHandle vertexBuffer = nvrhiDevice->createBuffer(vertexBufferDesc);

    // texture
    // assuming texture pixel data is loaded and decoded elsewhere.
    auto textureDesc = nvrhi::TextureDesc()
                               .setDimension(nvrhi::TextureDimension::Texture2D)
                               .setWidth(textureWidth)
                               .setHeight(textureHeight)
                               .setFormat(nvrhi::Format::SRGBA8_UNORM)
                               .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource)
                               .setDebugName("Geometry Texture");

    nvrhi::TextureHandle geometryTexture = nvrhiDevice->createTexture(textureDesc);


    // NOTE: Command list
    nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();


    // NOTE: Bind resources to graphics pipline at draw time
    auto bindingSetDesc = nvrhi::BindingSetDesc()
                                  .addItem(nvrhi::BindingSetItem::Texture_SRV(0, geometryTexture))
                                  .addItem(nvrhi::BindingSetItem::ConstantBuffer(0, constantBuffer));

    nvrhi::BindingSetHandle bindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, bindingLayout);


    // NOTE: Filling resource data (at initialization)
    commandList->open();

    commandList->writeBuffer(vertexBuffer, g_Vertices, sizeof(g_Vertices));

    const void  *textureData     = ...;
    const size_t textureRowPitch = ...;
    commandList->writeTexture(geometryTexture,
                              /*arraySlide = */ 0,
                              /*miplevel = */ 0,
                              textureData,
                              textureRowPitch);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);



    // NOTE: Rendering (drawing) geometry
    nvrhi::IFramebuffer* currentFramebuffer = ...;
    commandList->open();

    nvrhi::utils::ClearColorAttachment(commandList,currentFramebuffer,0, nvrhi::Color(0.f));

    float viewProjectionMatrix[16] = {...};
    commandList->writeBuffer(constantBuffer, viewProjectionMatrix, sizeof(viewProjectionMatrix));


    // set graphics state, pipline, framebuffer, viewport, bindings
    auto graphicsState = nvrhi::GraphicsState()
    .setPipeline(graphicsPipline)
    .setFramebuffer(currentFramebuffer)
    .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(nvrhi::Viewport(windowWidth, windowHeight)))
    .addBindingSet(bindingSet)
    .addVertexBuffer(vertexBuffer);

    commandList->setGraphicsState(graphicsState);

    // draw
    auto drawArguments = nvrhi::DrawArguments()
    .setVertexCount(std::size(g_Vertices));
    commandList->draw(drawArguments);

    commandList->close();
    nvrhiDevice->executeCommandList(commandList);

    // NOTE: Presentation functions...
    // TODO: https://github.com/NVIDIA-RTX/Donut-Samples/blob/main/examples/vertex_buffer/vertex_buffer.cpp

    return 0;
}
