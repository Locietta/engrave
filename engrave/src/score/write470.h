#pragma once

#include <string>

#include "engrave/src/runtime/score_io.h"
#include <engrave/error.h>
#include <engrave/output.h>

namespace mu::engraving {
struct Score;
} // namespace mu::engraving

namespace engrave::write470 {

// Internal, one-shot fault injection for export boundary tests; thread-local.
void set_warning_identity_test_fault(ExportWarning);

// Deliberately restricted adapter: unknown score structures and non-default
// 5.00-only styles refuse the export with object-specific warnings.
Result<ExportOutput> export_score(const music::EngineContext &, const music::ScoreProject &, mu::engraving::Score &,
                                  const std::string &format, const music::ExportOptions &);

} // namespace engrave::write470
