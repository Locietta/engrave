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

#include "tie.h"

namespace mu::engraving {
struct PartialTieSegment : TieSegment {
private:
    OBJECT_ALLOCATOR(engraving, PartialTieSegment)
    DECLARE_CLASSOF(ElementType::PARTIAL_TIE_SEGMENT)

public:
    PartialTieSegment(PartialTie *sp);
    PartialTieSegment(const PartialTieSegment &s);

    PartialTieSegment *clone() const override { return new PartialTieSegment(*this); }

    PartialTie *partial_tie() const { return (PartialTie *) spanner(); }
private:
    String format_bars_and_beats() const override;
};

struct PartialTie : Tie {
private:
    OBJECT_ALLOCATOR(engraving, PartialTie)
    DECLARE_CLASSOF(ElementType::PARTIAL_TIE);

    M_PROPERTY2(PartialSpannerDirection, partial_spanner_direction, set_partial_spanner_direction, PartialSpannerDirection::OUTGOING)

public:
    PartialTie(DummyParentOr<Note> parent);

    Note *parent_note() const { return parent() ? to_note(parent()) : nullptr; }

    PartialTie *clone() const override { return new PartialTie(*this); }

    inline bool is_outgoing() const { return partial_spanner_direction_value == PartialSpannerDirection::OUTGOING; }

    PropertyValue get_property(Pid property_id) const override;
    PropertyValue property_default(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;

    SlurTieSegment *new_slur_tie_segment() override;

    Note *note() const { return is_outgoing() ? start_note() : end_note(); }

    void set_start_note(Note *note) override;
    void set_end_note(Note *note) override;

    TieJumpPointList *tie_jump_points() override;
    const TieJumpPointList *tie_jump_points() const override;
    bool all_jump_points_inactive() const override;

    Note *start_note() const override;

    PartialTieSegment *front_segment() { return to_partial_tie_segment(Spanner::front_segment()); }

    const PartialTieSegment *front_segment() const { return to_partial_tie_segment(Spanner::front_segment()); }

    PartialTieSegment *back_segment() { return to_partial_tie_segment(Spanner::back_segment()); }

    const PartialTieSegment *back_segment() const { return to_partial_tie_segment(Spanner::back_segment()); }

    PartialTieSegment *segment_at(int n) { return to_partial_tie_segment(Spanner::segment_at(n)); }

    const PartialTieSegment *segment_at(int n) const { return to_partial_tie_segment(Spanner::segment_at(n)); }
};
} // namespace mu::engraving
