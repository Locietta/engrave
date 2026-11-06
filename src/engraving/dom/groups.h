/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MU_ENGRAVING_GROUPS_H
#define MU_ENGRAVING_GROUPS_H

#include <unordered_map>

#include "../types/groupnode.h"
#include "../types/types.h"
#include "durationtype.h"

namespace mu::engraving {
struct ChordRest;

//---------------------------------------------------------
//   @@ Groups
///    GroupNodes must be sorted by tick
//---------------------------------------------------------

struct Groups {
    Groups() {}

    Groups(const GroupNodes &nodes) : nodes_(nodes) {}

    const GroupNodes &nodes() const { return nodes_; }

    bool empty() const { return nodes_.empty(); }

    void add_node(const GroupNode &n) { nodes_.push_back(n); }

    BeamMode beam_mode(int tick, DurationType d) const;
    void add_stop(int pos, DurationType d, BeamMode bm);

    bool operator==(const Groups &g) const {
        if (g.nodes_.size() != nodes_.size()) {
            return false;
        }
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (!(g.nodes_.at(i) == nodes_.at(i))) {
                return false;
            }
        }
        return true;
    }

    void dump(const char *) const;

    static const Groups &endings(const Fraction &f);
    static BeamMode base_beam_mode(const ChordRest *cr, const ChordRest *prev = 0);
    static BeamMode actual_beam_mode(const ChordRest *cr, const ChordRest *prev);
    static BeamMode actual_beam_mode(const ChordRest *cr, const ChordRest *prev,
                                     const std::unordered_map<int, TDuration> *beat_subdivision);

private:
    GroupNodes nodes_;
};
} // namespace mu::engraving

#endif
