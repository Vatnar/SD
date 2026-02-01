#include "Core/Layer.hpp"

void Layer::OnRender() {
}

void Layer::RecordCommands(uint32_t imageIndex, uint32_t currentFrame) {
}

vk::CommandBuffer Layer::GetCommandBuffer(uint32_t currentFrame) {
  return {};
}

void Layer::OnUpdate(float dt) {
}
