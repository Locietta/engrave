#pragma once

#include <memory>
#include <string>

#include "engrave/src/score/export_options.h"
#include "engraving/infrastructure/mscio.h"
#include <engrave/error.h>

namespace mu::engraving {
struct EngravingProject;
} // namespace mu::engraving

namespace music {
struct EngineContext;

using ScoreProject = std::shared_ptr<mu::engraving::EngravingProject>;
engrave::Result<ScoreProject> load_native_score(const EngineContext &context, const std::string &bytes, const std::string &format,
                                                bool transaction_snapshot = false);
// Loads the .mscx of an MSCX directory (score_style.mss and META-INF/ beside
// it, the layout of the score templates) with load_native_score's post-load steps.
engrave::Result<ScoreProject> load_native_score_dir(const EngineContext &context, const std::string &mscx_path);
engrave::Result<mu::engraving::MscMemoryArchive> capture_memory_snapshot(const ScoreProject &project);
engrave::Result<ScoreProject> load_memory_snapshot(const EngineContext &context, const mu::engraving::MscMemoryArchive &archive);
engrave::Result<std::string> export_native_score(const EngineContext &context, const ScoreProject &project, const std::string &format,
                                                 int page);
engrave::Result<std::string> export_native_score(const EngineContext &context, const ScoreProject &project, const std::string &format,
                                                 const ExportOptions &options);
} // namespace music
