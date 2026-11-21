/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "jianpudurationline.h"

#include "chordrest.h"
#include "measure.h"
#include "staff.h"
#include "system.h"

namespace mu::engraving {
JianpuDurationLine::JianpuDurationLine(DummyParentOr<ChordRest> parent) : EngravingItem(ElementType::JIANPU_DURATION_LINE, parent) {
    set_selectable(false);
}

PointF JianpuDurationLine::page_pos() const {
    const ChordRest *cr = chord_rest();
    const Measure *measure = cr ? cr->measure() : nullptr;
    const System *system = measure ? measure->system() : nullptr;
    if (system && system->staves().size() > staff_idx()) return PointF(page_x(), y() + system->staff_ypage(staff_idx()));
    return EngravingItem::page_pos();
}

double JianpuDurationLine::mag() const {
    if (const ChordRest *cr = chord_rest()) return cr->mag();
    const Staff *st = staff();
    return st ? st->staff_mag(this) : 1.0;
}

ChordRest *JianpuDurationLine::chord_rest() const {
    EngravingObject *parent = ownership_parent();
    return parent && parent->is_chord_rest() ? to_chord_rest(parent) : nullptr;
}
} // namespace mu::engraving
