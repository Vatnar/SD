#pragma once

#include "Core/Logging.hpp"


namespace SD::Log::Game {
SD_LOG_CATEGORY_IMPL("Game", ENGINE_LOG_LEVEL_GAME)

namespace Combat {
SD_LOG_CATEGORY_IMPL("Game/Combat", ENGINE_LOG_LEVEL_GAME)
} // namespace Combat

namespace UI {
SD_LOG_CATEGORY_IMPL("Game/UI", ENGINE_LOG_LEVEL_GAME)
} // namespace UI

namespace Audio {
SD_LOG_CATEGORY_IMPL("Game/Audio", ENGINE_LOG_LEVEL_GAME)
} // namespace Audio
} // namespace SD::Log::Game
