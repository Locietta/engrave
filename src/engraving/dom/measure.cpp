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

/**
 \file
 Implementation of most part of class Measure.
*/

#include "measure.h"

#include "../editing/editclef.h"
#include "../editing/editkeysig.h"
#include "../editing/editmeasurerepeat.h"
#include "../editing/editmeasures.h"
#include "../editing/editstaff.h"
#include "../editing/editstaffbrackets.h"
#include "../editing/editsystemlocks.h"
#include "../editing/edittimesig.h"
#include "../editing/inserttime.h"
#include "../editing/navigation.h"
#include "../editing/transaction/transaction.h"
#include "accidental.h"
#include "anchors.h"
#include "barline.h"
#include "beam.h"
#include "bracket.h"
#include "bracketitem.h"
#include "chord.h"
#include "clef.h"
#include "durationelement.h"
#include "factory.h"
#include "hook.h"
#include "key.h"
#include "keysig.h"
#include "linkedobjects.h"
#include "log.h"
#include "marker.h"
#include "masterscore.h"
#include "measure.h"
#include "measurenumber.h"
#include "measurerepeat.h"
#include "mmrestrange.h"
#include "note.h"
#include "page.h"
#include "part.h"
#include "pitchspelling.h"
#include "playcounttext.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "select.h"
#include "sig.h"
#include "spacer.h"
#include "staff.h"
#include "stafflines.h"
#include "stafftype.h"
#include "stafftypechange.h"
#include "stem.h"
#include "system.h"
#include "tie.h"
#include "tiemap.h"
#include "timesig.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "tuplet.h"
#include "tupletmap.h"
#include "utils.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   MStaff
///   Per staff values of measure.
//---------------------------------------------------------

MStaff::~MStaff() {
    delete measure_number_;
    delete mm_range_text_;
    delete lines_;
    delete vspacer_up_;
    delete vspacer_down_;
}

MStaff::MStaff(const MStaff &m) {
    measure_number_ = 0;
    mm_range_text_ = 0;
    lines_ = Factory::copy_staff_lines(*m.lines_);
    has_voices_ = m.has_voices_;
    vspacer_up_ = 0;
    vspacer_down_ = 0;
    hide_if_empty_ = m.hide_if_empty_;
    visible_ = m.visible_;
    stemless_ = m.stemless_;
#ifndef NDEBUG
    corrupted_ = m.corrupted_;
#endif
    measure_repeat_count_ = 0;
}

//---------------------------------------------------------
//   MStaff::setScore
//---------------------------------------------------------

void MStaff::set_score(Score *score) {
    if (lines_) {
        lines_->set_score(score);
    }
    if (vspacer_up_) {
        vspacer_up_->set_score(score);
    }
    if (vspacer_down_) {
        vspacer_down_->set_score(score);
    }
    if (measure_number_) {
        measure_number_->set_score(score);
    }
    if (mm_range_text_) {
        mm_range_text_->set_score(score);
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void MStaff::set_track(track_idx_t track) {
    if (lines_) {
        lines_->set_track(track);
    }
    if (vspacer_up_) {
        vspacer_up_->set_track(track);
    }
    if (vspacer_down_) {
        vspacer_down_->set_track(track);
    }
    if (measure_number_) {
        measure_number_->set_track(track);
    }
    if (mm_range_text_) {
        mm_range_text_->set_track(track);
    }
}

//---------------------------------------------------------
//   Measure
//---------------------------------------------------------

Measure::Measure(Score *parent) : MeasureBase(ElementType::MEASURE, parent), timesig_(4, 4) {
    set_ticks(Fraction(4, 4));
    repeat_count_ = 2;

    size_t n = score()->nstaves();
    mstaves_.reserve(n);
    for (staff_idx_t staff_idx = 0; staff_idx < n; ++staff_idx) {
        MStaff *ms = new MStaff;
        Staff *staff = score()->staff(staff_idx);
        ms->set_lines(Factory::create_staff_lines(this));
        ms->lines()->set_track(staff_idx * ::mu::engraving::voices);
        ms->lines()->set_visible(!staff->is_lines_invisible(tick()));
        mstaves_.push_back(ms);
    }
    set_exclude_from_numbering(false);
    measure_number_mode_ = MeasureNumberMode::AUTO;
    user_stretch_ = 1.0;
    break_multi_measure_rest_ = false;
    mm_rest_ = nullptr;
    mm_rest_count_ = 0;
    set_flag(ElementFlag::MOVABLE, false);
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

Measure::Measure(const Measure &m) : MeasureBase(m) {
    segments_ = m.segments_.clone();
    timesig_ = m.timesig_;
    len_ = m.len_;
    repeat_count_ = m.repeat_count_;
    measure_number_ = m.measure_number_;
    measure_number_offset_ = m.measure_number_offset_;
    measure_number_mode_ = m.measure_number_mode_;
    user_stretch_ = m.user_stretch_;

    mstaves_.reserve(m.mstaves_.size());
    for (MStaff *ms : m.mstaves_) {
        mstaves_.push_back(new MStaff(*ms));
    }

    break_multi_measure_rest_ = m.break_multi_measure_rest_;
    mm_rest_ = m.mm_rest_;
    mm_rest_count_ = m.mm_rest_count_;
    playback_count_ = m.playback_count_;
}

//---------------------------------------------------------
//   createStaves
//---------------------------------------------------------

void Measure::create_staves(staff_idx_t staff_idx) {
    for (staff_idx_t n = mstaves_.size(); n <= staff_idx; ++n) {
        Staff *staff = score()->staff(n);
        MStaff *s = new MStaff;
        s->set_lines(Factory::create_staff_lines(this));
        s->lines()->set_track(n * ::mu::engraving::voices);
        s->lines()->set_visible(!staff->is_lines_invisible(tick()));
        mstaves_.push_back(s);
    }
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void Measure::set_score(Score *score) {
    MeasureBase::set_score(score);
    for (Segment *s = first(); s; s = s->next()) {
        s->set_score(score);
    }
}

MStaff *Measure::mstaff(staff_idx_t staff_index) const {
    if (staff_index < mstaves_.size()) {
        return mstaves_[staff_index];
    }

    return nullptr;
}

bool Measure::has_voices(staff_idx_t staff_idx) const {
    MStaff *staff = mstaff(staff_idx);
    return staff ? staff->has_voices() : false;
}

void Measure::set_has_voices(staff_idx_t staff_idx, bool v) {
    MStaff *staff = mstaff(staff_idx);

    if (staff) {
        staff->set_has_voices(v);
    }
}

StaffLines *Measure::staff_lines(staff_idx_t staff_idx) const {
    MStaff *staff = mstaff(staff_idx);

    return staff ? staff->lines() : nullptr;
}

Spacer *Measure::vspacer_down(staff_idx_t staff_idx) const {
    MStaff *staff = mstaff(staff_idx);

    return staff ? staff->vspacer_down() : nullptr;
}

Spacer *Measure::vspacer_up(staff_idx_t staff_idx) const {
    MStaff *staff = mstaff(staff_idx);

    return staff ? staff->vspacer_up() : nullptr;
}

void Measure::set_staff_visible(staff_idx_t staff_idx, bool visible) {
    MStaff *staff = mstaff(staff_idx);

    if (staff) {
        staff->set_visible(visible);
    }
}

void Measure::set_staff_stemless(staff_idx_t staff_idx, bool stemless) {
    MStaff *staff = mstaff(staff_idx);

    if (staff) {
        staff->set_stemless(stemless);
    }
}

AutoOnOff Measure::hide_staff_if_empty(staff_idx_t staff_idx) const {
    MStaff *staff = mstaff(staff_idx);

    return staff ? staff->hide_if_empty() : AutoOnOff::AUTO;
}

void Measure::set_hide_staff_if_empty(staff_idx_t staff_idx, AutoOnOff hide_if_empty) {
    MStaff *staff = mstaff(staff_idx);
    IF_ASSERT_FAILED(staff) { return; }

    staff->set_hide_if_empty(hide_if_empty);
}

void Measure::set_mm_range_text(staff_idx_t staff_idx, MMRestRange *t) { mstaves_[staff_idx]->set_mm_range_text(t); }

MMRestRange *Measure::mm_range_text(staff_idx_t staff_idx) const { return mstaves_[staff_idx]->mm_range_text(); }

//---------------------------------------------------------
//   Measure
//---------------------------------------------------------

Measure::~Measure() {
    for (Segment *s = first(); s;) {
        Segment *ns = s->next();
        delete s;
        s = ns;
    }
    muse::delete_all(mstaves_);
}

//---------------------------------------------------------
//   findAccidental
///   return current accidental value at note position
//---------------------------------------------------------

AccidentalVal Measure::find_accidental(const Note *note) const {
    const Chord *chord = note->chord();
    const Staff *v_staff = chord->score()->staff(chord->v_staff_idx());
    AccidentalState tversatz; // state of already set accidentals for this measure
    tversatz.init(v_staff->key_sig_event(tick()));

    const TrackRange track_range = v_staff->part()->track_range();
    track_idx_t main_track = chord->v_staff_idx() * ::mu::engraving::voices;

    for (const Segment *segment = first(); segment; segment = segment->next()) {
        if (segment->is_key_sig_type()) {
            const KeySig *ks = to_key_sig(segment->element(main_track));
            if (!ks) {
                continue;
            }
            tversatz.init(v_staff->key_sig_event(segment->tick()));
        } else if (segment->segment_type() == SegmentType::CHORD_REST) {
            for (track_idx_t track = track_range.start_track; track < track_range.end_track; ++track) {
                const EngravingItem *e = segment->element(track);
                if (!e || !e->is_chord()) {
                    continue;
                }
                const Chord *crd = to_chord(e);
                for (const Chord *chord1 : crd->grace_notes()) {
                    if (chord1->v_staff_idx() != chord->v_staff_idx()) {
                        continue;
                    }
                    for (const Note *note1 : chord1->notes()) {
                        if (note1->tie_back() && !note1->accidental()) {
                            continue;
                        }
                        //
                        // compute accidental
                        //
                        int tpc = note1->tpc();
                        int line = abs_step(tpc, note1->epitch());

                        if (note == note1) {
                            return tversatz.accidental_val(line);
                        }
                        tversatz.set_accidental_val(line, tpc2alter(tpc));
                    }
                }
                if (crd->v_staff_idx() != chord->v_staff_idx()) {
                    continue;
                }
                for (const Note *note1 : crd->notes()) {
                    if (note1->tie_back() && !note1->accidental()) {
                        continue;
                    }
                    //
                    // compute accidental
                    //
                    int tpc = note1->tpc();
                    int line = abs_step(tpc, note1->epitch());

                    if (note == note1) {
                        return tversatz.accidental_val(line);
                    }
                    tversatz.set_accidental_val(line, tpc2alter(tpc));
                }
            }
        }
    }
    LOGD() << "Measure::findAccidental: note not found";
    return AccidentalVal::NATURAL;
}

//---------------------------------------------------------
//   findAccidental
///   Compute accidental state at segment/staffIdx for
///   relative staff line.
//---------------------------------------------------------

AccidentalVal Measure::find_accidental(const Segment *s, staff_idx_t staff_idx, int line, bool &error) const {
    AccidentalState tversatz; // state of already set accidentals for this measure
    const Staff *staff = score()->staff(staff_idx);
    tversatz.init(staff->key_sig_event(tick()));

    SegmentType st = SegmentType::CHORD_REST;
    const TrackRange track_range = staff->part()->track_range();
    for (const Segment *segment = first(st); segment; segment = segment->next(st)) {
        if (segment == s && staff->is_pitched_staff(tick())) {
            ClefType clef = staff->clef(s->tick());
            int l = rel_step(line, clef);
            return tversatz.accidental_val(l, error);
        }
        for (track_idx_t track = track_range.start_track; track < track_range.end_track; ++track) {
            const EngravingItem *e = segment->element(track);
            if (!e || !e->is_chord()) {
                continue;
            }
            const Chord *chord = to_chord(e);
            for (const Chord *chord1 : chord->grace_notes()) {
                if (chord1->v_staff_idx() != staff_idx) {
                    continue;
                }
                for (const Note *note : chord1->notes()) {
                    if (note->tie_back() && !note->accidental()) {
                        continue;
                    }
                    int tpc = note->tpc();
                    int l = abs_step(tpc, note->epitch());
                    tversatz.set_accidental_val(l, tpc2alter(tpc));
                }
            }
            if (chord->v_staff_idx() != staff_idx) {
                continue;
            }
            for (const Note *note : chord->notes()) {
                if (note->tie_back() && !note->accidental()) {
                    continue;
                }
                int tpc = note->tpc();
                int l = abs_step(tpc, note->epitch());
                tversatz.set_accidental_val(l, tpc2alter(tpc));
            }
        }
    }
    LOGD() << "segment not found";
    return AccidentalVal::NATURAL;
}

//---------------------------------------------------------
//   tick2pos
///    return x position for tick relative to System
//---------------------------------------------------------

double Measure::tick2pos(Fraction tck) const {
    tck -= ticks();
    if (is_mm_rest()) {
        Segment *s = first(SegmentType::CHORD_REST);
        double x1 = s->x();
        double w = width() - x1;
        return x1 + (tck.ticks() * w) / (ticks().ticks() * mm_rest_count());
    }

    Segment *s;
    double x1 = 0;
    double x2 = 0;
    Fraction tick1 = Fraction(0, 1);
    Fraction tick2 = Fraction(0, 1);
    for (s = first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        x2 = s->x();
        tick2 = s->rtick();
        if (tck == tick2) {
            return x2 + pos().x();
        }
        if (tck <= tick2) {
            break;
        }
        x1 = x2;
        tick1 = tick2;
    }
    if (s == 0) {
        x2 = width();
        tick2 = ticks();
    }
    double dx = x2 - x1;
    Fraction dt = tick2 - tick1;
    x1 += dt.is_zero() ? 0.0 : (dx * (tck.ticks() - tick1.ticks()) / dt.ticks());
    return x1 + pos().x();
}

//---------------------------------------------------------
//   showsMeasureNumberInAutoMode
///    Whether the measure will show measure number(s) when MeasureNumberMode is set to AUTO
//---------------------------------------------------------

bool Measure::show_measure_number_in_auto_mode() const {
    // Check whether any measure number should be shown
    if (!style().style_b(Sid::SHOW_MEASURE_NUMBER)) {
        return false;
    }

    // Measure numbers should not be shown on measures excluded from numbering.
    if (exclude_from_numbering()) {
        return false;
    }

    int interval = style().style_i(Sid::MEASURE_NUMBER_INTERVAL);
    Measure *prev_measure = this->prev_measure();

    // Measure numbers should not show on first measure unless specified with Sid::showMeasureNumberOne
    // except, when showing numbers on each measure, and first measure is after anacrusis - then show always
    if (is_first_in_section() || (prev_measure->exclude_from_numbering() && prev_measure->is_first_in_section() && interval != 1)) {
        return style().style_b(Sid::SHOW_MEASURE_NUMBER_ONE);
    }

    if (style().style_b(Sid::MEASURE_NUMBER_SYSTEM)) {
        // Show either if
        //   1) This is the first measure of the system OR
        //   2) The previous measure in the system is the first, and is excluded from numbering.
        return (system() && is_first_in_system()) ||
               (prev_measure && prev_measure->exclude_from_numbering() && prev_measure->system() && prev_measure->is_first_in_system());
    } else {
        // In the case of an interval, we should show the measure number either if:
        //   1) We should show them every measure
        if (interval == 1) {
            return true;
        }

        //   2) (measureNumber + 1) % interval == 0 (or 1 if measure number one is numbered.)
        // If measure number 1 is numbered, and the interval is let's say 5, then we should number #1, 6, 11, 16, etc.
        // If measure number 1 is not numbered, with the same interval (5), then we should number #5, 10, 15, 20, etc.
        return ((measure_number() + 1) % interval) == (style().style_b(Sid::SHOW_MEASURE_NUMBER_ONE) ? 1 : 0);
    }
}

bool Measure::show_measure_number_on_staff(staff_idx_t staff_idx) const {
    IF_ASSERT_FAILED(staff_idx < score()->nstaves()) { return false; }

    return show_measure_number() && score()->staff(staff_idx)->should_show_measure_numbers() && !score()->all_staves_invisible();
}

//---------------------------------------------------------
//   showsMeasureNumber
///     Whether the Measure shows a MeasureNumber
//---------------------------------------------------------

bool Measure::show_measure_number() const {
    switch (measure_number_mode_) {
        case MeasureNumberMode::AUTO: return show_measure_number_in_auto_mode();
        case MeasureNumberMode::SHOW: return true;
        case MeasureNumberMode::HIDE: break;
    }
    return false;
}

//---------------------------------------------------------
//   findChord
///   Search for chord at position \a tick in \a track
//---------------------------------------------------------

Chord *Measure::find_chord(Fraction t, track_idx_t track) const {
    t -= tick();
    for (Segment *seg = last(); seg; seg = seg->prev()) {
        if (seg->rtick() < t) {
            return 0;
        }
        if (seg->rtick() == t) {
            EngravingItem *el = seg->element(track);
            if (el && el->is_chord()) {
                return to_chord(el);
            }
        }
    }
    return 0;
}

//---------------------------------------------------------
//   findChordRest
///   Search for chord or rest at position \a tick at \a staff in \a voice.
//---------------------------------------------------------

ChordRest *Measure::find_chord_rest(Fraction t, track_idx_t track) const {
    t -= tick();
    for (const Segment &seg : segments_) {
        if (seg.rtick() > t) {
            return 0;
        }
        if (seg.rtick() == t) {
            EngravingItem *el = seg.element(track);
            if (el && el->is_chord_rest()) {
                return to_chord_rest(el);
            }
        }
    }
    return 0;
}

//---------------------------------------------------------
//   tick2segment
//---------------------------------------------------------

Segment *Measure::tick2segment(const Fraction &input_t, SegmentType st) {
    Fraction t = input_t - tick();
    for (Segment &s : segments_) {
        if (s.rtick() == t) {
            if (s.segment_type() & st) {
                return &s;
            }
        }
        if (s.rtick() > t) {
            break;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   findSegmentR
//    Search for a segment of type st at measure relative
//    position t.
//---------------------------------------------------------

Segment *Measure::find_segment_r(SegmentType st, const Fraction &t) const {
    Segment *s;
    if (t > (ticks() * Fraction(1, 2))) {
        // search backwards
        for (s = last(); s && s->rtick() > t; s = s->prev()) {}
        while (s && s->prev() && s->prev()->rtick() == t) {
            s = s->prev();
        }
    } else {
        // search forwards
        for (s = first(); s && s->rtick() < t; s = s->next()) {}
    }
    for (; s && s->rtick() == t; s = s->next()) {
        if (s->segment_type() & st) {
            return s;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   undoGetSegmentR
//---------------------------------------------------------

Segment *Measure::undo_get_segment_r(SegmentType type, const Fraction &t) {
    Segment *s = find_segment_r(type, t);
    if (s == 0) {
        s = Factory::create_segment(this, type, t);
        score()->undo_add_element(s);
    }
    return s;
}

//---------------------------------------------------------
//   findFirstR
//    return first segment of type st at relative
//    position t.
//---------------------------------------------------------

Segment *Measure::find_first_r(SegmentType st, const Fraction &t) const {
    Segment *s;
    // search forwards
    for (s = first(); s && s->rtick() <= t; s = s->next()) {
        if (s->segment_type() & st) {
            return s;
        }
    }
    return 0;
}

Segment *Measure::get_chord_rest_or_time_tick_segment(const Fraction &f) {
    Segment *seg = find_segment(SegmentType::CHORD_REST, f);
    if (!seg) {
        seg = find_segment(SegmentType::TIME_TICK, f);
    }
    if (!seg) {
        if (f - tick() == ticks()) { // end of measure
            seg = get_segment(SegmentType::TIME_TICK, f);
        } else {
            seg = get_segment(SegmentType::CHORD_REST, f);
        }
    }

    return seg;
}

Segment *Measure::undo_get_chord_rest_or_time_tick_segment(const Fraction &f) {
    Segment *seg = find_segment(SegmentType::CHORD_REST, f);
    if (!seg) {
        seg = find_segment(SegmentType::TIME_TICK, f);
    }
    if (!seg) {
        if (f - tick() == ticks()) { // end of measure
            seg = undo_get_segment(SegmentType::TIME_TICK, f);
        } else {
            seg = undo_get_segment(SegmentType::CHORD_REST, f);
        }
    }

    return seg;
}

//---------------------------------------------------------
//   getSegmentR
///   Get a segment of type st at relative tick position t.
///   If the segment does not exist, it is created.
//---------------------------------------------------------

Segment *Measure::get_segment_r(SegmentType st, const Fraction &t) {
    Segment *s = find_segment_r(st, t);
    if (!s) {
        s = Factory::create_segment(this, st, t);
        add(s);
    }
    return s;
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to Measure.
//---------------------------------------------------------

void Measure::add(EngravingItem *e) {
    if (e->ownership_parent() != this) {
        e->set_ownership_parent(this);
    }

    ElementType type = e->type();

    switch (type) {
        case ElementType::SEGMENT: {
            Segment *seg = to_segment(e);
            Fraction t = seg->rtick();
            Segment *s;

            for (s = first(); s && s->rtick() < t; s = s->next()) {}
            while (s && s->rtick() == t) {
                if (!seg->is_chord_rest_type() && (seg->segment_type() == s->segment_type())) {
                    if (seg->is_type(SegmentType::BAR_LINE_TYPES)) {
                        // Barline segments are regenerated every layout
                        // We need to remove the regenerated segment when undoing to ensure the original element is added back to the score
                        segments_.remove(s);
                        s = s->next();
                        continue;
                    }
                    /// HACK: REMOVED to prevent crash in 4.4.3.
                    /// Adding multiple identical segments may cause problems, so we should resolve this properly
                    // return;
                }
                if (seg->goes_before(s)) {
                    break;
                }
                s = s->next();
            }
            seg->set_ownership_parent(this);
            segments_.insert(seg, s);
            //
            // update measure flags
            //
            if (seg->header()) {
                seg->measure()->set_header(true);
            }
            if (seg->trailer()) {
                seg->measure()->set_trailer(true);
            }
        } break;

        case ElementType::MEASURE_NUMBER:
            if (e->staff_idx() < mstaves_.size()) {
                mstaves_[e->staff_idx()]->set_measure_number(to_measure_number(e));
            }
            break;

        case ElementType::MMREST_RANGE:
            if (e->staff_idx() < mstaves_.size()) {
                mstaves_[e->staff_idx()]->set_mm_range_text(to_mm_rest_range(e));
            }
            break;

        case ElementType::SPACER: {
            Spacer *sp = to_spacer(e);
            switch (sp->spacer_type()) {
                case SpacerType::UP: mstaves_[e->staff_idx()]->set_vspacer_up(sp); break;
                case SpacerType::DOWN:
                case SpacerType::FIXED: mstaves_[e->staff_idx()]->set_vspacer_down(sp); break;
            }
            sp->set_gap(sp->gap()); // trigger relayout
        } break;
        case ElementType::JUMP:
        case ElementType::MARKER:
            if (e && (e->is_jump() || (e->is_marker() && to_marker(e)->is_right_marker()))) {
                // "To coda" markings act like jumps
                set_property(Pid::REPEAT_JUMP, true);
            }
            el().push_back(e);
            break;

        case ElementType::HBOX:
            if (e->staff()) {
                e->mutldata()->set_mag(e->staff()->staff_mag(tick())); // ?!
            }
            el().push_back(e);
            break;

        case ElementType::MEASURE: mm_rest_ = to_measure(e); break;

        case ElementType::STAFFTYPE_CHANGE: {
            StaffTypeChange *staff_type_change = to_staff_type_change(e);
            const StaffType *template_staff_type = staff_type_change->staff_type();

            Staff *staff = staff_type_change->staff();

            // This will need to point to the stafftype element within the stafftypelist for the staff
            StaffType *new_staff_type = nullptr;

            if (template_staff_type) {
                // executed on read, undo/redo, clone
                // setStaffType adds a copy to stafftypelist and returns a pointer to that element within stafftypelist
                new_staff_type = staff->set_staff_type(tick(), *template_staff_type);
            } else {
                // executed on add from palette
                // staffType returns a pointer to the current stafftype element in the list
                // setStaffType will make a copy and return a pointer to that element within list
                template_staff_type = staff->staff_type(tick());
                new_staff_type = staff->set_staff_type(tick(), *template_staff_type);
            }

            staff->staff_type_list_changed(tick());
            staff_type_change->set_staff_type(new_staff_type, false);

            MeasureBase::add(e);
        }
            return;

        default: MeasureBase::add(e); return;
    }

    e->added();
}

//---------------------------------------------------------
//   remove
///   Remove EngravingItem \a el from Measure.
//---------------------------------------------------------

void Measure::remove(EngravingItem *e) {
    assert(e->ownership_parent() == this);
    assert(e->score() == score());

    switch (e->type()) {
        case ElementType::SEGMENT: {
            Segment *s = to_segment(e);
            segments_.remove(s);
            //
            // update measure flags
            //
            if (s->header()) {
                s->measure()->check_header();
            }
            if (s->trailer()) {
                s->measure()->check_trailer();
            }
        } break;

        case ElementType::MEASURE_NUMBER: mstaves_[e->staff_idx()]->set_measure_number(nullptr); break;

        case ElementType::MMREST_RANGE: mstaves_[e->staff_idx()]->set_mm_range_text(nullptr); break;

        case ElementType::SPACER:
            switch (to_spacer(e)->spacer_type()) {
                case SpacerType::DOWN:
                case SpacerType::FIXED: mstaves_[e->staff_idx()]->set_vspacer_down(0); break;
                case SpacerType::UP: mstaves_[e->staff_idx()]->set_vspacer_up(0); break;
            }
            break;

        case ElementType::JUMP:
        case ElementType::MARKER:
            if (e->is_jump() || (e->is_marker() && to_marker(e)->is_right_marker())) {
                set_property(Pid::REPEAT_JUMP, false);
            }
            [[fallthrough]];
        case ElementType::HBOX:
            if (!el().remove(e)) {
                LOGD("Measure(%p)::remove(%s,%p) not found", this, e->type_name(), e);
            }
            break;

        case ElementType::CLEF:
        case ElementType::CHORD:
        case ElementType::REST:
        case ElementType::MMREST:
        case ElementType::TIMESIG:
            for (Segment *segment = first(); segment; segment = segment->next()) {
                size_t staves = score()->nstaves();
                size_t tracks = staves * ::mu::engraving::voices;
                for (track_idx_t track = 0; track < tracks; ++track) {
                    EngravingItem *ee = segment->element(track);
                    if (ee == e) {
                        segment->set_element(track, nullptr);
                        e->removed();
                        return;
                    }
                }
            }
            LOGD("Measure::remove: %s %p not found", e->type_name(), e);
            break;

        case ElementType::MEASURE: mm_rest_ = 0; break;

        case ElementType::STAFFTYPE_CHANGE: {
            StaffTypeChange *stc = to_staff_type_change(e);
            Staff *staff = stc->staff();
            if (staff) {
                // st currently points to an list element that is about to be removed
                // make a copy now to use on undo/redo
                StaffType *st = new StaffType(*stc->staff_type());
                if (!tick().is_zero()) {
                    staff->remove_staff_type(tick());
                }
                stc->set_staff_type(st, true);
            }
            MeasureBase::remove(e);
        }
            return;

        default: MeasureBase::remove(e); return;
    }

    e->removed();
}

//---------------------------------------------------------
//   change
//---------------------------------------------------------

void Measure::change(EngravingItem *o, EngravingItem *n) {
    if (o->is_tuplet()) {
        Tuplet *t = to_tuplet(n);
        for (DurationElement *e : t->elements()) {
            e->set_tuplet(t);
        }
    } else {
        remove(o);
        add(n);
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void Measure::spatium_changed(double /*oldValue*/, double /*newValue*/) {}

//-------------------------------------------------------------------
//   moveTicks
//    Also adjust endBarLine if measure len has changed. For this
//    diff == 0 cannot be optimized away
//-------------------------------------------------------------------

void Measure::move_ticks(const Fraction &diff) {
    // This part must be run also for diff = 0
    for (Segment *segment = last(); segment; segment = segment->prev()) {
        if (segment->segment_type() & (SegmentType::END_BAR_LINE | SegmentType::TIME_SIG_ANNOUNCE)) {
            segment->set_rtick(ticks());
        }
    }
    if (diff.numerator() == 0) {
        return;
    }
    std::set<Tuplet *> tuplets;
    set_tick(tick() + diff);
    for (Segment *segment = last(); segment; segment = segment->prev()) {
        if (segment->is_chord_rest_type()) {
            // Tuplet ticks are stored as absolute ticks, so they must be adjusted,
            // outer tuplets of nested ones too. But each tuplet must only be adjusted once.
            for (EngravingItem *e : segment->elist()) {
                if (e) {
                    ChordRest *cr = to_chord_rest(e);
                    for (Tuplet *tuplet = cr->tuplet(); tuplet && tuplets.insert(tuplet).second; tuplet = tuplet->tuplet()) {
                        tuplet->set_tick(tuplet->tick() + diff);
                    }
                }
            }
        }
    }
    tuplets.clear();
}

//---------------------------------------------------------
//   removeStaves
//---------------------------------------------------------

void Measure::remove_staves(staff_idx_t s_staff, staff_idx_t e_staff) {
    for (Segment *s = first(); s; s = s->next()) {
        for (int staff = static_cast<int>(e_staff) - 1; staff >= static_cast<int>(s_staff); --staff) {
            s->remove_staff(staff);
        }
    }
    for (EngravingItem *e : el()) {
        if (e->track() == muse::nidx) {
            continue;
        }
        voice_idx_t voice = e->voice();
        staff_idx_t staff_idx = e->staff_idx();
        if (staff_idx >= e_staff) {
            staff_idx -= e_staff - s_staff;
            e->set_track(staff_idx * ::mu::engraving::voices + voice);
        }
    }
}

//---------------------------------------------------------
//   insertStaves
//---------------------------------------------------------

void Measure::insert_staves(staff_idx_t s_staff, staff_idx_t e_staff) {
    for (EngravingItem *e : el()) {
        if (e->track() == muse::nidx) {
            continue;
        }
        staff_idx_t staff_idx = e->staff_idx();
        if (move_down_when_adding_staves(e, s_staff, e_staff)) {
            voice_idx_t voice = e->voice();
            staff_idx += e_staff - s_staff;
            e->set_track(staff_idx * ::mu::engraving::voices + voice);
        }
    }
    for (Segment *s = first(); s; s = s->next()) {
        for (staff_idx_t staff = s_staff; staff < e_staff; ++staff) {
            s->insert_staff(staff);
        }
    }
}

//---------------------------------------------------------
//   cmdRemoveStaves
//---------------------------------------------------------

void Measure::cmd_remove_staves(staff_idx_t s_staff, staff_idx_t e_staff) {
    track_idx_t s_track = s_staff * ::mu::engraving::voices;
    track_idx_t e_track = e_staff * ::mu::engraving::voices;

    for (Segment *s = first(); s; s = s->next()) {
        for (int track = static_cast<int>(e_track) - 1; track >= static_cast<int>(s_track); --track) {
            EngravingItem *el = s->element(track);
            if (el) {
                el->undo_unlink();
                score()->do_undo_remove_element(el);
            }
        }

        // Create copy, because s->annotations() will be modified during the loop
        std::vector<EngravingItem *> annotations = s->annotations();
        for (EngravingItem *e : annotations) {
            if (allow_remove_when_removing_staves(e, s_staff, e_staff)) {
                e->undo_unlink();
                score()->do_undo_remove_element(e);
            }
        }
    }

    for (EngravingItem *e : el()) {
        if (e->track() == muse::nidx) {
            continue;
        }

        if (allow_remove_when_removing_staves(e, s_staff, e_staff)) {
            e->undo_unlink();
            score()->do_undo_remove_element(e);
        }
    }

    score()->undo(new RemoveStaves(this, s_staff, e_staff));

    for (int i = static_cast<int>(e_staff) - 1; i >= static_cast<int>(s_staff); --i) {
        MStaff *ms = *(mstaves_.begin() + i);
        score()->undo(new RemoveMStaff(this, ms, i));
    }
    score()->undo_update_play_count_text(this);
}

//---------------------------------------------------------
//   cmdAddStaves
//---------------------------------------------------------

void Measure::cmd_add_staves(staff_idx_t s_staff, staff_idx_t e_staff, bool create_rest) {
    score()->undo(new InsertStaves(this, s_staff, e_staff));

    Segment *ts = find_segment(SegmentType::TIME_SIG, tick());
    Segment *bs = find_segment_r(SegmentType::END_BAR_LINE, ticks());

    for (staff_idx_t i = s_staff; i < e_staff; ++i) {
        Staff *staff = score()->staff(i);
        MStaff *ms = new MStaff;
        ms->set_lines(Factory::create_staff_lines(this));
        ms->lines()->set_track(i * ::mu::engraving::voices);
        ms->lines()->set_visible(!staff->is_lines_invisible(tick()));
        score()->undo(new InsertMStaff(this, ms, i));
    }

    if (!create_rest && !ts) {
        return;
    }

    // collect unique staves (only one instance for linked staves):

    std::vector<staff_idx_t> sl;
    for (staff_idx_t staff_idx = s_staff; staff_idx < e_staff; ++staff_idx) {
        Staff *s = score()->staff(staff_idx);
        if (s->links()) {
            bool already_in_list = false;
            for (staff_idx_t idx : sl) {
                if (s->links()->contains(score()->staff(idx))) {
                    already_in_list = true;
                    break;
                }
            }
            if (already_in_list) {
                continue;
            }
        }
        sl.push_back(staff_idx);
    }

    for (staff_idx_t staff_idx : sl) {
        if (create_rest) {
            score()->set_rest(tick(), staff_idx * ::mu::engraving::voices, ticks(), false, 0, timesig_ == ticks());
        }

        // replicate time signature
        if (ts) {
            TimeSig *ots = 0;
            bool constructed = false;
            for (track_idx_t track = 0; track < mstaves_.size() * ::mu::engraving::voices; ++track) {
                if (ts->element(track)) {
                    ots = to_time_sig(ts->element(track));
                    break;
                }
            }
            if (!ots) {
                // no time signature found; use measure timesig to construct one
                ots = Factory::create_time_sig(score()->dummy());
                ots->set_sig(timesig());
                constructed = true;
            }
            // do no replicate local time signatures
            if (ots && !ots->is_local()) {
                TimeSig *timesig = Factory::copy_time_sig(*ots);
                timesig->set_track(staff_idx * ::mu::engraving::voices);
                timesig->set_ownership_parent(ts);
                timesig->set_sig(ots->sig(), ots->time_sig_type());
                score()->undo_add_element(timesig);
                if (constructed) {
                    delete ots;
                }
            }
        }

        // replicate barline
        if (bs) {
            BarLine *obl = nullptr;
            for (track_idx_t track = 0; track < mstaves_.size() * ::mu::engraving::voices; ++track) {
                EngravingItem *e = bs->element(track);
                if (e && !e->generated()) {
                    obl = to_bar_line(e);
                    break;
                }
            }
            if (obl) {
                BarLine *barline = Factory::copy_bar_line(*obl);
                barline->set_span_staff(score()->staff(staff_idx)->bar_line_span());
                barline->set_track(staff_idx * ::mu::engraving::voices);
                barline->set_ownership_parent(bs);
                barline->set_generated(false);
                score()->undo_add_element(barline);
            }
        }
    }
}

//---------------------------------------------------------
//   insertMStaff
//---------------------------------------------------------

void Measure::insert_m_staff(MStaff *staff, staff_idx_t idx) {
    mstaves_.insert(mstaves_.begin() + idx, staff);
    for (staff_idx_t staff_idx = 0; staff_idx < mstaves_.size(); ++staff_idx) {
        mstaves_[staff_idx]->set_track(staff_idx * ::mu::engraving::voices);
    }
}

//---------------------------------------------------------
//   removeMStaff
//---------------------------------------------------------

void Measure::remove_m_staff(MStaff * /*staff*/, staff_idx_t idx) {
    mstaves_.erase(mstaves_.begin() + idx);
    for (staff_idx_t staff_idx = 0; staff_idx < mstaves_.size(); ++staff_idx) {
        mstaves_[staff_idx]->set_track(staff_idx * ::mu::engraving::voices);
    }
}

//---------------------------------------------------------
//   insertStaff
//---------------------------------------------------------

void Measure::insert_staff(Staff *staff, staff_idx_t staff_idx) {
    for (Segment *s = first(); s; s = s->next()) {
        s->insert_staff(staff_idx);
    }

    MStaff *ms = new MStaff;
    ms->set_lines(Factory::create_staff_lines(this));
    ms->lines()->set_track(staff_idx * ::mu::engraving::voices);
    ms->lines()->set_visible(!staff->is_lines_invisible(tick()));
    insert_m_staff(ms, staff_idx);
}

//---------------------------------------------------------
//   staffPageBoundingRect
//---------------------------------------------------------

RectF Measure::staff_page_bounding_rect(staff_idx_t staff_idx) const {
    System *s = system();
    IF_ASSERT_FAILED(s) { return RectF(); }
    RectF sys_staff_page_bbox = s->staff(staff_idx)->bbox().translated(s->page_pos());
    RectF measure_page_bbox = page_bounding_rect();
    return RectF(measure_page_bbox.x(), sys_staff_page_bbox.y(), measure_page_bbox.width(), sys_staff_page_bbox.height());
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

/**
 Return true if an EngravingItem of type \a type can be dropped on a Measure
*/

bool Measure::accept_drop(EditData &data) const {
    const EngravingItem *e = data.drop_element;

    if (data.track == muse::nidx || !system()) {
        return false;
    }

    const staff_idx_t staff_idx = track2staff(data.track);

    //! NOTE: Should match NotationInteraction::dragMeasureAnchorElement
    switch (e->type()) {
        case ElementType::VBOX:
        case ElementType::TBOX:
        case ElementType::FBOX:
        case ElementType::HBOX: {
            const Measure *m = is_mm_rest() ? mm_rest_first() : this;
            for (staff_idx_t staff_idx_loop = 0; staff_idx_loop < score()->nstaves(); ++staff_idx_loop) {
                if (m->is_measure_repeat_group_with_prev_m(staff_idx_loop)) {
                    return false;
                }
            }
            [[fallthrough]];
        }

        case ElementType::MEASURE_NUMBER:
        case ElementType::JUMP:
        case ElementType::MARKER:
        case ElementType::VOLTA:
        case ElementType::GRADUAL_TEMPO_CHANGE:
        case ElementType::KEYSIG:
        case ElementType::TIMESIG:
            // Drop to all staves or single staff depending on modifier
            return true;

        case ElementType::BRACKET:
        case ElementType::MEASURE_REPEAT:
        case ElementType::MEASURE:
        case ElementType::SPACER:
        case ElementType::IMAGE:
        case ElementType::BAR_LINE:
        case ElementType::SYMBOL:
        case ElementType::CLEF:
        case ElementType::STAFFTYPE_CHANGE:
            // Always drop to single staff
            return true;

        case ElementType::STRING_TUNINGS: return can_add_string_tunings(staff_idx);

        default: break;
    }
    return MeasureBase::accept_drop(data);
}

//---------------------------------------------------------
//   drop
///   Drop element.
///   Handle a dropped element at position \a pos of given
///   element \a type and \a subtype.
//---------------------------------------------------------

EngravingItem *Measure::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;
    staff_idx_t staff_idx = track2staff(data.track);
    if (staff_idx == muse::nidx) {
        delete e;
        return nullptr;
    }
    Staff *staff = score()->staff(staff_idx);

    switch (e->type()) {
        case ElementType::MARKER:
        case ElementType::JUMP:
            e->set_ownership_parent(this);
            e->set_track(0);
            score()->undo_add_element(e);
            return e;

        case ElementType::STRING_TUNINGS: {
            staff = staff->is_primary_staff() ? staff : staff->primary_staff();
            if (!staff) {
                delete e;
                return nullptr;
            }
            staff_idx = staff->idx();
            Segment *parent_seg = first(SegmentType::DURATION);
            if (!parent_seg) {
                delete e;
                return nullptr;
            }
            e->set_ownership_parent(parent_seg);
            e->set_track(staff2track(staff_idx));
            score()->undo_add_element(e);
            return e;
        }

        case ElementType::MEASURE_NUMBER:
            undo_change_property(Pid::MEASURE_NUMBER_MODE, static_cast<int>(MeasureNumberMode::SHOW));
            delete e;
            break;

        case ElementType::BRACKET: {
            Selection sel = score()->selection();
            staff_idx_t first_staff = sel.staff_start();
            staff_idx_t last_staff = sel.staff_end() - 1;
            size_t level = 0;
            for (staff_idx_t idx = 0; idx < score()->nstaves(); ++idx) {
                for (const BracketItem *bi : score()->brackets(idx)) {
                    if (bi->intersects(first_staff, last_staff)) {
                        level = std::max(level, bi->column() + 1);
                    }
                }
            }

            Bracket *b = to_bracket(e);
            if (sel.is_range()) {
                EditStaffBrackets::undo_add_bracket(score(), staff, level, b->bracket_type(), sel.staff_end() - sel.staff_start());
            } else {
                EditStaffBrackets::undo_add_bracket(score(), staff, level, b->bracket_type(), 1);
            }
            delete b;
        } break;

        case ElementType::CLEF:
            EditClef::undo_change_clef(tx, score(), staff, this, to_clef(e)->clef_type());
            delete e;
            break;

        case ElementType::KEYSIG: {
            KeySigEvent k = to_key_sig(e)->key_sig_event();
            delete e;

            if (data.modifiers & CONTROL_MODIFIER) {
                // apply only to this stave
                EditKeySig::undo_change_key_sig(tx, score(), staff, tick(), k);
            } else {
                // apply to all staves:
                for (Staff *s : score()->staves()) {
                    EditKeySig::undo_change_key_sig(tx, score(), s, tick(), k);
                }
            }

            break;
        }

        case ElementType::TIMESIG: {
            EditTimeSig::add_time_sig(tx, score(), this, staff_idx, to_time_sig(e), data.modifiers & CONTROL_MODIFIER);
            break;
        }

        case ElementType::SPACER: {
            Spacer *spacer = to_spacer(e);
            spacer->set_track(track_zero_voice(data.track));
            spacer->set_ownership_parent(this);
            if (spacer->spacer_type() == SpacerType::FIXED) {
                double gap = spatium() * 10;
                System *s = system();
                const staff_idx_t next_vis_staff_idx = s->next_visible_staff(staff_idx);
                const bool system_end = (next_vis_staff_idx == muse::nidx);
                if (system_end) {
                    System *ns = 0;
                    for (System *ts : score()->systems()) {
                        if (ns) {
                            ns = ts;
                            break;
                        }
                        if (ts == s) {
                            ns = ts;
                        }
                    }
                    if (ns == s) {
                        double y1 = s->staff_ypage(staff_idx);
                        double y2 = s->page()->height() - s->page()->bm();
                        gap = y2 - y1 - score()->staff(staff_idx)->staff_height();
                    } else if (ns && ns->page() == s->page()) {
                        double y1 = s->staff_ypage(staff_idx);
                        double y2 = ns->staff_ypage(0);
                        gap = y2 - y1 - score()->staff(staff_idx)->staff_height();
                    }
                } else {
                    double y1 = s->staff_ypage(staff_idx);
                    double y2 = s->staff_ypage(next_vis_staff_idx);
                    gap = y2 - y1 - score()->staff(staff_idx)->staff_height();
                }
                spacer->set_gap(Spatium::from_absolute(gap, spatium()));
            }
            score()->undo_add_element(spacer);
            trigger_layout();
            return spacer;
        }

        case ElementType::BAR_LINE: {
            BarLine *bl = to_bar_line(e);

            if (bl->play_count() != -1) {
                undo_change_property(Pid::REPEAT_COUNT, bl->play_count());
            }

            // if dropped bar line refers to span rather than to subtype
            // or if Ctrl key used
            if ((bl->span_from() && bl->span_to()) || data.control()) {
                // get existing bar line for this staff, and drop the change to it
                Segment *seg = undo_get_segment_r(SegmentType::END_BAR_LINE, ticks());
                BarLine *cbl = to_bar_line(seg->element(track_zero_voice(data.track)));
                if (cbl) {
                    return cbl->drop(tx, data);
                }
            } else if (bl->bar_line_type() == BarLineType::START_REPEAT) {
                Measure *m2 = is_mm_rest() ? mm_rest_first() : this;
                for (size_t st_idx = 0; st_idx < score()->nstaves(); ++st_idx) {
                    if (m2->is_measure_repeat_group_with_prev_m(st_idx)) {
                        MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                        return nullptr;
                    }
                }
                for (Score *lscore : score()->score_list()) {
                    Measure *lmeasure = lscore->tick2measure(m2->tick());
                    if (lmeasure) {
                        lmeasure->undo_change_property(Pid::REPEAT_START, true);
                    }
                }
            } else if (bl->bar_line_type() == BarLineType::END_REPEAT) {
                Measure *m2 = is_mm_rest() ? mm_rest_last() : this;
                for (size_t st_idx = 0; st_idx < score()->nstaves(); ++st_idx) {
                    if (m2->is_measure_repeat_group_with_next_m(st_idx)) {
                        MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                        return nullptr;
                    }
                }
                for (Score *lscore : score()->score_list()) {
                    Measure *lmeasure = lscore->tick2measure(m2->tick());
                    if (lmeasure) {
                        lmeasure->undo_change_property(Pid::REPEAT_END, true);
                    }
                }
                score()->undo_update_play_count_text(this);
            } else if (bl->bar_line_type() == BarLineType::END_START_REPEAT) {
                Measure *m2 = is_mm_rest() ? mm_rest_last() : this;
                for (size_t st_idx = 0; st_idx < score()->nstaves(); ++st_idx) {
                    if (m2->is_measure_repeat_group_with_next_m(st_idx)) {
                        MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                        return nullptr;
                    }
                }
                for (Score *lscore : score()->score_list()) {
                    Measure *lmeasure = lscore->tick2measure(m2->tick());
                    if (lmeasure) {
                        lmeasure->undo_change_property(Pid::REPEAT_END, true);
                        lmeasure = lmeasure->next_measure();
                        if (lmeasure) {
                            lmeasure->undo_change_property(Pid::REPEAT_START, true);
                        }
                    }
                }
            } else {
                Segment *seg = undo_get_segment_r(SegmentType::END_BAR_LINE, ticks());
                // if any staff lacks a barline, create one
                for (size_t st_idx = 0; st_idx < score()->nstaves(); ++st_idx) {
                    BarLine *staff_bar_line = to_bar_line(seg->element(st_idx * ::mu::engraving::voices));
                    if (!staff_bar_line) {
                        staff_bar_line = Factory::create_bar_line(seg);
                        staff_bar_line->set_track(st_idx * ::mu::engraving::voices);
                        undo_add_element(staff_bar_line);
                    }
                }
                // drop to barline
                return seg->element(track_zero_voice(data.track))->drop(tx, data);
            }
            break;
        }

        case ElementType::MEASURE_REPEAT: {
            int num_measures = to_measure_repeat(e)->num_measures();
            delete e;
            EditMeasureRepeat::add_measure_repeat(tx, score(), this, num_measures, staff_idx);
            break;
        }

        case ElementType::STAFFTYPE_CHANGE: {
            e->set_ownership_parent(this);
            e->set_track(track_zero_voice(data.track));
            score()->undo_add_element(e);
        } break;

        case ElementType::VBOX:
        case ElementType::TBOX:
        case ElementType::FBOX:
        case ElementType::HBOX: return score()->insert_box(to_measure_base(e), this);

        default: return MeasureBase::drop(tx, data);
    }

    return nullptr;
}

//---------------------------------------------------------
//   adjustToLen
//    change actual measure len, adjust elements to
//    new len
//---------------------------------------------------------

void Measure::adjust_to_len(Fraction nf, bool append_rests_if_necessary) {
    Fraction ol = ticks();
    Fraction nl = nf;
    Fraction diff = nl - ol;

    Fraction start_tick = end_tick();
    if (diff < Fraction(0, 1)) {
        start_tick += diff;
    }

    score()->undo_insert_time(start_tick, diff);

    for (Score *s : score()->score_list()) {
        s->undo(new InsertTime(s, start_tick, diff));
        Measure *m = s->tick2measure(tick());
        s->undo(new ChangeMeasureLen(m, nf));
        if (nl > ol) {
            // move EndBarLine, TimeSigAnnounce, KeySigAnnounce
            for (Segment *seg = m->first(); seg; seg = seg->next()) {
                if (seg->segment_type() &
                    (SegmentType::END_BAR_LINE | SegmentType::COURTESY_TIME_SIG_TYPES | SegmentType::COURTESY_KEY_SIG_TYPES)) {
                    seg->set_rtick(nl);
                }
            }
        }
    }
    Score *s = score()->master_score();
    Measure *m = s->tick2measure(tick());
    std::vector<staff_idx_t> sl = s->unique_staves();

    for (staff_idx_t staff_idx : sl) {
        int rests = 0;
        int chords = 0;
        Rest *rest = 0;
        for (Segment *segment = m->first(); segment; segment = segment->next()) {
            track_idx_t strack = staff_idx * ::mu::engraving::voices;
            track_idx_t etrack = strack + ::mu::engraving::voices;
            for (track_idx_t track = strack; track < etrack; ++track) {
                EngravingItem *e = segment->element(track);
                if (e) {
                    if (e->is_rest()) {
                        ++rests;
                        rest = to_rest(e);
                    } else if (e->is_chord()) {
                        ++chords;
                    }
                }
            }
        }
        Fraction stretch = s->staff(staff_idx)->time_stretch(tick());
        // if just a single rest
        if (rests == 1 && chords == 0) {
            // if measure value didn't change, stick to whole measure rest
            if (timesig_ == nf) {
                rest->undo_change_property(Pid::DURATION, nf * stretch);
                rest->undo_change_property(Pid::DURATION_TYPE_WITH_DOTS, DurationTypeWithDots(DurationType::V_MEASURE));
            } else { // if measure value did change, represent with rests actual measure value
                // convert the measure duration in a list of values (no dots for rests)
                std::vector<TDuration> dur_list =
                    to_rhythmic_duration_list(nf * stretch,
                                              /*isRest=*/true,
                                              /*rtickStart=*/Fraction(0, 1),
                                              /*nominal=*/score()->sigmap()->timesig(tick().ticks()).nominal(),
                                              /*measure=*/this,
                                              /*maxDots=*/0, stretch);
                if (dur_list.empty()) {
                    LOGD("Could not make durations for: %d/%d", (nf * stretch).numerator(), (nf * stretch).denominator());
                    continue;
                }
                // set the existing rest to the first value of the duration list
                TDuration first_dur = dur_list[0];
                rest->undo_change_property(Pid::DURATION, first_dur.is_measure() ? ticks() : first_dur.fraction());
                rest->undo_change_property(Pid::DURATION_TYPE_WITH_DOTS, dur_list[0].type_with_dots());

                // add rests for any other duration list value
                Fraction tick_offset = tick() + rest->actual_ticks();
                for (unsigned i = 1; i < dur_list.size(); i++) {
                    Rest *new_rest = Factory::create_rest(s->dummy());
                    TDuration dur = dur_list.at(i);
                    new_rest->set_duration_type(dur);
                    new_rest->set_ticks(dur.is_measure() ? ticks() : dur.fraction());
                    new_rest->set_track(rest->track());
                    score()->undo_add_cr(new_rest, this, tick_offset);
                    tick_offset += new_rest->actual_ticks();
                }
            }
            continue;
        }

        track_idx_t strack = staff_idx * ::mu::engraving::voices;
        track_idx_t etrack = strack + ::mu::engraving::voices;

        for (track_idx_t trk = strack; trk < etrack; ++trk) {
            Fraction n = diff;
            bool r_flag = false;
            if (n < Fraction(0, 1)) {
                for (Segment *segment = m->last(); segment;) {
                    Segment *pseg = segment->prev();
                    if (segment->segment_type() == SegmentType::CHORD_REST) {
                        const auto annotations = segment->annotations(); // make a copy since we alter the list
                        for (EngravingItem *a : annotations) {
                            if (a->track() == trk) {
                                s->undo_remove_element(a);
                            }
                        }
                        EngravingItem *e = segment->element(trk);
                        if (e && e->is_chord_rest()) {
                            ChordRest *cr = to_chord_rest(e);
                            if (cr->duration_type() == DurationType::V_MEASURE) {
                                Fraction actual_ticks = cr->actual_ticks();
                                n += actual_ticks;
                                cr->set_duration_type(TDuration(actual_ticks));
                            } else {
                                n += cr->actual_ticks();
                            }
                            s->undo_remove_element(e);
                            if (n >= Fraction(0, 1)) {
                                break;
                            }
                        }
                    } else if (segment->segment_type() == SegmentType::BREATH) {
                        EngravingItem *e = segment->element(trk);
                        if (e) {
                            s->undo_remove_element(e);
                        }
                    }
                    segment = pseg;
                }
                r_flag = true;
            }
            voice_idx_t voice = trk % ::mu::engraving::voices;
            if (append_rests_if_necessary && (n > Fraction(0, 1)) && (r_flag || voice == 0)) {
                // add rest to measure
                Fraction rtick = tick() + nl - n;
                track_idx_t track = staff_idx * ::mu::engraving::voices + voice;
                s->set_rest(rtick, track, n * stretch, false, 0, false);
            }
        }
    }
    if (diff < Fraction(0, 1)) {
        //
        //  CHECK: do not remove all slurs
        //
        for (EngravingItem *e : m->el()) {
            if (e->is_slur()) {
                s->undo_remove_element(e);
            }
        }
    }

    Selection &sel = score()->selection();
    if (!sel.is_range()) {
        return;
    }
    // If the selection started and/or ended inside this measure, we should pass it
    // updated start/end segments...
    const Segment *old_start = sel.start_segment();
    if (old_start && sel.start_measure_base() == this) {
        Segment *new_start = this->tick2segment(old_start->tick());
        sel.set_start_segment(new_start ? new_start : first());
    }
    const Segment *old_end = sel.end_segment();
    if (old_end && sel.end_measure_base() == this) {
        Segment *new_end = this->tick2segment(old_end->tick());
        sel.set_end_segment(new_end ? new_end : last()->next1());
    }
}

//---------------------------------------------------------
//   visible
//---------------------------------------------------------

bool Measure::visible(staff_idx_t staff_idx) const {
    if (staff_idx >= score()->staves().size()) {
        LOGD("Measure::visible: bad staffIdx: %zu", staff_idx);
        return false;
    }
    if (system() && (system()->staves().empty() || !system()->staff(staff_idx)->show())) {
        return false;
    }
    if (score()->staff(staff_idx)->cutaway() && is_empty(staff_idx)) {
        return false;
    }
    return score()->staff(staff_idx)->show() && mstaves_[staff_idx]->visible();
}

//---------------------------------------------------------
//   stemless
//---------------------------------------------------------

bool Measure::stemless(staff_idx_t staff_idx) const {
    const Staff *staff = score()->staff(staff_idx);
    if (!staff) {
        return false;
    }

    return staff->stemless(tick()) || mstaves_[staff_idx]->stemless() || staff->staff_type(tick())->stemless();
}

//---------------------------------------------------------
//   isFinalMeasureOfSection
//    returns true if this measure is final actual measure of a section
//    takes into consideration fact that subsequent measures base objects
//    may have section break before encountering next actual measure
//---------------------------------------------------------

bool Measure::is_final_measure_of_section() const {
    const MeasureBase *mb = static_cast<const MeasureBase *>(this);

    do {
        if (mb->section_break()) {
            return true;
        }

        mb = mb->next();
    } while (mb && !mb->is_measure()); // loop until reach next actual measure or end of score

    return false;
}

LayoutBreak *Measure::section_break_element(bool include_next_frames) const {
    const MeasureBase *mb = static_cast<const MeasureBase *>(this);

    do {
        if (LayoutBreak *section_break = mb->section_break_element()) {
            return section_break;
        }

        mb = mb->next();
    } while (include_next_frames && mb && !mb->is_measure()); // loop until reach next actual measure or end of score

    return nullptr;
}

//---------------------------------------------------------
//   isAnacrusis
//---------------------------------------------------------

bool Measure::is_anacrusis() const {
    const MeasureBase *pm = prev();

    if (exclude_from_numbering() || !pm || pm->is_box() || pm->line_break() || pm->page_break() || pm->section_break()) {
        if (timesig() - ticks() > Fraction(0, 1)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   isFirstInSystem
//---------------------------------------------------------

bool Measure::is_first_in_system() const {
    IF_ASSERT_FAILED(system()) { return false; }
    return system()->first_measure() == this;
}

bool Measure::is_last_in_system() const {
    IF_ASSERT_FAILED(system()) { return false; }
    return system()->last_measure() == this;
}

//---------------------------------------------------------
//   isFirstInSection
//---------------------------------------------------------

bool Measure::is_first_in_section() const {
    for (MeasureBase *m = prev(); m; m = m->prev()) {
        if (m->section_break()) {
            return true;
        } else if (m->is_measure()) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Measure::scan_elements(std::function<void(EngravingItem *)> func) {
    size_t nstaves = score()->nstaves();
    if (nstaves == 0) {
        return;
    }

    MeasureBase::scan_elements(func);

    for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        MStaff *ms = mstaves_[staff_idx];
        MeasureNumber *measure_number = ms->measure_number();
        if (measure_number && measure_number->system_flag() && show_measure_number_on_staff(staff_idx)) {
            func(measure_number);
        }

        if (!(visible(staff_idx) && score()->staff(staff_idx)->show()) && !is_cutaway_clef(staff_idx)) {
            continue;
        }

        if (measure_number && !measure_number->system_flag()) {
            func(measure_number);
        }

        func(ms->lines());
        if (ms->vspacer_up()) {
            func(ms->vspacer_up());
        }
        if (ms->vspacer_down()) {
            func(ms->vspacer_down());
        }

        if (ms->mm_range_text()) {
            func(ms->mm_range_text());
        }
    }

    for (Segment *s = first(); s; s = s->next()) {
        if (!s->enabled()) {
            continue;
        }
        s->scan_elements(func);
    }
}

//---------------------------------------------------------
//   connectTremolo
///   Connect two-notes tremolo and update duration types
///   for the involved chords.
//---------------------------------------------------------

void Measure::connect_tremolo() {
    const size_t ntracks = score()->ntracks();
    constexpr SegmentType st = SegmentType::CHORD_REST;
    for (Segment *s = first(st); s; s = s->next(st)) {
        for (track_idx_t i = 0; i < ntracks; ++i) {
            EngravingItem *e = s->element(i);
            if (!e || !e->is_chord()) {
                continue;
            }

            Chord *c = to_chord(e);
            TremoloTwoChord *tremolo = c->tremolo_two_chord();
            if (tremolo) {
                // Ensure correct duration type for chord
                c->set_duration_type(tremolo->duration_type());

                // If it is the first tremolo's chord, find the second
                // chord for tremolo, if needed.
                if (!tremolo->chord1()) {
                    tremolo->set_chords(c, tremolo->chord2());
                } else if (tremolo->chord1() != c || tremolo->chord2()) {
                    continue;
                }

                for (Segment *ls = s->next(st); ls; ls = ls->next(st)) {
                    if (EngravingItem *element = ls->element(i)) {
                        if (!element->is_chord()) {
                            LOGD("cannot connect tremolo");
                            continue;
                        }
                        Chord *nc = to_chord(element);
                        tremolo->set_chords(c, nc);
                        nc->set_tremolo_two_chord(tremolo);
                        break;
                    }
                }

                if (!tremolo->chord2()) {
                    // this is an invalid tremolo! a continued tremolo was started on one note without a valid next note in that measure
                    // remove the tremolo entirely
                    c->set_tremolo_two_chord(nullptr);
                    score()->remove_element(tremolo);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   createVoice
//    Create a voice on demand by filling the measure
//    with a whole measure rest.
//    Check if there are any chord/rests in track; if
//    not create a whole measure rest
//---------------------------------------------------------

void Measure::create_voice(int track) {
    for (Segment *s = first(); s; s = s->next()) {
        if (s->segment_type() != SegmentType::CHORD_REST) {
            continue;
        }
        if (s->element(track) == 0) {
            score()->set_rest(s->tick(), track, ticks(), true, 0);
        }
        break;
    }
}

//---------------------------------------------------------
//   sortStaves
//---------------------------------------------------------

void Measure::sort_staves(std::vector<staff_idx_t> &dst) {
    std::vector<MStaff *> ms;
    ms.reserve(dst.size());
    for (staff_idx_t idx : dst) {
        ms.push_back(mstaves_[idx]);
    }
    mstaves_ = ms;

    for (staff_idx_t staff_idx = 0; staff_idx < mstaves_.size(); ++staff_idx) {
        mstaves_[staff_idx]->lines()->set_track(staff_idx * ::mu::engraving::voices);
    }
    for (Segment &s : segments_) {
        s.sort_staves(dst);
    }

    for (EngravingItem *e : el()) {
        if (e->track() == muse::nidx || e->is_top_system_object()) {
            continue;
        }
        voice_idx_t voice = e->voice();
        staff_idx_t staff_idx = e->staff_idx();
        staff_idx_t idx = muse::index_of(dst, staff_idx);
        e->set_track(idx * ::mu::engraving::voices + voice);
    }

    score()->undo_update_play_count_text(this);
}

//---------------------------------------------------------
//   checkMultiVoices
///   Check for more than on voice in this measure and staff and
///   set MStaff->hasVoices
//---------------------------------------------------------

void Measure::check_multi_voices(staff_idx_t staff_idx) {
    if (has_voices(staff_idx, tick(), ticks())) {
        mstaves_[staff_idx]->set_has_voices(true);
    } else {
        mstaves_[staff_idx]->set_has_voices(false);
    }
}

//---------------------------------------------------------
//   hasVoices
//---------------------------------------------------------

bool Measure::has_voices(staff_idx_t staff_idx, Fraction stick, Fraction len, bool consider_invisible) const {
    Staff *st = score()->staff(staff_idx);
    if (st->is_tab_staff(stick)) {
        // TODO: tab staves use different rules for stem direction etc
        // see for example https://musescore.org/en/node/308371
        // we should consider coming up with a more comprehensive solution
        // but for now, we are forcing measures on tab staves to be consider as a whole -
        // either they have voices or not
        // (rather than checking tick ranges)
        stick = tick();
        len = stretched_len(st);
    }

    track_idx_t strack = staff_idx * ::mu::engraving::voices + 1;
    track_idx_t etrack = staff_idx * ::mu::engraving::voices + ::mu::engraving::voices;
    Fraction etick = stick + len;

    for (Segment *s = first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        if (s->tick() >= etick) {
            break;
        }
        for (track_idx_t track = strack; track < etrack; ++track) {
            ChordRest *cr = to_chord_rest(s->element(track));
            if (cr) {
                if (cr->end_tick() <= stick) {
                    continue;
                }
                if (consider_invisible) {
                    return true;
                }
                bool v = false;
                if (cr->is_chord()) {
                    Chord *c = to_chord(cr);

                    // consider a chord visible if stem, hook(s) or beam(s) are visible
                    if ((c->stem() && c->stem()->visible()) || (c->hook() && c->hook()->visible()) || (c->beam() && c->beam()->visible())) {
                        v = true;
                    } else {
                        // or any of its notes
                        for (Note *n : c->notes()) {
                            if (n->visible()) {
                                v = true;
                                break;
                            }
                        }
                    }
                } else if (cr->is_rest()) {
                    v = cr->visible() && !to_rest(cr)->is_gap();
                }
                if (v) {
                    return true;
                }
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasVoice
//---------------------------------------------------------

bool Measure::has_voice(track_idx_t track) const {
    if (track >= score()->ntracks()) {
        return false;
    }
    for (Segment *s = first(); s; s = s->next()) {
        if (s->segment_type() != SegmentType::CHORD_REST) {
            continue;
        }
        if (s->element(track)) {
            return true;
        }
    }
    return false;
}

//-------------------------------------------------------------------
//   isEmpty
///   Check if the measure is filled by a full-measure rest, or is
///   full of rests on this staff, that may have fermatas on them.
///   If staff is -1, then check for all staves.
//-------------------------------------------------------------------

bool Measure::is_empty(staff_idx_t staff_idx) const {
    if (is_mm_rest()) {
        return true;
    }
    track_idx_t strack = 0;
    track_idx_t etrack = 0;
    if (staff_idx == muse::nidx) {
        strack = 0;
        etrack = score()->nstaves() * ::mu::engraving::voices;
    } else {
        strack = staff_idx * ::mu::engraving::voices;
        etrack = strack + ::mu::engraving::voices;
    }
    if (is_measure_repeat_group(staff_idx)) {
        return false;
    }
    for (Segment *s = first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *e = s->element(track);
            if (e && !e->is_rest()) {
                return false;
            }
            // Check for cross-staff chords
            bool has_staves = score()->staff(track / ::mu::engraving::voices)->part()->staves().size() > 1;
            if (has_staves) {
                if (strack >= ::mu::engraving::voices) {
                    e = s->element(track - ::mu::engraving::voices);
                    if (e && !e->is_rest() && e->v_staff_idx() == staff_idx) {
                        return false;
                    }
                }
                if (etrack < score()->nstaves() * ::mu::engraving::voices) {
                    e = s->element(track + ::mu::engraving::voices);
                    if (e && !e->is_rest() && e->v_staff_idx() == staff_idx) {
                        return false;
                    }
                }
            }
        }
        for (EngravingItem *a : s->annotations()) {
            if (a && staff_idx == muse::nidx) {
                return false;
            }
            if (!a || a->system_flag() || !a->visible() || a->is_fermata()) {
                continue;
            }
            size_t atrack = a->track();
            if (atrack >= strack && atrack < etrack) {
                return false;
            }
        }
    }
    return true;
}

//---------------------------------------------------------
//   isCutawayClef
///    Check for empty measure with only
///    a Courtesy Clef before End Bar Line
//---------------------------------------------------------

bool Measure::is_cutaway_clef(staff_idx_t staff_idx) const {
    if (!score()->staff(staff_idx) || !mstaves_[staff_idx]) {
        return false;
    }
    bool empty = (score()->staff(staff_idx)->cutaway() && is_empty(staff_idx)) || !mstaves_[staff_idx]->visible();
    if (!empty) {
        return false;
    }
    track_idx_t strack;
    track_idx_t etrack;
    if (staff_idx == muse::nidx) {
        strack = 0;
        etrack = score()->nstaves() * ::mu::engraving::voices;
    } else {
        strack = staff_idx * ::mu::engraving::voices;
        etrack = strack + ::mu::engraving::voices;
    }
    // find segment before EndBarLine
    Segment *s = nullptr;
    for (Segment *ls = last(); ls; ls = ls->prev()) {
        if (ls->segment_type() == SegmentType::END_BAR_LINE) {
            s = ls->prev();
            break;
        }
    }
    while (s && (s->is_time_tick_type() || s->is_breath_type())) {
        s = s->prev();
    }
    if (!s) {
        return false;
    }
    for (track_idx_t track = strack; track < etrack; ++track) {
        EngravingItem *e = s->element(track);
        if (!e || !e->is_clef()) {
            continue;
        }
        if ((next_measure() && (next_measure()->system() == system())) || to_clef(e)->show_courtesy()) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   isFullMeasureRest
//    Check for an empty measure, filled with full measure
//    rests.
//---------------------------------------------------------

bool Measure::is_full_measure_rest() const {
    track_idx_t strack = 0;
    track_idx_t etrack = score()->nstaves() * ::mu::engraving::voices;

    Segment *s = first(SegmentType::CHORD_REST);
    for (track_idx_t track = strack; track < etrack; ++track) {
        EngravingItem *e = s->element(track);
        if (e) {
            if (!e->is_rest()) {
                return false;
            }
            Rest *rest = to_rest(e);
            if (rest->duration_type().type() != DurationType::V_MEASURE) {
                return false;
            }
        }
    }
    return true;
}

//---------------------------------------------------------
//   isEmpty
//---------------------------------------------------------

bool Measure::empty() const {
    if (exclude_from_numbering()) {
        return false;
    }
    int n = 0;
    track_idx_t tracks = mstaves_.size() * ::mu::engraving::voices;
    static const SegmentType st = SegmentType::CHORD_REST;
    for (const Segment *s = first(st); s; s = s->next(st)) {
        bool rest_found = false;
        for (track_idx_t track = 0; track < tracks; ++track) {
            if ((track % ::mu::engraving::voices) == 0 && !score()->staff(track / ::mu::engraving::voices)->show()) {
                track += ::mu::engraving::voices - 1;
                continue;
            }
            if (s->element(track)) {
                if (!s->element(track)->is_rest()) {
                    return false;
                }
                rest_found = true;
            }
        }
        if (rest_found) {
            ++n;
        }
        // measure is not empty if there is more than one rest
        if (n > 1) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   isOnlyRests
//---------------------------------------------------------

bool Measure::is_only_rests(track_idx_t track) const {
    static const SegmentType st = SegmentType::CHORD_REST;
    for (const Segment *s = first(st); s; s = s->next(st)) {
        if (s->segment_type() != st || !s->element(track)) {
            continue;
        }
        if (!s->element(track)->is_rest()) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   isOnlyGapRests
//---------------------------------------------------------

bool Measure::is_only_gap_rests(track_idx_t track) const {
    static const SegmentType st{SegmentType::CHORD_REST};
    for (const Segment *s = first(st); s; s = s->next(st)) {
        if (s->segment_type() != st || !s->element(track)) {
            continue;
        }
        if (!s->element(track)->is_rest() || !to_rest(s->element(track))->is_gap()) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   stretchedLen
//---------------------------------------------------------

Fraction Measure::stretched_len(const Staff *staff) const { return ticks() * staff->time_stretch(tick()); }

//---------------------------------------------------------
//   cloneMeasure
//---------------------------------------------------------

Measure *Measure::clone_measure(Score *sc, const Fraction &tick, TieMap *tie_map) {
    Measure *m = new Measure(sc);
    m->timesig_ = timesig_;
    m->len_ = len_;
    m->repeat_count_ = repeat_count_;

    assert(sc->staves().size() >=
           mstaves_.size()); // destination score we're cloning into must have at least as many staves as measure being cloned

    m->set_measure_number(measure_number());
    m->set_measure_number_offset(measure_number_offset());
    m->set_exclude_from_numbering(exclude_from_numbering());
    m->user_stretch_ = user_stretch_;
    m->break_multi_measure_rest_ = break_multi_measure_rest_;
    m->playback_count_ = playback_count_;

    m->set_tick(tick);
    m->set_line_break(line_break());
    m->set_page_break(page_break());
    m->set_section_break(section_break());

    m->set_header(header());
    m->set_trailer(trailer());

    size_t tracks = sc->nstaves() * ::mu::engraving::voices;
    TupletMap tuplet_map;

    for (Segment *oseg = first(); oseg; oseg = oseg->next()) {
        Segment *s = Factory::create_segment(m, oseg->segment_type(), oseg->rtick());
        s->set_enabled(oseg->enabled());
        s->set_visible(oseg->visible());
        s->set_header(oseg->header());
        s->set_trailer(oseg->trailer());

        m->segments_.push_back(s);
        for (track_idx_t track = 0; track < tracks; ++track) {
            EngravingItem *oe = oseg->element(track);
            for (EngravingItem *e : oseg->annotations()) {
                if (e->generated() || e->track() != track) {
                    continue;
                }
                EngravingItem *ne = e->clone();
                ne->set_track(track);
                ne->set_offset(e->offset());
                ne->set_score(sc);
                s->add(ne);
            }
            if (!oe) {
                continue;
            }
            EngravingItem *ne = oe->clone();
            if (oe->is_chord_rest()) {
                ChordRest *ocr = to_chord_rest(oe);
                ChordRest *ncr = to_chord_rest(ne);
                Tuplet *ot = ocr->tuplet();
                if (ot) {
                    Tuplet *nt = tuplet_map.find_new(ot);
                    if (nt == 0) {
                        nt = Factory::copy_tuplet(*ot);
                        nt->clear();
                        nt->set_track(track);
                        nt->set_score(sc);
                        nt->set_ownership_parent(m);
                        nt->set_tick(m->tick() + ot->rtick());
                        tuplet_map.add(ot, nt);
                    }
                    ncr->set_tuplet(nt);
                    nt->add(ncr);
                }
                if (oe->is_chord()) {
                    Chord *och = to_chord(ocr);
                    Chord *nch = to_chord(ncr);
                    size_t n = och->notes().size();
                    for (size_t i = 0; i < n; ++i) {
                        Note *on = och->notes().at(i);
                        Note *nn = nch->notes().at(i);
                        if (on->tie_for()) {
                            Tie *tie = on->tie_for()->clone();
                            tie->set_score(sc);
                            nn->set_tie_for(tie);
                            tie->set_start_note(nn);
                            tie_map->add(on->tie_for(), tie);
                        }
                        if (on->tie_back()) {
                            Tie *tie = tie_map->find_new(on->tie_back());
                            if (tie) {
                                nn->set_tie_back(tie);
                                tie->set_end_note(nn);
                            } else {
                                LOGD("cloneMeasure: cannot find tie, track %zu", track);
                            }
                        }
                    }
                }
            }
            ne->set_offset(oe->offset());
            ne->set_score(sc);
            s->add(ne);
        }
    }
    for (EngravingItem *e : el()) {
        EngravingItem *ne = e->clone();
        ne->set_score(sc);
        ne->set_offset(e->offset());
        m->add(ne);
    }
    return m;
}

//---------------------------------------------------------
//   snap
//---------------------------------------------------------

Fraction Measure::snap(const Fraction &tick, const PointF p) const {
    Segment *s = first();
    for (; s->next(); s = s->next()) {
        double x = s->x();
        double dx = s->next()->x() - x;
        if (s->tick() == tick) {
            x += dx / 3.0 * 2.0;
        } else if (s->next()->tick() == tick) {
            x += dx / 3.0;
        } else {
            x += dx * .5;
        }
        if (p.x() < x) {
            break;
        }
    }
    return s->tick();
}

//---------------------------------------------------------
//   snapNote
//---------------------------------------------------------

Fraction Measure::snap_note(const Fraction & /*tick*/, const PointF p, int staff) const {
    Segment *s = first();
    for (;;) {
        Segment *ns = s->next();
        while (ns && ns->element(staff) == 0) {
            ns = ns->next();
        }
        if (ns == 0) {
            break;
        }
        double x = s->x();
        double nx = x + (ns->x() - x) * .5;
        if (p.x() < nx) {
            break;
        }
        s = ns;
    }
    return s->tick();
}

//---------------------------------------------------------
//   searchSegment
///   Finds a segment which x position is most close to the
///   given \p x.
///   \param x The x coordinate in measure coordinates.
///   \param st Type of segments to search.
///   \param strack start of track range (strack included)
///   in which the found segment should contain elements.
///   \param etrack end of track range (etrack excluded)
///   in which the found segment should contain elements.
///   \param preferredSegment If not nullptr, will give
///   more space to the given segment when searching it by
///   coordinate.
///   \returns The segment that was found.
//---------------------------------------------------------

Segment *Measure::search_segment(double x, SegmentType st, track_idx_t strack, track_idx_t etrack, const Segment *preferred_segment,
                                 double spacing_factor) const {
    const track_idx_t last_track = etrack - 1;
    for (Segment *segment = first(st); segment; segment = segment->next(st)) {
        if (!segment->has_elements(strack, last_track)) {
            continue;
        }
        if (segment->is_time_tick_type() && segment->rtick() == ticks()) {
            continue;
        }
        Segment *ns = segment->next(st);
        for (; ns; ns = ns->next(st)) {
            if (ns->has_elements(strack, last_track)) {
                break;
            }
        }
        if (!ns) {
            return segment;
        }
        if (preferred_segment == segment) {
            if (x < (segment->x() + (ns->x() - segment->x()))) {
                return segment;
            }
        } else if (preferred_segment == ns) {
            if (x <= segment->x()) {
                return segment;
            }
        } else {
            if (x < (segment->x() + (ns->x() - segment->x()) * spacing_factor)) {
                return segment;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Measure::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TIMESIG_NOMINAL: return PropertyValue::from_value(timesig_);
        case Pid::TIMESIG_ACTUAL: return PropertyValue::from_value(len_);
        case Pid::EXCLUDE_FROM_NUMBERING: return exclude_from_numbering();
        case Pid::MEASURE_NUMBER_OFFSET: return measure_number_offset();
        case Pid::MEASURE_NUMBER_MODE: return int(measure_number_mode());
        case Pid::BREAK_MMR: return break_multi_measure_rest();
        case Pid::REPEAT_COUNT: return repeat_count();
        case Pid::USER_STRETCH: return user_stretch();
        case Pid::REPEAT_END: return repeat_end();
        case Pid::REPEAT_START: return repeat_start();
        case Pid::REPEAT_JUMP: return repeat_jump();
        default: return MeasureBase::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Measure::set_property(Pid property_id, const PropertyValue &value) {
    switch (property_id) {
        case Pid::TIMESIG_NOMINAL: timesig_ = value.value<Fraction>(); break;
        case Pid::TIMESIG_ACTUAL: len_ = value.value<Fraction>(); break;
        case Pid::EXCLUDE_FROM_NUMBERING:
            set_exclude_from_numbering(value.to_bool());
            trigger_layout_all();
            return true;
        case Pid::MEASURE_NUMBER_OFFSET:
            set_measure_number_offset(value.to_int());
            trigger_layout_all();
            return true;
        case Pid::MEASURE_NUMBER_MODE: set_measure_number_mode(MeasureNumberMode(value.to_int())); break;
        case Pid::BREAK_MMR: set_break_multi_measure_rest(value.to_bool()); break;
        case Pid::REPEAT_COUNT:
            set_repeat_count(value.to_int());
            score()->undo_update_play_count_text(this);
            break;
        case Pid::USER_STRETCH: set_user_stretch(value.to_double()); break;
        case Pid::REPEAT_END:
            set_repeat_end(value.to_bool());
            score()->invalidate_repeat_list();
            break;
        case Pid::REPEAT_START:
            set_repeat_start(value.to_bool());
            score()->invalidate_repeat_list();
            break;
        case Pid::REPEAT_JUMP:
            set_repeat_jump(value.to_bool());
            score()->invalidate_repeat_list();
            break;
        default: return MeasureBase::set_property(property_id, value);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Measure::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::TIMESIG_NOMINAL:
        case Pid::TIMESIG_ACTUAL: return PropertyValue();
        case Pid::EXCLUDE_FROM_NUMBERING: return false;
        case Pid::MEASURE_NUMBER_OFFSET: return 0;
        case Pid::MEASURE_NUMBER_MODE: return int(MeasureNumberMode::AUTO);
        case Pid::BREAK_MMR: return false;
        case Pid::REPEAT_COUNT: return 2;
        case Pid::USER_STRETCH: return 1.0;
        case Pid::REPEAT_END:
        case Pid::REPEAT_START:
        case Pid::REPEAT_JUMP: return false;
        default: break;
    }
    return MeasureBase::property_default(property_id);
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void Measure::undo_change_property(Pid id, const PropertyValue &v) { undo_change_property(id, v, property_flags(id)); }

void Measure::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if ((get_property(id) == v) && (property_flags(id) == ps)) {
        return;
    }
    if (id == Pid::REPEAT_START || id == Pid::REPEAT_END) {
        // change also coresponding mmRestMeasure
        Measure *m = const_cast<Measure *>(to_measure(this)->covering_mm_rest_or_this());
        if (m != this) {
            m->undo_change_property(id, v);
        }
    }
    EngravingObject::undo_change_property(id, v, ps);
}

//-------------------------------------------------------------------
//   mmRestFirst
//    this is a multi measure rest
//    returns first measure of replaced sequence of empty measures
//-------------------------------------------------------------------

Measure *Measure::mm_rest_first() const {
    assert(is_mm_rest());
    if (prev()) {
        return to_measure(prev()->next());
    }
    return score()->first_measure();
}

//-------------------------------------------------------------------
//   mmRestLast
//    this is a multi measure rest
//    returns last measure of replaced sequence of empty measures
//-------------------------------------------------------------------

Measure *Measure::mm_rest_last() const {
    assert(is_mm_rest());
    if (next()) {
        return to_measure(next()->prev());
    }
    return score()->last_measure();
}

//---------------------------------------------------------
//   coveringMMRestOrThis
//    if multi-measure rests are enabled,
//        and this measure is covered by an MMRest,
//            return that MMRest.
//    otherwise, return the measure itself.
//---------------------------------------------------------

Measure *Measure::covering_mm_rest_or_this() {
    if (!style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
        return this;
    }

    if (mm_rest_) {
        return mm_rest_;
    }

    if (is_mm_rest()) {
        return this;
    }

    const Measure *m = this;
    while (m && !m->mm_rest_) {
        m = m->prev_measure();
    }

    if (m && !m->mm_rest_->mm_rest_last()->is_before(this)) {
        return m->mm_rest_;
    }

    return this;
}

const Measure *Measure::covering_mm_rest_or_this() const { return const_cast<Measure *>(this)->covering_mm_rest_or_this(); }

int Measure::measure_repeat_count(staff_idx_t staff_idx) const {
    if (staff_idx >= mstaves_.size()) {
        return 0;
    }

    return mstaves_[staff_idx]->measure_repeat_count();
}

bool Measure::contains_measure_repeat(const staff_idx_t staff_idx_from, const staff_idx_t staff_idx_to) const {
    for (staff_idx_t idx = staff_idx_from; idx <= staff_idx_to; ++idx) {
        if (measure_repeat_count(idx) > 0) {
            return true;
        }
    }

    return false;
}

void Measure::set_measure_repeat_count(int n, staff_idx_t staff_idx) {
    if (staff_idx >= mstaves_.size()) {
        return;
    }

    mstaves_[staff_idx]->set_measure_repeat_count(n);
}

bool Measure::is_measure_repeat_group(staff_idx_t staff_idx) const { return measure_repeat_count(staff_idx) > 0; }

//---------------------------------------------------------
//   isMeasureRepeatGroupWithNextM
//    true if this and next measure are part of same MeasureRepeat group
//---------------------------------------------------------

bool Measure::is_measure_repeat_group_with_next_m(staff_idx_t staff_idx) const {
    if (!is_measure_repeat_group(staff_idx) || !next_measure() || !next_measure()->is_measure_repeat_group(staff_idx)) {
        return false;
    }
    if (measure_repeat_count(staff_idx) == next_measure()->measure_repeat_count(staff_idx) - 1) {
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   isMeasureRepeatGroupWithPrevM
//    true if this and prev measure are part of same MeasureRepeat group
//---------------------------------------------------------

bool Measure::is_measure_repeat_group_with_prev_m(staff_idx_t staff_idx) const { return measure_repeat_count(staff_idx) > 1; }

//---------------------------------------------------------
//   firstMeasureOfGroup
//    for measures within group containing MeasureRepeat,
//    return the measure (possibly this) at start of group
//---------------------------------------------------------

Measure *Measure::first_of_measure_repeat_group(staff_idx_t staff_idx) const {
    if (!is_measure_repeat_group(staff_idx)) {
        return nullptr;
    }
    Measure *m = const_cast<Measure *>(this);
    for (int i = 1; i < measure_repeat_count(staff_idx); ++i) {
        m = m->prev_measure();
    }
    return m;
}

//---------------------------------------------------------
//   measureRepeatElement
//    access MeasureRepeat element from anywhere in related group
//---------------------------------------------------------

MeasureRepeat *Measure::measure_repeat_element(staff_idx_t staff_idx) const {
    Measure *m = first_of_measure_repeat_group(staff_idx);
    if (!m) {
        return nullptr;
    }
    while (m && m->is_measure_repeat_group(staff_idx)) {
        track_idx_t strack = staff2track(staff_idx);
        track_idx_t etrack = staff2track(staff_idx + 1);
        for (track_idx_t track = strack; track < etrack; ++track) {
            // should only be in first track, but just in case
            for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                // should only be in first segment, but just in case
                EngravingItem *e = s->element(track);
                if (e && e->is_measure_repeat()) {
                    return to_measure_repeat(e);
                }
            }
        }
        Measure *nm = m->next_measure();
        if (nm && nm->measure_repeat_count(staff_idx) == m->measure_repeat_count(staff_idx) + 1) {
            m = nm;
        } else {
            break;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   measureRepeatNumMeasures
//---------------------------------------------------------

int Measure::measure_repeat_num_measures(staff_idx_t staff_idx) const {
    MeasureRepeat *el = measure_repeat_element(staff_idx);
    if (!el) {
        return 0;
    }
    return el->num_measures();
}

//---------------------------------------------------------
//   isOneMeasureRepeat
//---------------------------------------------------------

bool Measure::is_one_measure_repeat(staff_idx_t staff_idx) const { return measure_repeat_num_measures(staff_idx) == 1; }

//---------------------------------------------------------
//   nextIsOneMeasureRepeat
//---------------------------------------------------------

bool Measure::next_is_one_measure_repeat(staff_idx_t staff_idx) const {
    if (!next_measure()) {
        return false;
    }
    return next_measure()->is_one_measure_repeat(staff_idx);
}

//---------------------------------------------------------
//   prevIsOneMeasureRepeat
//---------------------------------------------------------

bool Measure::prev_is_one_measure_repeat(staff_idx_t staff_idx) const {
    if (!prev_measure()) {
        return false;
    }
    return prev_measure()->is_one_measure_repeat(staff_idx);
}

ChordRest *Measure::last_chord_rest(track_idx_t track) const {
    for (const Segment *seg = last(); seg; seg = seg->prev()) {
        if (!seg->is_chord_rest_type()) {
            continue;
        }
        ChordRest *cr = seg->cr(track);
        if (!cr) {
            continue;
        }

        return cr;
    }
    return nullptr;
}

ChordRest *Measure::first_chord_rest(track_idx_t track) const {
    for (const Segment *seg = first(); seg; seg = seg->next()) {
        if (!seg->is_chord_rest_type()) {
            continue;
        }
        ChordRest *cr = seg->cr(track);
        if (!cr) {
            continue;
        }

        return cr;
    }
    return nullptr;
}

//-------------------------------------------------------------------
//   userStretch
//-------------------------------------------------------------------

double Measure::user_stretch() const { return score()->layout_options().is_mode(LayoutMode::FLOAT) ? 1.0 : user_stretch_; }

//---------------------------------------------------------
//   nextElementStaff
//---------------------------------------------------------

EngravingItem *Measure::next_element_staff(staff_idx_t staff, EngravingItem *from_item) {
    EngravingItem *e = from_item ? from_item : score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }

    // handle measure elements
    if (e->ownership_parent() == this) {
        auto i = std::find(el().begin(), el().end(), e);
        if (i != el().end()) {
            if (++i != el().end()) {
                EngravingItem *res_element = *i;
                if (res_element) {
                    return res_element;
                }
            }
        }
    }

    for (; e && !e->is_segment(); e = e->layout_parent()) {}
    Segment *seg = to_segment(e);
    Segment *next_segment = seg ? seg->next() : first();
    EngravingItem *next = next_segment->first_element_of_segment(staff);
    if (next) {
        return next;
    }

    return Navigation::last_element(score());
}

//---------------------------------------------------------
//   prevElementStaff
//---------------------------------------------------------

EngravingItem *Measure::prev_element_staff(staff_idx_t staff, EngravingItem *from_item) {
    EngravingItem *e = from_item ? from_item : score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }

    // handle measure elements
    if (e->ownership_parent() == this) {
        auto i = std::find(el().rbegin(), el().rend(), e);
        if (i != el().rend()) {
            if (++i != el().rend()) {
                EngravingItem *res_element = *i;
                if (res_element) {
                    return res_element;
                }
            }
        }
    }

    Measure *prev_m = prev_measure_mm();
    if (prev_m) {
        Segment *seg = prev_m->last();
        if (seg) {
            return seg->last_element_for_navigation(staff);
        }
    }
    return Navigation::first_element(score());
}

double Measure::first_note_rest_segment_x(bool leading) const {
    const System *sys = system();
    double margin = style().style_absolute(Sid::HEADER_TO_LINE_START_DISTANCE);
    for (const Segment *seg = first(); seg; seg = seg->next()) {
        if (seg->is_chord_rest_type()) {
            double note_rest_pos = seg->measure()->pos().x() + seg->pos().x();
            if (!leading) {
                return note_rest_pos;
            }

            // first CR found; back up to previous segment
            seg = seg->prev_active();
            while (seg && (seg->all_elements_invisible() || seg->has_time_sig_above_staves())) {
                seg = seg->prev_active();
            }
            if (seg) {
                // find maximum width
                double width = 0.0;
                size_t n = score()->nstaves();
                for (staff_idx_t i = 0; i < n; ++i) {
                    if (!sys->staff(i)->show()) {
                        continue;
                    }
                    EngravingItem *e = seg->element(i * ::mu::engraving::voices);
                    if (e && e->add_to_skyline()) {
                        width = std::max(width, e->pos().x() + e->ldata()->bbox().right());
                    }
                }
                if (seg->is_start_repeat_bar_line_type()) {
                    margin = style().style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE);
                }
                return std::min(seg->measure()->pos().x() + seg->pos().x() + width + margin, note_rest_pos);
            } else if (!is_first_in_system() && !is_first_in_section() && prev_measure()) {
                const BarLine *end_bl = prev_measure()->end_bar_line();
                const Segment *end_bl_seg = end_bl ? end_bl->segment() : nullptr;

                if (!end_bl_seg) {
                    return note_rest_pos;
                }

                double width = 0.0;
                if (end_bl && end_bl->add_to_skyline()) {
                    width = std::max(width, end_bl->pos().x() + end_bl->ldata()->bbox().right());
                }

                const double start_bl_margin = style().style_absolute(Sid::BARLINE_TO_LINE_START_DISTANCE);

                return std::min(end_bl_seg->measure()->pos().x() + end_bl_seg->pos().x() + width + start_bl_margin, note_rest_pos);
            } else {
                return note_rest_pos;
            }
        }
    }

    return margin;
}

double Measure::ending_x_for_open_ended_lines() const {
    const System *sys = system();
    double margin = style().style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE);
    double system_end_x = sys->ldata()->bbox().width();

    Segment *last_seg = last();
    while (last_seg && !last_seg->is_type(SegmentType::BAR_LINE_TYPES)) {
        last_seg = last_seg->prev_enabled();
    }
    if (!last_seg) {
        return system_end_x - margin;
    }

    return last_seg->x() + x() - margin;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

void Measure::style_changed() {
    if (end_bar_line_type() == BarLineType::END_REPEAT) {
        score()->undo_update_play_count_text(this);
    }
    MeasureBase::style_changed();
}

//---------------------------------------------------
//    computeTicks
//    set ticks for all segments
//       return minTick
//---------------------------------------------------

void Measure::compute_ticks() {
    for (Segment *segment = first_active(); segment; segment = segment->next_active()) {
        Segment *next_segment = segment->next_active();
        Fraction next_tick = next_segment ? next_segment->rtick() : ticks();
        segment->set_ticks(next_tick - segment->rtick());
    }

    for (Segment *segment = first(SegmentType::TIME_TICK); segment; segment = segment->next(SegmentType::TIME_TICK)) {
        segment->set_ticks(Fraction(0, 1));
        Segment *next_segment = segment->next();
        while (next_segment) {
            Fraction tick_diff = next_segment->rtick() - segment->rtick();
            if (!tick_diff.is_zero()) {
                segment->set_ticks(tick_diff);
                break;
            }
            next_segment = next_segment->next();
        }
        if (segment->ticks().is_zero()) {
            segment->set_ticks(ticks() - segment->rtick());
        }
    }
}

//---------------------------------------------------------
//   anacrusisOffset
//      determine if measure is anacrusis
//      and return tick offset relative to measure end
//---------------------------------------------------------

Fraction Measure::anacrusis_offset() const { return is_anacrusis() ? (timesig() - ticks()) : Fraction(0, 1); }

//---------------------------------------------------------
//   endBarLine
//      return the first one
//---------------------------------------------------------

const BarLine *Measure::end_bar_line() const { return end_bar_line(0, true); }

const BarLine *Measure::end_bar_line(staff_idx_t staff_idx, bool first) const {
    // search barline segment:
    Segment *s = last();
    while (s && !s->is_end_bar_line_type()) {
        s = s->prev();
    }

    if (s) {
        for (const EngravingItem *e : s->elist()) {
            // Return first barline or barline with matching staffIdx
            if (e && (e->staff_idx() == staff_idx || first)) {
                return to_bar_line(e);
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   endBarLineType
//    Assume all barlines have same type if there is more
//    than one.
//---------------------------------------------------------

BarLineType Measure::end_bar_line_type() const {
    const BarLine *bl = end_bar_line();
    return bl ? bl->bar_line_type() : BarLineType::NORMAL;
}

//---------------------------------------------------------
//   endBarLineVisible
//    Assume all barlines have same visibility if there is more
//    than one.
//---------------------------------------------------------

bool Measure::end_bar_line_visible() const {
    const BarLine *bl = end_bar_line();
    return bl ? bl->visible() : true;
}

const BarLine *Measure::start_bar_line() const { return start_bar_line(0, true); }

const BarLine *Measure::start_bar_line(staff_idx_t staff_idx, bool first_staff) const {
    // search barline segment:
    Segment *s = first();
    while (s && !(s->is_start_repeat_bar_line_type() || s->is_begin_bar_line_type())) {
        s = s->next();
    }
    // search first element
    if (s) {
        for (const EngravingItem *e : s->elist()) {
            if (e && (e->staff_idx() == staff_idx || first_staff)) {
                return to_bar_line(e);
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void Measure::trigger_layout() const {
    if (prev() || next()) { // avoid triggering layout before getting added to a score
        score()->set_layout(tick(), end_tick(), 0, score()->nstaves() - 1, this);
    }
}

void Measure::trigger_layout(staff_idx_t staff_idx) const { score()->set_layout(tick(), end_tick(), staff_idx, staff_idx, this); }

//---------------------------------------------------------
//   setEndBarLineType
//     Create a *generated* barline with the given type and
//     properties if none exists. Modify if it exists.
//     Useful for import filters.
//---------------------------------------------------------

void Measure::set_end_bar_line_type(BarLineType val, track_idx_t track, bool visible, Color color) {
    Segment *seg = undo_get_segment(SegmentType::END_BAR_LINE, end_tick());
    // get existing bar line for this staff, if any
    BarLine *bl = to_bar_line(seg->element(track));
    if (!bl) {
        // no suitable bar line: create a new one
        bl = Factory::create_bar_line(seg);
        bl->set_track(track);
        Part *part = score()->staff(track / ::mu::engraving::voices)->part();
        // by default, barlines for multi-staff parts should span across staves
        if (part && part->nstaves() > 1) {
            bl->set_span_staff(true);
        }
        score()->add_element(bl);
    }
    bl->set_generated(false);
    bl->set_bar_line_type(val);
    bl->set_visible(visible);
    if (color.is_valid()) {
        bl->set_color(color);
    }
}

//---------------------------------------------------------
//   checkHeader
//---------------------------------------------------------

void Measure::check_header() {
    for (Segment *seg = first(); seg; seg = seg->next()) {
        if (seg->enabled() && seg->header()) {
            set_header(seg->header());
            break;
        }
    }
}

//---------------------------------------------------------
//   checkTrailer
//---------------------------------------------------------

void Measure::check_trailer() {
    for (Segment *seg = last(); seg != first(); seg = seg->prev()) {
        if (seg->enabled() && seg->trailer()) {
            set_trailer(seg->trailer());
            break;
        }
    }
}

void Measure::check_end_of_measure_change() {
    bool found = false;
    for (Segment *seg = last(); seg != first(); seg = seg->prev()) {
        if (seg->enabled() && seg->end_of_measure_change()) {
            set_end_of_measure_change(seg->end_of_measure_change());
            found = true;
            break;
        }
    }

    if (!found) {
        set_end_of_measure_change(false);
    }
}

bool Measure::can_add_string_tunings(staff_idx_t staff_idx) const {
    Staff *staff = score()->staff(staff_idx);
    if (!staff) {
        return false;
    }

    if (!staff->is_primary_staff()) {
        staff = staff->primary_staff();
        if (!staff) {
            return false;
        }

        staff_idx = staff->idx();
    }

    const StringData *string_data = staff->origin_part(tick())->instrument(tick())->string_data();
    if (!string_data || string_data->fretted_strings() == 0) {
        return false;
    }

    // already a string tunings element in this measure
    bool already_has_string_tunings = false;
    for (const Segment &segment : segments_) {
        for (EngravingItem *element : segment.annotations()) {
            if (element && element->is_string_tunings() && element->staff_idx() == staff_idx) {
                already_has_string_tunings = true;
                break;
            }
        }
    }

    return !already_has_string_tunings;
}

bool Measure::can_add_staff_type_change(staff_idx_t staff_idx) const {
    if (is_mm_rest()) {
        return false;
    }

    for (const EngravingObject *child : el()) {
        if (!child || !child->is_staff_type_change()) {
            continue;
        }

        const StaffTypeChange *stc = to_staff_type_change(child);
        if (stc->staff_idx() == staff_idx) {
            // Staff already has a StaffTypeChange at this measure...
            return false;
        }
    }

    return true;
}

Fraction Measure::max_ticks() const {
    Segment *s = first();
    Fraction maxticks = Fraction(0, 1);
    if (is_mm_rest()) {
        return timesig();
    }
    while (s) {
        if (s->enabled() && s->is_chord_rest_type()) {
            maxticks = std::max(maxticks, s->ticks());
        }
        s = s->next();
    }
    return maxticks;
}

void Measure::respace_segments() {
    double x = 0.0;
    // Find starting x position (i.e. position of first relevant segment)
    for (Segment &s : segments_) {
        if (s.enabled() && s.visible() && !s.all_elements_invisible()) {
            x = s.x();
            break;
        }
    }
    // Start respacing segments
    for (Segment &s : segments_) {
        if (s.is_time_tick_type()) {
            continue;
        }
        s.mutldata()->set_pos_x(x);
        if (s.enabled() && s.visible() && !s.all_elements_invisible()) {
            x += s.width(LdAccess::BAD);
        }
    }
    // Update measure width
    set_width(x);
}
} // namespace mu::engraving
