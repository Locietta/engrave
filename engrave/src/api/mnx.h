// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "engrave/src/runtime/score_io.h"

namespace engrave {
music::ScoreProject open_mnx(music::EngineContext &context, const std::string &bytes);
} // namespace engrave
