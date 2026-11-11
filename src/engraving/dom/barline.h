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

namespace mu::engraving {
struct Factory;
struct Segment;
struct Transaction;

static constexpr int min_barline_fromto_dist = 2;
static constexpr int min_barline_span_fromto = -2;

// bar line span for 1-line staves is special: goes from 2sp above the line to 2sp below the line;
static constexpr int barline_span_1_linestaff_from = -4;
static constexpr int barline_span_1_linestaff_to = 4;

// data for some preset bar line span types
static constexpr int barline_span_tic_k1_from = -1;
static constexpr int barline_span_tic_k1_to = -7;
static constexpr int barline_span_tic_k2_from = -2;
static constexpr int barline_span_tic_k2_to = -6;
static constexpr int barline_span_shor_t1_from = 2;
static constexpr int barline_span_shor_t1_to = -2;
static constexpr int barline_span_shor_t2_from = 1;
static constexpr int barline_span_shor_t2_to = -1;

//---------------------------------------------------------
//   BarLineTableItem
//---------------------------------------------------------

struct BarLineTableItem {
    BarLineType type;
    const muse::TranslatableString &user_name;
};

//---------------------------------------------------------
//   @@ BarLine
//
//   @P barLineType  enum  (BarLineType.NORMAL, .DOUBLE, .START_REPEAT, .END_REPEAT, .BROKEN, .END, .END_START_REPEAT, .DOTTED)
//---------------------------------------------------------

struct BarLine final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::BAR_LINE)

public:
    ~BarLine() override;

    BarLine &operator=(const BarLine &) = delete;

    void set_ownership_parent(DummyParentOr<Segment> parent);

    BarLine *clone() const override { return new BarLine(*this); }

    PointF canvas_pos() const override; ///< position in canvas coordinates
    PointF page_pos() const override;   ///< position in page coordinates

    void scan_elements(std::function<void(EngravingItem *)> func) override;
    void set_track(track_idx_t t) override;
    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    Segment *segment() const { return to_segment(ownership_parent()); }

    Measure *measure() const { return ownership_parent() ? to_measure(ownership_parent()->ownership_parent()) : nullptr; }

    void set_span_staff(const bool val) { span_staff_ = val; }

    void set_span_from(int val) { span_from_ = val; }

    void set_span_to(int val) { span_to_ = val; }

    void set_show_tips(bool val);

    bool span_staff() const { return span_staff_; }

    int span_from() const { return span_from_; }

    int span_to() const { return span_to_; }

    bool show_tips() const;

    const ElementList *el() const { return &el_; }

    static String translated_user_type_name(BarLineType);

    void set_bar_line_type(BarLineType i) { bar_line_type_ = i; }

    BarLineType bar_line_type() const { return bar_line_type_; }

    bool is_top() const;
    bool is_bottom() const;

    int subtype() const override { return int(bar_line_type_); }

    TranslatableString subtype_user_name() const override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;
    using EngravingObject::undo_change_property;
    EngravingObject *property_delegate(Pid) const override;

    void set_play_count(int play_count) { play_count_ = play_count; }

    int play_count() const { return play_count_; }

    PlayCountText *play_count_text() const;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    void set_selected(bool f) override;

    void style_changed() override;

    static const std::vector<BarLineTableItem> bar_line_table;

    void calc_y();

    struct LayoutData : public EngravingItem::LayoutData {
        double y1 = 0.0;
        double y2 = 0.0;
    };

    DECLARE_LAYOUTDATA_METHODS(BarLine)

private:

    friend struct Factory;
    BarLine(DummyParentOr<Segment> parent);
    BarLine(const BarLine &);

    bool span_staff_ = false; // span barline to next staff if true
    int span_from_ = 0;       // line number on start and end staves
    int span_to_ = 0;
    BarLineType bar_line_type_ = BarLineType::NORMAL;

    ElementList el_; ///< fermata or other articulations

    int play_count_ = -1; // For use during copy & paste
};
} // namespace mu::engraving
