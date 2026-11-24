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

#include "engravingitem.h"
#include "score.h"

namespace mu::engraving {
struct Factory;

struct EditTimeTickAnchors {

    static TimeTickAnchor *create_time_tick_anchor(Measure *measure, Fraction rel_tick, staff_idx_t staff_idx, bool use_mm_rests = true,
                                                   bool layout_output = false);
    static void update_layout(Measure *measure);
};

struct MoveElementAnchors {
    static void move_segment(EngravingItem *element, Segment *new_seg, Fraction tick_diff);
    static void check_measure_boundaries_and_move_if_need(EngravingItem *element);

private:

    static void do_move_segment(EngravingItem *element, Segment *new_seg, Fraction tick_diff);
    static void do_move_segment(FiguredBass *element, Segment *new_seg, Fraction tick_diff);
    static void do_move_harmony_or_fret_diagram_segment(EngravingItem *element, Segment *new_seg, Fraction tick_diff);

    static void move_snapped_items(EngravingItem *element, Segment *new_seg, Fraction tick_diff);
};

struct TimeTickAnchor : EngravingItem {
    DECLARE_CLASSOF(ElementType::TIME_TICK_ANCHOR)

private:
    TimeTickAnchor(DummyParentOr<Segment> parent);
    friend struct Factory;

public:
    Segment *segment() const { return to_segment(ownership_parent_item()); }

    TimeTickAnchor *clone() const override { return new TimeTickAnchor(*this); }

    enum class DrawRegion : unsigned char { OUT_OF_RANGE, EXTENDED_REGION, MAIN_REGION };

    DrawRegion draw_region() const;

    voice_idx_t voice_idx() const;

    struct LayoutData : public EngravingItem::LayoutData {
        bool darker() const { return darker_; }

        void set_darker(bool v) { darker_ = v; }

    private:
        bool darker_ = false;
    };
    DECLARE_LAYOUTDATA_METHODS(TimeTickAnchor)
};
} // namespace mu::engraving
