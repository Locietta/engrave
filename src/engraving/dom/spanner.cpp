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
#include "spanner.h"

#include "../editing/navigation.h"
#include "anchors.h"
#include "chord.h"
#include "chordrest.h"
#include "location.h"
#include "log.h"
#include "lyrics.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "translation.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   SpannerSegment
//---------------------------------------------------------

SpannerSegment::SpannerSegment(const ElementType &type, Spanner *sp, ElementFlags f) : EngravingItem(type, sp, f) {
    set_spanner_segment_type(SpannerSegmentType::SINGLE);
}

SpannerSegment::SpannerSegment(const SpannerSegment &s) : EngravingItem(s) {
    spanner_segment_type_ = s.spanner_segment_type_;
    p2_ = s.p2_;
    offset2_ = s.offset2_;
}

SpannerSegment::~SpannerSegment() {
    // A system only places a segment, so it holds a plain back-reference to it: take
    // that reference with us, rather than relying on every deletion site to do it.
    move_to_system(nullptr);
}

void SpannerSegment::set_spanner(Spanner *val) { set_ownership_parent(val); }

void SpannerSegment::set_ownership_parent(Spanner *spanner) { EngravingItem::set_ownership_parent(spanner); }

EngravingItem *SpannerSegment::layout_parent() const { return system_; }

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double SpannerSegment::mag() const {
    if (spanner()->system_flag()) {
        return 1.0;
    }
    return staff() ? staff()->staff_mag(spanner()->tick()) : 1.0;
}

Fraction SpannerSegment::tick() const {
    const Spanner *sp = spanner();
    return sp ? sp->tick() : Fraction(0, 1);
}

//---------------------------------------------------------
//   moveToSystem
//---------------------------------------------------------

void SpannerSegment::move_to_system(System *s) {
    if (system_ == s) {
        return;
    }
    if (system_) {
        system_->remove(this);
    }
    system_ = s;
    if (s) {
        s->add(this);
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void SpannerSegment::spatium_changed(double ov, double nv) {
    EngravingItem::spatium_changed(ov, nv);
    if (offset_is_spatium_dependent()) {
        offset2_ *= (nv / ov);
    }
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *SpannerSegment::property_delegate(Pid pid) const {
    switch (pid) {
        case Pid::PLAY:
        case Pid::COLOR:
        case Pid::Z:
        case Pid::VISIBLE:
        case Pid::PLACEMENT:
        case Pid::EXCLUDE_FROM_OTHER_PARTS:
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER:
        case Pid::SPANNER_TICK:
        case Pid::SPANNER_TICKS:
        case Pid::SPANNER_TRACK2: return spanner();
        default: break;
    }

    return nullptr;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

engraving::PropertyValue SpannerSegment::get_property(Pid pid) const {
    if (EngravingObject *e = const_cast<SpannerSegment *>(this)->property_delegate(pid)) {
        return e->get_property(pid);
    }
    switch (pid) {
        case Pid::OFFSET2: return offset2_;
        default: return EngravingItem::get_property(pid);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool SpannerSegment::set_property(Pid pid, const PropertyValue &v) {
    if (EngravingObject *e = property_delegate(pid)) {
        return e->set_property(pid, v);
    }
    switch (pid) {
        case Pid::OFFSET2:
            offset2_ = v.value<PointF>();
            trigger_layout();
            break;
        default: return EngravingItem::set_property(pid, v);
    }
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue SpannerSegment::property_default(Pid pid) const {
    if (EngravingObject *e = const_cast<SpannerSegment *>(this)->property_delegate(pid)) {
        return e->property_default(pid);
    }
    switch (pid) {
        case Pid::OFFSET2: return PropertyValue();
        default: return EngravingItem::property_default(pid);
    }
}

PointF SpannerSegment::default_pos() const { return spanner() ? spanner()->default_pos() : PointF(); }

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid SpannerSegment::get_property_style(Pid pid) const {
    if (EngravingObject *e = const_cast<SpannerSegment *>(this)->property_delegate(pid)) {
        return e->get_property_style(pid);
    }
    return EngravingItem::get_property_style(pid);
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void SpannerSegment::reset_property(Pid pid) {
    if (EngravingObject *e = property_delegate(pid)) {
        return e->reset_property(pid);
    }
    return EngravingItem::reset_property(pid);
}

//---------------------------------------------------------
//   styleChanged
//---------------------------------------------------------

void SpannerSegment::style_changed() { spanner()->style_changed(); }

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void SpannerSegment::reset() {
    undo_change_property(Pid::OFFSET2, PointF());
    EngravingItem::reset();
    spanner()->reset();
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void SpannerSegment::undo_change_property(Pid pid, const PropertyValue &val, PropertyFlags ps) {
    if (pid == Pid::AUTOPLACE && (val.to_bool() == true && !autoplace())) {
        // Switching autoplacement on. Save user-defined
        // placement properties to undo stack.
        undo_push_property(Pid::OFFSET2);
        // other will be saved in EngravingItem::undoChangeProperty
    }
    EngravingItem::undo_change_property(pid, val, ps);
}

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void SpannerSegment::set_selected(bool f) {
    EngravingItem::set_selected(f);
    if (spanner()->selected() != f) {
        spanner()->set_selected(f);
    }
}

//---------------------------------------------------------
//   setVisible
//---------------------------------------------------------

void SpannerSegment::set_visible(bool f) {
    if (Spanner *sp = spanner()) {
        sp->set_visible(f);
    }
    EngravingItem::set_visible(f);
}

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void SpannerSegment::set_color(const Color &col) {
    if (Spanner *sp = spanner()) {
        sp->set_color(col);
    }
    EngravingItem::set_color(col);
}

//---------------------------------------------------------
//   setZ
//---------------------------------------------------------

void SpannerSegment::set_z(int val) {
    if (Spanner *sp = spanner()) {
        sp->set_z(val);
    }
    EngravingItem::set_z(val);
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *SpannerSegment::next_segment_element() { return spanner()->next_segment_element(); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *SpannerSegment::prev_segment_element() { return spanner()->prev_segment_element(); }

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void SpannerSegment::trigger_layout() const {
    if (Spanner *sp = spanner()) {
        sp->trigger_layout();
    }
}

std::list<EngravingObject *> SpannerSegment::link_list_for_property_propagation() const {
    std::list<EngravingObject *> result;
    result.push_back(const_cast<SpannerSegment *>(this));

    if (is_middle_type()) {
        return result;
    }

    const Spanner *sp = spanner();
    for (const EngravingObject *linked_spanner : sp->link_list()) {
        if (linked_spanner == sp || to_spanner(linked_spanner)->placement() != sp->placement()) {
            continue;
        }
        const std::vector<SpannerSegment *> &linked_segments = to_spanner(linked_spanner)->spanner_segments();
        if (linked_segments.empty()) {
            continue;
        }
        if (is_single_begin_type()) {
            result.push_back(linked_segments.front());
        } else if (is_single_end_type()) {
            result.push_back(linked_segments.back());
        }
    }

    return result;
}

bool SpannerSegment::is_property_linked_to_master(Pid id) const {
    bool linked_for_spanner_segment = EngravingItem::is_property_linked_to_master(id);
    if (!linked_for_spanner_segment) {
        return false;
    }

    // The property is linked for the spanner segment, but may be unlinked for the spanner, in which case we consider it unlinked
    return spanner()->is_property_linked_to_master(id);
}

bool SpannerSegment::is_user_modified() const {
    bool modified = !autoplace() || !visible() ||
                    (property_flags(Pid::MIN_DISTANCE) == PropertyFlags::UNSTYLED ||
                     get_property(Pid::MIN_DISTANCE) != property_default(Pid::MIN_DISTANCE)) ||
                    (!offset().is_null() || !user_off2().is_null());

    return modified;
}

bool SpannerSegment::allow_time_anchor() const { return spanner()->allow_time_anchor(); }

int SpannerSegment::subtype() const { return spanner()->subtype(); }

TranslatableString SpannerSegment::subtype_user_name() const { return spanner()->subtype_user_name(); }

//---------------------------------------------------------
//   Spanner
//---------------------------------------------------------

Spanner::Spanner(const ElementType &type, EngravingObject *parent, ElementFlags f) : EngravingItem(type, parent, f) {}

Spanner::Spanner(const Spanner &s) : EngravingItem(s) {
    play_spanner_ = s.play_spanner_;
    start_element_ = s.start_element_;
    end_element_ = s.end_element_;
    tick_ = s.tick_;
    ticks_ = s.ticks_;
    track2_ = s.track2_;

    for (auto *segment : s.segments_) {
        SpannerSegment *new_segment = to_spanner_segment(segment->clone());
        add(new_segment); // reparents the clone to this spanner
    }
}

Spanner::~Spanner() {
    // A spanner owns its segments (they are its DOM children), so it has to free
    // them. Without this they would only be unparented onto the dummy, where they
    // would sit until the score is destroyed - with a dangling spanner() pointer.
    erase_spanner_segments();
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Spanner::mag() const {
    if (system_flag()) {
        return 1.0;
    }
    return staff() ? staff()->staff_mag(tick()) : 1.0;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Spanner::add(EngravingItem *e) {
    SpannerSegment *ls = to_spanner_segment(e);
    ls->set_spanner(this);
    ls->set_selected(selected());
    ls->set_track(track());
    //      ls->setAutoplace(autoplace());
    ls->EngravingItem::set_z(z());
    ls->EngravingItem::set_visible(visible());
    segments_.push_back(ls);
    e->added();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Spanner::remove(EngravingItem *e) {
    SpannerSegment *ss = to_spanner_segment(e);
    if (ss->system()) {
        ss->system()->remove(ss);
    }
    segments_.erase(std::remove(segments_.begin(), segments_.end(), ss), segments_.end());
}

void Spanner::removed() {
    for (SpannerSegment *seg : segments_) {
        seg->move_to_system(nullptr);
    }

    for (SpannerSegment *seg : unused_segments_) {
        seg->move_to_system(nullptr);
    }
}

//---------------------------------------------------------
//   removeUnmanaged
//
//    Remove the Spanner and its segments from objects which may know about them
//
//    This method and the following are used for spanners which are contained within compound elements
//    which manage their parts themselves without using the standard management supplied by Score;
//    Example can be the LyricsLine within a Lyrics element or the FiguredBassLine within a FiguredBass
//    (not implemented yet).
//---------------------------------------------------------

void Spanner::remove_unmanaged() {
    for (SpannerSegment *ss : spanner_segments()) {
        if (ss->system()) {
            ss->move_to_system(nullptr);
        }
    }
    score()->remove_unmanaged_spanner(this);
}

//---------------------------------------------------------
//   insertTimeUnmanaged
//---------------------------------------------------------

void Spanner::insert_time_unmanaged(const Fraction &from_tick, const Fraction &len) {
    Fraction new_tick1 = tick();
    Fraction new_tick2 = tick2();

    // check spanner start and end point
    if (len > Fraction(0, 1)) {    // adding time
        if (tick() >= from_tick) { // start after insertion point: shift start to right
            new_tick1 += len;
        }
        if (tick2() > from_tick) { // end after insertion point: shift end to right
            new_tick2 += len;
        }
    }
    if (len < Fraction(0, 1)) { // removing time
        Fraction to_tick = from_tick - len;
        if (tick() > from_tick) {   // start after beginning of removed time
            if (tick() < to_tick) { // start within removed time: bring start at removing point
                if (ownership_parent()) {
                    ownership_parent_item()->remove(this);
                    return;
                } else {
                    new_tick1 = from_tick;
                }
            } else { // start after removed time: shift start to left
                new_tick1 += len;
            }
        }
        if (tick2() > from_tick) {   // end after start of removed time
            if (tick2() < to_tick) { // end within removed time: bring end at removing point
                new_tick2 = from_tick;
            } else { // end after removed time: shift end to left
                new_tick2 += len;
            }
        }
    }

    // update properties as required
    if (new_tick2 <= new_tick1) { // if no longer any span: remove it
        if (ownership_parent()) {
            ownership_parent_item()->remove(this);
        }
    } else { // if either TICKS or TICK did change, update property
        if (new_tick2 - new_tick1 != tick2() - tick()) {
            set_property(Pid::SPANNER_TICKS, new_tick2 - new_tick1);
        }
        if (new_tick1 != tick()) {
            set_property(Pid::SPANNER_TICK, new_tick1);
        }
    }
}

//---------------------------------------------------------
//   isVisibleCR
//---------------------------------------------------------

static bool is_visible_cr(EngravingItem *e) {
    if (!e || !e->is_chord_rest()) {
        return true; // assume visible
    }
    ChordRest *cr = to_chord_rest(e);
    return cr->measure()->visible(cr->staff_idx());
}

//---------------------------------------------------------
//   eitherEndVisible
//---------------------------------------------------------

bool Spanner::either_end_visible() const {
    if (is_volta()) {
        return true;
    }
    if (!score()->staff(staff_idx())->show()) {
        return false;
    }
    return is_visible_cr(start_element()) || is_visible_cr(end_element());
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void Spanner::set_score(Score *s) {
    EngravingItem::set_score(s);
    for (SpannerSegment *seg : segments_) {
        seg->set_score(s);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Spanner::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PLAY: return play_spanner_;
        case Pid::SPANNER_TICK: return tick_;
        case Pid::SPANNER_TICKS: return ticks_;
        case Pid::SPANNER_TRACK2: return track2();
        case Pid::LOCATION_STAVES: return (track2() / ::mu::engraving::voices) - (track() / ::mu::engraving::voices);
        case Pid::LOCATION_VOICES: return (track2() % ::mu::engraving::voices) - (track() / ::mu::engraving::voices);
        case Pid::LOCATION_FRACTIONS: return ticks_;
        case Pid::LOCATION_MEASURES:
        case Pid::LOCATION_GRACE:
        case Pid::LOCATION_NOTE: return Location::get_location_property(property_id, start_element(), end_element());
        default: break;
    }
    return EngravingItem::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Spanner::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::PLAY: set_play_spanner(v.to_bool()); break;
        case Pid::SPANNER_TICK:
            trigger_layout(); // spanner may have moved to another system
            set_tick(v.value<Fraction>());
            if (score() && is_in_spanner_map() && score()->spanner_map().remove_spanner(this)) {
                score()->add_spanner(this, /*computeStartEnd =*/false);
            }
            break;
        case Pid::SPANNER_TICKS:
            trigger_layout(); // spanner may now span for a smaller number of systems
            set_ticks(v.value<Fraction>());
            set_end_element(0); // invalidate
            break;
        case Pid::TRACK:
            set_track(v.value<track_idx_t>());
            set_start_element(0); // invalidate
            break;
        case Pid::SPANNER_TRACK2:
            set_track2(v.value<track_idx_t>());
            set_end_element(0); // invalidate
            break;
        case Pid::POSITION_LINKED_TO_MASTER:
            set_position_linked_to_master(v.to_bool());
            if (is_position_linked_to_master()) {
                for (SpannerSegment *seg : spanner_segments()) {
                    seg->relink_properties_to_master(PropertyGroup::POSITION);
                }
                relink_properties_to_master(PropertyGroup::POSITION);
            }
            break;
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            set_appearance_linked_to_master(v.to_bool());
            if (is_appearance_linked_to_master()) {
                for (SpannerSegment *seg : spanner_segments()) {
                    seg->relink_properties_to_master(PropertyGroup::APPEARANCE);
                }
                relink_properties_to_master(PropertyGroup::APPEARANCE);
            }
            break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Spanner::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::PLAY: return true;
        default: break;
    }
    return EngravingItem::property_default(property_id);
}

//---------------------------------------------------------
//   computeStartElement
//---------------------------------------------------------

void Spanner::compute_start_element() {
    EngravingItem *old_start_element = start_element_;

    do_compute_start_element();

    if (old_start_element && old_start_element->is_chord()) {
        to_chord(old_start_element)->remove_starting_spanner(this);
    }

    Chord *start_chord = start_element_ && start_element_->is_chord() ? to_chord(start_element_) : nullptr;
    if (start_chord) {
        start_chord->add_starting_spanner(this);
    }
}

void Spanner::do_compute_start_element() {
    switch (anchor()) {
        case Anchor::SEGMENT: {
            Segment *start_seg = start_segment();
            if (!start_seg) {
                return;
            }
            if (system_flag()) {
                start_element_ = start_segment();
            } else {
                EngravingItem *start_el = start_seg->element(track());
                if (start_el) {
                    start_element_ = start_el;
                } else {
                    start_element_ = start_seg->first_element(track2staff(track()));
                }
            }
        } break;

        case Anchor::MEASURE: start_element_ = score()->tick2measure(tick()); break;

        case Anchor::CHORDREST: start_element_ = start_cr(); break;
        case Anchor::NOTE: break;
    }
}

//---------------------------------------------------------
//   computeEndElement
//---------------------------------------------------------

void Spanner::compute_end_element() {
    EngravingItem *old_end_element = end_element_;

    do_compute_end_element();

    if (old_end_element && old_end_element->is_chord()) {
        to_chord(old_end_element)->remove_ending_spanner(this);
    }

    Chord *end_chord = end_element_ && end_element_->is_chord() ? to_chord(end_element_) : nullptr;
    if (end_chord) {
        end_chord->add_ending_spanner(this);
    }
}

void Spanner::do_compute_end_element() {
    switch (anchor()) {
        case Anchor::SEGMENT: {
            Segment *end_seg = end_segment();
            if (!end_seg) {
                return;
            }
            if (system_flag()) {
                end_element_ = end_seg;
            } else {
                track_idx_t track_idx = effective_track2();
                EngravingItem *end_el = end_seg->element(track_idx);
                if (end_el) {
                    end_element_ = end_el;
                } else {
                    end_element_ = end_seg->first_element(track2staff(track_idx));
                }
            }
        } break;

        case Anchor::MEASURE: end_element_ = score()->tick2measure(tick2() - Fraction(1, 1920)); break;

        case Anchor::NOTE: break;
        case Anchor::CHORDREST: end_element_ = end_cr(); break;
    }
}

bool Spanner::can_be_cross_staff() const {
    switch (type()) {
        case ElementType::SLUR:
        case ElementType::TIE:
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET:
        case ElementType::GLISSANDO:
        case ElementType::NOTELINE: return true;
        default: return false;
    }
}

//---------------------------------------------------------
//   startElementFromSpanner
//
//    Given a Spanner and an end element, determines a start element suitable for the end
//    element of a new Spanner, so that it is 'parallel' to the old one.
//    Can be used while cloning a linked Spanner, to update the cloned spanner start and end elements
//    (Spanner(const Spanner&) copies start and end elements from the original to the copy).
//    NOTES:      Only spanners with Anchor::NOTE are currently supported.
//                Going back from end to start ensures the 'other' anchor of this is already set up
//                      (for instance, while cloning staves)
//---------------------------------------------------------

Note *Spanner::start_element_from_spanner(Spanner *sp, EngravingItem *new_end) {
    if (sp->anchor() != Anchor::NOTE) {
        return nullptr;
    }

    Note *old_start = to_note(sp->start_element());
    Note *old_end = to_note(sp->end_element());
    if (old_start == nullptr || old_end == nullptr) {
        return nullptr;
    }
    Note *new_start = nullptr;
    Score *score = new_end->score();
    // determine the track where to expect the 'parallel' start element
    track_idx_t new_track = (new_end->track() - old_end->track()) + old_start->track();
    // look in notes linked to oldStart for a note with the
    // same score as new score and appropriate track
    for (EngravingObject *new_el : old_start->link_list()) {
        if (to_note(new_el)->score() == score && to_note(new_el)->track() == new_track) {
            new_start = to_note(new_el);
            break;
        }
    }
    return new_start;
}

//---------------------------------------------------------
//   endElementFromSpanner
//
//    Given a Spanner and a start element, determines an end element suitable for the start
//    element of a new Spanner, so that it is 'parallel' to the old one.
//    Can be used while cloning a linked Spanner, to update the cloned spanner start and end elements
//    (Spanner(const Spanner&) copies start and end elements from the original to the copy).
//    NOTES:      Only spanners with Anchor::NOTE are currently supported.
//---------------------------------------------------------

Note *Spanner::end_element_from_spanner(Spanner *sp, EngravingItem *new_start) {
    if (sp->anchor() != Anchor::NOTE) {
        return nullptr;
    }

    Note *old_start = to_note(sp->start_element());
    Note *old_end = to_note(sp->end_element());
    if (old_start == nullptr || old_end == nullptr) {
        return nullptr;
    }
    Note *new_end = nullptr;
    Score *score = new_start->score();
    // determine the track where to expect the 'parallel' start element
    track_idx_t new_track = new_start->track() + (old_end->track() - old_start->track());
    // look in notes linked to oldEnd for a note with the
    // same score as new score and appropriate track
    for (EngravingObject *new_el : old_end->link_list()) {
        if (to_note(new_el)->score() == score && to_note(new_el)->track() == new_track) {
            new_end = to_note(new_el);
            break;
        }
    }
    return new_end;
}

//---------------------------------------------------------
//   setNoteSpan
//
//    Sets up all the variables congruent with given start and end note anchors.
//---------------------------------------------------------

void Spanner::set_note_span(Note *start_note, Note *end_note) {
    if (anchor() != Anchor::NOTE) {
        return;
    }

    Score *score = start_note ? start_note->score() : end_note->score();
    Note *parent = start_note ? start_note : end_note;
    Fraction tick = start_note ? start_note->tick() : end_note->tick();
    Fraction end_tick = end_note ? end_note->tick() : start_note->tick();
    track_idx_t track = start_note ? start_note->track() : end_note->track();
    track_idx_t end_track = end_note ? end_note->track() : start_note->track();

    set_score(score);
    set_ownership_parent(parent);
    set_start_element(start_note);
    set_end_element(end_note);
    set_tick(tick);
    set_tick2(end_tick);
    set_track(track);
    set_track2(end_track);
}

//---------------------------------------------------------
//   startChord
//---------------------------------------------------------

Chord *Spanner::start_chord() {
    if (anchor() != Anchor::CHORDREST) {
        return nullptr;
    }
    if (!start_element_) {
        start_element_ = find_start_chord();
    }

    if (start_element_ && start_element_->is_chord()) {
        return to_chord(start_element_);
    }

    return nullptr;
}

//---------------------------------------------------------
//   endChord
//---------------------------------------------------------

Chord *Spanner::end_chord() {
    if (anchor() != Anchor::CHORDREST) {
        return nullptr;
    }
    if (!end_element_ && type() == ElementType::SLUR) {
        end_element_ = find_end_chord();
    }

    if (end_element_ && end_element_->is_chord()) {
        return to_chord(end_element_);
    }

    return nullptr;
}

//---------------------------------------------------------
//   startCR
//---------------------------------------------------------

ChordRest *Spanner::start_cr() {
    assert(anchor() == Anchor::SEGMENT || anchor() == Anchor::CHORDREST);
    if (!start_element_ || start_element_->score() != score()) {
        // TODO: This is a bit weird and prevents this method from being const...
        start_element_ = find_start_cr();
    }
    return (start_element_ && start_element_->is_chord_rest()) ? to_chord_rest(start_element_) : nullptr;
}

//---------------------------------------------------------
//   endCR
//---------------------------------------------------------

ChordRest *Spanner::end_cr() {
    assert(anchor() == Anchor::SEGMENT || anchor() == Anchor::CHORDREST);
    if ((!end_element_ || end_element_->score() != score())) {
        // TODO: This is a bit weird and prevents this method from being const...
        end_element_ = find_end_cr();
    }
    return (end_element_ && end_element_->is_chord_rest()) ? to_chord_rest(end_element_) : nullptr;
}

//---------------------------------------------------------
//   findStartChord
//---------------------------------------------------------

Chord *Spanner::find_start_chord() const {
    assert(anchor() == Anchor::CHORDREST);
    ChordRest *cr = score()->find_cr(tick(), track());
    return cr && cr->is_chord() ? to_chord(cr) : nullptr;
}

//---------------------------------------------------------
//   findEndChord
//---------------------------------------------------------

Chord *Spanner::find_end_chord() const {
    assert(anchor() == Anchor::CHORDREST);
    Segment *s = score()->tick2segment_mm(tick2(), false, SegmentType::CHORD_REST);
    ChordRest *end_cr = s ? to_chord_rest(s->element(track2())) : nullptr;
    if (end_cr && !end_cr->is_chord()) {
        end_cr = nullptr;
    }
    return to_chord(end_cr);
}

//---------------------------------------------------------
//   findStartCR
//---------------------------------------------------------

ChordRest *Spanner::find_start_cr() const {
    assert(anchor() == Anchor::SEGMENT || anchor() == Anchor::CHORDREST);
    return score()->find_cr(tick(), track());
}

//---------------------------------------------------------
//   findEndCR
//---------------------------------------------------------

ChordRest *Spanner::find_end_cr() const {
    assert(anchor() == Anchor::SEGMENT || anchor() == Anchor::CHORDREST);
    Segment *s = score()->tick2segment_mm(tick2(), false, SegmentType::CHORD_REST);
    const track_idx_t tr2 = effective_track2();
    ChordRest *end_cr = s ? to_chord_rest(s->element(tr2)) : nullptr;
    return end_cr;
}

//---------------------------------------------------------
//   startSegment
//---------------------------------------------------------

Segment *Spanner::start_segment() const {
    assert(score() != nullptr);

    bool mm_rest = style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS);
    Fraction start_tick = tick();
    track_idx_t track_idx = track();
    staff_idx_t staff_idx = track2staff(track_idx);

    Segment *start_seg = score()->tick2segment(start_tick, true, SegmentType::CHORD_REST, mm_rest);

    if (!start_seg || !start_seg->has_elements(staff_idx) || (is_voice_specific() && !start_seg->element(track_idx))) {
        start_seg = score()->tick2segment(start_tick, true, SegmentType::TIME_TICK, mm_rest);
    }

    if (!start_seg && start_tick < score()->end_tick()) {
        Measure *measure = mm_rest ? score()->tick2measure_mm(start_tick) : score()->tick2measure(start_tick);
        if (measure) {
            TimeTickAnchor *anchor =
                EditTimeTickAnchors::create_time_tick_anchor(measure, start_tick - measure->tick(), track2staff(track_idx));
            IF_ASSERT_FAILED(anchor) { return nullptr; }
            EditTimeTickAnchors::update_layout(measure);
            return anchor->segment();
        }
    }

    if (!start_seg) {
        start_seg = score()->tick2right_segment(start_tick, mm_rest);
    }

    return start_seg;
}

//---------------------------------------------------------
//   endSegment
//---------------------------------------------------------

Segment *Spanner::end_segment() const {
    assert(score() != nullptr);

    bool mm_rest = style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS);
    Fraction end_tick = tick2();
    track_idx_t track_idx = effective_track2();
    staff_idx_t staff_idx = track2staff(track_idx);

    Segment *end_seg = score()->tick2segment(end_tick, true, SegmentType::CHORD_REST, mm_rest);

    if (!end_seg || !end_seg->has_elements(staff_idx) || (is_voice_specific() && !end_seg->element(track_idx))) {
        end_seg = score()->tick2segment(end_tick, true, SegmentType::TIME_TICK, mm_rest);
    }

    if (!end_seg && !end_tick.is_zero()) {
        Measure *measure = mm_rest ? score()->tick2measure_mm(end_tick) : score()->tick2measure(end_tick);
        if (measure) {
            TimeTickAnchor *anchor =
                EditTimeTickAnchors::create_time_tick_anchor(measure, end_tick - measure->tick(), track2staff(track_idx));
            EditTimeTickAnchors::update_layout(measure);
            return anchor->segment();
        }
    }

    if (!end_seg) {
        end_seg = score()->tick2left_segment(end_tick, mm_rest);
    }

    return end_seg;
}

bool Spanner::element_applies_to_track(const track_idx_t ref_track) const {
    if (!has_voice_assignment_properties()) {
        return ref_track == track() || ref_track == track2();
    }

    const VoiceAssignment voice_assignment = get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();

    return EngravingItem::element_applies_to_track(track(), ref_track, voice_assignment, part()) ||
           EngravingItem::element_applies_to_track(track2(), ref_track, voice_assignment, part());
}

//---------------------------------------------------------
//   startMeasure
//---------------------------------------------------------

Measure *Spanner::start_measure() const { return to_measure(start_element_); }

//---------------------------------------------------------
//   endMeasure
//---------------------------------------------------------

Measure *Spanner::end_measure() const {
    assert(anchor() == Spanner::Anchor::MEASURE);
    return to_measure(end_element_);
}

Measure *Spanner::find_start_measure() const {
    if (!start_element_) {
        return nullptr;
    }

    return to_measure(start_element_->find_ancestor(ElementType::MEASURE));
}

Measure *Spanner::find_end_measure() const {
    if (!end_element_) {
        return nullptr;
    }

    return to_measure(end_element_->find_ancestor(ElementType::MEASURE));
}

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void Spanner::set_selected(bool f) {
    EngravingItem::set_selected(f);

    for (SpannerSegment *ss : spanner_segments()) {
        if (ss->selected() != f) {
            ss->set_selected(f);
        }
    }
}

//---------------------------------------------------------
//   setVisible
//---------------------------------------------------------

void Spanner::set_visible(bool f) {
    for (SpannerSegment *ss : spanner_segments()) {
        ss->EngravingItem::set_visible(f);
    }
    EngravingItem::set_visible(f);
}

//---------------------------------------------------------
//   setAutoplace
//---------------------------------------------------------

void Spanner::set_autoplace(bool f) {
    for (SpannerSegment *ss : spanner_segments()) {
        ss->EngravingItem::set_autoplace(f);
    }
    EngravingItem::set_autoplace(f);
}

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void Spanner::set_color(const Color &col) {
    for (SpannerSegment *ss : spanner_segments()) {
        ss->EngravingItem::set_color(col);
    }
    EngravingItem::set_color(col);
}

//---------------------------------------------------------
//   setZ
//---------------------------------------------------------

void Spanner::set_z(int val) {
    for (SpannerSegment *ss : spanner_segments()) {
        ss->EngravingItem::set_z(val);
    }
    EngravingItem::set_z(val);
}

//---------------------------------------------------------
//   setStartElement
//---------------------------------------------------------

void Spanner::set_start_element(EngravingItem *e) {
#ifndef NDEBUG
    if (anchor() == Anchor::NOTE) {
        assert(!e || e->is_note());
    }
#endif
    start_element_ = e;
}

//---------------------------------------------------------
//   setEndElement
//---------------------------------------------------------

void Spanner::set_end_element(EngravingItem *e) {
#ifndef NDEBUG
    if (anchor() == Anchor::NOTE) {
        assert(!e || e->is_note());
    }
#endif
    end_element_ = e;
}

//---------------------------------------------------------
//   nextSpanner
//---------------------------------------------------------

Spanner *Spanner::next_spanner(EngravingItem *e, staff_idx_t active_staff) {
    std::multimap<int, Spanner *> mmap = score()->spanner();
    auto range = mmap.equal_range(tick().ticks());
    if (range.first != range.second) { // range not empty
        for (auto i = range.first; i != range.second; ++i) {
            if (i->second == e) {
                while (i != range.second) {
                    ++i;
                    if (i == range.second) {
                        return nullptr;
                    }
                    Spanner *s = i->second;
                    EngravingItem *st = s->start_element();
                    if (!st) {
                        continue;
                    }
                    if (s->start_segment() == to_spanner(e)->start_segment()) {
                        if (st->staff_idx() == active_staff) {
                            return s;
                        } else if (st->is_measure() && active_staff == 0) {
                            return s;
                        }
                    }
                }
                break;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevSpanner
//---------------------------------------------------------

Spanner *Spanner::prev_spanner(EngravingItem *e, staff_idx_t active_staff) {
    std::multimap<int, Spanner *> mmap = score()->spanner();
    auto range = mmap.equal_range(tick().ticks());
    if (range.first != range.second) { // range not empty
        for (auto i = range.first; i != range.second; ++i) {
            if (i->second == e) {
                if (i == range.first) {
                    return nullptr;
                }
                while (i != range.first) {
                    --i;
                    Spanner *s = i->second;
                    EngravingItem *st = s->start_element();
                    if (s->start_segment() == to_spanner(e)->start_segment()) {
                        if (st->staff_idx() == active_staff) {
                            return s;
                        } else if (st->is_measure() && active_staff == 0) {
                            return s;
                        }
                    }
                }
                break;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Spanner::next_segment_element() {
    Segment *s = start_segment();
    if (s) {
        return s->first_element_for_navigation(staff_idx());
    }
    return Navigation::last_element(score());
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Spanner::prev_segment_element() {
    Segment *s = end_segment();
    if (s) {
        return s->last_element_for_navigation(staff_idx());
    }
    return Navigation::first_element(score());
}

//---------------------------------------------------------
//   setTick
//---------------------------------------------------------

void Spanner::set_tick(const Fraction &v) {
    if (tick_ == v) {
        return;
    }

    tick_ = v;

    Score *score = this->score();

    if (score && is_in_spanner_map()) {
        score->spanner_map().set_dirty();
    }
}

//---------------------------------------------------------
//   setTick2
//---------------------------------------------------------

void Spanner::set_tick2(const Fraction &f) {
    Fraction ticks = f - tick_;
    IF_ASSERT_FAILED(f >= tick_) {
        ticks = tick_ - f;
        tick_ = f;
    }
    set_ticks(ticks);
}

//---------------------------------------------------------
//   setTicks
//---------------------------------------------------------

void Spanner::set_ticks(const Fraction &f) {
    if (ticks_ == f) {
        return;
    }

    IF_ASSERT_FAILED(f.positive()) { ticks_ = -f; }
    else {
        ticks_ = f;
    }

    Score *score = this->score();

    if (score && is_in_spanner_map()) {
        score->spanner_map().set_dirty();
    }
}

bool Spanner::is_voice_specific() const {
    static const std::unordered_set<ElementType> voice_specific_spanners{
        ElementType::TRILL,
        ElementType::HAIRPIN,
        ElementType::LET_RING,
    };

    return voice_specific_spanners.find(type()) != voice_specific_spanners.end();
}

track_idx_t Spanner::track2() const { return can_be_cross_staff() ? track2_ : track_; }

void Spanner::set_track2(track_idx_t v) {
    if (!can_be_cross_staff()) {
        return;
    }

    track2_ = v;
}

track_idx_t Spanner::effective_track2() const { return can_be_cross_staff() && track2_ != muse::nidx ? track2_ : track_; }

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void Spanner::trigger_layout() const {
    // Don't trigger layout until the tick is correctly set, otherwise is triggers layout of the entire score
    if (tick_.numerator() < 0) {
        return;
    }

    // Spanners do not have parent even when added to a score, so can't check parent here
    const track_idx_t tr2 = effective_track2();
    score()->set_layout(tick_, tick_ + ticks_, staff_idx(), track2staff(tr2), this);
}

//---------------------------------------------------------
//   pushUnusedSegment
//---------------------------------------------------------

void Spanner::push_unused_segment(SpannerSegment *seg) {
    if (!seg) {
        return;
    }
    seg->move_to_system(nullptr);
    unused_segments_.push_back(seg);
}

//---------------------------------------------------------
//   popUnusedSegment
//    Take the next unused segment for reusing it.
//    If there is no unused segments left returns nullptr.
//---------------------------------------------------------

SpannerSegment *Spanner::pop_unused_segment() {
    if (unused_segments_.empty()) {
        return nullptr;
    }
    SpannerSegment *seg = unused_segments_.front();
    unused_segments_.pop_front();
    return seg;
}

//---------------------------------------------------------
//   reuse
//    called when segment from unusedSegments is added
//    back to the spanner.
//---------------------------------------------------------

void Spanner::reuse(SpannerSegment *seg) { add(seg); }

//---------------------------------------------------------
//   reuseSegments
//    Adds \p number segments from unusedSegments to this
//    spanner via reuse() call. Returns number of new
//    segments that still need to be created, that is,
//    returns (number - nMovedSegments).
//---------------------------------------------------------

int Spanner::reuse_segments(int number) {
    while (number > 0) {
        SpannerSegment *seg = pop_unused_segment();
        if (!seg) {
            break;
        }
        reuse(seg);
        --number;
    }
    return number;
}

//---------------------------------------------------------
//   fixupSegments
//    Makes number of segments match targetNumber.
//    Tries to reuse unused segments. If there are no
//    unused segments left, uses \p createSegment to create
//    the needed segments.
//    Previously unused segments are added via reuse() call
//---------------------------------------------------------

void Spanner::fixup_segments(unsigned int target_number, std::function<SpannerSegment *()> create_segment) {
    const int diff = target_number - int(nsegments());
    if (diff == 0) {
        return;
    }
    if (diff > 0) {
        const int ncreate = reuse_segments(diff);
        for (int i = 0; i < ncreate; ++i) {
            add(create_segment());
        }
    } else { // diff < 0
        const int nremove = -diff;
        for (int i = 0; i < nremove; ++i) {
            SpannerSegment *seg = segments_.back();
            segments_.pop_back();
            push_unused_segment(seg);
        }
    }
}

bool Spanner::is_user_modified() const {
    for (SpannerSegment *seg : segments_) {
        if (seg->is_user_modified()) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   eraseSpannerSegments
//    Completely erase all spanner segments, both used and
//    unused.
//---------------------------------------------------------

void Spanner::erase_spanner_segments() {
    // Unplace the used segments explicitly, so that System::remove() calls removed()
    // on a fully alive segment. ~SpannerSegment would unplace them too, but from there
    // the virtual call no longer reaches overrides below SpannerSegment.
    // Unused segments were already unplaced on their way into unusedSegments_.
    for (SpannerSegment *seg : segments_) {
        if (System *system = seg->system()) {
            system->remove(seg);
        }
    }

    muse::delete_all(segments_);
    muse::delete_all(unused_segments_);
    segments_.clear();
    unused_segments_.clear();
}

String SpannerSegment::format_bars_and_beats() const {
    const Spanner *spanner = this->spanner();

    if (!spanner) {
        return EngravingItem::format_bars_and_beats();
    }

    const Segment *end_segment = spanner->end_segment();

    if (!end_segment) {
        end_segment = score()->last_segment()->prev1_mm(SegmentType::CHORD_REST);
    }

    if (end_segment->tick() > Fraction(0, 1) && end_segment->tick() != score()->last_segment()->prev1_mm(SegmentType::CHORD_REST)->tick() &&
        spanner->type() != ElementType::SLUR && spanner->type() != ElementType::HAMMER_ON_PULL_OFF && spanner->type() != ElementType::TIE) {
        end_segment = end_segment->prev1_mm(SegmentType::CHORD_REST);
    }

    IF_ASSERT_FAILED(end_segment) { return EngravingItem::format_bars_and_beats(); }

    return format_start_bars_and_beats(spanner->start_segment()) + u' ' + format_end_bars_and_beats(end_segment);
}

String SpannerSegment::format_start_bars_and_beats(const Segment *segment) const {
    EngravingItem::BarBeat barbeat = segment->barbeat();
    String result = muse::mtrc("engraving", "Start measure: %1").arg(String::number(barbeat.bar));

    if (barbeat.displayed_bar != barbeat.bar) {
        result += u"; " + muse::mtrc("engraving", "Start displayed measure: %1").arg(barbeat.displayed_bar);
    }

    result += u"; " + muse::mtrc("engraving", "Start beat: %1").arg(barbeat.beat);
    return result;
}

String SpannerSegment::format_end_bars_and_beats(const Segment *segment) const {
    EngravingItem::BarBeat barbeat = segment->barbeat();
    String result = muse::mtrc("engraving", "End measure: %1").arg(String::number(barbeat.bar));

    if (barbeat.displayed_bar != barbeat.bar) {
        result += u"; " + muse::mtrc("engraving", "End displayed measure: %1").arg(barbeat.displayed_bar);
    }

    result += u"; " + muse::mtrc("engraving", "End beat: %1").arg(barbeat.beat);
    return result;
}

bool SpannerSegment::collect_for_drawing() const {
    return EngravingItem::collect_for_drawing() && (spanner()->either_end_visible() || system_flag());
}
} // namespace mu::engraving
