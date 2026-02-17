#include "Core/Layer.hpp"

namespace SD {
void Layer::OnRender(vk::CommandBuffer& cmd) {
  spdlog::get("engine")->warn("Called OnRender on base layer");
}

void Layer::OnUpdate(float dt) {
  spdlog::get("engine")->warn("Called OnUpdate on base layer");
}
} // namespace SD
