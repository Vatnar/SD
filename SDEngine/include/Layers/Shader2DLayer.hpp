#pragma once
#include <VLA/Matrix.hpp>

#include "Core/Events/InputEvent.hpp"
#include "Core/Layer.hpp"
#include "Core/ShaderCompiler.hpp"
#include "Core/VulkanConfig.hpp"
#include "Core/VulkanContext.hpp"
#include "Utils/Utils.hpp"

class Shader2DLayer : public Layer {
public:
  // For general 2D shader we dont need all of these
  struct ViewProjection {
    VLA::Matrix4x4f proj;
    VLA::Matrix4x4f view;
    VLA::Matrix4x4f model;
  };

  // TODO: Generalize textures, loading and vertices.
  struct Vertex {
    float position[3];
    float texCoord[2];

    static vk::VertexInputBindingDescription getBindingDescription();

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions();
  };

  explicit Shader2DLayer(VulkanContext& vulkanCtx) :
    mVulkanCtx(vulkanCtx), mClearColor({0.0f, 0.0f, 0.0f, 1.0f}) {}


  [[nodiscard]] vk::CommandBuffer GetCommandBuffer(uint32_t currentFrame) override;

  void OnAttach() override;

  void OnDetach() override;

  void UpdateUniformBuffer(uint32_t currentImage) const;

  void RecordCommands(uint32_t imageIndex, uint32_t currentFrame) override;

  void OnRender() override {}

  void OnEvent(InputEvent& e) override;

  void OnSwapchainRecreated() override;

  void OnUpdate(float dt) override;

  // shaders
  void SetVertexSource(const std::string_view& vertexSource) {
    mVertexSource = vertexSource;
    mNewShaderSource = true;
  }
  void SetPixelSource(const std::string_view& pixelSource) {
    mPixelSource = pixelSource;
    mNewShaderSource = true;
  }

private:
  void CreateRenderPass();

  void CreateDescriptorSetLayout();

  bool CreateGraphicsPipeline();

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

  // Shaders
  std::string mVertexSource = "assets/shaders/vertex.hlsl";
  std::string mPixelSource = "assets/shaders/pixel.hlsl";
  std::filesystem::file_time_type mLastWriteVertex = {};
  std::filesystem::file_time_type mLastWritePixel = {};

  bool mNewShaderSource = false;

  std::array<float, 4> mClearColor{};
};
