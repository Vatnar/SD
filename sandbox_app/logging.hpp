#pragma once

#include <SD/core/logging.hpp>

namespace sd::log::game {
SD_LOG_CATEGORY_IMPL("game", ENGINE_LOG_LEVEL_GAME)

namespace combat {
SD_LOG_CATEGORY_IMPL("game/combat", ENGINE_LOG_LEVEL_GAME)
}

namespace ui {
SD_LOG_CATEGORY_IMPL("game/ui", ENGINE_LOG_LEVEL_GAME)
}

namespace audio {
SD_LOG_CATEGORY_IMPL("game/audio", ENGINE_LOG_LEVEL_GAME)
}
} // namespace sd::log::game
