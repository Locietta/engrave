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

#include "textlinebase.h"

namespace mu::engraving {
struct NoteLine;

struct NoteLineSegment final : TextLineBaseSegment {
private:
    OBJECT_ALLOCATOR(engraving, NoteLineSegment)
    DECLARE_CLASSOF(ElementType::NOTELINE_SEGMENT)

public:
    NoteLineSegment(NoteLine *sp);

    NoteLine *note_line() const { return to_note_line(spanner()); }

    NoteLineSegment *clone() const override { return new NoteLineSegment(*this); }

    EngravingObject *property_delegate(Pid) const override;
};

struct NoteLine final : TextLineBase {
private:
    OBJECT_ALLOCATOR(engraving, NoteLine)
    DECLARE_CLASSOF(ElementType::NOTELINE)

public:
    NoteLine(EngravingObject *parent);
    NoteLine(const NoteLine &);

    ~NoteLine() {}

    NoteLine *clone() const override { return new NoteLine(*this); }

    Anchor anchor() const override { return Anchor::NOTE; }

    LineSegment *create_line_segment() override;

    PropertyValue property_default(Pid) const override;
    PropertyValue get_property(Pid) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;

    bool allow_time_anchor() const override { return false; }

    NoteLineEndPlacement line_end_placement() { return line_end_placement_; }

    void set_line_end_placement(NoteLineEndPlacement v) { line_end_placement_ = v; }

    void reset() override;

    bool enforce_min_length() { return line_end_placement_ != NoteLineEndPlacement::LEFT_EDGE; }

protected:
    Sid default_pos_sid() const override;

    bool is_in_spanner_map() const override { return false; }

private:
    NoteLineEndPlacement line_end_placement_ = NoteLineEndPlacement::OFFSET_ENDS;
};
} // namespace mu::engraving
