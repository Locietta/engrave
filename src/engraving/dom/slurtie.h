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

#include "draw/types/painterpath.h"
#include "spanner.h"

namespace mu::engraving {
//---------------------------------------------------------
//   SlurPos
//---------------------------------------------------------

struct SlurTiePos {
    PointF p1;                 // start point of slur in system coordinates
    System *system1 = nullptr; // start system of slur
    PointF p2;                 // end point of slur in system coordinates
    System *system2 = nullptr; // end system of slur
};

struct SlurOffsets {
    PointF o[4];
};

//---------------------------------------------------------
//   UP
//---------------------------------------------------------

struct UP {
    PointF p;   // layout position relative to pos()
    PointF off; // user offset in point units

    PointF pos() const { return p + off; }

    bool operator!=(const UP &up) const { return p != up.p || off != up.off; }
};

//---------------------------------------------------------
//   CubicBezier
//    Helper class to optimize cubic Bezier curve points
//    calculation.
//---------------------------------------------------------

struct CubicBezier {
private:
    PointF p1;
    PointF p2;
    PointF p3;
    PointF p4;

public:
    CubicBezier(PointF input_p1, PointF input_p2, PointF input_p3, PointF input_p4)
        : p1(input_p1), p2(input_p2), p3(input_p3), p4(input_p4) {}

    PointF point_at_percent(double t) const {
        assert(t >= 0.0 && t <= 1.0);
        const double r = 1.0 - t;
        const PointF b123 = r * (r * p1 + t * p2) + t * (r * p2 + t * p3);
        const PointF b234 = r * (r * p2 + t * p3) + t * (r * p3 + t * p4);
        return r * b123 + t * b234;
    }
};

struct SlurTie;

//---------------------------------------------------------
//   SlurTieSegment
//---------------------------------------------------------

struct SlurTieSegment : SpannerSegment {
private:
    OBJECT_ALLOCATOR(engraving, SlurTieSegment)

public:

    virtual void spatium_changed(double, double) override;

    SlurTie *slur_tie() const { return (SlurTie *) spanner(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;
    virtual EngravingObject *property_delegate(Pid) const override;
    virtual void reset() override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;
    void move(const PointF &s) override;

    int subtype() const override;
    TranslatableString subtype_user_name() const override;

    void set_slur_offset(Grip i, const PointF &val) { ups_[int(i)].off = val; }

    const UP &ups(Grip i) const { return ups_[int(i)]; }

    UP &ups(Grip i) { return ups_[int(i)]; }

    virtual bool is_user_modified() const override;

    virtual double end_width() const = 0;
    virtual double mid_width() const = 0;
    virtual double dotted_width() const = 0;

    struct LayoutData : public SpannerSegment::LayoutData {
        ld_field<muse::draw::PainterPath> path = "path";
        ld_field<double> mid_thickness = "midThickness";
    };
    DECLARE_LAYOUTDATA_METHODS(SlurTieSegment)

protected:
    SlurTieSegment(const ElementType &type, SlurTie *sp);
    SlurTieSegment(const SlurTieSegment &);

    struct UP ups_[int(Grip::GRIPS)];
};

//-------------------------------------------------------------------
//   @@ SlurTie
//   @P lineType       int  (0 - solid, 1 - dotted, 2 - dashed, 3 - wide dashed)
//   @P slurDirection  enum (Direction.AUTO, Direction.DOWN, Direction.UP)
//   @P maskSlurTie    enum (AutoOnOff.AUTO, AutoOnOff.ON, AutoOnOff.OFF)
//-------------------------------------------------------------------

struct SlurTie : Spanner {
private:
    OBJECT_ALLOCATOR(engraving, SlurTie)

public:
    SlurTie(const ElementType &type, EngravingObject *parent);
    SlurTie(const SlurTie &);
    ~SlurTie();

    bool up() const { return up_; }

    void set_up(bool val) { up_ = val; }

    virtual void reset() override;

    DirectionV slur_direction() const { return slur_direction_; }

    void set_slur_direction(DirectionV d) { slur_direction_ = d; }

    void undo_set_slur_direction(DirectionV d);

    virtual bool contains(const PointF &) const { return false; } // not selectable

    SlurStyleType style_type() const { return style_type_; }

    void set_style_type(SlurStyleType type) { style_type_ = type; }

    int subtype() const override { return static_cast<int>(style_type_) + 1; }

    TranslatableString subtype_user_name() const override;

    AutoOnOff mask_slur_tie() const { return mask_slur_tie_; }

    void set_mask_slur_tie(AutoOnOff val) { mask_slur_tie_ = val; }

    virtual SlurTieSegment *new_slur_tie_segment() = 0;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    void fixup_segments(unsigned nsegs);

    virtual double scaling_factor() const = 0;

protected:

    bool up_ = true; // actual direction

    DirectionV slur_direction_ = DirectionV::AUTO;

private:

    SlurStyleType style_type_ = SlurStyleType::UNDEFINED;
    AutoOnOff mask_slur_tie_ = AutoOnOff::AUTO;
};
} // namespace mu::engraving
