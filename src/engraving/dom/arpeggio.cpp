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

#include "arpeggio.h"

#include "../editing/elementeditdata.h"
#include "../types/typesconv.h"

#include "accidental.h"
#include "chord.h"
#include "note.h"
#include "page.h"
#include "part.h"
#include "property.h"
#include "score.h"
#include "segment.h"
#include "staff.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

Arpeggio::Arpeggio(DummyParentOr<Chord> parent, ElementType type) : EngravingItem(type, parent, ElementFlag::MOVABLE) {
    arpeggio_type_ = ArpeggioType::NORMAL;
    span_ = 1;
    user_len1_ = 0.0;
    user_len2_ = 0.0;
    play_arpeggio_ = true;
    stretch_ = 1.0;

    EngravingObject *p = parent;
    if (type == ElementType::ARPEGGIO && p->is_chord()) {
        to_chord(p)->set_span_arpeggio(this);
    }
}

Arpeggio::~Arpeggio() {
    // Remove reference to this arpeggio in any chords it may have spanned
    Chord *chord_value = chord();
    if (!chord_value || !chord_value->segment()) {
        return;
    }
    for (track_idx_t track_value = track(); track_value <= track() + span_; track_value++) {
        EngravingItem *e = chord_value->segment()->element(track_value);
        if (e && e->is_chord() && to_chord(e)->span_arpeggio() == this) {
            to_chord(e)->set_span_arpeggio(nullptr);
        }
    }
}

const muse::TranslatableString &Arpeggio::arpeggio_type_name() const { return TConv::user_name(arpeggio_type_); }

void Arpeggio::find_and_attach_to_chords() {
    const track_idx_t strack = track();
    const Chord *chord_value = chord();
    const Part *part = chord_value->part();
    const track_idx_t btrack = part->track_range().end_track;

    if (strack + span_ > btrack) {
        rebase_end_anchor(AnchorRebaseDirection::UP);
    }

    const track_idx_t etrack = strack + (span_ - 1);
    track_idx_t last_track = strack;
    for (track_idx_t track = strack; track <= etrack; track++) {
        EngravingItem *e = chord_value->segment()->element(track);
        if (e && e->is_chord()) {
            to_chord(e)->undo_change_span_arpeggio(this);
            last_track = track;
        }
    }

    if (last_track != etrack) {
        track_idx_t new_span = last_track - strack + 1;
        undo_change_property(Pid::ARPEGGIO_SPAN, new_span);
    }
}

void Arpeggio::detach_from_chords(track_idx_t strack, track_idx_t etrack) {
    Chord *chord_value = chord();
    if (!chord_value) {
        return;
    }
    for (track_idx_t track = strack; track <= etrack; track++) {
        EngravingItem *e = chord_value->segment()->element(track);
        if (e && e->is_chord() && to_chord(e)->span_arpeggio() == this) {
            to_chord(e)->undo_change_span_arpeggio(nullptr);
        }
    }
}

void Arpeggio::rebase_start_anchor(AnchorRebaseDirection direction) {
    if (direction == AnchorRebaseDirection::UP) {
        // Move arpeggio to chord above
        Staff *s = staff();
        Part *part = s->part();
        track_idx_t top_track = part->track_range().start_track;
        if (track() > top_track) {
            // Loop through voices til we find a chord
            for (int cur_track = static_cast<int>(track()) - 1; cur_track >= static_cast<int>(top_track); cur_track--) {
                EngravingItem *e = chord()->segment()->element(cur_track);
                if (e && e->is_chord()) {
                    track_idx_t new_span = span_ + track() - cur_track;
                    if (new_span != 0) {
                        undo_change_property(Pid::ARPEGGIO_SPAN, new_span);
                        score()->undo_change_parent(this, e, e->staff_idx());
                        break;
                    }
                }
            }
        }
    } else if (direction == AnchorRebaseDirection::DOWN) {
        // Move arpeggio to chord below
        for (track_idx_t cur_track = track() + 1; cur_track <= track() + span_ - 1; cur_track++) {
            EngravingItem *e = chord()->segment()->element(cur_track);
            if (e && e->is_chord()) {
                track_idx_t new_span = span_ + track() - cur_track;
                if (new_span != 0) {
                    chord()->undo_change_span_arpeggio(nullptr);
                    undo_change_property(Pid::ARPEGGIO_SPAN, new_span);
                    score()->undo_change_parent(this, e, e->staff_idx());
                    break;
                }
            }
        }
    }
}

void Arpeggio::rebase_end_anchor(AnchorRebaseDirection direction) {
    if (direction == AnchorRebaseDirection::UP) {
        // Move end to chord above
        for (int cur_track = static_cast<int>(track()) + span_ - 2; cur_track >= static_cast<int>(track()); cur_track--) {
            EngravingItem *e = chord()->segment()->element(cur_track);
            if (e && e->is_chord()) {
                track_idx_t new_span = cur_track - track() + 1;
                if (new_span != 0) {
                    EngravingItem *old_end_chord = chord()->segment()->element(end_track());
                    if (old_end_chord && old_end_chord->is_chord()) {
                        to_chord(old_end_chord)->undo_change_span_arpeggio(nullptr);
                    }
                    undo_change_property(Pid::ARPEGGIO_SPAN, new_span);
                }
                break;
            }
        }
    } else if (direction == AnchorRebaseDirection::DOWN) {
        // Move end to chord below
        Staff *s = staff();
        Part *part = s->part();
        track_idx_t btrack = part->track_range().end_track;
        if (track() + span_ < btrack) {
            // Loop through voices til we find a chord
            for (track_idx_t cur_track = track() + span_; cur_track < btrack; cur_track++) {
                EngravingItem *e = chord()->segment()->element(cur_track);
                if (e && e->is_chord()) {
                    track_idx_t new_span = cur_track - track() + 1;
                    to_chord(e)->undo_change_span_arpeggio(this);
                    undo_change_property(Pid::ARPEGGIO_SPAN, new_span);
                    break;
                }
            }
        }
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void Arpeggio::spatium_changed(double old_value, double new_value) {
    user_len1_ *= (new_value / old_value);
    user_len2_ *= (new_value / old_value);
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Arpeggio::accept_drop(EditData &data) const {
    return data.drop_element->type() == ElementType::ARPEGGIO || data.drop_element->type() == ElementType::CHORD_BRACKET;
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Arpeggio::drop(Transaction &, EditData &data) {
    EngravingItem *e = data.drop_element;
    switch (e->type()) {
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET: {
            Arpeggio *a = to_arpeggio(e);
            if (ownership_parent()) {
                score()->undo_remove_element(this);
            }
            a->set_track(track());
            a->set_ownership_parent(ownership_parent());
            score()->undo_add_element(a);
        }
            return e;
        default: delete e; break;
    }
    return 0;
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Arpeggio::reset() {
    undo_change_property(Pid::ARP_USER_LEN1, 0.0);
    undo_change_property(Pid::ARP_USER_LEN2, 0.0);
    EngravingItem::reset();
}

bool Arpeggio::cross_staff() const { return (track() + span() - 1) / ::mu::engraving::voices != v_staff_idx(); }

staff_idx_t Arpeggio::v_staff_idx() const { return chord() ? chord()->v_staff_idx() : EngravingItem::v_staff_idx(); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

engraving::PropertyValue Arpeggio::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::ARPEGGIO_TYPE: return int(arpeggio_type_);
        case Pid::TIME_STRETCH: return stretch();
        case Pid::ARP_USER_LEN1: return user_len1();
        case Pid::ARP_USER_LEN2: return user_len2();
        case Pid::PLAY: return play_arpeggio_;
        case Pid::ARPEGGIO_SPAN: return span_;
        default: break;
    }
    return EngravingItem::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Arpeggio::set_property(Pid property_id, const engraving::PropertyValue &val) {
    switch (property_id) {
        case Pid::ARPEGGIO_TYPE: set_arpeggio_type(ArpeggioType(val.to_int())); break;
        case Pid::TIME_STRETCH: set_stretch(val.to_double()); break;
        case Pid::ARP_USER_LEN1: set_user_len1(val.to_double()); break;
        case Pid::ARP_USER_LEN2: set_user_len2(val.to_double()); break;
        case Pid::PLAY: set_play_arpeggio(val.to_bool()); break;
        case Pid::ARPEGGIO_SPAN: set_span(val.to_int()); break;
        default:
            if (!EngravingItem::set_property(property_id, val)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue Arpeggio::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::ARP_USER_LEN1: return 0.0;
        case Pid::ARP_USER_LEN2: return 0.0;
        case Pid::TIME_STRETCH: return 1.0;
        case Pid::PLAY: return true;
        case Pid::ARPEGGIO_SPAN: return 1;
        default: break;
    }
    return EngravingItem::property_default(property_id);
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Arpeggio::subtype_user_name() const { return arpeggio_type_name(); }
