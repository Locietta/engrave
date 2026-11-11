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

#include "slurtie.h"

#include "../rendering/iscorerenderer.h"
#include "iengravingconfiguration.h" // IWYU pragma: keep
#include "log.h"
#include "note.h"
#include "page.h"
#include "score.h"
#include "system.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   SlurTieSegment
//---------------------------------------------------------

SlurTieSegment::SlurTieSegment(const ElementType &type, SlurTie *sp) : SpannerSegment(type, sp) { set_flag(ElementFlag::ON_STAFF, true); }

SlurTieSegment::SlurTieSegment(const SlurTieSegment &b) : SpannerSegment(b) {
    for (int i = 0; i < int(Grip::GRIPS); ++i) {
        ups_[i] = b.ups_[i];
        ups_[i].p = PointF();
    }
}

//---------------------------------------------------------
//   move
//---------------------------------------------------------

void SlurTieSegment::move(const PointF &s) {
    EngravingItem::move(s);
    for (int k = 0; k < int(Grip::GRIPS); ++k) {
        ups_[k].p += s;
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void SlurTieSegment::spatium_changed(double old_value, double new_value) {
    EngravingItem::spatium_changed(old_value, new_value);
    double diff = new_value / old_value;
    for (UP &u : ups_) {
        u.off *= diff;
    }
}

//---------------------------------------------------------
//   gripsPositions
//---------------------------------------------------------

bool SlurTieSegment::is_user_modified() const {
    return SpannerSegment::is_user_modified() ||
           !(visible() && autoplace() && color() == configuration()->default_color() && offset().is_null() &&
             ups(Grip::START).off.is_null() && ups(Grip::BEZIER1).off.is_null() && ups(Grip::BEZIER2).off.is_null() &&
             ups(Grip::END).off.is_null());
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *SlurTieSegment::property_delegate(Pid pid) const {
    switch (pid) {
        case Pid::SLUR_STYLE_TYPE:
        case Pid::SLUR_DIRECTION:
        case Pid::MASK_SLURTIE: return slur_tie();
        default: return SpannerSegment::property_delegate(pid);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue SlurTieSegment::get_property(Pid property_id) const {
    if (EngravingObject *e = const_cast<SlurTieSegment *>(this)->property_delegate(property_id)) {
        return e->get_property(property_id);
    }

    switch (property_id) {
        case Pid::SLUR_UOFF1: return ups(Grip::START).off;
        case Pid::SLUR_UOFF2: return ups(Grip::BEZIER1).off;
        case Pid::SLUR_UOFF3: return ups(Grip::BEZIER2).off;
        case Pid::SLUR_UOFF4: return ups(Grip::END).off;
        default: return SpannerSegment::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool SlurTieSegment::set_property(Pid property_id, const PropertyValue &v) {
    if (EngravingObject *e = const_cast<SlurTieSegment *>(this)->property_delegate(property_id)) {
        return e->set_property(property_id, v);
    }

    switch (property_id) {
        case Pid::SLUR_UOFF1: ups(Grip::START).off = v.value<PointF>(); break;
        case Pid::SLUR_UOFF2: ups(Grip::BEZIER1).off = v.value<PointF>(); break;
        case Pid::SLUR_UOFF3: ups(Grip::BEZIER2).off = v.value<PointF>(); break;
        case Pid::SLUR_UOFF4: ups(Grip::END).off = v.value<PointF>(); break;
        default: return SpannerSegment::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue SlurTieSegment::property_default(Pid id) const {
    if (EngravingObject *e = const_cast<SlurTieSegment *>(this)->property_delegate(id)) {
        return e->property_default(id);
    }

    switch (id) {
        case Pid::SLUR_UOFF1:
        case Pid::SLUR_UOFF2:
        case Pid::SLUR_UOFF3:
        case Pid::SLUR_UOFF4: return PointF();
        default: return SpannerSegment::property_default(id);
    }
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void SlurTieSegment::reset() {
    EngravingItem::reset();
    undo_reset_property(Pid::SLUR_UOFF1);
    undo_reset_property(Pid::SLUR_UOFF2);
    undo_reset_property(Pid::SLUR_UOFF3);
    undo_reset_property(Pid::SLUR_UOFF4);
    slur_tie()->reset();
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void SlurTieSegment::undo_change_property(Pid pid, const PropertyValue &val, PropertyFlags ps) {
    if (pid == Pid::AUTOPLACE && (val.to_bool() == true && !autoplace())) {
        // Switching autoplacement on. Save user-defined
        // placement properties to undo stack.
        undo_push_property(Pid::SLUR_UOFF1);
        undo_push_property(Pid::SLUR_UOFF2);
        undo_push_property(Pid::SLUR_UOFF3);
        undo_push_property(Pid::SLUR_UOFF4);
        // other will be saved in base classes.
    }
    SpannerSegment::undo_change_property(pid, val, ps);
}

//---------------------------------------------------------
//   SlurTie
//---------------------------------------------------------

SlurTie::SlurTie(const ElementType &type, EngravingObject *parent) : Spanner(type, parent) {
    slur_direction_ = DirectionV::AUTO;
    up_ = true;
    style_type_ = SlurStyleType::SOLID;
    mask_slur_tie_ = AutoOnOff::AUTO;
}

SlurTie::SlurTie(const SlurTie &t) : Spanner(t) {
    up_ = t.up_;
    slur_direction_ = t.slur_direction_;
    style_type_ = t.style_type_;
    mask_slur_tie_ = t.mask_slur_tie_;
}

//---------------------------------------------------------
//   SlurTie
//---------------------------------------------------------

SlurTie::~SlurTie() {}

//---------------------------------------------------------
//   undoSetSlurDirection
//---------------------------------------------------------

void SlurTie::undo_set_slur_direction(DirectionV d) { undo_change_property(Pid::SLUR_DIRECTION, PropertyValue::from_value<DirectionV>(d)); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue SlurTie::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SLUR_STYLE_TYPE: return style_type();
        case Pid::SLUR_DIRECTION: return PropertyValue::from_value<DirectionV>(slur_direction());
        case Pid::MASK_SLURTIE: return mask_slur_tie();
        default: return Spanner::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool SlurTie::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SLUR_STYLE_TYPE: set_style_type(v.value<SlurStyleType>()); break;
        case Pid::SLUR_DIRECTION: set_slur_direction(v.value<DirectionV>()); break;
        case Pid::MASK_SLURTIE: set_mask_slur_tie(v.value<AutoOnOff>()); break;
        default: return Spanner::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue SlurTie::property_default(Pid id) const {
    switch (id) {
        case Pid::SLUR_STYLE_TYPE: return SlurStyleType::SOLID;
        case Pid::SLUR_DIRECTION: return PropertyValue::from_value<DirectionV>(DirectionV::AUTO);
        case Pid::MASK_SLURTIE: return AutoOnOff::AUTO;
        default: return Spanner::property_default(id);
    }
}

//---------------------------------------------------------
//   fixupSegments
//---------------------------------------------------------

void SlurTie::fixup_segments(unsigned nsegs) {
    Spanner::fixup_segments(nsegs, [this]() { return new_slur_tie_segment(); });
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void SlurTie::reset() {
    EngravingItem::reset();
    undo_reset_property(Pid::SLUR_DIRECTION);
    undo_reset_property(Pid::SLUR_STYLE_TYPE);
    undo_reset_property(Pid::MASK_SLURTIE);
}

muse::TranslatableString SlurTie::subtype_user_name() const {
    switch (style_type_) {
        case SlurStyleType::SOLID: return TranslatableString("engraving/slurstyletype", "Solid");
        case SlurStyleType::DOTTED: return TranslatableString("engraving/slurstyletype", "Dotted");
        case SlurStyleType::DASHED: return TranslatableString("engraving/slurstyletype", "Dashed");
        case SlurStyleType::WIDE_DASHED: return TranslatableString("engraving/slurstyletype", "Wide dashed");
        default: return TranslatableString("engraving/slurstyletype", "Undefined");
    }
}

int SlurTieSegment::subtype() const { return slur_tie()->subtype(); }

muse::TranslatableString SlurTieSegment::subtype_user_name() const { return slur_tie()->subtype_user_name(); }
} // namespace mu::engraving
