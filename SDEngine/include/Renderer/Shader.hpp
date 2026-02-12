#pragma once

#include "Core/VulkanConfig.hpp"
#include <memory>

// TODO: This is a simple shader abstraction. A more complete system would handle
// shader reflection to automatically determine descriptor set layouts and vertex input descriptions.
// It would also manage the vk::Pipeline and vk::PipelineLayout.

class Shader {
public:
    // For simplicity, this just holds the shader modules.
    // A real implementation would also cache the compiled SPIR-V.
    vk::UniqueShaderModule vertexShader;
    vk::UniqueShaderModule fragmentShader;

    // The pipeline and layout would be created and stored here.
    // vk::UniquePipelineLayout pipelineLayout;
    // vk::UniquePipeline pipeline;
};
