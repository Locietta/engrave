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

#include "draw/types/color.h"
#include "spanner.h"

namespace mu::engraving {
class SLine;
class System;

//---------------------------------------------------------
//   @@ LineSegment
///    Virtual base class for segmented lines segments
///    (OttavaSegment, HairpinSegment, TrillSegment...)
///
///    This class describes one segment of an segmented
///    line object. Line objects can span multiple staves.
///    For every staff a segment is created.
//---------------------------------------------------------

class LineSegment : public SpannerSegment {
    OBJECT_ALLOCATOR(engraving, LineSegment)
protected:

    LineSegment(const ElementType &type, SLine *sp, ElementFlags f = ElementFlag::NOTHING);

public:

    LineSegment(const LineSegment &);

    SLine *line() const { return (SLine *) spanner(); }

    virtual void spatium_changed(double, double) override;
    virtual void local_spatium_changed(double, double) override;

    friend class SLine;

    virtual EngravingObject *property_delegate(Pid) const override;

    Spatium line_width() const;

    double absolute_from_spatium(const Spatium &sp) const override;

protected:
    virtual void rebase_offsets_on_anchor_changed(Grip grip, const PointF &old_pos, System *sys);

private:
    PointF left_anchor_position(const double &system_position_y) const;
    PointF right_anchor_position(const double &system_position_y) const;

    Segment *find_segment_for_grip(Grip grip, PointF pos) const;
    static PointF delta_rebase_left(const Segment *old_seg, const Segment *new_seg);
    static PointF delta_rebase_right(const Segment *old_seg, const Segment *new_seg);
    static Fraction last_segment_end_tick(const Segment *last_seg, const Spanner *s);
    LineSegment *rebase_anchor(Grip grip, Segment *new_seg);
};

//---------------------------------------------------------
//   @@ SLine
///    virtual base class for Hairpin, Trill and TextLine
//---------------------------------------------------------

class SLine : public Spanner {
    OBJECT_ALLOCATOR(engraving, SLine)

public:
    SLine(const ElementType &type, EngravingObject *parent, ElementFlags = ElementFlag::NOTHING);
    SLine(const SLine &);

    virtual LineSegment *create_line_segment() = 0;
    void set_len(double l);

    bool diagonal() const { return diagonal_; }

    void set_diagonal(bool v) { diagonal_ = v; }

    Spatium line_width() const { return line_width_; }

    Color line_color() const { return line_color_; }

    LineType line_style() const { return line_style_; }

    void set_line_width(const Spatium &v) { line_width_ = v; }

    void set_line_color(const Color &v) { line_color_ = v; }

    void set_line_style(LineType v) { line_style_ = v; }

    double dash_line_len() const { return dash_line_len_; }

    void set_dash_line_len(double val) { dash_line_len_ = val; }

    double dash_gap_len() const { return dash_gap_len_; }

    void set_dash_gap_len(double val) { dash_gap_len_ = val; }

    LineSegment *front_segment() { return to_line_segment(Spanner::front_segment()); }

    const LineSegment *front_segment() const { return to_line_segment(Spanner::front_segment()); }

    LineSegment *back_segment() { return to_line_segment(Spanner::back_segment()); }

    const LineSegment *back_segment() const { return to_line_segment(Spanner::back_segment()); }

    LineSegment *segment_at(int n) { return to_line_segment(Spanner::segment_at(n)); }

    const LineSegment *segment_at(int n) const { return to_line_segment(Spanner::segment_at(n)); }

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    virtual PointF line_pos(Grip grip, System **system) const;

    virtual bool allow_time_anchor() const override { return true; }

    void undo_move_start(Fraction tick_diff);
    void undo_move_end(Fraction tick_diff);

    static Note *guess_final_note(Note *start_note);

private:

    friend class LineSegment;

    Spatium line_width_;
    Color line_color_;
    LineType line_style_ = LineType::SOLID;
    double dash_line_len_ = 5.0;
    double dash_gap_len_ = 5.0;
    bool diagonal_ = false;
};
} // namespace mu::engraving
