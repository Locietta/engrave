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

#include "keysig.h"

#include "editing/editkeysig.h"
#include "editing/transaction/transaction.h"
#include "editing/transpose.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   KeySig
//---------------------------------------------------------

KeySig::KeySig(DummyParentOr<Segment> s) : EngravingItem(ElementType::KEYSIG, s, ElementFlag::ON_STAFF) {
    show_courtesy_ = true;
    hide_naturals_ = false;
}

KeySig::KeySig(const KeySig &k) : EngravingItem(k) {
    show_courtesy_ = k.show_courtesy_;
    sig_ = k.sig_;
    hide_naturals_ = false;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double KeySig::mag() const { return staff() ? staff()->staff_mag(this) : 1.0; }

//---------------------------------------------------------
//   setKey
//---------------------------------------------------------

void KeySig::set_key(Key concert_key) {
    KeySigEvent e;
    e.set_concert_key(concert_key);
    if (staff() && !style().style_b(Sid::CONCERT_PITCH)) {
        const Part *part = staff()->origin_part(tick());
        Interval v = part->instrument(tick())->transpose();
        if (!v.is_zero()) {
            v.flip();
            Key transposed_key = Transpose::transpose_key(concert_key, v, part->prefer_sharp_flat());
            e.set_key(transposed_key);
        }
    }
    set_key_sig_event(e);
}

//---------------------------------------------------------
//   setKey
//---------------------------------------------------------

void KeySig::set_key(Key concert_key, Key transposed_key) {
    KeySigEvent e;
    e.set_concert_key(concert_key);
    e.set_key(transposed_key);
    set_key_sig_event(e);
}

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool KeySig::operator==(const KeySig &k) const { return sig_ == k.sig_; }

//---------------------------------------------------------
//   isChange
//---------------------------------------------------------

bool KeySig::is_change() const {
    if (!staff()) {
        return false;
    }
    if (!segment() || segment()->segment_type() != SegmentType::KEY_SIG) {
        return false;
    }
    const Fraction key_tick = tick();
    return staff()->current_key_tick(key_tick) == key_tick;
}

//---------------------------------------------------------
//   changeKeySigEvent
//---------------------------------------------------------

void KeySig::change_key_sig_event(const KeySigEvent &t) {
    if (sig_ == t) {
        return;
    }
    set_key_sig_event(t);
}

PointF KeySig::staff_offset() const {
    const Segment *seg = segment();
    const Measure *meas = seg ? seg->measure() : nullptr;
    if (meas && meas->end_tick() == tick()) {
        // Courtesy key sig should be adjusted by the following staffType's offset
        return EngravingItem::staff_offset();
    }
    return PointF(0.0, 0.0);
}

EngravingObject *KeySig::property_delegate(Pid property_id) const {
    if (!is_courtesy_value) {
        return nullptr;
    }
    switch (property_id) {
        case Pid::KEY:
        case Pid::KEY_CONCERT:
        case Pid::SHOW_COURTESY:
        case Pid::KEYSIG_MODE: {
            Segment *this_seg = segment();
            Segment *next_ks_seg = this_seg ? this_seg->next1(SegmentType::KEY_SIG) : nullptr;
            if (next_ks_seg && next_ks_seg->tick() == this_seg->tick()) {
                return next_ks_seg->element(track());
            }
            break;
        }
        default: break;
    }

    return nullptr;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue KeySig::get_property(Pid property_id) const {
    if (EngravingObject *e = property_delegate(property_id)) {
        return e->get_property(property_id);
    }
    switch (property_id) {
        case Pid::KEY: return int(key());
        case Pid::KEY_CONCERT: return int(concert_key());
        case Pid::SHOW_COURTESY: return show_courtesy();
        case Pid::KEYSIG_MODE: return mode();
        case Pid::IS_COURTESY: return is_courtesy_value;
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool KeySig::set_property(Pid property_id, const PropertyValue &v) {
    if (EngravingObject *e = property_delegate(property_id)) {
        return e->set_property(property_id, v);
    }
    switch (property_id) {
        case Pid::KEY:
            if (generated()) {
                return false;
            }
            set_key(sig_.concert_key(), Key(v.to_int()));
            break;
        case Pid::KEY_CONCERT:
            if (generated()) {
                return false;
            }
            set_key(Key(v.to_int()));
            break;
        case Pid::SHOW_COURTESY:
            if (generated()) {
                return false;
            }
            set_show_courtesy(v.to_bool());
            break;
        case Pid::KEYSIG_MODE:
            if (generated()) {
                return false;
            }
            set_mode(v.value<KeyMode>());
            staff()->set_key(tick(), key_sig_event());
            break;
        case Pid::IS_COURTESY: is_courtesy_value = v.to_bool(); break;
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout_all();
    set_generated(false);
    return true;
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void KeySig::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (EngravingObject *e = property_delegate(id)) {
        /* If this is a courtesy, we must change the property of the _real_ key signature,
         * and we want to avoid EngravingObject::undoChangeProperty unsetting GENERATED
         * unless something really has changed about the courtesy itself: */
        e->undo_change_property(id, v, ps);
        // If removing the courtesy, deselect it first:
        if (id == Pid::SHOW_COURTESY && is_courtesy_value && !v.to_bool() && selected() && e->is_key_sig()) {
            score()->deselect(this);
            score()->select(to_engraving_item(e), SelectType::ADD, staff_idx());
        }
        return;
    }
    EngravingItem::undo_change_property(id, v, ps);
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue KeySig::property_default(Pid id) const {
    switch (id) {
        case Pid::KEY: return int(Key::INVALID);
        case Pid::KEY_CONCERT: return int(Key::INVALID);
        case Pid::SHOW_COURTESY: return true;
        case Pid::KEYSIG_MODE: return KeyMode::UNKNOWN;
        case Pid::IS_COURTESY: return false;
        default: return EngravingItem::property_default(id);
    }
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *KeySig::next_segment_element() { return segment()->first_in_next_segments(staff_idx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *KeySig::prev_segment_element() { return segment()->last_in_prev_segments(staff_idx()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   translatedSubtypeUserName
//---------------------------------------------------------

muse::TranslatableString KeySig::subtype_user_name() const { return TConv::user_name(key(), is_atonal(), is_custom()); }
} // namespace mu::engraving
