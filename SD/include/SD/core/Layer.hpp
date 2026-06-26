#pragma once

#include "SD/core/id_types.hpp"
#include "SD/export.hpp"

namespace sd {

struct Application;
struct Scene;
struct View;

struct SD_EXPORT Layer {
  bool         is_active  = true;
  const char*  debug_name = nullptr;
  Scene*       scene      = nullptr;
  Application* app        = nullptr;
  int          stage_id   = 0;
  ViewId       view_id    = ViewId{0};
  View*        view       = nullptr;
};

template<typename T> concept IsLayer = std::is_base_of_v<Layer, T>;

struct Panel : Layer {};
struct System : Layer {};
struct RenderStage : Layer {};

} // namespace sd
