// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace mu::engraving {
struct Chord;
// Returns whether the cross-measure change requires a stem refresh.
bool normalize_cross_measure(Chord &chord, bool enabled);
} // namespace mu::engraving
