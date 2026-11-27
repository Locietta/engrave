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

#include "stafftextbase.h"

#include "log.h"
#include "score.h"
#include "segment.h"
#include "staff.h"

using namespace mu::engraving;

StaffTextBase::StaffTextBase(const ElementType &type, DummyParentOr<Segment> parent, TextStyleType tid, ElementFlags flags)
    : TextBase(type, parent, tid, flags) {
    set_swing_parameters(Constants::division / 2, 60);
}

StaffTextBase::StaffTextBase(const StaffTextBase &st) : StaffTextBase(st.type(), st) {}

StaffTextBase::StaffTextBase(ElementType type, const StaffTextBase &st)
    : TextBase(type, st), channel_actions_(st.channel_actions_), swing_parameters_(st.swing_parameters_),
      set_aeolus_stops_(st.set_aeolus_stops_), swing_(st.swing_), capo_(st.capo_) {
    for (size_t i = 0; i < 4; ++i) {
        channel_names_[i] = st.channel_names_[i];
        aeolus_stops_[i] = st.aeolus_stops_[i];
    }
}

void StaffTextBase::clear() {
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        channel_names_[voice].clear();
    }
    clear_aeolus_stops();
}

void StaffTextBase::clear_aeolus_stops() {
    for (int i = 0; i < 4; ++i) {
        aeolus_stops_[i] = 0;
    }
}

void StaffTextBase::set_aeolus_stop(int group, int idx, bool val) {
    if (val) {
        aeolus_stops_[group] |= (1 << idx);
    } else {
        aeolus_stops_[group] &= ~(1 << idx);
    }
}

void StaffTextBase::set_aeolus_stop(int group, int val) { aeolus_stops_[group] = val; }

bool StaffTextBase::get_aeolus_stop(int group, int idx) const { return aeolus_stops_[group] & (1 << idx); }

int StaffTextBase::aeolus_stop(int group) const { return aeolus_stops_[group]; }

Segment *StaffTextBase::segment() const {
    if (!ownership_parent()->is_segment()) {
        LOGD("parent %s", ownership_parent()->type_name());
        return 0;
    }
    Segment *s = to_segment(ownership_parent());
    return s;
}

void StaffTextBase::added() {
    TextBase::added();

    Score *s = score();
    if (s && swing()) {
        s->update_swing();
    }
}

void StaffTextBase::removed() {
    TextBase::removed();

    Score *s = score();
    if (s && swing()) {
        s->update_swing();
    }
}
