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
struct LaissezVibSegment : TieSegment {
private:
    OBJECT_ALLOCATOR(engraving, LaissezVibSegment)
    DECLARE_CLASSOF(ElementType::LAISSEZ_VIB_SEGMENT)

public:
    LaissezVibSegment(LaissezVib *sp);
    LaissezVibSegment(const LaissezVibSegment &s);

    LaissezVibSegment *clone() const override { return new LaissezVibSegment(*this); }

    LaissezVib *laissez_vib() const { return (LaissezVib *) spanner(); }

    struct LayoutData : public TieSegment::LayoutData {
        SymId symbol = SymId::NO_SYM;
        ld_field<PointF> pos_relative_to_note = {"[LaissezVibSegment] posRelativeToNote", PointF()};
    };
    DECLARE_LAYOUTDATA_METHODS(LaissezVibSegment)
private:
    String format_bars_and_beats() const override;
};

struct LaissezVib : Tie {
private:
    OBJECT_ALLOCATOR(engraving, LaissezVib)
    DECLARE_CLASSOF(ElementType::LAISSEZ_VIB);

public:
    LaissezVib(DummyParentOr<Note> parent);

    LaissezVib *clone() const override { return new LaissezVib(*this); }

    PropertyValue get_property(Pid property_id) const override;
    PropertyValue property_default(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;

    Spatium min_length() const { return min_length_; }

    void set_min_length(Spatium val) { min_length_ = val; }

    SymId sym_id() const;

    SlurTieSegment *new_slur_tie_segment() override;

    void set_end_note(Note *note) override;
    void set_end_element(EngravingItem *) override;

    LaissezVibSegment *front_segment() { return to_laissez_vib_segment(Spanner::front_segment()); }

    const LaissezVibSegment *front_segment() const { return to_laissez_vib_segment(Spanner::front_segment()); }

    LaissezVibSegment *back_segment() { return to_laissez_vib_segment(Spanner::back_segment()); }

    const LaissezVibSegment *back_segment() const { return to_laissez_vib_segment(Spanner::back_segment()); }

    LaissezVibSegment *segment_at(int n) { return to_laissez_vib_segment(Spanner::segment_at(n)); }

    const LaissezVibSegment *segment_at(int n) const { return to_laissez_vib_segment(Spanner::segment_at(n)); }
private:
    Spatium min_length_ = 2.0_sp;
};
} // namespace mu::engraving
