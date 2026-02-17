#pragma once
#include <VLA/Matrix.hpp>

#include "Core/Layer.hpp"
#include "Core/ShaderCompiler.hpp"
#include "Core/Vulkan/VulkanConfig.hpp"
#include "Core/Vulkan/VulkanContext.hpp"
#include "Core/Vulkan/VulkanWindow.hpp"
#include "Utils/Utils.hpp"

// TODO: REMOVE
using namespace SD;
class Shader2DLayer : public Layer {
public:
  struct ViewProjection {
    VLA::Matrix4x4f proj;
    VLA::Matrix4x4f view;
    VLA::Matrix4x4f model;
  };

  struct Vertex {
    float position[3];
    float texCoord[2];

    static vk::VertexInputBindingDescription getBindingDescription();

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
  };

  Shader2DLayer(Scene& scene, VulkanContext& vulkanCtx, VulkanWindow& window,
                const std::vector<std::string>& texturePaths);


  void OnAttach() override;

  void OnDetach() override;

  void UpdateUniformBuffer(u32 currentImage) const;
  void OnRender(vk::CommandBuffer cmd) override;

  void OnEvent(Event& e) override;

  void OnSwapchainRecreated() override;

private:
  void CreateCompatibleRenderPass();

  void CreateDescriptorSetLayout();

  void CreateGraphicsPipeline();


  VulkanContext& mVulkanCtx;
  VulkanWindow& mWindow;
  std::vector<std::string> mTexturePaths;

  vk::UniqueRenderPass mCompatibleRenderPass;

  vk::UniqueDescriptorSetLayout mDescriptorSetLayout;
  vk::UniquePipelineLayout mPipelineLayout;
  vk::UniquePipeline mPipeline;

  vk::UniqueDescriptorPool mDescriptorPool;
  std::vector<vk::DescriptorSet> mDescriptorSets;

  vk::UniqueBuffer mVertexBuffer;
  vk::UniqueDeviceMemory mVertexBufferMemory;

  std::vector<vk::UniqueBuffer> mUniformBuffers;
  std::vector<vk::UniqueDeviceMemory> mUniformBuffersMemory;
  std::vector<void*> mUniformBuffersMapped;

  Texture mTexture;
  vk::UniqueSampler mSampler;

  std::vector<std::string> mShaderPaths = {"assets/shaders/pixel.hlsl",
                                           "assets/shaders/changeTestPixel.hlsl"};
  u32 mCurrentShaderIndex = 0;

  std::array<float, 4> mClearColor{};
};
