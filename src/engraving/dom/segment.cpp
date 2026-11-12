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

#include "segment.h"

#include <climits>

#include "../editing/editspanner.h"
#include "../editing/navigation.h"
#include "accidental.h"
#include "barline.h"
#include "beam.h"
#include "chord.h"
#include "chordrest.h"
#include "clef.h"
#include "engravingitem.h"
#include "hammeronpulloff.h"
#include "harppedaldiagram.h"
#include "hook.h"
#include "instrchange.h"
#include "keysig.h"
#include "log.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "ornament.h"
#include "part.h"
#include "rendering/score/tlayout.h"
#include "rest.h"
#include "score.h"
#include "staff.h"
#include "staffstate.h"
#include "system.h"
#include "systemlockindicator.h"
#include "timesig.h"
#include "translation.h"
#include "tuplet.h"
#include "types/typesconv.h"
#include "utils.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   subTypeName
//---------------------------------------------------------

const char *Segment::sub_type_name() const { return sub_type_name(segment_type_); }

const char *Segment::sub_type_name(SegmentType t) {
    switch (t) {
        case SegmentType::INVALID: return "Invalid";
        case SegmentType::BEGIN_BAR_LINE: return "BeginBarLine";
        case SegmentType::HEADER_CLEF: return "HeaderClef";
        case SegmentType::CLEF: return "Clef";
        case SegmentType::KEY_SIG: return "Key Signature";
        case SegmentType::AMBITUS: return "Ambitus";
        case SegmentType::TIME_SIG: return "Time Signature";
        case SegmentType::START_REPEAT_BAR_LINE: return "Begin Repeat";
        case SegmentType::CLEF_START_REPEAT_ANNOUNCE: return "Clef Repeat Start Courtesy";
        case SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE: return "Key Sig Repeat Start Courtesy";
        case SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE: return "Time Sig Repeat Start Courtesy";
        case SegmentType::BAR_LINE: return "BarLine";
        case SegmentType::BREATH: return "Breath";
        case SegmentType::CHORD_REST: return "ChordRest";
        case SegmentType::CLEF_REPEAT_ANNOUNCE: return "Clef Repeat Courtesy";
        case SegmentType::KEY_SIG_REPEAT_ANNOUNCE: return "Key Sig Repeat Courtesy";
        case SegmentType::TIME_SIG_REPEAT_ANNOUNCE: return "Time Sig Repeat Courtesy";
        case SegmentType::END_BAR_LINE: return "EndBarLine";
        case SegmentType::KEY_SIG_ANNOUNCE: return "Key Sig Courtesy";
        case SegmentType::TIME_SIG_ANNOUNCE: return "Time Sig Courtesy";
        case SegmentType::TIME_TICK: return "Time tick";
        default: return "??";
    }
}

//---------------------------------------------------------
//   setElement
//---------------------------------------------------------

void Segment::set_element(track_idx_t track, EngravingItem *el) {
    if (el) {
        el->set_ownership_parent(this);
        elist_[track] = el;
        set_empty(false);
    } else {
        elist_[track] = 0;
        check_empty();
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Segment::remove_element(track_idx_t track) {
    EngravingItem *el = element(track);
    if (el && el->is_chord_rest()) {
        ChordRest *cr = (ChordRest *) el;
        Beam *beam = cr->beam();
        if (beam) {
            beam->remove(cr);
        }
        Tuplet *tuplet = cr->tuplet();
        if (tuplet) {
            tuplet->remove(cr);
        }
    }
}

//---------------------------------------------------------
//   Segment
//---------------------------------------------------------

Segment::Segment(DummyParentOr<Measure> m)
    : EngravingItem(ElementType::SEGMENT, m, ElementFlag::EMPTY | ElementFlag::ENABLED | ElementFlag::NOT_SELECTABLE) {
    init();
}

Segment::Segment(DummyParentOr<Measure> m, SegmentType st, const Fraction &t)
    : EngravingItem(ElementType::SEGMENT, m, ElementFlag::EMPTY | ElementFlag::ENABLED | ElementFlag::NOT_SELECTABLE) {
    //      assert(t >= Fraction(0,1));
    //      assert(t <= m->ticks());
    segment_type_ = st;
    tick_ = t;
    init();
}

//---------------------------------------------------------
//   Segment
//---------------------------------------------------------

Segment::Segment(const Segment &s) : EngravingItem(s) {
    segment_type_ = s.segment_type_;
    tick_ = s.tick_;
    extra_leading_space_ = s.extra_leading_space_;

    for (EngravingItem *e : s.annotations_) {
        add(e->clone());
    }

    elist_.reserve(s.elist_.size());
    for (EngravingItem *e : s.elist_) {
        EngravingItem *ne = 0;
        if (e) {
            ne = e->clone();
            ne->set_ownership_parent(this);
        }
        elist_.push_back(ne);
    }
    shapes_ = s.shapes_;
}

void Segment::set_ownership_parent(DummyParentOr<Measure> parent) { EngravingItem::set_ownership_parent(parent); }

//---------------------------------------------------------
//   setSegmentType
//---------------------------------------------------------

void Segment::set_segment_type(SegmentType t) {
    assert(segment_type_ != SegmentType::CLEF || t != SegmentType::CHORD_REST);
    segment_type_ = t;
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void Segment::set_score(Score *score) {
    EngravingItem::set_score(score);
    for (EngravingItem *e : elist_) {
        if (e) {
            e->set_score(score);
        }
    }
    for (EngravingItem *e : annotations_) {
        e->set_score(score);
    }
}

Segment::~Segment() {
    for (EngravingItem *e : elist_) {
        if (!e) {
            continue;
        }

        Staff *staff = e->staff();

        if (e->is_time_sig() && staff) {
            staff->remove_time_sig(to_time_sig(e));
        }

        delete e;
    }

    muse::delete_all(annotations_);
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void Segment::init() {
    size_t staves = score()->nstaves();
    size_t tracks = staves * ::mu::engraving::voices;
    elist_.assign(tracks, 0);
    pre_appended_items_.assign(tracks, 0);
    shapes_.assign(staves, Shape());
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction Segment::tick() const { return tick_ + measure()->tick(); }

//---------------------------------------------------------
//   next1
///   return next \a Segment, don’t stop searching at end
///   of \a Measure
//---------------------------------------------------------

Segment *Segment::next1() const {
    if (next()) {
        return next();
    }
    Measure *m = measure()->next_measure();
    return m ? m->first() : 0;
}

//---------------------------------------------------------
//   next1enabled
//---------------------------------------------------------

Segment *Segment::next1enabled() const {
    Segment *s = next1();
    while (s && !s->enabled()) {
        s = s->next1();
    }
    return s;
}

//---------------------------------------------------------
//   next1MM
//---------------------------------------------------------

Segment *Segment::next1_mm() const {
    if (next()) {
        return next();
    }
    Measure *m = measure()->next_measure_mm();
    return m ? m->first() : 0;
}

Segment *Segment::next1(SegmentType types) const {
    for (Segment *s = next1(); s; s = s->next1()) {
        if (s->segment_type() & types) {
            return s;
        }
    }
    return 0;
}

Segment *Segment::next1_with_elems_on_staff(staff_idx_t staff_idx, SegmentType seg_type) const {
    Segment *next = next1(seg_type);

    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices - 1;
    while (next && !next->has_elements(start_track, end_track)) {
        next = next->next1(seg_type);
    }

    return next;
}

Segment *Segment::next1_with_elems_on_track(track_idx_t track_idx, SegmentType seg_type) const {
    Segment *next = next1(seg_type);

    while (next && !next->has_elements(track_idx, track_idx)) {
        next = next->next1(seg_type);
    }

    return next;
}

Segment *Segment::next1_mm(SegmentType types) const {
    for (Segment *s = next1_mm(); s; s = s->next1_mm()) {
        if (s->segment_type() & types) {
            return s;
        }
    }
    return 0;
}

Segment *Segment::next1_m_menabled() const {
    Segment *s = next1_mm();
    while (s && !s->enabled()) {
        s = s->next1_mm();
    }
    return s;
}

//---------------------------------------------------------
//   next
//    got to next segment which has subtype in types
//---------------------------------------------------------

Segment *Segment::next(SegmentType types) const {
    for (Segment *s = next(); s; s = s->next()) {
        if (s->segment_type() & types) {
            return s;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   nextInStaff
///   Returns next \c Segment in the staff with given index
//---------------------------------------------------------

Segment *Segment::next_in_staff(staff_idx_t staff_idx, SegmentType type) const {
    Segment *s = next(type);
    const track_idx_t min_track = staff_idx * ::mu::engraving::voices;
    const track_idx_t max_track = (staff_idx + 1) * ::mu::engraving::voices - 1;
    while (s && !s->has_elements(min_track, max_track)) {
        s = s->next(type);
    }
    return s;
}

//---------------------------------------------------------
//   prev
//    got to previous segment which has subtype in types
//---------------------------------------------------------

Segment *Segment::prev(SegmentType types) const {
    for (Segment *s = prev(); s; s = s->prev()) {
        if (s->segment_type() & types) {
            return s;
        }
    }
    return 0;
}

Segment *Segment::prev_with_elements_on_track(track_idx_t track_idx, SegmentType seg_type) const {
    Segment *prev_seg = prev(seg_type);

    while (prev_seg && !prev_seg->element(track_idx)) {
        prev_seg = prev_seg->prev(seg_type);
    }

    return prev_seg;
}

//---------------------------------------------------------
//   prev1
///   return previous \a Segment, don’t stop searching at
///   \a Measure begin
//---------------------------------------------------------

Segment *Segment::prev1() const {
    if (prev()) {
        return prev();
    }
    Measure *m = measure()->prev_measure();
    return m ? m->last() : 0;
}

Segment *Segment::prev1_with_elems_on_staff(staff_idx_t staff_idx, SegmentType seg_type) const {
    Segment *prev = prev1(seg_type);

    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices - 1;
    while (prev && !prev->has_elements(start_track, end_track)) {
        prev = prev->prev1(seg_type);
    }

    return prev;
}

Segment *Segment::prev1_with_elems_on_track(track_idx_t track_idx, SegmentType seg_type) const {
    Segment *prev = prev1(seg_type);

    while (prev && !prev->has_elements(track_idx, track_idx)) {
        prev = prev->prev1(seg_type);
    }

    return prev;
}

Segment *Segment::prev1enabled() const {
    Segment *s = prev1();
    while (s && !s->enabled()) {
        s = s->prev1();
    }
    return s;
}

Segment *Segment::prev1_mm() const {
    if (prev()) {
        return prev();
    }
    Measure *m = measure()->prev_measure_mm();
    return m ? m->last() : 0;
}

Segment *Segment::prev1_m_menabled() const {
    Segment *s = prev1_mm();
    while (s && !s->enabled()) {
        s = s->prev1_mm();
    }
    return s;
}

Segment *Segment::prev1(SegmentType types) const {
    for (Segment *s = prev1(); s; s = s->prev1()) {
        if (s->segment_type() & types) {
            return s;
        }
    }
    return 0;
}

Segment *Segment::prev1_mm(SegmentType types) const {
    for (Segment *s = prev1_mm(); s; s = s->prev1_mm()) {
        if (s->segment_type() & types) {
            return s;
        }
    }
    return 0;
}

Segment *Segment::next_active() const {
    Segment *ns = next();
    while (ns && !ns->is_active()) {
        ns = ns->next();
    }
    return ns;
}

Segment *Segment::next_enabled() const {
    Segment *ns = next();
    while (ns && !ns->enabled()) {
        ns = ns->next();
    }
    return ns;
}

Segment *Segment::prev_active() const {
    Segment *ps = prev();
    while (ps && !ps->is_active()) {
        ps = ps->prev();
    }
    return ps;
}

Segment *Segment::prev_enabled() const {
    Segment *ps = prev();
    while (ps && !ps->enabled()) {
        ps = ps->prev();
    }
    return ps;
}

//---------------------------------------------------------
//   nextCR
//    get next ChordRest Segment
//---------------------------------------------------------

Segment *Segment::next_cr(track_idx_t track, bool same_staff) const {
    track_idx_t strack = track;
    track_idx_t etrack;
    if (same_staff) {
        strack &= ~(::mu::engraving::voices - 1);
        etrack = strack + ::mu::engraving::voices;
    } else {
        etrack = strack + 1;
    }
    for (Segment *seg = next1(); seg; seg = seg->next1()) {
        if (seg->is_chord_rest_type()) {
            if (track == muse::nidx) {
                return seg;
            }
            for (track_idx_t t = strack; t < etrack; ++t) {
                if (seg->element(t)) {
                    return seg;
                }
            }
        }
    }
    return 0;
}

//---------------------------------------------------------
//   nextChordRest
//    get the next ChordRest, start at this segment
//---------------------------------------------------------

ChordRest *Segment::next_chord_rest(track_idx_t track, bool backwards, bool stop_at_measure_boundary) const {
    const Segment *seg = this;
    while (seg) {
        EngravingItem *el = seg->element(track);
        if (el && el->is_chord_rest()) {
            return to_chord_rest(el);
        }

        if (backwards) {
            seg = stop_at_measure_boundary ? seg->prev() : seg->prev1();
            continue;
        }

        seg = stop_at_measure_boundary ? seg->next() : seg->next1();
    }
    return nullptr;
}

EngravingItem *Segment::element(track_idx_t track) const {
    if (track >= elist_.size()) {
        return nullptr;
    }

    return elist_[track];
}

System *Segment::system() const {
    Measure *m = measure();
    return m ? m->system() : nullptr;
}

//---------------------------------------------------------
//   insertStaff
//---------------------------------------------------------

void Segment::insert_staff(staff_idx_t staff) {
    track_idx_t track = staff * ::mu::engraving::voices;
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        elist_.insert(elist_.begin() + track, 0);
        pre_appended_items_.insert(pre_appended_items_.begin() + track, 0);
    }
    shapes_.insert(shapes_.begin() + staff, Shape());

    for (EngravingItem *e : annotations_) {
        if (move_down_when_adding_staves(e, staff)) {
            e->set_track(e->track() + ::mu::engraving::voices);
        }
    }
    fix_staff_idx();
}

//---------------------------------------------------------
//   removeStaff
//---------------------------------------------------------

void Segment::remove_staff(staff_idx_t staff) {
    track_idx_t track = staff * ::mu::engraving::voices;
    elist_.erase(elist_.begin() + track, elist_.begin() + track + ::mu::engraving::voices);
    pre_appended_items_.erase(pre_appended_items_.begin() + track, pre_appended_items_.begin() + track + ::mu::engraving::voices);
    shapes_.erase(shapes_.begin() + staff);

    for (EngravingItem *e : annotations_) {
        staff_idx_t staff_idx = e->staff_idx();
        if (staff_idx > staff) {
            e->set_track(e->track() - ::mu::engraving::voices);
        }
    }

    fix_staff_idx();
}

//---------------------------------------------------------
//   checkElement
//---------------------------------------------------------

void Segment::check_element(EngravingItem *el, track_idx_t track) {
    // prevent segmentation fault on out of bounds index
    IF_ASSERT_FAILED(track < elist_.size()) { return; }
    // generated elements can be overwritten
    if (elist_[track] && !elist_[track]->generated()) {
        LOGD("add(%s): there is already a %s at track %zu tick %d", el->type_name(), elist_[track]->type_name(), track, tick().ticks());
        //            abort();
    }
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Segment::add_layout_annotation(EngravingItem *item) {
    assert(item->is_layout_copy());
    item->set_ownership_parent(this);
    annotations_.push_back(item);
}

void Segment::remove_layout_copy(EngravingItem *item) {
    assert(item->is_layout_copy());
    std::erase(annotations_, item);
    if (item->track() < elist_.size() && elist_[item->track()] == item) elist_[item->track()] = nullptr;
    delete item;
    check_empty();
}

void Segment::add(EngravingItem *el) {
    //      LOGD("%p segment %s add(%d, %d, %s)", this, subTypeName(), tick(), el->track(), el->typeName());

    if (el->ownership_parent() != this) {
        el->set_ownership_parent(this);
    }

    track_idx_t track = el->track();
    assert(track != muse::nidx);
    assert(track < score()->ntracks());
    assert(el->score() == score());
    assert(score()->nstaves() * ::mu::engraving::voices == elist_.size());

    switch (el->type()) {
        case ElementType::MEASURE_REPEAT:
            elist_[track] = el;
            set_empty(false);
            break;

        //fallthrough
        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM:
        case ElementType::TEMPO_TEXT:
        case ElementType::DYNAMIC:
        case ElementType::EXPRESSION:
        case ElementType::SYMBOL:
        case ElementType::STAFF_TEXT:
        case ElementType::STAVE_SHARING_LABEL:
        case ElementType::SYSTEM_TEXT:
        case ElementType::TRIPLET_FEEL:
        case ElementType::PLAYTECH_ANNOTATION:
        case ElementType::CAPO:
        case ElementType::REHEARSAL_MARK:
        case ElementType::MARKER:
        case ElementType::IMAGE:
        case ElementType::TEXT:
        case ElementType::TREMOLOBAR:
        case ElementType::TAB_DURATION_SYMBOL:
        case ElementType::FIGURED_BASS:
        case ElementType::FERMATA:
        case ElementType::STICKING: annotations_.push_back(el); break;

        case ElementType::STRING_TUNINGS: {
            annotations_.push_back(el);
            // Shared parts display copies of their origin parts' string tunings. Tunings should not be registered in them
            if (!el->part()->is_shared_part()) {
                el->part()->add_string_tunings(to_string_tunings(el));
            }
            break;
        }

        case ElementType::PLAY_COUNT_TEXT:
            assert(is_type(SegmentType::BAR_LINE_TYPES));
            if (!find_annotation(ElementType::PLAY_COUNT_TEXT, el->track(), el->track())) {
                annotations_.push_back(el);
            }
            break;

        case ElementType::STAFF_STATE:
            if (to_staff_state(el)->staff_state_type() == StaffStateType::INSTRUMENT) {
                StaffState *ss = to_staff_state(el);
                Part *part = el->part();
                part->set_instrument(ss->instrument(), tick());
            }
            annotations_.push_back(el);
            break;

        case ElementType::INSTRUMENT_CHANGE: {
            InstrumentChange *is = to_instrument_change(el);
            Part *part = is->part();
            part->set_instrument(is->instrument(), tick());
            annotations_.push_back(el);
            break;
        }

        case ElementType::HARP_DIAGRAM:
            // Shared parts display copies of their origin parts' diagrams; the origin parts own them
            if (el->part()->is_shared_part()) {
                annotations_.push_back(el);
                break;
            }
            // already a diagram in this segment
            if (el->part()->harp_diagrams().count(to_harp_pedal_diagram(el)->segment()->tick().ticks()) > 0) {
                break;
            }
            el->part()->add_harp_diagram(to_harp_pedal_diagram(el));
            annotations_.push_back(el);
            break;

        case ElementType::CLEF:
            assert(segment_type_ & SegmentType::CLEF_TYPES);
            check_element(el, track);
            elist_[track] = el;
            if (!el->generated()) {
                el->staff()->set_clef(to_clef(el));
            }
            set_empty(false);
            break;

        case ElementType::TIMESIG:
            assert(segment_type() & SegmentType::TIME_SIG_TYPES);
            check_element(el, track);
            elist_[track] = el;
            el->staff()->add_time_sig(to_time_sig(el));
            set_empty(false);
            if (segment_type() & SegmentType::COURTESY_TIME_SIG_TYPES) {
                to_time_sig(el)->set_is_courtesy(true);
            }
            break;

        case ElementType::KEYSIG:
            assert(segment_type_ & SegmentType::KEY_SIG_TYPES);
            check_element(el, track);
            elist_[track] = el;
            if (segment_type_ & SegmentType::COURTESY_KEY_SIG_TYPES) {
                to_key_sig(el)->set_is_courtesy(true);
            }
            if (!el->generated() && !to_key_sig(el)->is_courtesy()) {
                el->staff()->set_key(tick(), to_key_sig(el)->key_sig_event());
            }
            set_empty(false);
            break;

        case ElementType::CHORD:
        case ElementType::REST:
        case ElementType::MMREST:
            assert(segment_type_ == SegmentType::CHORD_REST);
            {
                if (track % ::mu::engraving::voices) {
                    bool v;
                    if (el->is_chord()) {
                        v = false;
                        // consider chord visible if any note is visible
                        Chord *c = to_chord(el);
                        for (Note *n : c->notes()) {
                            if (n->visible()) {
                                v = true;
                                break;
                            }
                        }
                    } else {
                        v = el->visible();
                    }

                    if (v && measure()->score()->ntracks() > track) {
                        measure()->set_has_voices(track / ::mu::engraving::voices, true);
                    }
                }
            }
            // fall through

        case ElementType::BAR_LINE:
        case ElementType::BREATH:
            if (track < score()->nstaves() * ::mu::engraving::voices) {
                check_element(el, track);
                elist_[track] = el;
            }
            set_empty(false);
            break;

        case ElementType::AMBITUS:
            assert(segment_type_ == SegmentType::AMBITUS);
            check_element(el, track);
            elist_[track] = el;
            set_empty(false);
            break;

        case ElementType::TIME_TICK_ANCHOR:
            assert(segment_type_ == SegmentType::TIME_TICK);
            elist_[track] = el;
            set_empty(false);
            break;

        default: ASSERT_X(String(u"Segment::add() unknown %1").arg(String::from_ascii(el->type_name()))); return;
    }

    el->added();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Segment::remove(EngravingItem *el) {
    // LOGD("%p Segment::remove %s %p", this, el->typeName(), el);

    track_idx_t track = el->track();

    switch (el->type()) {
        case ElementType::CHORD:
        case ElementType::REST: {
            elist_[track] = 0;
            staff_idx_t staff_idx = el->staff_idx();
            measure()->check_multi_voices(staff_idx);
            // spanners with this cr as start or end element will need relayout
            SpannerMap &smap = score()->spanner_map();
            auto spanners = smap.find_overlapping(tick().ticks(), tick().ticks());
            for (auto interval : spanners) {
                Spanner *s = interval.value;
                EngravingItem *start = s->start_element();
                EngravingItem *end = s->end_element();
                if (s->start_element() == el) {
                    start = nullptr;
                }
                if (s->end_element() == el) {
                    end = nullptr;
                }
                if (start != s->start_element() || end != s->end_element()) {
                    score()->undo(new ChangeStartEndSpanner(s, start, end));
                }
            }
        } break;

        case ElementType::MMREST:
        case ElementType::MEASURE_REPEAT:
        case ElementType::TIME_TICK_ANCHOR: elist_[track] = 0; break;

        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM: score()->rebuild_fret_box();
        //fallthrough
        case ElementType::DYNAMIC:
        case ElementType::EXPRESSION:
        case ElementType::FIGURED_BASS:
        case ElementType::IMAGE:
        case ElementType::MARKER:
        case ElementType::REHEARSAL_MARK:
        case ElementType::STAFF_TEXT:
        case ElementType::STAVE_SHARING_LABEL:
        case ElementType::SYSTEM_TEXT:
        case ElementType::TRIPLET_FEEL:
        case ElementType::PLAYTECH_ANNOTATION:
        case ElementType::SYMBOL:
        case ElementType::TAB_DURATION_SYMBOL:
        case ElementType::TEMPO_TEXT:
        case ElementType::TEXT:
        case ElementType::TREMOLOBAR:
        case ElementType::FERMATA:
        case ElementType::STICKING:
        case ElementType::PLAY_COUNT_TEXT: remove_annotation(el); break;
        case ElementType::CAPO:
            for (Staff *staff : el->part()->staves()) {
                staff->remove_capo_params(el->tick());
            }
            remove_annotation(el);
            break;

        case ElementType::STRING_TUNINGS:
            if (!el->part()->is_shared_part()) {
                el->part()->remove_string_tunings(to_string_tunings(el));
            }
            remove_annotation(el);
            break;

        case ElementType::STAFF_STATE:
            if (to_staff_state(el)->staff_state_type() == StaffStateType::INSTRUMENT) {
                el->part()->remove_instrument(to_staff_state(el)->instrument(), tick());
            }
            remove_annotation(el);
            break;

        case ElementType::INSTRUMENT_CHANGE:
            // Ensure that the entry in the Part's InstrumentList for this segment's tick is the one this InstrumentChange owns:
            el->part()->remove_instrument(to_instrument_change(el)->instrument(), tick());
            remove_annotation(el);
            break;

        case ElementType::HARP_DIAGRAM:
            if (!el->part()->is_shared_part()) {
                el->part()->remove_harp_diagram(to_harp_pedal_diagram(el));
            }
            remove_annotation(el);
            break;

        case ElementType::TIMESIG:
            elist_[track] = 0;
            el->staff()->remove_time_sig(to_time_sig(el));
            break;

        case ElementType::KEYSIG:
            elist_[track] = 0;
            if (!el->generated() && !to_key_sig(el)->is_courtesy()) {
                el->staff()->remove_key(tick());
            }
            break;

        case ElementType::CLEF:
            el->staff()->remove_clef(to_clef(el));
            // updateNoteLines(this, el->track());
            // fall through

        case ElementType::BAR_LINE:
        case ElementType::AMBITUS: elist_[track] = 0; break;

        case ElementType::BREATH: elist_[track] = 0; break;

        default: ASSERT_X(String(u"Segment::remove() unknown %1").arg(String::from_ascii(el->type_name()))); return;
    }
    trigger_layout();
    check_empty();
    el->removed();
}

//---------------------------------------------------------
//   segmentType
//    returns segment type suitable for storage of EngravingItem
//---------------------------------------------------------

SegmentType Segment::segment_type(ElementType type) {
    switch (type) {
        case ElementType::CHORD:
        case ElementType::REST:
        case ElementType::MMREST:
        case ElementType::MEASURE_REPEAT:
        case ElementType::JUMP:
        case ElementType::MARKER: return SegmentType::CHORD_REST;
        case ElementType::CLEF: return SegmentType::CLEF;
        case ElementType::KEYSIG: return SegmentType::KEY_SIG;
        case ElementType::TIMESIG: return SegmentType::TIME_SIG;
        case ElementType::BAR_LINE: return SegmentType::START_REPEAT_BAR_LINE;
        case ElementType::BREATH: return SegmentType::BREATH;
        default: LOGD("Segment:segmentType():  bad type: <%s>", TConv::to_xml(type).ascii()); return SegmentType::INVALID;
    }
}

//---------------------------------------------------------
//   sortStaves
//---------------------------------------------------------

void Segment::sort_staves(std::vector<staff_idx_t> &dst) {
    std::vector<EngravingItem *> dl;
    dl.reserve(dst.size());

    for (staff_idx_t i = 0; i < dst.size(); ++i) {
        track_idx_t start_track = dst[i] * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;
        for (track_idx_t k = start_track; k < end_track; ++k) {
            dl.push_back(elist_[k]);
        }
    }
    std::swap(elist_, dl);
    std::map<staff_idx_t, staff_idx_t> map;
    for (staff_idx_t k = 0; k < dst.size(); ++k) {
        map.insert({dst[k], k});
    }
    for (EngravingItem *e : annotations_) {
        if (e->is_top_system_object()) {
            continue;
        }
        e->set_track(map[e->staff_idx()] * ::mu::engraving::voices + e->voice());
    }
    fix_staff_idx();
}

//---------------------------------------------------------
//   fixStaffIdx
//---------------------------------------------------------

void Segment::fix_staff_idx() {
    int track = 0;
    for (EngravingItem *e : elist_) {
        if (e) {
            e->set_track(track);
        }
        ++track;
    }
}

//---------------------------------------------------------
//   checkEmpty
//---------------------------------------------------------

void Segment::check_empty() const {
    if (!annotations_.empty()) {
        set_empty(false);
        return;
    }
    set_empty(true);
    for (const EngravingItem *e : elist_) {
        if (e) {
            set_empty(false);
            break;
        }
    }
}

double Segment::x_pos_in_system_coords() const { return ldata()->pos().x() + measure()->x(); }

void Segment::set_x_pos_in_system_coords(double x) { mutldata()->set_pos_x(x - measure()->x()); }

bool Segment::is_tuplet_subdivision_on_staff(staff_idx_t staff_idx) const {
    Fraction time_stretch = score()->staff(staff_idx)->time_stretch(tick());
    int denom = (tick_ * time_stretch).reduced().denominator();
    bool denom_is_pow_of_two = (denom & (denom - 1)) == 0;
    // A non-power-of-two denominator is possible only with tuplets
    return !denom_is_pow_of_two;
}

bool Segment::is_inside_tuplet_on_staff(staff_idx_t staff_idx) const {
    const Segment *ref_cr_seg =
        is_chord_rest_type() && has_elements(staff_idx) ? this : prev1_with_elems_on_staff(staff_idx, SegmentType::CHORD_REST);
    if (!ref_cr_seg || ref_cr_seg->measure() != measure()) {
        return false;
    }

    track_idx_t start_track = staff2track(staff_idx);
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    for (track_idx_t track = start_track; track < end_track; ++track) {
        ChordRest *chord_rest = to_chord_rest(ref_cr_seg->element(track));
        if (chord_rest && chord_rest->tuplet() && tick() != chord_rest->top_tuplet()->tick()) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   swapElements
//---------------------------------------------------------

void Segment::swap_elements(track_idx_t i1, track_idx_t i2) {
    std::iter_swap(elist_.begin() + i1, elist_.begin() + i2);
    if (elist_[i1]) {
        elist_[i1]->set_track(i1);
    }
    if (elist_[i2]) {
        elist_[i2]->set_track(i2);
    }
    trigger_layout();
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Segment::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TICK: return tick_;
        case Pid::LEADING_SPACE: return extra_leading_space();
        case Pid::END_OF_MEASURE_CHANGE: return end_of_measure_change();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Segment::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LEADING_SPACE: return 0.0_sp;
        default: return EngravingItem::property_default(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Segment::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::TICK: set_rtick(v.value<Fraction>()); break;
        case Pid::LEADING_SPACE:
            set_extra_leading_space(v.value<Spatium>());
            for (EngravingItem *e : elist_) {
                if (e) {
                    e->set_generated(false);
                }
            }
            break;
        case Pid::END_OF_MEASURE_CHANGE: set_end_of_measure_change(v.to_bool()); break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   widthInStaff
//---------------------------------------------------------

double Segment::width_in_staff(staff_idx_t staff_idx, SegmentType next_seg_type) const {
    const double seg_x = x();
    double next_seg_x = seg_x;

    Segment *next_seg = next_in_staff(staff_idx, next_seg_type);
    if (next_seg) {
        next_seg_x = next_seg->x();
    } else {
        Segment *last_seg = measure()->last_enabled();
        if (last_seg->segment_type() & next_seg_type) {
            next_seg_x = last_seg->x() + last_seg->width();
        } else {
            next_seg_x = last_seg->x();
        }
    }

    return next_seg_x - seg_x;
}

//---------------------------------------------------------
//   ticksInStaff
//---------------------------------------------------------

Fraction Segment::ticks_in_staff(staff_idx_t staff_idx) const {
    const Fraction seg_tick = tick();
    Fraction next_seg_tick = seg_tick;

    Segment *next_seg = next_in_staff(staff_idx, SegmentType::DURATION);
    if (next_seg) {
        next_seg_tick = next_seg->tick();
    } else {
        Segment *last_seg = measure()->last();
        next_seg_tick = last_seg->tick() + last_seg->ticks();
    }

    return next_seg_tick - seg_tick;
}

//---------------------------------------------------------
//   splitsTuplet
//---------------------------------------------------------

bool Segment::splits_tuplet() const {
    for (EngravingItem *e : elist_) {
        if (!(e && e->is_chord_rest())) {
            continue;
        }
        // an outer tuplet's first element is the inner tuplet, not the chord or rest
        const DurationElement *el = to_chord_rest(e);
        for (const Tuplet *t = el->tuplet(); t; el = t, t = t->tuplet()) {
            if (el != t->elements().front()) {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   operator<
///   return true if segment is before s in list
//---------------------------------------------------------

bool Segment::operator<(const Segment &s) const {
    if (tick() < s.tick()) {
        return true;
    }
    if (tick() > s.tick()) {
        return false;
    }
    for (Segment *ns = next1(); ns && (ns->tick() == s.tick()); ns = ns->next1()) {
        if (ns == &s) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   operator>
///   return true if segment is after s in list
//---------------------------------------------------------

bool Segment::operator>(const Segment &s) const {
    if (tick() > s.tick()) {
        return true;
    }
    if (tick() < s.tick()) {
        return false;
    }
    for (Segment *ns = prev1(); ns && (ns->tick() == s.tick()); ns = ns->prev1()) {
        if (ns == &s) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasElements
///  Returns true if the segment has at least one element.
///  Annotations are not considered.
//---------------------------------------------------------

bool Segment::has_elements() const {
    for (const EngravingItem *e : elist_) {
        if (e) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasElements
///  return true if an annotation of type type or and element is found in the track range
//---------------------------------------------------------

bool Segment::has_elements(track_idx_t min_track, track_idx_t max_track) const {
    for (track_idx_t cur_track = min_track; cur_track <= max_track; cur_track++) {
        if (element(cur_track)) {
            return true;
        }
    }
    return false;
}

bool Segment::has_elements(staff_idx_t staff_idx) const {
    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices - 1;
    return has_elements(start_track, end_track);
}

//---------------------------------------------------------
//   allElementsInvisible
///  return true if all elements in the segment are invisible
//---------------------------------------------------------

bool Segment::all_elements_invisible() const {
    if (is_type(SegmentType::TIME_TICK)) {
        return true;
    }

    if (is_type(SegmentType::BAR_LINE_TYPES | SegmentType::CHORD_REST)) {
        return false;
    }

    System *sys = system();
    for (staff_idx_t staff_idx = 0; staff_idx < score()->nstaves(); ++staff_idx) {
        Staff *staff = score()->staves().at(staff_idx);
        if (!staff->visible()) {
            continue;
        }
        if (sys && staff_idx < sys->staves().size() && !sys->staves().at(staff_idx)->show()) {
            continue;
        }
        track_idx_t end_track = staff_idx * ::mu::engraving::voices + ::mu::engraving::voices;
        for (track_idx_t track = staff_idx * ::mu::engraving::voices; track < end_track; ++track) {
            EngravingItem *e = elist_[track];
            if (e && e->visible() && !muse::real_is_equal(e->width(), 0.0)) {
                return false;
            }
        }
    }

    return true;
}

//---------------------------------------------------------
//   hasAnnotationOrElement
///  return true if an annotation of type type or and element is found in the track range
//---------------------------------------------------------

bool Segment::has_annotation_or_element(ElementType type, track_idx_t min_track, track_idx_t max_track) const {
    for (const EngravingItem *e : annotations_) {
        if (e->type() == type && e->track() >= min_track && e->track() <= max_track) {
            return true;
        }
    }
    return has_elements(min_track, max_track);
}

bool Segment::has_annotation_or_element(staff_idx_t staff_idx) const {
    for (const EngravingItem *e : annotations_) {
        if (e->staff_idx() == staff_idx) {
            return true;
        }
    }
    return has_elements(staff_idx);
}

//---------------------------------------------------------
//   findAnnotation
///  Returns the first found annotation of type type
///  or nullptr if nothing was found.
//---------------------------------------------------------

EngravingItem *Segment::find_annotation(ElementType type, track_idx_t min_track, track_idx_t max_track) const {
    for (EngravingItem *e : annotations_) {
        if (e->type() == type && e->track() >= min_track && e->track() <= max_track) {
            return e;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   findAnnotations
///  Returns the list of found annotations
///  or an empty list if nothing was found.
//---------------------------------------------------------

std::vector<EngravingItem *> Segment::find_annotations(ElementType type, track_idx_t min_track, track_idx_t max_track) const {
    std::vector<EngravingItem *> found;
    for (EngravingItem *e : annotations_) {
        if (e->type() == type && e->track() >= min_track && e->track() <= max_track) {
            found.push_back(e);
        }
    }
    return found;
}

//---------------------------------------------------------
//   removeAnnotation
//---------------------------------------------------------

void Segment::remove_annotation(EngravingItem *e) {
    for (auto i = annotations_.begin(); i != annotations_.end(); ++i) {
        if (*i == e) {
            annotations_.erase(i);
            break;
        }
    }
}

//---------------------------------------------------------
//   clearAnnotations
//---------------------------------------------------------

void Segment::clear_annotations() { annotations_.clear(); }

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Segment::scan_elements(std::function<void(EngravingItem *)> func) {
    bool scan_all_time_sigs = (is_type(SegmentType::TIME_SIG_TYPES) &&
                               style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() != TimeSigPlacement::NORMAL);
    for (size_t track = 0; track < score()->nstaves() * ::mu::engraving::voices; ++track) {
        size_t staff_idx = track / ::mu::engraving::voices;
        bool this_measure_visible = measure()->visible(staff_idx) && score()->staff(staff_idx)->show();
        if (!scan_all_time_sigs && !this_measure_visible) {
            Measure *next_measure = measure()->next_measure();
            bool next_measure_visible = next_measure && next_measure->system() == measure()->system() && next_measure->visible(staff_idx) &&
                                        score()->staff(staff_idx)->show();
            if (!((is_end_bar_line_type() && (next_measure_visible || measure()->is_cutaway_clef(staff_idx))) ||
                  (is_clef_type() && measure()->is_cutaway_clef(staff_idx)))) {
                track += ::mu::engraving::voices - 1;
                continue;
            }
        }
        EngravingItem *e = element(track);
        if (e == 0) {
            continue;
        }
        e->scan_elements(func);
    }
    for (EngravingItem *e : annotations()) {
        if (!e->system_flag() && (!measure() || !measure()->visible(e->staff_idx()))) {
            continue;
        }
        e->scan_elements(func);
    }
}

//---------------------------------------------------------
//   firstElement
//   This function returns the first main element from a
//   segment, or a barline if it spanns in the staff
//---------------------------------------------------------

EngravingItem *Segment::first_element_for_navigation(staff_idx_t staff) const {
    if (is_chord_rest_type()) {
        track_idx_t strack = staff * ::mu::engraving::voices;
        track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t v = strack; v < etrack; ++v) {
            EngravingItem *el = element(v);
            if (!el) {
                continue;
            }
            return el->is_chord() ? to_chord(el)->notes().back() : el;
        }
    } else {
        return get_element(staff);
    }
    return 0;
}

//---------------------------------------------------------
//   lastElement
//   This function returns the last main element from a
//   segment, or a barline if it spanns in the staff
//---------------------------------------------------------

EngravingItem *Segment::last_element_for_navigation(staff_idx_t staff) const {
    if (segment_type() == SegmentType::CHORD_REST) {
        for (int voice = static_cast<int>(staff * ::mu::engraving::voices + (::mu::engraving::voices - 1));
             voice / static_cast<int>(::mu::engraving::voices) == static_cast<int>(staff); voice--) {
            EngravingItem *el = element(voice);
            if (!el) { //there is no chord or rest on this voice
                continue;
            }
            if (el->is_chord()) {
                return to_chord(el)->notes().front();
            } else {
                return el;
            }
        }
    } else {
        return get_element(staff);
    }

    return 0;
}

//---------------------------------------------------------
//   getElement
//   protected because it is used by the firstElement and
//   lastElement functions when segment types that have
//   just one element to avoid duplicated code
//
//   Use firstElement, or lastElement instead of this
//---------------------------------------------------------

EngravingItem *Segment::get_element(staff_idx_t staff) const {
    segment_type();
    if (segment_type() == SegmentType::CHORD_REST) {
        return first_element_for_navigation(staff);
    } else if (segment_type() & (SegmentType::END_BAR_LINE | SegmentType::BAR_LINE | SegmentType::START_REPEAT_BAR_LINE)) {
        for (int i = static_cast<int>(staff); i >= 0; i--) {
            if (!element(i * ::mu::engraving::voices)) {
                continue;
            }
            BarLine *b = to_bar_line(element(i * ::mu::engraving::voices));
            if (i + b->span_staff() >= static_cast<int>(staff)) {
                return element(i * ::mu::engraving::voices);
            }
        }
    } else {
        return element(staff * ::mu::engraving::voices);
    }
    return 0;
}

//---------------------------------------------------------
//   nextAnnotation
//   return next element in _annotations
//---------------------------------------------------------

EngravingItem *Segment::next_annotation(EngravingItem *e) const {
    if (annotations_.empty() || e == annotations_.back()) {
        return nullptr;
    }
    auto ei = std::find(annotations_.begin(), annotations_.end(), e);
    if (ei == annotations_.end()) {
        return nullptr; // element not found
    }
    // TODO: firstVisibleStaff() for system elements? see Spanner::nextSpanner()
    auto res_it = std::find_if(ei + 1, annotations_.end(),
                               [e](EngravingItem *next_elem) { return next_elem && next_elem->staff_idx() == e->staff_idx(); });

    return annotations_.end() == res_it ? nullptr : *res_it;
}

//---------------------------------------------------------
//   prevAnnotation
//   return previous element in _annotations
//---------------------------------------------------------

EngravingItem *Segment::prev_annotation(EngravingItem *e) const {
    if (e == annotations_.front()) {
        return nullptr;
    }
    auto reverse_it = std::find(annotations_.rbegin(), annotations_.rend(), e);
    if (reverse_it == annotations_.rend()) {
        return nullptr; // element not found
    }
    // TODO: firstVisibleStaff() for system elements? see Spanner::nextSpanner()
    auto res_it = std::find_if(reverse_it + 1, annotations_.rend(),
                               [e](EngravingItem *prev_elem) { return prev_elem && prev_elem->staff_idx() == e->staff_idx(); });

    return annotations_.rend() == res_it ? nullptr : *res_it;
}

//---------------------------------------------------------
//   firstAnnotation
//---------------------------------------------------------

EngravingItem *Segment::first_annotation(staff_idx_t active_staff) const {
    for (auto i = annotations_.begin(); i != annotations_.end(); ++i) {
        // TODO: firstVisibleStaff() for system elements? see Spanner::nextSpanner()
        if ((*i)->staff_idx() == active_staff) {
            return *i;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   lastAnnotation
//---------------------------------------------------------

EngravingItem *Segment::last_annotation(staff_idx_t active_staff) const {
    for (auto i = annotations_.rbegin(); i != annotations_.rend(); ++i) {
        // TODO: firstVisibleStaff() for system elements? see Spanner::nextSpanner()
        EngravingItem *e = *i;
        IF_ASSERT_FAILED(e) { continue; }
        if (e->staff_idx() == active_staff) {
            return e;
        }
    }
    return nullptr;
}

//--------------------------------------------------------
//   firstInNextSegments
//   Searches for the next segment that has elements on the
//   active staff and returns its first element
//
//   Uses firstElement so it also returns a barline if it
//   spans into the active staff
//--------------------------------------------------------

EngravingItem *Segment::first_in_next_segments(staff_idx_t active_staff) const {
    EngravingItem *re = 0;
    const Segment *seg = this;
    while (!re) {
        seg = seg->next1_m_menabled();
        if (!seg) { //end of staff, or score
            break;
        }

        re = seg->first_element_for_navigation(active_staff);
    }

    if (re) {
        return re;
    }

    if (!seg) {                                       //end of staff
        if (active_staff + 1 >= score()->nstaves()) { //end of score
            return 0;
        }
        seg = score()->first_segment_mm(SegmentType::ALL);
        return seg->element((active_staff + 1) * ::mu::engraving::voices);
    }
    return 0;
}

//---------------------------------------------------------
//   firstElementOfSegment
//   returns the first non null element
//---------------------------------------------------------

EngravingItem *Segment::first_element_of_segment(staff_idx_t active_staff) const {
    if (is_time_tick_type()) {
        return nullptr;
    }

    for (auto i : elist_) {
        if (i && i->staff_idx() == active_staff) {
            if (i->is_duration_element()) {
                DurationElement *de = to_duration_element(i);
                Tuplet *tuplet = de->tuplet();
                if (tuplet && de == tuplet->elements().front()) {
                    return tuplet;
                }
            }

            if (i->is_chord()) {
                Chord *chord = to_chord(i);
                return chord->first_grace_or_note();
            } else {
                return i;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   nextElementOfSegment
//   returns the next element after the given element
//---------------------------------------------------------

EngravingItem *Segment::next_element_of_segment(EngravingItem *e, staff_idx_t active_staff) const {
    for (size_t track = 0; track < score()->nstaves() * ::mu::engraving::voices - 1; ++track) {
        if (element(track) == 0) {
            continue;
        }
        EngravingItem *el = element(track);
        if (el == e) {
            EngravingItem *next = element(track + 1);
            while (track < score()->nstaves() * ::mu::engraving::voices - 1 && (!next || next->staff_idx() != active_staff)) {
                next = element(++track);
            }
            if (!next || next->staff_idx() != active_staff) {
                return nullptr;
            }
            if (next->is_chord()) {
                return to_chord(next)->notes().back();
            } else {
                return next;
            }
        }
        if (el->is_chord()) {
            std::vector<Note *> notes = to_chord(el)->notes();
            auto i = std::find(notes.begin(), notes.end(), e);
            if (i == notes.end()) {
                continue;
            }
            if (i != notes.begin()) {
                return *(i - 1);
            } else {
                EngravingItem *next_el = element(++track);
                while (track < score()->nstaves() * ::mu::engraving::voices - 1 && (!next_el || next_el->staff_idx() != active_staff)) {
                    next_el = element(++track);
                }
                if (!next_el || next_el->staff_idx() != active_staff) {
                    return nullptr;
                }
                if (next_el->is_chord()) {
                    return to_chord(next_el)->notes().back();
                }
                return next_el;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevElementOfSegment
//   returns the previous element before the given element
//---------------------------------------------------------

EngravingItem *Segment::prev_element_of_segment(EngravingItem *e, staff_idx_t active_staff) const {
    for (int track = static_cast<int>(score()->nstaves() * ::mu::engraving::voices) - 1; track > 0; --track) {
        if (element(track) == 0) {
            continue;
        }
        EngravingItem *el = element(track);
        if (el == e) {
            EngravingItem *prev = element(track - 1);
            while (track > 0 && (!prev || prev->staff_idx() != active_staff)) {
                prev = element(--track);
            }
            if (!prev) {
                return nullptr;
            }
            if (prev->staff_idx() == e->staff_idx()) {
                if (prev->is_chord()) {
                    return to_chord(prev)->notes().front();
                } else {
                    return prev;
                }
            }
            return nullptr;
        }
        if (el->is_chord()) {
            Chord *chord = to_chord(el);
            const auto grace_notes_before = chord->grace_notes_before();
            if (!grace_notes_before.empty()) {
                ChordRest *next = Navigation::prev_chord_rest(chord);
                if (next) {
                    if (next->is_chord()) {
                        return to_chord(next)->notes().back();
                    }
                    return to_rest(next);
                }
            }

            std::vector<Note *> notes = chord->notes();
            auto i = std::find(notes.begin(), notes.end(), e);
            if (i == notes.end()) {
                continue;
            }
            if (i != --notes.end()) {
                return *(i + 1);
            } else {
                EngravingItem *prev_el = element(--track);
                while (track > 0 && (!prev_el || prev_el->staff_idx() != active_staff)) {
                    prev_el = element(--track);
                }
                if (!prev_el) {
                    return nullptr;
                }
                if (prev_el->staff_idx() == e->staff_idx()) {
                    if (prev_el->is_chord()) {
                        return to_chord(prev_el)->notes().front();
                    }
                    return prev_el;
                }
                return nullptr;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   lastElementOfSegment
//   returns the last element
//---------------------------------------------------------

EngravingItem *Segment::last_element_of_segment(staff_idx_t active_staff) const {
    if (is_time_tick_type()) {
        return nullptr;
    }

    const std::vector<EngravingItem *> &elements = elist_;
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        EngravingItem *item = *it;
        if (item && item->staff_idx() == active_staff) {
            if (item->is_chord()) {
                Chord *chord = to_chord(item);
                if (!chord->grace_notes_after().empty()) {
                    return chord->grace_notes_after().back()->notes().back();
                }

                const std::vector<Articulation *> &articulations = chord->articulations();
                if (!articulations.empty()) {
                    Articulation *last_artic = articulations.back();
                    if (last_artic->is_tapping()) {
                        Tapping *tap = to_tapping(last_artic);
                        if (tap->half_slur_below()) {
                            return tap->half_slur_below()->front_segment();
                        } else if (tap->half_slur_above()) {
                            return tap->half_slur_above()->front_segment();
                        }
                    }
                    return last_artic;
                }
                return chord->up_note();
            }
            return item;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   firstSpanner
//---------------------------------------------------------

Spanner *Segment::first_spanner(staff_idx_t active_staff) const {
    std::multimap<int, Spanner *> mmap = score()->spanner();
    auto range = mmap.equal_range(tick().ticks());
    if (range.first != range.second) { // range not empty
        for (auto i = range.first; i != range.second; ++i) {
            Spanner *s = i->second;
            if (s->segments_empty()) {
                continue;
            }

            EngravingItem *e = s->start_element();
            if (!e) {
                continue;
            }
            if (s->start_segment() == this) {
                if (e->staff_idx() == active_staff || (e->is_measure() && active_staff == 0)) {
                    return s;
                }
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   lastSpanner
//---------------------------------------------------------

Spanner *Segment::last_spanner(staff_idx_t active_staff) const {
    std::multimap<int, Spanner *> mmap = score()->spanner();
    auto range = mmap.equal_range(tick().ticks());
    if (range.first != range.second) { // range not empty
        for (auto i = --range.second;; --i) {
            Spanner *s = i->second;
            if (s->segments_empty()) {
                continue;
            }

            EngravingItem *e = s->start_element();
            if (!e) {
                continue;
            }
            if (s->start_segment() == this) {
                if (e->staff_idx() == active_staff || (e->is_measure() && active_staff == 0)) {
                    return s;
                }
            }
            if (i == range.first) {
                break;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   notChordRestType
//---------------------------------------------------------

bool Segment::not_chord_rest_type() const { return segment_type() != SegmentType::CHORD_REST; }

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *Segment::next_element(staff_idx_t active_staff) {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }
    if (!e) {
        return nullptr;
    }
    switch (e->type()) {
        case ElementType::DYNAMIC:
        case ElementType::HARMONY:
        case ElementType::EXPRESSION:
        case ElementType::SYMBOL:
        case ElementType::FERMATA:
        case ElementType::FRET_DIAGRAM:
        case ElementType::TEMPO_TEXT:
        case ElementType::STAFF_TEXT:
        case ElementType::STAVE_SHARING_LABEL:
        case ElementType::SYSTEM_TEXT:
        case ElementType::TRIPLET_FEEL:
        case ElementType::PLAYTECH_ANNOTATION:
        case ElementType::CAPO:
        case ElementType::STRING_TUNINGS:
        case ElementType::REHEARSAL_MARK:
        case ElementType::MARKER:
        case ElementType::IMAGE:
        case ElementType::TEXT:
        case ElementType::TREMOLOBAR:
        case ElementType::TAB_DURATION_SYMBOL:
        case ElementType::FIGURED_BASS:
        case ElementType::STAFF_STATE:
        case ElementType::INSTRUMENT_CHANGE:
        case ElementType::HARP_DIAGRAM:
        case ElementType::STICKING:
        case ElementType::TUPLET: {
            EngravingItem *next = nullptr;
            if (e->ownership_parent() == this) {
                next = next_annotation(e);

                if (next && next->is_fret_diagram()) {
                    FretDiagram *fret_diagram = to_fret_diagram(next);
                    if (fret_diagram->harmony()) {
                        next = fret_diagram->harmony();
                    }
                }
            }
            if (next) {
                return next;
            } else {
                Spanner *s = first_spanner(active_staff);
                if (s) {
                    return s->spanner_segments().front();
                }
            }
            Segment *next_segment = this->next1_m_menabled();
            while (next_segment) {
                EngravingItem *next_el = next_segment->first_element_of_segment(active_staff);
                if (next_el) {
                    return next_el;
                }

                if (EngravingItem *annotation = next_segment->first_annotation(active_staff)) {
                    if (annotation && annotation->is_fret_diagram()) {
                        FretDiagram *fret_diagram = to_fret_diagram(annotation);
                        if (fret_diagram->harmony()) {
                            annotation = fret_diagram->harmony();
                        }
                    }

                    return annotation;
                }

                next_segment = next_segment->next1_m_menabled();
            }
            break;
        }
        case ElementType::SEGMENT: {
            if (!annotations_.empty()) {
                EngravingItem *next = first_annotation(active_staff);
                if (next) {
                    if (next->is_fret_diagram()) {
                        if (Harmony *harmony = to_fret_diagram(next)->harmony()) {
                            return harmony;
                        }
                    }

                    return next;
                }
            }
            Spanner *sp = first_spanner(active_staff);
            if (sp) {
                return sp->spanner_segments().front();
            }

            Segment *next_segment = this->next1_m_menabled();
            while (next_segment) {
                if (EngravingItem *annotation = next_segment->first_annotation(active_staff)) {
                    return annotation;
                }

                EngravingItem *next_el = next_segment->first_element_of_segment(active_staff);
                if (next_el) {
                    return next_el;
                }
                next_segment = next_segment->next1_m_menabled();
            }
            break;
        }
        default: {
            EngravingItem *p;
            if (e->is_tie_segment() || e->is_glissando_segment() || e->is_guitar_bend_segment()) {
                SpannerSegment *s = to_spanner_segment(e);
                Spanner *sp = s->spanner();
                p = sp->start_element();
            } else {
                p = e;
                EngravingItem *pp = p->ownership_parent_item();
                if (pp->is_note() || pp->is_rest() || (pp->is_chord() && !p->is_note())) {
                    p = pp;
                }
            }
            EngravingItem *el = p;
            for (; p && !p->is_segment(); p = p->layout_parent()) {}
            Segment *seg = to_segment(p);
            // next in _elist
            EngravingItem *next_el = seg->next_element_of_segment(el, active_staff);
            if (next_el) {
                return next_el;
            }
            if (!annotations_.empty()) {
                EngravingItem *next = seg->first_annotation(active_staff);
                if (next) {
                    if (next->is_staff_text()) {
                        if (SoundFlag *sound_flag = to_staff_text(next)->sound_flag()) {
                            return sound_flag;
                        }
                    }
                    if (next->is_fret_diagram()) {
                        if (Harmony *harmony = to_fret_diagram(next)->harmony()) {
                            return harmony;
                        }
                    }

                    return next;
                }
            }
            Spanner *s = first_spanner(active_staff);
            if (s) {
                return s->spanner_segments().front();
            }

            for (SpannerSegment *spanner_seg : system()->spanner_segments()) {
                if (spanner_seg->staff_idx() == active_staff && spanner_seg->is_hammer_on_pull_off_segment()) {
                    for (HammerOnPullOffText *hopo_text : to_hammer_on_pull_off_segment(spanner_seg)->hopo_text()) {
                        if (hopo_text->start_chord() && hopo_text->start_chord()->segment() == this) {
                            return hopo_text;
                        }
                    }
                }
            }

            Segment *next_segment = seg->next1_m_menabled();
            for (; next_segment && next_segment->is_time_tick_type(); next_segment = next_segment->next1_m_menabled()) {
                if (EngravingItem *annotation = next_segment->first_annotation(active_staff)) {
                    return annotation;
                }
                if (Spanner *spanner = next_segment->first_spanner(active_staff)) {
                    return spanner->spanner_segments().front();
                }
            }
            if (!next_segment) {
                MeasureBase *mb = measure()->next();
                return mb && mb->is_box() ? mb : Navigation::last_element(score());
            }

            Measure *nsm = next_segment->measure();
            if (nsm != measure()) {
                // check for frame, measure elements
                MeasureBase *nmb = measure()->next_mm();
                EngravingItem *nme = nsm->el().empty() ? nullptr : nsm->el().front();
                if (nsm != nmb) {
                    return nmb;
                } else if (nme && nme->is_text_base() && nme->staff_idx() == e->staff_idx()) {
                    return nme;
                } else if (nme && nme->is_layout_break() && e->staff_idx() == 0) {
                    return nme;
                } else if (nmb->is_end_of_system_lock()) {
                    System *system = nmb->system();
                    if (system) {
                        for (SystemLockIndicator *lock_ind : system->system_lock_indicators()) {
                            if (nmb->system_lock() == lock_ind->system_lock()) {
                                return lock_ind;
                            }
                        }
                    }
                } else if (nmb->is_end_of_page_lock()) {
                    System *system = nmb->system();
                    PageLockIndicator *pli = system ? system->page_lock_indicator() : nullptr;
                    if (pli) {
                        return pli;
                    }
                } //TODO: StaffVisibilityIndicator handling
            }

            while (next_segment) {
                next_el = next_segment->first_element_of_segment(active_staff);
                if (next_el) {
                    return next_el;
                }

                if (EngravingItem *annotation = next_segment->first_annotation(active_staff)) {
                    return annotation;
                }

                next_segment = next_segment->next1_m_menabled();
            }
        } break;
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *Segment::prev_element(staff_idx_t active_staff) {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().back();
    }
    if (!e) {
        return nullptr;
    }
    switch (e->type()) {
        case ElementType::DYNAMIC:
        case ElementType::EXPRESSION:
        case ElementType::HARMONY:
        case ElementType::SYMBOL:
        case ElementType::FERMATA:
        case ElementType::FRET_DIAGRAM:
        case ElementType::TEMPO_TEXT:
        case ElementType::STAFF_TEXT:
        case ElementType::STAVE_SHARING_LABEL:
        case ElementType::SOUND_FLAG:
        case ElementType::SYSTEM_TEXT:
        case ElementType::TRIPLET_FEEL:
        case ElementType::PLAYTECH_ANNOTATION:
        case ElementType::CAPO:
        case ElementType::STRING_TUNINGS:
        case ElementType::REHEARSAL_MARK:
        case ElementType::MARKER:
        case ElementType::IMAGE:
        case ElementType::TEXT:
        case ElementType::TREMOLOBAR:
        case ElementType::TAB_DURATION_SYMBOL:
        case ElementType::FIGURED_BASS:
        case ElementType::STAFF_STATE:
        case ElementType::INSTRUMENT_CHANGE:
        case ElementType::HARP_DIAGRAM:
        case ElementType::STICKING: {
            if (e->is_staff_text()) {
                if (SoundFlag *sound_flag = to_staff_text(e)->sound_flag()) {
                    return sound_flag;
                }
            }

            EngravingItem *prev_annotation = nullptr;
            if (e->ownership_parent() == this) {
                prev_annotation = this->prev_annotation(e);
            }
            if (prev_annotation) {
                return prev_annotation;
            } else if (const Segment *prev_seg = prev1()) {
                const Spanner *s = prev_seg->last_spanner(active_staff);
                if (s) {
                    return s->spanner_segments().back();
                }
            }

            if (not_chord_rest_type()) {
                EngravingItem *last_el = last_element_of_segment(active_staff);
                if (last_el) {
                    return last_el;
                }
            }
            if (is_time_tick_type()) {
                Segment *prev_seg = prev1_m_menabled();
                return prev_seg ? prev_seg->last_element_of_segment(active_staff) : nullptr;
            }
            track_idx_t track = score()->nstaves() * ::mu::engraving::voices - 1;
            Segment *s = this;
            EngravingItem *el = s->element(track);
            while (track > 0 && (!el || el->staff_idx() != active_staff)) {
                if (s != this) {
                    if (EngravingItem *annotation = s->last_annotation(active_staff)) {
                        return annotation;
                    }
                }

                el = s->element(--track);

                if (track == 0) {
                    track = score()->nstaves() * ::mu::engraving::voices - 1;
                    s = s->prev1_m_menabled();
                }
            }
            if (el->staff_idx() != active_staff) {
                return nullptr;
            }
            if (el->is_chord_rest()) {
                ChordRest *cr = this->cr(el->track());
                if (cr) {
                    EngravingItem *el_cr = cr->last_element_before_segment();
                    if (el_cr) {
                        return el_cr;
                    }
                }
            }
            if (el->is_chord()) {
                return to_chord(el)->last_element_before_segment();
            } else if (el->is_note()) {
                Chord *c = to_note(el)->chord();
                return c->last_element_before_segment();
            } else {
                return el;
            }
        }
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET:
        case ElementType::TREMOLO_SINGLECHORD:
        case ElementType::TREMOLO_TWOCHORD: {
            EngravingItem *el = this->element(e->track());
            assert(el->is_chord());
            return to_chord(el)->prev_element();
        }
        default: {
            EngravingItem *el = e;
            Segment *seg = this;
            if (e->is_tie_segment() || e->is_glissando_segment() || e->is_note_line_segment()) {
                SpannerSegment *s = to_spanner_segment(e);
                Spanner *sp = s->spanner();
                el = sp->start_element();
                seg = sp->start_segment();
            } else {
                EngravingItem *ep = e->ownership_parent_item();
                if (ep->is_note() || ep->is_rest() || (ep->is_chord() && !e->is_note())) {
                    el = e->ownership_parent_item();
                }
            }

            EngravingItem *prev = seg->prev_element_of_segment(el, active_staff);
            if (prev) {
                if (prev->is_chord_rest()) {
                    ChordRest *cr = seg->cr(prev->track());
                    if (cr) {
                        EngravingItem *el_cr = cr->last_element_before_segment();
                        if (el_cr) {
                            return el_cr;
                        }
                    }
                }
                if (prev->is_chord()) {
                    return to_chord(prev)->last_element_before_segment();
                } else if (prev->is_note()) {
                    Chord *c = to_note(prev)->chord();
                    return c->last_element_before_segment();
                } else {
                    return prev;
                }
            } else {
                System *system = seg->system();
                for (SpannerSegment *spanner_seg : system->spanner_segments()) {
                    if (spanner_seg->staff_idx() == active_staff && spanner_seg->is_hammer_on_pull_off_segment()) {
                        for (HammerOnPullOffText *hopo_text : to_hammer_on_pull_off_segment(spanner_seg)->hopo_text()) {
                            if (hopo_text->end_chord() && hopo_text->end_chord()->segment() == seg) {
                                return hopo_text;
                            }
                        }
                    }
                }
            }

            Segment *prev_seg = seg->prev1_m_menabled();
            for (; prev_seg && prev_seg->is_time_tick_type(); prev_seg = prev_seg->prev1_m_menabled()) {
                if (Spanner *spanner = prev_seg->last_spanner(active_staff)) {
                    return spanner->spanner_segments().front();
                }
                if (EngravingItem *annotation = prev_seg->last_annotation(active_staff)) {
                    return annotation;
                }
            }
            if (!prev_seg) {
                MeasureBase *mb = measure()->prev();
                return mb && mb->is_box() ? mb : Navigation::first_element(score());
            }

            Measure *psm = prev_seg->measure();
            if (psm != measure()) {
                // check for frame, measure elements
                MeasureBase *pmb = measure()->prev_mm();
                EngravingItem *me = measure()->el().empty() ? nullptr : measure()->el().back();
                if (me && me->is_text_base() && me->staff_idx() == e->staff_idx()) {
                    return me;
                } else if (me && me->is_layout_break() && e->staff_idx() == 0) {
                    return me;
                } else if (measure()->is_end_of_page_lock()) {
                    System *system = measure()->system();
                    PageLockIndicator *pli = system ? system->page_lock_indicator() : nullptr;
                    if (pli) {
                        return pli;
                    }
                } else if (measure()->is_end_of_system_lock()) {
                    System *system = measure()->system();
                    if (system) {
                        for (SystemLockIndicator *lock_ind : system->system_lock_indicators()) {
                            if (measure()->system_lock() == lock_ind->system_lock()) {
                                return lock_ind;
                            }
                        }
                    }
                } else if (psm != pmb) {
                    return pmb;
                } //TODO: StaffVisibilityIndicator handling
            }

            if (EngravingItem *annotation = prev_seg->last_annotation(active_staff)) {
                return annotation;
            }
            if (!prev) {
                prev = prev_seg->last_element_of_segment(active_staff);
            }

            while (!prev && prev_seg) {
                prev_seg = prev_seg->prev1_m_menabled();

                if (EngravingItem *annotation = prev_seg->last_annotation(active_staff)) {
                    prev = annotation;
                } else {
                    prev = prev_seg->last_element_of_segment(active_staff);
                }
            }
            if (!prev_seg) {
                return Navigation::first_element(score());
            }

            if (prev_seg->not_chord_rest_type()) {
                EngravingItem *last_el = prev_seg->last_element_of_segment(active_staff);
                if (last_el) {
                    return last_el;
                }
            }
            Spanner *s1 = prev_seg->last_spanner(active_staff);
            if (s1) {
                return s1->spanner_segments().front();
            } else if (!prev_seg->annotations().empty()) {
                EngravingItem *next = prev_seg->last_annotation(active_staff);
                if (next) {
                    return next;
                }
            }
            if (prev->is_chord_rest() || prev->is_note()) {
                ChordRest *cr = prev_seg->cr(prev->track());
                if (cr) {
                    EngravingItem *el_cr = cr->last_element_before_segment();
                    if (el_cr) {
                        return el_cr;
                    }
                }
            }
            if (prev->is_chord()) {
                return to_chord(prev)->last_element_before_segment();
            } else if (prev->is_note()) {
                Chord *c = to_note(prev)->chord();
                return c->last_element_before_segment();
            } else {
                return prev;
            }
        }
    }
}

EngravingItem *Segment::first_element(staff_idx_t staff_idx) const {
    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    for (track_idx_t track = start_track; track < end_track; ++track) {
        if (EngravingItem *item = elist_[track]) {
            return item;
        }
    }
    return nullptr;
}

//--------------------------------------------------------
//   lastInPrevSegments
//   Searches for the previous segment that has elements on
//   the active staff and returns its last element
//
//   Uses lastElement so it also returns a barline if it
//   spans into the active staff
//--------------------------------------------------------

EngravingItem *Segment::last_in_prev_segments(staff_idx_t active_staff) const {
    EngravingItem *re = 0;
    const Segment *seg = this;

    while (!re) {
        seg = seg->prev1_m_menabled();
        if (!seg) { //end of staff, or score
            break;
        }

        re = seg->last_element_of_segment(active_staff);
    }

    if (re) {
        return re;
    }

    if (!seg) {                  //end of staff
        if (active_staff == 0) { //end of score
            return 0;
        }

        re = 0;
        seg = score()->last_segment_mm();
        while (true) {
            //if (seg->segmentType() == SegmentType::EndBarLine)
            //      score()->inputState().setTrack((activeStaff - 1) * VOICES ); //correction

            if ((re = seg->last_element_for_navigation(active_staff - 1)) != 0) {
                return re;
            }

            seg = seg->prev1_m_menabled();
        }
    }

    return 0;
}

//---------------------------------------------------------
//   accessibleExtraInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   createShapes
//---------------------------------------------------------

void Segment::create_shapes() {
    set_visible(false);
    for (size_t staff_idx = 0; staff_idx < score()->nstaves(); ++staff_idx) {
        create_shape(staff_idx);
    }
}

//---------------------------------------------------------
//   createShape
//---------------------------------------------------------

void Segment::create_shape(staff_idx_t staff_idx) {
    Shape &s = shapes_[staff_idx];
    s.clear();

    if (const System *system = this->system()) {
        const std::vector<SysStaff *> &staves = system->staves();

        if (staff_idx < staves.size() && !staves[staff_idx]->show()) {
            return;
        }
    }

    if (!score()->staff(staff_idx)->show()) {
        return;
    }

    track_idx_t strack = staff_idx * ::mu::engraving::voices;
    track_idx_t etrack = strack + ::mu::engraving::voices;
    for (EngravingItem *e : elist_) {
        if (!e) {
            continue;
        }
        track_idx_t effective_track = e->v_staff_idx() * ::mu::engraving::voices + e->voice();
        if (effective_track >= strack && effective_track < etrack) {
            set_visible(true);
            if (e->is_rest() && to_rest(e)->is_full_measure_rest() && measure()->has_voices(e->staff_idx())) {
                // A full measure rest in a measure with multiple voices must be ignored
                // Unless the measure is made of *only* full rests
                bool is_all_full_rests = true;
                for (track_idx_t track = strack; track < etrack; ++track) {
                    EngravingItem *element = elist_[track];
                    if (element && !(element->is_rest() && to_rest(element)->is_full_measure_rest())) {
                        is_all_full_rests = false;
                        break;
                    }
                }
                if (!is_all_full_rests) {
                    continue;
                }
            }
            if (e->is_mm_rest() || (e->is_measure_repeat() && to_measure_repeat(e)->num_measures() > 1)) {
                continue;
            }
            if (e->add_to_skyline() || (e->is_chord_rest() && to_chord_rest(e)->lyrics().size() > 0)) {
                s.add(e->shape().translate((e->is_clef() ? e->ldata()->pos() : e->pos()) + e->staff_offset()));
            }
            // Non-standard trills display a cue note that we must add to shape here
            if (e->is_chord()) {
                add_articulations_to_shape(to_chord(e), s);
            }
        }
    }

    for (EngravingItem *e : annotations_) {
        if (!e || e->staff_idx() != staff_idx) {
            continue;
        }

        set_visible(true);

        // Still include harmony attached to fret diagram
        if (e->is_fret_diagram() && !e->add_to_skyline() && to_fret_diagram(e)->harmony()) {
            e = to_harmony(to_fret_diagram(e)->harmony());
        }

        if (!e->add_to_skyline()) {
            continue;
        }

        if (e->is_harmony() || e->is_fret_diagram()) {
            // TODO: eliminate once and for all this addHorizontalSpace hack [M.S.]
            PointF pos = e->pos();
            // Harmony attached to invisible fret diagram
            const EngravingItem *parent = e->layout_parent();
            if (e->is_harmony() && parent && parent->is_fret_diagram()) {
                pos += parent->pos();
            }
            RectF bbox = e->ldata()->bbox().translated(pos);
            if (Parenthesis *p = e->left_paren()) {
                bbox.unite(p->ldata()->bbox().translated(p->pos() + pos));
            }
            if (Parenthesis *p = e->right_paren()) {
                bbox.unite(p->ldata()->bbox().translated(p->pos() + pos));
            }
            s.add_horizontal_spacing(e, bbox.left(), bbox.right());
            if (e->is_fret_diagram()) {
                if (Harmony *harmony = to_fret_diagram(e)->harmony()) {
                    RectF harm_bbox = harmony->ldata()->bbox().translated(harmony->pos() + e->pos());
                    if (Parenthesis *p = harmony->left_paren()) {
                        harm_bbox.unite(p->ldata()->bbox().translated(p->pos() + harmony->pos() + e->pos()));
                    }
                    if (Parenthesis *p = harmony->right_paren()) {
                        harm_bbox.unite(p->ldata()->bbox().translated(p->pos() + harmony->pos() + e->pos()));
                    }
                    s.add_horizontal_spacing(harmony, harm_bbox.left(), harm_bbox.right());
                }
            }
        } else if (!e->is_rehearsal_mark() && !e->is_fret_diagram() && !e->is_harmony() && !e->is_tempo_text() && !e->is_dynamic() &&
                   !e->is_expression() && !e->is_figured_bass() && !e->is_symbol() && !e->is_f_symbol() && !e->is_system_text() &&
                   !e->is_triplet_feel() && !e->is_instrument_change() && !e->is_articulation_family() && !e->is_staff_text() &&
                   !e->is_harp_pedal_diagram() && !e->is_play_tech_annotation() && !e->is_capo() && !e->is_string_tunings() &&
                   !e->is_play_count_text()) {
            // annotations added here are candidates for collision detection
            s.add(e->shape().translate(e->pos() + e->staff_offset()));
        }
    }

    for (track_idx_t track = strack; track < etrack; ++track) {
        if (!pre_appended_items_[track]) {
            continue;
        }
        EngravingItem *item = pre_appended_items_[track];
        if (item->is_grace_notes_group()) {
            to_grace_notes_group(item)->add_to_shape();
        } else {
            Shape &shape = shapes_[item->v_staff_idx()];
            shape.add(item->shape().translate(item->pos() + item->staff_offset()));
        }
    }
}

void Segment::add_articulations_to_shape(const Chord *chord, Shape &shape) {
    auto add_tapping_half_slur_to_shape = [&](TappingHalfSlur *slur) {
        IF_ASSERT_FAILED(!slur->segments_empty()) { return; }
        TappingHalfSlurSegment *slur_seg = to_tapping_half_slur_segment(slur->front_segment());
        Shape slur_seg_shape = slur_seg->shape();
        // Semi-hack: we don't know the exact position at this stage, but we know that it
        // must end approx on the center of the notehead
        Note *note = slur->up() ? chord->up_note() : chord->down_note();
        double approx_pos_x = chord->x() + note->x() + 0.5 * note->head_width() - slur_seg_shape.right();
        slur_seg_shape.translate_x(approx_pos_x);
        shape.add(slur_seg_shape);
    };

    for (Articulation *art : chord->articulations()) {
        if (art->is_ornament() || !art->add_to_skyline()) {
            continue;
        }
        shape.add(art->shape().translated(art->pos() + chord->pos()));
        if (art->is_tapping()) {
            if (TappingHalfSlur *half_slur = to_tapping(art)->half_slur_above()) {
                add_tapping_half_slur_to_shape(half_slur);
            }
            if (TappingHalfSlur *half_slur = to_tapping(art)->half_slur_below()) {
                add_tapping_half_slur_to_shape(half_slur);
            }
        }
    }

    Ornament *ornament = chord->find_ornament();
    if (ornament) {
        Chord *cue_note_chord = ornament->cue_note_chord();
        if (cue_note_chord && cue_note_chord->up_note()->add_to_skyline()) {
            shape.add(cue_note_chord->shape().translate(cue_note_chord->pos() + cue_note_chord->staff_offset()));
        }
    }
}

//---------------------------------------------------------
//   minRight
//    calculate minimum distance needed to the right
//---------------------------------------------------------

double Segment::min_right() const {
    double distance = 0.0;
    for (Shape sh : shapes()) {
        sh.remove_if([](ShapeElement &el) { return el.item() && el.item()->is_articulation_or_fermata(); });
        distance = std::max(distance, sh.right());
    }
    return distance;
}

double Segment::min_left() const {
    double distance = DBL_MAX;
    for (Shape sh : shapes()) {
        sh.remove_if([](ShapeElement &el) { return el.item() && el.item()->is_articulation_or_fermata(); });
        double l = sh.left();
        if (l < distance) {
            distance = l;
        }
    }
    return distance != DBL_MAX ? -distance : 0.0;
}

void Segment::set_spacing(double val) { spacing_ = val; }

double Segment::spacing() const { return spacing_; }

bool Segment::has_time_sig_above_staves() const {
    return is_type(SegmentType::TIME_SIG_TYPES) &&
           style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() == TimeSigPlacement::ABOVE_STAVES;
}

bool Segment::make_space_for_time_sig_above_staves() const {
    bool make_space = style().style_b(Sid::TIME_SIG_CENTER_ON_BARLINE) ?
                          style().style_v(Sid::TIME_SIG_VS_MARGIN_CENTERED).value<TimeSigVSMargin>() == TimeSigVSMargin::CREATE_SPACE :
                          style().style_v(Sid::TIME_SIG_VS_MARGIN_NON_CENTERED).value<TimeSigVSMargin>() == TimeSigVSMargin::CREATE_SPACE;
    return has_time_sig_above_staves() && make_space;
}

bool Segment::has_time_sig_across_staves() const {
    return is_type(SegmentType::TIME_SIG_TYPES) &&
           style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() == TimeSigPlacement::ACROSS_STAVES;
}

bool Segment::can_write_spanner_start_end(track_idx_t track, const Spanner *spanner) const {
    staff_idx_t staff_idx = track2staff(track);
    if (is_chord_rest_type() && (element(track) || (!spanner->is_voice_specific() && has_elements(staff_idx)))) {
        return true;
    }

    if (is_time_tick_type()) {
        Segment *cr_seg_at_same_tick =
            score()->tick2segment(tick(), true, SegmentType::CHORD_REST, style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS));
        if (!cr_seg_at_same_tick || !cr_seg_at_same_tick->can_write_spanner_start_end(track, spanner)) {
            return true;
        }
    }

    return score()->last_measure()->last() == this;
}

double Segment::elements_top_offset_from_skyline(staff_idx_t staff_index) const {
    System *segment_system = measure()->system();
    SysStaff *staff_system = segment_system ? segment_system->staff(staff_index) : nullptr;

    if (!staff_system) {
        return 0;
    }

    SkylineLine north = staff_system->skyline().north();
    int top_offset = INT_MAX;
    for (const ShapeElement &element : north.elements()) {
        Segment *seg = prev1enabled();
        if (!seg) {
            continue;
        }
        bool ok = seg->page_pos().x() <= element.left() && element.left() <= page_pos().x();
        if (!ok) {
            continue;
        }

        if (element.top() < top_offset) {
            top_offset = element.top();
        }
    }

    if (top_offset == INT_MAX) {
        top_offset = 0;
    }

    return top_offset;
}

double Segment::elements_bottom_offset_from_skyline(staff_idx_t staff_index) const {
    System *segment_system = measure()->system();
    SysStaff *staff_system = segment_system ? segment_system->staff(staff_index) : nullptr;

    if (!staff_system) {
        return 0;
    }

    SkylineLine south = staff_system->skyline().south();
    int bottom_offset = INT_MIN;
    for (const ShapeElement &element : south.elements()) {
        Segment *seg = prev1enabled();
        if (!seg) {
            continue;
        }
        bool ok = seg->page_pos().x() <= element.left() && element.left() <= page_pos().x();
        if (!ok) {
            continue;
        }

        if (element.bottom() > bottom_offset) {
            bottom_offset = element.bottom();
        }
    }

    if (bottom_offset == INT_MIN) {
        bottom_offset = staff_system->bbox().height();
    }

    return bottom_offset;
}

//------------------------------------------------------
// shortestChordRest()
// returns the shortest chordRest of a segment. IMPORTANT:
// this is not the same as the ticks() of the segment. The
// actual duration of the segment may be shorter than its
// shortest chordRest. Invisible chordRests are ignored.
//------------------------------------------------------
Fraction Segment::shortest_chord_rest() const {
    Fraction shortest = measure()->ticks(); // Initializing at the highest possible value ( = time signature of the measure)
    Fraction cur = measure()->ticks();
    for (auto elem : elist()) {
        if (!elem || !elem->staff()->show() || !elem->is_chord_rest() || (elem->is_rest() && to_rest(elem)->is_gap()) ||
            (!elem->visible() &&
             measure()->has_voices(elem->staff_idx(), measure()->tick(), measure()->ticks(), /*considerInvisible*/ true))) {
            continue;
        }
        cur = to_chord_rest(elem)->actual_ticks();
        assert(cur.is_not_zero());
        if (cur < shortest) {
            shortest = cur;
        }
    }
    return shortest;
}

bool Segment::has_accidentals() const {
    if (!is_chord_rest_type()) {
        return false;
    }
    for (EngravingItem *e : elist()) {
        if (!e || !e->is_chord() || (e->staff() && !e->staff()->show())) {
            continue;
        }
        for (Note *note : to_chord(e)->notes()) {
            if (note->accidental() && note->accidental()->add_to_skyline()) {
                return true;
            }
        }
    }
    return false;
}

bool Segment::goes_before(const Segment *next_segment) const {
    Measure *meas = measure();
    Measure *prev_measure = meas->prev_measure();
    const bool this_measure_is_start_repeat = meas->repeat_start();
    const bool prev_measure_is_end_repeat = prev_measure ? prev_measure->repeat_end() : false;

    bool this_is_clef = is_clef_type();
    bool next_is_clef = next_segment->is_clef_type();
    bool this_is_barline = is_type(SegmentType::BAR_LINE | SegmentType::END_BAR_LINE | SegmentType::START_REPEAT_BAR_LINE);
    bool next_is_barline = next_segment->is_type(SegmentType::BAR_LINE | SegmentType::END_BAR_LINE | SegmentType::START_REPEAT_BAR_LINE);

    bool this_is_start_repeat = is_start_repeat_bar_line_type();
    bool next_is_start_repeat = next_segment->is_start_repeat_bar_line_type();
    bool this_is_key_sig = is_key_sig_type();
    bool this_is_time_sig = is_time_sig_type();
    bool next_is_key_sig = next_segment->is_key_sig_type();
    bool next_is_time_sig = next_segment->is_time_sig_type();

    // Place non header clefs AFTER header clefs, key signatures, time signatures
    const bool first_system_measure = meas->find_segment_r(SegmentType::HEADER_CLEF, Fraction(0, 1));
    bool this_is_header = is_header_clef_type() || (rtick() == Fraction(0, 1) && first_system_measure && !this_is_clef);
    bool next_is_header =
        next_segment->is_header_clef_type() || (next_segment->rtick() == Fraction(0, 1) && first_system_measure && !next_is_clef);

    bool this_is_end_of_measure = end_of_measure_change();

    // Place segments in correct place when "Place all changes before the barline" is enabled
    if (this_is_end_of_measure && next_segment->tick() != meas->end_tick()) {
        return false;
    }

    if (this_is_end_of_measure && next_is_barline) {
        return true;
    }

    // Place key signatures and time signatures in correct place when "Allow changes between end-start repeats" is enabled
    if ((this_is_key_sig || this_is_time_sig || this_is_clef) && next_is_start_repeat && this_measure_is_start_repeat &&
        prev_measure_is_end_repeat) {
        return style().style_b(Sid::CHANGES_BETWEEN_END_START_REPEAT);
    }
    if ((next_is_key_sig || next_is_time_sig || next_is_clef) && this_is_start_repeat && this_measure_is_start_repeat &&
        prev_measure_is_end_repeat) {
        return !style().style_b(Sid::CHANGES_BETWEEN_END_START_REPEAT);
    }

    if (this_is_clef && next_is_barline) {
        ClefToBarlinePosition clef_pos = ClefToBarlinePosition::AUTO;
        for (EngravingItem *item : elist()) {
            if (item && item->is_clef()) {
                clef_pos = to_clef(item)->clef_to_barline_position();
                break;
            }
        }
        return clef_pos != ClefToBarlinePosition::AFTER;
    }

    if (this_is_barline && next_is_clef) {
        ClefToBarlinePosition clef_pos = ClefToBarlinePosition::AUTO;
        for (EngravingItem *item : next_segment->elist()) {
            if (item && item->is_clef()) {
                clef_pos = to_clef(item)->clef_to_barline_position();
                break;
            }
        }
        return clef_pos == ClefToBarlinePosition::AFTER;
    }

    if (this_is_clef && (next_is_key_sig || next_is_time_sig) && rtick() == Fraction(0, 1) && next_segment->rtick() == Fraction(0, 1) &&
        !next_is_header && this_measure_is_start_repeat && prev_measure_is_end_repeat) {
        // Between repeats
        return true;
    }

    if ((this_is_key_sig || this_is_time_sig) && next_is_clef && rtick() == Fraction(0, 1) && next_segment->rtick() == Fraction(0, 1) &&
        !this_is_header && this_measure_is_start_repeat && prev_measure_is_end_repeat) {
        // Between repeats
        return false;
    }

    return segment_type() < next_segment->segment_type();
}
} // namespace mu::engraving
