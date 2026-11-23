#pragma once

#include <string>

#include "engrave/src/score/export_options.h"
#include <engrave/error.h>

namespace mu::engraving {
struct Score;
} // namespace mu::engraving

namespace music {
struct EngineContext;

engrave::Result<std::string> render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format, int page);
engrave::Result<std::string> render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format,
                                          const ExportOptions &options);
} // namespace music
