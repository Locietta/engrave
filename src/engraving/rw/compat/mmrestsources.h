// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace mu::engraving {
struct Score;

namespace rw {
// Older native files store authored elements inside display MMRs. Restore source
// ownership once during loading, before timeline/repeat/tie finalization.
void restore_mm_rest_sources(Score &score);
} // namespace rw
} // namespace mu::engraving
