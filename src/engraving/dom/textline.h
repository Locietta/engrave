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

#ifndef MU_ENGRAVING_TEXTLINE_H
#define MU_ENGRAVING_TEXTLINE_H

#include "textlinebase.h"

namespace mu::engraving {
class Note;

//---------------------------------------------------------
//   @@ TextLineSegment
//---------------------------------------------------------

class TextLineSegment final : public TextLineBaseSegment {
    OBJECT_ALLOCATOR(engraving, TextLineSegment)
    DECLARE_CLASSOF(ElementType::TEXTLINE_SEGMENT)

public:
    TextLineSegment(TextLine *sp, bool system = false);

    TextLineSegment *clone() const override { return new TextLineSegment(*this); }

    void init_style();

    virtual EngravingObject *property_delegate(Pid) const override;

    TextLine *text_line() const { return to_text_line(spanner()); }
};

//---------------------------------------------------------
//   @@ TextLine
//---------------------------------------------------------

class TextLine final : public TextLineBase {
    OBJECT_ALLOCATOR(engraving, TextLine)
    DECLARE_CLASSOF(ElementType::TEXTLINE)

public:
    TextLine(DummyParentOr<EngravingItem> parent, bool system = false);
    TextLine(const TextLine &);

    ~TextLine() {}

    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;

    TextLine *clone() const override { return new TextLine(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    void init_style();

    LineSegment *create_line_segment() override;

    bool allow_time_anchor() const override;

    PropertyValue property_default(Pid) const override;
    bool set_property(Pid id, const PropertyValue &) override;
    PropertyValue get_property(Pid id) const override;

protected:
    Sid default_pos_sid() const override;
};
} // namespace mu::engraving
#endif
