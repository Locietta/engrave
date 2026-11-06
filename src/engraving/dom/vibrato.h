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

#ifndef MU_ENGRAVING_VIBRATO_H
#define MU_ENGRAVING_VIBRATO_H

#include "line.h"

namespace mu::engraving {
struct Vibrato;

//---------------------------------------------------------
//   @@ VibratoSegment
//---------------------------------------------------------

struct VibratoSegment final : LineSegment {
private:
    OBJECT_ALLOCATOR(engraving, VibratoSegment)
    DECLARE_CLASSOF(ElementType::VIBRATO_SEGMENT)

public:
    VibratoSegment(Vibrato *sp);

    VibratoSegment *clone() const override { return new VibratoSegment(*this); }

    Vibrato *vibrato() const { return to_vibrato(spanner()); }

    EngravingObject *property_delegate(Pid) const override;

    const SymIdList &symbols() const { return symbols_; }

    void set_symbols(const SymIdList &s) { symbols_ = s; }

    void symbol_line(SymId start, SymId fill);
    void symbol_line(SymId start, SymId fill, SymId end);

private:
    SymIdList symbols_;
};

//---------------------------------------------------------
//   Vibrato
//---------------------------------------------------------

struct Vibrato final : SLine {
private:
    OBJECT_ALLOCATOR(engraving, Vibrato)
    DECLARE_CLASSOF(ElementType::VIBRATO)

public:
    Vibrato(DummyParentOr<EngravingItem> parent);
    ~Vibrato();

    Vibrato *clone() const override { return new Vibrato(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;
    PointF line_pos(Grip grip, System **system) const override;

    void set_vibrato_type(VibratoType tt) { vibrato_type_ = tt; }

    VibratoType vibrato_type() const { return vibrato_type_; }

    String vibrato_type_user_name() const;

    Segment *segment() const { return (Segment *) ownership_parent(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    int subtype() const override { return int(vibrato_type_); }

    TranslatableString subtype_user_name() const override;

protected:
    Sid default_pos_sid() const override;

private:
    VibratoType vibrato_type_ = VibratoType::GUITAR_VIBRATO;
};
} // namespace mu::engraving

#endif
