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

#ifndef MU_ENGRAVING_STAFFLINES_H
#define MU_ENGRAVING_STAFFLINES_H

#include <vector>

#include "engravingitem.h"

namespace mu::engraving {
//-------------------------------------------------------------------
//   @@ StaffLines
///    The StaffLines class is the graphic representation of a staff,
///    it draws the horizontal staff lines.
//-------------------------------------------------------------------

struct StaffLines final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, StaffLines)
    DECLARE_CLASSOF(ElementType::STAFF_LINES)

public:

    StaffLines *clone() const override { return new StaffLines(*this); }

    PointF page_pos() const override;   ///< position in page coordinates
    PointF canvas_pos() const override; ///< position in page coordinates

    const std::vector<LineF> &lines() const { return lines_; }

    void set_lines(const std::vector<LineF> &l) { lines_ = l; }

    Measure *measure() const { return (Measure *) ownership_parent(); }

    double y1() const;

    double lw() const { return lw_; }

    void set_lw(double w) { lw_ = w; }

    bool collect_for_drawing() const override;

private:
    friend struct Factory;
    StaffLines(DummyParentOr<Measure> parent);

    double lw_ = 0.0;
    std::vector<LineF> lines_;
};
} // namespace mu::engraving

#endif // MU_LIBMSCORE_STAFFLINES_H
