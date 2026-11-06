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

#include "laissezvib.h"

#include "chord.h"
#include "measure.h"
#include "note.h"
#include "score.h"
#include "staff.h"

namespace mu::engraving {
static const ElementStyle laissez_vib_style{{Sid::MIN_LAISSEZ_VIB_LENGTH, Pid::MIN_LENGTH}};

LaissezVib::LaissezVib(DummyParentOr<Note> parent) : Tie(ElementType::LAISSEZ_VIB, parent) {
    init_element_style(&::mu::engraving::laissez_vib_style);
}

PropertyValue LaissezVib::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::MIN_LENGTH: return min_length();
        default: return Tie::get_property(property_id);
    }
}

PropertyValue LaissezVib::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::MIN_LENGTH: return 2.0_sp;
        default: return Tie::property_default(property_id);
    }
}

bool LaissezVib::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::MIN_LENGTH: set_min_length(v.value<Spatium>()); break;
        default: return Tie::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

SymId LaissezVib::sym_id() const { return up() ? SymId::ARTIC_LAISSEZ_VIBRER_ABOVE : SymId::ARTIC_LAISSEZ_VIBRER_BELOW; }

SlurTieSegment *LaissezVib::new_slur_tie_segment() {
    LaissezVibSegment *seg = new LaissezVibSegment(this);
    seg->set_track(track());
    return seg;
}

void LaissezVib::set_end_note(Note *note) { set_end_element(note); }

void LaissezVib::set_end_element(EngravingItem *e) {
    UNUSED(e);
    ASSERT_X("Laissez vibrer ties do not have an end note");
}

LaissezVibSegment::LaissezVibSegment(LaissezVib *sp) : TieSegment(ElementType::LAISSEZ_VIB_SEGMENT, sp) {}

LaissezVibSegment::LaissezVibSegment(const LaissezVibSegment &s) : TieSegment(s) {}

String LaissezVibSegment::format_bars_and_beats() const {
    const Spanner *spanner = this->spanner();
    const Segment *start_seg = spanner ? spanner->start_segment() : nullptr;

    if (!start_seg) {
        return EngravingItem::format_bars_and_beats();
    }

    return start_seg->format_bars_and_beats();
}
} // namespace mu::engraving
