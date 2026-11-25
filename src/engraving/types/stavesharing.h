/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "global/btree.h"
#include "global/flatmap.h"
#include "types.h"

namespace mu::engraving {
// Map from origin track to shared track
using SharedTrackMap = muse::BTreeMap<track_idx_t, track_idx_t>;

struct RenderedStaveSharingState {
    SharedTrackMap shared_track_map;
    bool is_user_map_valid = false;
    bool capacity_exhausted = false;
    bool operator==(const RenderedStaveSharingState &) const = default;
};

using RenderedStaveSharingStates = muse::FlatMap<Fraction, RenderedStaveSharingState>;
} // namespace mu::engraving
