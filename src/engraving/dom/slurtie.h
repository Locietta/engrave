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

#include "spanner.h"

#include "draw/types/painterpath.h"

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

class CubicBezier {
    PointF p1;
    PointF p2;
    PointF p3;
    PointF p4;

public:
    CubicBezier(PointF _p1, PointF _p2, PointF _p3, PointF _p4) : p1(_p1), p2(_p2), p3(_p3), p4(_p4) {}

    PointF pointAtPercent(double t) const {
        assert(t >= 0.0 && t <= 1.0);
        const double r = 1.0 - t;
        const PointF B123 = r * (r * p1 + t * p2) + t * (r * p2 + t * p3);
        const PointF B234 = r * (r * p2 + t * p3) + t * (r * p3 + t * p4);
        return r * B123 + t * B234;
    }
};

class SlurTie;

//---------------------------------------------------------
//   SlurTieSegment
//---------------------------------------------------------

class SlurTieSegment : public SpannerSegment {
    OBJECT_ALLOCATOR(engraving, SlurTieSegment)

public:

    virtual void spatiumChanged(double, double) override;

    SlurTie *slurTie() const { return (SlurTie *) spanner(); }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;
    virtual EngravingObject *propertyDelegate(Pid) const override;
    virtual void reset() override;
    void undoChangeProperty(Pid id, const PropertyValue &, PropertyFlags ps) override;
    void move(const PointF &s) override;

    int subtype() const override;
    TranslatableString subtypeUserName() const override;

    void setSlurOffset(Grip i, const PointF &val) { ups_[int(i)].off = val; }

    const UP &ups(Grip i) const { return ups_[int(i)]; }

    UP &ups(Grip i) { return ups_[int(i)]; }

    virtual bool isUserModified() const override;

    virtual double endWidth() const = 0;
    virtual double midWidth() const = 0;
    virtual double dottedWidth() const = 0;

    struct LayoutData : public SpannerSegment::LayoutData {
        ld_field<muse::draw::PainterPath> path = "path";
        ld_field<double> midThickness = "midThickness";
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

class SlurTie : public Spanner {
    OBJECT_ALLOCATOR(engraving, SlurTie)

public:
    SlurTie(const ElementType &type, EngravingObject *parent);
    SlurTie(const SlurTie &);
    ~SlurTie();

    bool up() const { return up_; }

    void setUp(bool val) { up_ = val; }

    virtual void reset() override;

    DirectionV slurDirection() const { return slurDirection_; }

    void setSlurDirection(DirectionV d) { slurDirection_ = d; }

    void undoSetSlurDirection(DirectionV d);

    virtual bool contains(const PointF &) const { return false; } // not selectable

    SlurStyleType styleType() const { return styleType_; }

    void setStyleType(SlurStyleType type) { styleType_ = type; }

    int subtype() const override { return static_cast<int>(styleType_) + 1; }

    TranslatableString subtypeUserName() const override;

    AutoOnOff maskSlurTie() const { return maskSlurTie_; }

    void setMaskSlurTie(AutoOnOff val) { maskSlurTie_ = val; }

    virtual SlurTieSegment *newSlurTieSegment() = 0;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    void fixupSegments(unsigned nsegs);

    virtual double scalingFactor() const = 0;

protected:

    bool up_ = true; // actual direction

    DirectionV slurDirection_ = DirectionV::AUTO;

private:

    SlurStyleType styleType_ = SlurStyleType::Undefined;
    AutoOnOff maskSlurTie_ = AutoOnOff::AUTO;
};
} // namespace mu::engraving
