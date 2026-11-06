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

#include "stafflines.h"

#include "measure.h"
#include "score.h"
#include "staff.h"
#include "system.h"

using namespace mu;

// Anatomy of StaffLines:
//
//    step         - The possible vertical positions of a note are counted as steps.
//                   The top staff line is step position zero.
//    lines        - number of visible staff lines
//    lineDistance - The distance between lines, measured in step units. A standard five line
//                   staff has a line distance of two steps.
//    stepDistance - The distance between steps measured in scaled spatium/2 units. A standard five
//                   line staff has a step distance of 0.5 which results in a line distance of
//                   one spatium. The spatium unit is scaled by staff size.
//    yoffset      - vertical offset to align with other staves of different height
//    stepOffset   - This value changes the staff line step numbering.
//

namespace mu::engraving {
//---------------------------------------------------------
//   StaffLines
//---------------------------------------------------------

StaffLines::StaffLines(DummyParentOr<Measure> parent) : EngravingItem(ElementType::STAFF_LINES, parent) { set_selectable(false); }

//---------------------------------------------------------
//   pagePos
//---------------------------------------------------------

PointF StaffLines::page_pos() const {
    System *system = measure()->system();
    return PointF(measure()->x() + system->x(), system->staff(staff_idx())->y() + system->y());
}

//---------------------------------------------------------
//   canvasPos
//---------------------------------------------------------

PointF StaffLines::canvas_pos() const {
    PointF p(page_pos());
    EngravingItem *e = layout_parent();
    while (e) {
        if (e->type() == ElementType::PAGE) {
            p += e->pos();
            break;
        }
        e = e->layout_parent();
    }
    return p;
}

bool StaffLines::collect_for_drawing() const {
    staff_idx_t idx = staff_idx();
    return EngravingItem::collect_for_drawing() && (measure()->visible(idx) || measure()->is_cutaway_clef(idx)) &&
           score()->staff(idx)->show();
}

//---------------------------------------------------------
//   y1
//---------------------------------------------------------

double StaffLines::y1() const {
    System *system = measure()->system();
    /*      if (system == 0 || staffIdx() >= system->staves()->size())
            return 0.0;
      */
    return system->staff(staff_idx())->y() + ldata()->pos().y();
}
} // namespace mu::engraving
