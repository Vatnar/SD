#pragma once
#include <VulkanConfig.hpp>

#include "InputEvent.hpp"
#include "Layer.hpp"
#include "ShaderCompiler.hpp"
#include "Utils.hpp"
#include "VulkanContext.hpp"

class TestLayer : public Layer {
public:
  struct ViewProjection {
    float viewProj[16];
  };

  struct Vertex {
    float position[3];
    float texCoord[2];

    static vk::VertexInputBindingDescription getBindingDescription();

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
  };

  explicit TestLayer(VulkanContext& vulkanCtx) :
    mVulkanCtx(vulkanCtx), mClearColor({0.0f, 0.0f, 0.0f, 1.0f}) {}


  [[nodiscard]] vk::CommandBuffer GetCommandBuffer(uint32_t currentFrame) override;

  void OnAttach() override;

  void OnDetach() override;

  void UpdateUniformBuffer(uint32_t currentImage) const;

  void RecordCommands(uint32_t imageIndex, uint32_t currentFrame) override;

  void OnRender() override {}

  void OnEvent(InputEvent& e) override;

  void OnSwapchainRecreated() override;

private:
  void CreateRenderPass();

  void CreateDescriptorSetLayout();

  void CreateGraphicsPipeline();

  void CreateFramebuffers();

  void CreateCommandBuffers();

  VulkanContext& mVulkanCtx;

  vk::UniqueRenderPass mRenderPass;
  vk::UniqueDescriptorSetLayout mDescriptorSetLayout;
  vk::UniquePipelineLayout mPipelineLayout;
  vk::UniquePipeline mPipeline;
  std::vector<vk::UniqueFramebuffer> mFramebuffers;

  vk::UniqueDescriptorPool mDescriptorPool;
  std::vector<vk::DescriptorSet> mDescriptorSets;

  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  vk::UniqueBuffer mVertexBuffer;
  vk::UniqueDeviceMemory mVertexBufferMemory;

  std::vector<vk::UniqueBuffer> mUniformBuffers;
  std::vector<vk::UniqueDeviceMemory> mUniformBuffersMemory;
  std::vector<void*> mUniformBuffersMapped;

  Texture mTexture;
  vk::UniqueSampler mSampler;

  std::array<float, 4> mClearColor{};
};
