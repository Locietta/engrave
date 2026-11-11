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

#ifndef MU_ENGRAVING_TRILL_H
#define MU_ENGRAVING_TRILL_H

#include "line.h"

namespace mu::engraving {
struct Trill;
struct Accidental;

//---------------------------------------------------------
//   @@ TrillSegment
//---------------------------------------------------------

struct TrillSegment final : LineSegment {
private:
    DECLARE_CLASSOF(ElementType::TRILL_SEGMENT)

public:
    TrillSegment(Trill *sp);

    Trill *trill() const { return (Trill *) spanner(); }

    TrillSegment *clone() const override { return new TrillSegment(*this); }

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    EngravingObject *property_delegate(Pid) const override;

    void remove(EngravingItem *) override;

    const SymIdList &symbols() const { return symbols_; }

    void set_symbols(const SymIdList &s) { symbols_ = s; }

    void symbol_line(SymId start, SymId fill);
    void symbol_line(SymId start, SymId fill, SymId end);

protected:

private:
    SymIdList symbols_;
};

//---------------------------------------------------------
//   @@ Trill
//   @P trillType  enum (Trill.DOWNPRALL_LINE, .PRALLPRALL_LINE, .PURE_LINE, .TRILL_LINE, .UPPRALL_LINE)
//---------------------------------------------------------

struct Trill final : SLine {
private:
    DECLARE_CLASSOF(ElementType::TRILL)

public:
    Trill(DummyParentOr<EngravingItem> parent);
    Trill(const Trill &t);

    Trill *clone() const override { return new Trill(*this); }

    EngravingItem *linked_clone() override;

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;
    void remove(EngravingItem *) override;

    void set_track(track_idx_t n) override;
    void set_score(Score *s) override;
    void compute_start_element() override;
    static PointF trill_line_pos(const SLine *line, Grip grip, System **system);
    PointF line_pos(Grip grip, System **system) const override;

    void set_trill_type(TrillType tt);

    TrillType trill_type() const { return trill_type_; }

    int subtype() const override { return int(trill_type_); }

    TranslatableString subtype_user_name() const override;

    void set_ornament_style(OrnamentStyle val) { ornament_style_ = val; }

    OrnamentStyle ornament_style() const { return ornament_style_; }

    String trill_type_user_name() const;

    Accidental *accidental() const { return accidental_; }

    void set_accidental(Accidental *a) { accidental_ = a; }

    Chord *cue_note_chord() const { return cue_note_chord_; }

    void set_cue_note_chord(Chord *c) { cue_note_chord_ = c; }

    Segment *segment() const { return (Segment *) ownership_parent(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    Ornament *ornament() const { return ornament_; }

    void set_ornament(Ornament *o) { ornament_ = o; }

    Sid default_pos_sid() const override;

protected:
    void do_compute_end_element() override;

private:
    TrillType trill_type_ = TrillType::TRILL_LINE;
    Accidental *accidental_ = nullptr;
    Chord *cue_note_chord_ = nullptr;
    OrnamentStyle ornament_style_ = OrnamentStyle::DEFAULT; // for use in ornaments such as trill
    Ornament *ornament_ = nullptr;
};
} // namespace mu::engraving

#endif
