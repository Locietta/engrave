// SPDX-License-Identifier: GPL-3.0-only
// Adapted from partitioncloud/webmscore web/importexport/positionjsonwriter.cpp.
// The pinned engine revision is the vendor/musescore submodule (docs/dev/fork.md).
#pragma once

#include <cstddef>
#include <functional>
#include <utility>

#include <nlohmann/json.hpp>

namespace mu::engraving {
struct Score;
} // namespace mu::engraving

namespace music {
// Layout coordinates use the upstream 1200-DPI units; event times are milliseconds.
// Requires a laid-out score and execution on its owner thread. IDs are local to this result.
nlohmann::json score_positions(mu::engraving::Score *score, bool segments);

struct PositionVisitor {
    std::function<void(int, double, double, double, double, int)> element;
    std::function<void(int, int)> event;
};

// The counting pass does not build an id index. The emitting pass can reserve
// the exact count before the shared traversal begins.
std::pair<double, double> visit_score_positions(mu::engraving::Score *score, bool segments, const PositionVisitor &visitor,
                                                bool resolve_ids, std::size_t element_count = 0);
} // namespace music
