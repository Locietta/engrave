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
    StringList midiActionNames;
};

//---------------------------------------------------------
//   StaffTextBase
//---------------------------------------------------------

class StaffTextBase : public TextBase {
    OBJECT_ALLOCATOR(engraving, StaffTextBase)

public:
    StaffTextBase(const ElementType &type, DummyParentOr<Segment> parent, TextStyleType tid, ElementFlags = ElementFlag::ON_STAFF);

    void clear();

    Segment *segment() const;

    String channelName(voice_idx_t voice) const { return channelNames_[voice]; }

    void setChannelName(voice_idx_t v, const String &s) { channelNames_[v] = s; }

    void setSwingParameters(int unit, int ratio) {
        swingParameters_.swingUnit = unit;
        swingParameters_.swingRatio = ratio;
    }

    const std::vector<ChannelActions> &channelActions() const { return channelActions_; }

    std::vector<ChannelActions> &channelActions() { return channelActions_; }

    const SwingParameters &swingParameters() const { return swingParameters_; }

    void clearAeolusStops();
    void setAeolusStop(int group, int idx, bool val);
    void setAeolusStop(int group, int val);
    bool getAeolusStop(int group, int idx) const;
    int aeolusStop(int group) const;

    void setSetAeolusStops(bool val) { setAeolusStops_ = val; }

    void setSwing(bool checked) { swing_ = checked; }

    void setCapo(int fretId) { capo_ = fretId; }

    bool setAeolusStops() const { return setAeolusStops_; }

    bool swing() const { return swing_; }

    int capo() const { return capo_; }

    bool positionRelativeToNoteheadRest() const override { return true; }

    void added() override;
    void removed() override;

private:
    String channelNames_[4];
    std::vector<ChannelActions> channelActions_;
    SwingParameters swingParameters_;
    bool setAeolusStops_ = false;
    int aeolusStops_[4]{0, 0, 0, 0};
    bool swing_ = false;
    int capo_ = 0;
};
} // namespace mu::engraving
