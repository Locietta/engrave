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

#pragma once

#include "textbase.h"

namespace mu::engraving {
//---------------------------------------------------------
//   ChannelActions
//---------------------------------------------------------

struct ChannelActions {
    int channel = 0;
    StringList midi_action_names;
};

//---------------------------------------------------------
//   StaffTextBase
//---------------------------------------------------------

struct StaffTextBase : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, StaffTextBase)

public:
    StaffTextBase(const ElementType &type, DummyParentOr<Segment> parent, TextStyleType tid, ElementFlags = ElementFlag::ON_STAFF);

    void clear();

    Segment *segment() const;

    String channel_name(voice_idx_t voice) const { return channel_names_[voice]; }

    void set_channel_name(voice_idx_t v, const String &s) { channel_names_[v] = s; }

    void set_swing_parameters(int unit, int ratio) {
        swing_parameters_.swing_unit = unit;
        swing_parameters_.swing_ratio = ratio;
    }

    const std::vector<ChannelActions> &channel_actions() const { return channel_actions_; }

    std::vector<ChannelActions> &channel_actions() { return channel_actions_; }

    const SwingParameters &swing_parameters() const { return swing_parameters_; }

    void clear_aeolus_stops();
    void set_aeolus_stop(int group, int idx, bool val);
    void set_aeolus_stop(int group, int val);
    bool get_aeolus_stop(int group, int idx) const;
    int aeolus_stop(int group) const;

    void set_set_aeolus_stops(bool val) { set_aeolus_stops_ = val; }

    void set_swing(bool checked) { swing_ = checked; }

    void set_capo(int fret_id) { capo_ = fret_id; }

    bool set_aeolus_stops() const { return set_aeolus_stops_; }

    bool swing() const { return swing_; }

    int capo() const { return capo_; }

    bool position_relative_to_notehead_rest() const override { return true; }

    void added() override;
    void removed() override;

private:
    String channel_names_[4];
    std::vector<ChannelActions> channel_actions_;
    SwingParameters swing_parameters_;
    bool set_aeolus_stops_ = false;
    int aeolus_stops_[4]{0, 0, 0, 0};
    bool swing_ = false;
    int capo_ = 0;
};
} // namespace mu::engraving
