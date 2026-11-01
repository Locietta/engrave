#pragma once

#include <engrave/error.h>
#include <engrave/output.h>

#include <string>

#include "engrave/src/runtime/noqt_runtime.h"

namespace mu::engraving { class Score; }

namespace engrave::write470 {

// Deliberately restricted adapter: unknown score structures and non-default
// 5.00-only styles refuse the export with object-specific warnings.
Result<ExportOutput> export_score(const music::EngineContext&, const music::ScoreProject&,
                                  mu::engraving::Score&, const std::string& format,
                                  const music::ExportOptions&);

} // namespace engrave::write470
