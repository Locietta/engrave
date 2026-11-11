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

#ifndef MU_ENGRAVING_PALM_MUTE_H
#define MU_ENGRAVING_PALM_MUTE_H

#include "chordtextlinebase.h"

namespace mu::engraving {
struct PalmMute;

//---------------------------------------------------------
//   @@ PalmMuteSegment
//---------------------------------------------------------

struct PalmMuteSegment final : TextLineBaseSegment {
private:
    DECLARE_CLASSOF(ElementType::PALM_MUTE_SEGMENT)

public:
    PalmMuteSegment(PalmMute *sp);

    PalmMuteSegment *clone() const override { return new PalmMuteSegment(*this); }

    PalmMute *palm_mute() const { return (PalmMute *) spanner(); }

    friend struct PalmMute;
};

//---------------------------------------------------------
//   @@ PalmMute
//---------------------------------------------------------

struct PalmMute final : ChordTextLineBase {
private:
    DECLARE_CLASSOF(ElementType::PALM_MUTE)

public:
    PalmMute(DummyParentOr<EngravingItem> parent);

    PalmMute *clone() const override { return new PalmMute(*this); }

    //      virtual void write(XmlWriter& xml) const override;

    LineSegment *create_line_segment() override;
    PropertyValue property_default(Pid property_id) const override;

    void set_channel();

protected:
    Sid default_pos_sid() const override;

    friend struct PalmMuteLine;
};
} // namespace mu::engraving
#endif
