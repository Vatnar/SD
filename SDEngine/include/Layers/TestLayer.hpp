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
class TestLayer : public Layer {
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

  TestLayer(Scene& scene, VulkanContext& vulkanCtx, VulkanWindow& window) :
    Layer(scene), mVulkanCtx(vulkanCtx), mWindow(window), mClearColor({0.0f, 0.0f, 0.0f, 1.0f}) {}


  void OnAttach() override;

  void OnDetach() override;

  void UpdateUniformBuffer(uint32_t currentImage) const;

  void OnRender(vk::CommandBuffer& cmd) override;

  void OnEvent(Event& e) override;

  void OnSwapchainRecreated() override;

private:
  void CreateCompatibleRenderPass();

  void CreateDescriptorSetLayout();

  void CreateGraphicsPipeline();


  VulkanContext& mVulkanCtx;
  VulkanWindow& mWindow;

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

  std::array<float, 4> mClearColor{};
};
