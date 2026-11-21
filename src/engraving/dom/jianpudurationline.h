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

#pragma once

#include "engravingitem.h"

namespace mu::engraving {
struct ChordRest;

// Augmentation line after a Jianpu digit or rest. Layout output only: it lives in
// ChordRest::LayoutData and is never read or written.
//    parent:     ChordRest
//    x-origin:   ChordRest
//    y-origin:   SStaff
struct JianpuDurationLine final : EngravingItem {
    DECLARE_CLASSOF(ElementType::JIANPU_DURATION_LINE)

    explicit JianpuDurationLine(DummyParentOr<ChordRest> parent);
    JianpuDurationLine &operator=(const JianpuDurationLine &) = delete;

    JianpuDurationLine *clone() const override { return new JianpuDurationLine(*this); }

    PointF page_pos() const override;
    ChordRest *chord_rest() const;
    double mag() const override;

    struct LayoutData : public EngravingItem::LayoutData {
        double line_width = 0.0;
        double len = 0.0;
    };
    DECLARE_LAYOUTDATA_METHODS(JianpuDurationLine)
};
} // namespace mu::engraving
