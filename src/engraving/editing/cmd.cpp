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
 Handling of several GUI commands.
*/

#include <assert.h>

#include <amc/flatset.hpp>

#include "../dom/accidental.h"
#include "../dom/articulation.h"
#include "../dom/barline.h"
#include "../dom/box.h"
#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/clef.h"
#include "../dom/drumset.h"
#include "../dom/durationtype.h"
#include "../dom/dynamic.h"
#include "../dom/factory.h"
#include "../dom/glissando.h"
#include "../dom/guitarbend.h"
#include "../dom/hairpin.h"
#include "../dom/harmony.h"
#include "../dom/key.h"
#include "../dom/laissezvib.h"
#include "../dom/layoutbreak.h"
#include "../dom/linkedobjects.h"
#include "../dom/lyrics.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/mscore.h"
#include "../dom/note.h"
#include "../dom/ornament.h"
#include "../dom/page.h"
#include "../dom/part.h"
#include "../dom/pitchspelling.h"
#include "../dom/rehearsalmark.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/sig.h"
#include "../dom/slur.h"
#include "../dom/spacer.h"
#include "../dom/staff.h"
#include "../dom/stafftype.h"
#include "../dom/stafftypechange.h"
#include "../dom/stem.h"
#include "../dom/stringdata.h"
#include "../dom/system.h"
#include "../dom/tapping.h"
#include "../dom/tie.h"
#include "../dom/timesig.h"
#include "../dom/tremolotwochord.h"
#include "../dom/tuplet.h"
#include "../dom/utils.h"
#include "editchord.h"
#include "editnote.h"
#include "editpagelocks.h"
#include "editspanner.h"
#include "editsystemlocks.h"
#include "engraving/dom/dummyparent.h"
#include "infrastructure/messagebox.h"
#include "log.h"
#include "navigation.h"
#include "noteinput.h"
#include "rw/xmlreader.h"
#include "style/style.h"
#include "transaction/transaction.h"
#include "translation.h"
#include "transpose.h"

using namespace muse::io;
using namespace mu::engraving;

namespace mu::engraving {
void CmdState::reset() {
    changes.clear();
    mutation_count = 0;
    excerpts_changed = false;
    instruments_changed = false;
    layout_flags = LayoutFlag::NO_FLAGS;
    update_mode_ = UpdateMode::DO_NOTHING;
    start_tick_ = Fraction(-1, 1);
    end_tick_ = Fraction(-1, 1);

    start_staff_ = muse::nidx;
    end_staff_ = muse::nidx;
    el_ = nullptr;
    one_element_ = true;
    mb_ = nullptr;
    one_measure_base_ = true;
    locked_ = false;
}

void CmdState::set_tick(const Fraction &t) {
    if (locked_) {
        return;
    }

    if (start_tick_ == Fraction(-1, 1) || t < start_tick_) {
        start_tick_ = t;
    }
    if (end_tick_ == Fraction(-1, 1) || t > end_tick_) {
        end_tick_ = t;
    }
    set_update_mode(UpdateMode::LAYOUT);
}

void CmdState::set_staff(staff_idx_t st) {
    if (locked_ || st == muse::nidx) {
        return;
    }

    if (start_staff_ == muse::nidx || st < start_staff_) {
        start_staff_ = st;
    }
    if (end_staff_ == muse::nidx || st > end_staff_) {
        end_staff_ = st;
    }
}

void CmdState::set_measure_base(const MeasureBase *mb) {
    if (!mb || mb_ == mb || locked_) {
        return;
    }

    one_measure_base_ = !mb_;
    mb_ = mb;
}

void CmdState::set_element(const EngravingItem *e) {
    if (!e || el_ == e || locked_) {
        return;
    }

    one_element_ = !el_;
    el_ = e;

    if (one_measure_base_) {
        set_measure_base(e->find_measure_base());
    }
}

void CmdState::unset_element(const EngravingItem *e) {
    changes.changed_objects.erase(const_cast<EngravingItem *>(e));
    if (el_ == e) {
        el_ = nullptr;
    }
    if (mb_ == e) {
        mb_ = nullptr;
    }
}

void CmdState::assign_update_mode(UpdateMode m) { update_mode_ = m; }

void CmdState::set_update_mode(UpdateMode m) {
    if (int(m) > int(update_mode_)) {
        assign_update_mode(m);
    }
}

void CmdState::delete_later(EngravingObject *e) { postponed_deletions_.push_back(e); }

std::vector<EngravingObject *> CmdState::take_postponed_deletions() {
    std::vector<EngravingObject *> result;
    result.swap(postponed_deletions_);
    return result;
}

void CmdState::flush_postponed_deletions() {
    for (auto batch = take_postponed_deletions(); !batch.empty(); batch = take_postponed_deletions()) {
        for (EngravingObject *e : batch) {
            if (e->is_system()) {
                System *s = to_system(e);
                std::list<SpannerSegment *> spanners = s->spanner_segments();
                for (SpannerSegment *ss : spanners) {
                    if (ss->system() == s) {
                        ss->move_to_system(nullptr);
                    }
                }
            }
            delete e;
        }
    }
}

///   Start a GUI command by clearing the redraw area
///   and starting a user-visible undo.

void Score::start_cmd(const TranslatableString &action_name) { master_score()->transaction_manager()->begin_transaction(action_name); }

///   End a GUI command by (if \a undo) ending a user-visible undo
///   and (always) updating the redraw area.

void Score::end_cmd(bool failed) { master_score()->transaction_manager()->end_transaction(failed); }

//    Deprecated: use Transaction::push instead.

#ifndef NDEBUG

#endif

//    layout & update

void Score::update() { master_score()->update(); }

void MasterScore::update(bool reset_cmd_state, NormalizeMode mode) {
    if (updates_locked_) {
        return;
    }

    cmd_state_.flush_postponed_deletions();
    normalize_document(*this, {.mode = mode});

    if (cmd_state_.layout_range()) invalidate_layouts(cmd_state_.start_tick(), cmd_state_.end_tick());

    if (need_update_ticks_and_time_sig_map()) {
        update_ticks_and_time_sig_map();
    }

    for (Score *s : score_list()) {
        s->clear_refresh_rect();
    }

    if (reset_cmd_state) {
        cmd_state_.reset();
    }

    for (Score *score : score_list()) {
        Selection &sel = score->selection();
        if (sel.is_range()) {
            sel.update_selected_elements();
        }
    }
}

//    fills gaps in voice with rests,
//    from previous cr (or beginning of measure) to next cr (or end of measure)

void Score::expand_voice(Segment *s, track_idx_t track) {
    if (!s || track == muse::nidx) {
        LOGD("expand voice: no segment or invalid track");
        return;
    }
    if (s->element(track)) {
        return;
    }

    // find previous segment with cr in this track
    Segment *ps;
    for (ps = s; ps; ps = ps->prev(SegmentType::CHORD_REST)) {
        if (ps->element(track)) {
            break;
        }
    }
    if (ps) {
        ChordRest *cr = to_chord_rest(ps->element(track));
        Fraction tick = cr->end_tick();
        if (tick > s->tick()) {
            // previous cr extends past current segment
            LOGD("expandVoice: cannot insert element here");
            return;
        }
        if (cr->is_chord()) {
            // previous cr ends on or before current segment
            // for chords, move ps to just after cr ends
            // so we can fill any gap that might exist
            // but don't move ps if previous cr is a rest
            // this will be combined with any new rests needed to fill up to s->tick() below
            ps = ps->measure()->undo_get_segment(SegmentType::CHORD_REST, tick);
        }
    }
    //
    // fill up to s->tick() with rests
    //
    Measure *m = s->measure();
    Fraction stick = ps ? ps->tick() : m->tick();

    const Staff *curr_staff = staff(track2staff(track));
    IF_ASSERT_FAILED(curr_staff) { return; }
    const Fraction stretch = curr_staff->time_stretch(stick);

    Fraction ticks = s->tick() - stick;
    if (ticks.is_not_zero()) {
        set_rest(stick, track, ticks * stretch, false, 0);
    }

    //
    // fill from s->tick() until next chord/rest in measure
    //
    Segment *ns;
    for (ns = s->next(SegmentType::CHORD_REST); ns; ns = ns->next(SegmentType::CHORD_REST)) {
        if (ns->element(track)) {
            break;
        }
    }
    ticks = ns ? (ns->tick() - s->tick()) : (m->ticks() - s->rtick());
    if (ticks == m->ticks()) {
        add_rest(s, track, TDuration(DurationType::V_MEASURE), 0);
    } else {
        set_rest(s->tick(), track, ticks * stretch, false, 0);
    }
}

Note *Score::set_grace_note(Chord *ch, int pitch, NoteType type, int len, std::size_t index) {
    TDuration duration;
    duration.set_val(len);
    return set_grace_note(ch, pitch, type, duration, index);
}

// The index is in storage order. The after-grace view traverses that order in reverse.
Note *Score::set_grace_note(Chord *ch, int pitch, NoteType type, const TDuration &duration, std::size_t index) {
    Chord *chord = Factory::create_chord(this->dummy());
    Note *note = Factory::create_note(chord);

    // allow grace notes to be added to other grace notes
    // by really adding to parent chord
    if (ch->note_type() != NoteType::NORMAL) {
        ch = to_chord(ch->ownership_parent());
    }

    chord->set_track(ch->track());
    chord->set_ownership_parent(ch);
    chord->add(note);

    // find corresponding note within chord and use its tpc information
    // if no note with same pitch found, derive tpc from pitch / key
    if (Note *n = ch->find_note(pitch)) {
        note->set_nval(n->note_val(), ch->tick());
    } else {
        note->set_pitch(pitch);
        note->set_tpc_from_pitch();
    }

    chord->set_duration_type(duration);
    chord->set_ticks(duration.fraction());
    chord->set_note_type(type);
    chord->set_grace_index(index);
    chord->set_show_stem_slash_in_advance();
    chord->mutldata()->set_mag(ch->staff()->staff_mag(chord->tick()) * style().style_d(Sid::GRACE_NOTE_MAG));

    undo_add_element(chord);
    return note;
}

GuitarBend *Score::add_guitar_bend(GuitarBendType type, Note *note, Note *end_note) {
    if (note->is_pre_bend_or_dive_start()) {
        if (type == GuitarBendType::BEND && note->staff_type()->is_tab_staff()) {
            GuitarBend *pre_bend = note->bend_for();
            Note *main_note = pre_bend ? pre_bend->end_note() : nullptr;
            if (main_note) {
                return add_guitar_bend(type, main_note, nullptr);
            }
        }
        return nullptr;
    }

    if (note->is_grace_bend_start() && type != GuitarBendType::PRE_BEND) {
        return nullptr;
    }

    if (GuitarBend *bend_back = note->bend_back();
        bend_back && !bend_back->is_dive() && (type == GuitarBendType::PRE_BEND || type == GuitarBendType::GRACE_NOTE_BEND)) {
        return nullptr;
    }

    if (GuitarBend *bend_for = note->bend_for();
        bend_for && !bend_for->is_dive() && (type == GuitarBendType::BEND || type == GuitarBendType::SLIGHT_BEND)) {
        return nullptr;
    }

    if (type == GuitarBendType::DIP) {
        for (Note *n : note->chord()->notes()) {
            if (GuitarBend *bend_for = n->bend_for(); bend_for && bend_for->bend_type() == GuitarBendType::DIP) {
                return nullptr; // Only one dip per chord
            }
        }
    }

    Chord *chord = note->chord();
    bool is_dive = static_cast<int>(type) >= static_cast<int>(GuitarBendType::DIVE) &&
                   static_cast<int>(type) <= static_cast<int>(GuitarBendType::SCOOP);

    if (type == GuitarBendType::BEND || type == GuitarBendType::DIVE) {
        for (Spanner *sp : note->spanner_for()) {
            if ((sp->is_guitar_bend() && to_guitar_bend(sp)->is_dive() == is_dive) || sp->is_glissando()) {
                return nullptr;
            }
        }

        if (!end_note) {
            end_note = SLine::guess_final_note(note);
        }

        bool suitable_end_note = end_note;
        if (end_note) {
            for (Spanner *sp : end_note->spanner_back()) {
                if (sp->is_tie() || sp->is_glissando() || (sp->is_guitar_bend() && to_guitar_bend(sp)->is_dive() == is_dive)) {
                    suitable_end_note = false;
                    break;
                }
            }
        }

        if (!suitable_end_note) {
            end_note = GuitarBend::create_end_note(note, type);
        }

        if (!end_note) {
            return nullptr;
        }
    }

    GuitarBend *bend = new GuitarBend(score()->dummy());
    bend->set_tick(chord->tick());
    bend->set_track(chord->track());

    if (type == GuitarBendType::BEND || type == GuitarBendType::DIVE) {
        bend->set_bend_type(chord->is_grace() && type == GuitarBendType::BEND ? GuitarBendType::GRACE_NOTE_BEND : type);
        bend->set_start_element(note);
        bend->set_tick2(end_note->tick());
        bend->set_track2(end_note->track());
        bend->set_end_element(end_note);
        bend->set_ownership_parent(note);
        if (type == GuitarBendType::BEND) {
            GuitarBend::fix_notes_fretting_for_standard_bend(note, end_note);
        }
    } else {
        bend->set_bend_type(type);
        bend->set_tick2(chord->tick());
        bend->set_track2(chord->track());

        if (type == GuitarBendType::PRE_BEND || type == GuitarBendType::GRACE_NOTE_BEND || type == GuitarBendType::PRE_DIVE) {
            const auto graces_before = chord->grace_notes_before();

            // Add to existing grace chord only if it already has a bend/dive ghost.
            const bool has_ghost_grace =
                !graces_before.empty() &&
                std::any_of(graces_before.back()->notes().begin(), graces_before.back()->notes().end(),
                            [](const Note *n) { return n->is_pre_bend_or_dive_start() || n->is_grace_bend_start(); });
            Note *grace_note = has_ghost_grace ? NoteInput::add_note(transaction_manager()->current_or_dummy_transaction(), this,
                                                                     graces_before.back(), note->note_val()) :
                                                 set_grace_note(chord, note->pitch(), NoteType::APPOGGIATURA, Constants::division / 2);
            grace_note->transpose_diatonic(type == GuitarBendType::PRE_DIVE ? 1 : -1, true, false);
            GuitarBend::fix_notes_fretting_for_grace_bend(grace_note, note);

            Chord *grace_chord = grace_note->chord();
            for (EngravingObject *item : grace_chord->link_list()) {
                Chord *linked_grace = to_chord(item);
                linked_grace->set_no_stem(true);
                linked_grace->set_beam_mode(BeamMode::NONE);
            }

            // Add bend
            bend->set_ownership_parent(grace_note);
            bend->set_start_element(grace_note);
            bend->set_end_element(note);
        } else if (type == GuitarBendType::SLIGHT_BEND || type == GuitarBendType::DIP || type == GuitarBendType::SCOOP) {
            bend->set_ownership_parent(note);
            bend->set_start_element(note);
            // Slight bends don't end on another note
            bend->set_end_element(note);
        }
    }

    Chord *start_chord = bend->start_note()->chord();
    if (start_chord->is_grace()) {
        for (EngravingObject *item : start_chord->link_list()) {
            Chord *linked_grace = to_chord(item);
            if (linked_grace->staff_type()->is_tab_staff()) {
                linked_grace->set_no_stem(true);
                linked_grace->set_beam_mode(BeamMode::NONE);
            }
        }
    }

    score()->undo_add_element(bend);

    if (GuitarBend *overlapping = bend->overlapping_bend_or_dive()) {
        int half_bend_amount = std::floor(overlapping->bend_amount_in_quarter_tones() / 2);
        if (bend->is_dive()) {
            bend->undo_change_property(Pid::GUITAR_BEND_AMOUNT, half_bend_amount);
        } else {
            overlapping->undo_change_property(Pid::GUITAR_BEND_AMOUNT, half_bend_amount);
        }
    }

    return bend;
}

//    pitch == -1  -> set rest
//    "sd" is in local (stretched) time
//    return segment of last created note/rest

Segment *Score::set_note_rest_at(Segment *segment, track_idx_t track, NoteVal nval, Fraction sd, DirectionV stem_direction,
                                 bool force_accidental, const amc::FlatSet<SymId> &articulation_ids, bool rhythmic,
                                 EngravingItem **last_entered) {
    assert(segment->segment_type() == SegmentType::CHORD_REST);
    bool is_rest = nval.is_rest();
    Fraction tick = segment->tick();
    EngravingItem *nr = nullptr;
    Tie *tie = nullptr;
    ChordRest *cr = to_chord_rest(segment->element(track));
    Tuplet *tuplet = cr ? cr->tuplet() : nullptr;
    Measure *measure = nullptr;
    bool target_is_rest = cr && cr->is_rest();

    // preserve lyrics before the ChordRest is removed or replaced
    std::vector<Lyrics *> lyrics_to_preserve;
    bool should_preserve_lyrics = false;
    if (!is_rest && cr) {
        lyrics_to_preserve = cr->lyrics();
        should_preserve_lyrics = !lyrics_to_preserve.empty();
    }
    bool lyrics_preserved = false;

    for (;;) {
        if (track % ::mu::engraving::voices) {
            expand_voice(segment, track);
        }
        if (target_is_rest && !cr->is_rest()) {
            undo_remove_element(cr);
            segment = add_rest(segment, track, cr->ticks(), cr->tuplet())->segment();
        }
        // the returned gap ends at the measure boundary or at tuplet end
        Fraction dd = make_gap(segment, track, sd, tuplet);

        if (dd.is_zero()) {
            LOGD("cannot get gap at %d type: %d/%d", tick.ticks(), sd.numerator(), sd.denominator());
            break;
        }

        measure = segment->measure();
        Fraction time_stretch = staff(track2staff(track))->time_stretch(tick);
        std::vector<TDuration> dl;
        if (rhythmic) {
            dl = to_rhythmic_duration_list(dd, is_rest, segment->rtick() * time_stretch, sigmap()->timesig(tick).nominal(), measure, 1,
                                           time_stretch);
        } else {
            dl = to_duration_list(dd, true);
        }
        size_t n = dl.size();
        for (size_t i = 0; i < n; ++i) {
            const TDuration &d = dl[i];
            ChordRest *ncr = nullptr;
            Note *note = nullptr;
            Tie *add_tie = nullptr;
            if (is_rest) {
                nr = ncr = Factory::create_rest(this->dummy());
                nr->set_track(track);
                ncr->set_duration_type(d);
                ncr->set_ticks(d.is_measure() ? measure->ticks() * time_stretch : d.fraction());
            } else {
                nr = note = Factory::create_note(this->dummy());

                if (tie) {
                    tie->set_end_note(note);
                    note->set_tie_back(tie);
                    add_tie = tie;
                }
                Chord *chord = Factory::create_chord(this->dummy());
                chord->set_track(track);
                chord->set_duration_type(d);
                chord->set_ticks(d.fraction());
                chord->set_stem_direction(stem_direction);
                chord->add(note);
                if (cr && cr->is_chord()) {
                    std::vector<Chord *> grace_notes = to_chord(cr)->grace_notes();
                    for (Chord *grace : grace_notes) {
                        undo_change_parent(grace, chord, chord->staff_idx());
                    }
                }
                note->set_nval(nval, tick);
                if (force_accidental) {
                    int tpc = style().style_b(Sid::CONCERT_PITCH) ? nval.tpc1 : nval.tpc2;
                    AccidentalVal alter = tpc2alter(tpc);
                    AccidentalType at = Accidental::value2subtype(alter);
                    Accidental *a = Factory::create_accidental(note);
                    a->set_accidental_type(at);
                    a->set_role(AccidentalRole::USER);
                    note->add(a);
                }

                ncr = chord;
                if (i + 1 < n) {
                    tie = Factory::create_tie(note);
                    tie->set_start_note(note);
                    tie->set_tick(tie->start_note()->tick());
                    tie->set_track(track);
                    note->set_tie_for(tie);
                }
            }
            if (tuplet) {
                ncr->set_tuplet(tuplet);
            }
            undo_add_cr(ncr, measure, tick);

            if (should_preserve_lyrics && !lyrics_preserved) {
                // reattach the preserved lyrics to the first replacement ChordRest
                // to ensure they are not duplicated during subsequent operations
                lyrics_preserved = true;
                for (Lyrics *lyric : lyrics_to_preserve) {
                    undo_change_parent(lyric, ncr, ncr->staff_idx());
                }
            }

            if (add_tie) {
                undo_add_element(add_tie);
            }
            set_play_note(true);
            segment = ncr->segment();
            tick += ncr->actual_ticks();

            if (!articulation_ids.empty()) {
                if (ncr->is_chord()) {
                    to_chord(ncr)->update_articulations(articulation_ids);
                }
            }
        }

        sd -= dd;
        if (sd.is_zero()) {
            break;
        }

        if (tick >= score()->end_tick()) {
            append_measures(1);
        }
        Segment *nseg = tick2segment(tick, false, SegmentType::CHORD_REST);
        if (!nseg) {
            break;
        }
        segment = nseg;

        cr = to_chord_rest(segment->element(track));
        // if the cr was too big for the tuplet, get the next tuplet so we can spill into that one
        tuplet = cr && cr->tuplet() ? cr->tuplet() : nullptr;
        if (!cr) {
            if (track % ::mu::engraving::voices) {
                cr = add_rest(segment, track, TDuration(DurationType::V_MEASURE), 0);
            } else {
                LOGD("no rest in voice 0");
                break;
            }
        }
        //
        //  Note does not fit on current measure, create Tie to
        //  next part of note
        if (!is_rest) {
            tie = Factory::create_tie((Note *) nr);
            tie->set_start_note((Note *) nr);
            tie->set_tick(tie->start_note()->tick());
            tie->set_track(nr->track());
            ((Note *) nr)->set_tie_for(tie);
        }
    }
    if (tie) {
        connect_ties();
    }

    if (last_entered) *last_entered = nr;
    return segment;
}

Segment *Score::set_note_rest(Segment *segment, track_idx_t track, NoteVal nval, Fraction sd, DirectionV stem_direction,
                              bool force_accidental, const amc::FlatSet<SymId> &articulation_ids, bool rhythmic,
                              InputState *external_input_state) {
    assert(segment->segment_type() == SegmentType::CHORD_REST);
    EngravingItem *nr = nullptr;
    segment = set_note_rest_at(segment, track, nval, sd, stem_direction, force_accidental, articulation_ids, rhythmic, &nr);

    if (nr) {
        if (external_input_state) {
            auto *cr = nr->is_rest() ? to_chord_rest(nr) : to_note(nr)->chord();
            external_input_state->set_track(nr->track());
            external_input_state->set_segment(cr->segment());
        } else {
            select(nr, SelectType::SINGLE, 0);
        }
    }
    return segment;
}

//    make time gap at tick by removing/shortening
//    chord/rest
//
//    if keepChord, the chord at tick is not removed
//
//    "sd" is in local (stretched) time
//
//    gap does not exceed measure or scope of tuplet
//
//    return size of actual gap in local time

Fraction Score::make_gap(Segment *segment, track_idx_t track, const Fraction &input_sd, Tuplet *tuplet, bool keep_chord,
                         bool delete_annotations) {
    assert(input_sd.numerator());

    Staff *stf = staff(track2staff(track));
    IF_ASSERT_FAILED(stf) { return Fraction(); }

    Measure *measure = segment->measure();
    Fraction accumulated;
    Fraction sd = input_sd;

    //
    // remember first segment which should
    // not be deleted (it may contain other elements we want to preserve)
    //
    Segment *first_segment = segment;
    const Fraction first_segment_end = first_segment->tick() + first_segment->ticks();
    Fraction next_tick = segment->tick();

    for (Segment *seg = first_segment; seg; seg = seg->next(SegmentType::CHORD_REST)) {
        //
        // voices != 0 may have gaps:
        //
        ChordRest *cr = to_chord_rest(seg->element(track));
        Fraction time_stretch = stf->time_stretch(seg->tick());
        if (!cr) {
            if (seg->tick() < next_tick) {
                continue;
            }
            Segment *seg1 = seg->next(SegmentType::CHORD_REST);
            Fraction tick2 = seg1 ? seg1->tick() : seg->measure()->end_tick();
            Fraction td(tick2 - seg->tick());
            td /= actual_ticks(Fraction(1, 1), tuplet, time_stretch);
            if (td > sd) {
                td = sd;
            }
            accumulated += td;
            sd -= td;
            if (sd.is_zero()) {
                break;
            }
            next_tick = tick2;
            continue;
        }
        if (seg->tick() > next_tick) {
            // there was a gap
            Fraction td(seg->tick() - next_tick);
            td /= actual_ticks(Fraction(1, 1), tuplet, time_stretch);
            if (td > sd) {
                td = sd;
            }
            accumulated += td;
            sd -= td;
            if (sd.is_zero()) {
                break;
            }
        }
        //
        // limit to tuplet level
        //
        if (tuplet) {
            bool tuplet_end = true;
            Tuplet *t = cr->tuplet();
            while (t) {
                if (t == tuplet) {
                    tuplet_end = false;
                    break;
                }
                t = t->tuplet();
            }
            if (tuplet_end) {
                break;
            }
        }
        Fraction td(cr->ticks());

        // remove tremolo between 2 notes, if present
        if (cr->is_chord()) {
            Chord *c = to_chord(cr);
            if (c->tremolo_two_chord()) {
                undo_remove_element(c->tremolo_two_chord());
            }
        }
        Tuplet *ltuplet = cr->tuplet();
        if (ltuplet != tuplet) {
            //
            // Current location points to the start of a (nested)tuplet.
            // We have to remove the complete tuplet.

            // get the outermost tuplet inside the current one
            while (ltuplet->tuplet() != tuplet) {
                ltuplet = ltuplet->tuplet();
            }

            // get last segment of tuplet, drilling down to leaf nodes as necessary
            Tuplet *t = ltuplet;
            while (t->elements().back()->is_tuplet()) {
                t = to_tuplet(t->elements().back());
            }
            seg = to_chord_rest(t->elements().back())->segment();

            // now delete the full tuplet
            td = ltuplet->ticks();
            cmd_delete_tuplet(ltuplet, false);
        } else {
            if (seg != first_segment || !keep_chord) {
                undo_remove_element(cr);
            }
            // even if there was a tuplet, we didn't remove it
            ltuplet = 0;
        }
        next_tick += actual_ticks(td, tuplet, time_stretch);
        if (sd < td) {
            //
            // we removed too much
            //
            accumulated = input_sd;
            Fraction rd = td - sd;
            Fraction tick = cr->tick() + actual_ticks(sd, tuplet, time_stretch);

            std::vector<TDuration> d_list;
            if (tuplet) {
                d_list = to_duration_list(rd, false);
                std::reverse(d_list.begin(), d_list.end());
            } else {
                TimeSig *time_sig = stf->time_sig(tick);
                TimeSigFrac ref_time_sig = time_sig ? time_sig->sig() : sigmap()->timesig(tick).nominal();
                Fraction r_tick_start = (tick - measure->tick()) * stf->time_stretch(tick);
                d_list = to_rhythmic_duration_list(rd, true, r_tick_start, ref_time_sig, measure, 0, stf->time_stretch(tick));
            }
            if (d_list.empty()) {
                break;
            }

            for (TDuration d : d_list) {
                if (ltuplet) {
                    // take care not to recreate tuplet we just deleted
                    Rest *r = set_rest(tick, track, d.fraction(), false, tuplet, false);
                    tick += r->actual_ticks();
                } else {
                    tick += add_clone(cr, tick, d)->actual_ticks();
                }
            }
            break;
        }
        accumulated += td;
        sd -= td;
        if (sd.is_zero()) {
            break;
        }
    }

    const Fraction t1 = first_segment_end;
    const Fraction t2 = first_segment->tick() + actual_ticks(accumulated, tuplet, stf->time_stretch(first_segment->tick()));
    if (t1 < t2) {
        // Delete annotations that require an anchor to the previous segment
        Segment *s1 = tick2right_segment(t1);
        Segment *s2 = tick2right_segment(t2);
        const bool segs_valid = s1 && s2 && (*s2) > (*s1);
        if (segs_valid && delete_annotations) {
            for (Segment *s = s1; s && s != s2; s = s->next1()) {
                const auto annotations = s->annotations(); // make a copy since we alter the list
                for (EngravingItem *annotation : annotations) {
                    if (!annotation->system_flag() && !annotation->allow_time_anchor() && annotation->track() == track) {
                        delete_item(annotation);
                    }
                }
            }
        }

        delete_slurs_from_range(t1, t2, track, track + 1);
    }

    return accumulated;
}

//    cr  - start of gap
//    gap - gap len in local (stretched) time

std::vector<Fraction> Score::split_gap_to_measure_boundaries(ChordRest *cr, Fraction gap) {
    std::vector<Fraction> flist;

    Tuplet *tuplet = cr->tuplet();
    if (tuplet) {
        Fraction rest = tuplet->elements_duration();
        for (DurationElement *de : tuplet->elements()) {
            if (de == cr) {
                break;
            }
            rest -= de->ticks();
        }
        if (rest < gap) {
            LOGD("does not fit in tuplet");
        } else {
            flist.push_back(gap);
        }
        return flist;
    }

    Segment *s = cr->segment();
    while (gap > Fraction(0, 1)) {
        Measure *m = s->measure();
        Fraction time_stretch = cr->staff()->time_stretch(s->tick());
        Fraction rest = (m->ticks() - s->rtick()) * time_stretch;
        if (rest >= gap) {
            flist.push_back(gap);
            return flist;
        }
        flist.push_back(rest);
        gap -= rest;
        if (m->next_measure() != 0) {
            m = m->next_measure();
        }
        s = m->first(SegmentType::CHORD_REST);
    }
    return flist;
}

//    - dstF is in local (stretched) time

void Score::change_c_rlen(ChordRest *cr, const TDuration &d) {
    Fraction dst_f;
    if (d.type() == DurationType::V_MEASURE) {
        dst_f = cr->measure()->stretched_len(cr->staff());
    } else {
        dst_f = d.fraction();
    }
    change_c_rlen(cr, dst_f);
}

void Score::change_c_rlen(ChordRest *cr, const Fraction &dst_f, bool fill_with_rest) {
    if (cr->is_measure_repeat()) {
        // it is not clear what should this
        // operation mean for measure repeats.
        return;
    }
    Fraction src_f(cr->ticks());
    if (src_f == dst_f) {
        if (cr->is_full_measure_rest()) {
            undo_change_chord_rest_len(cr, dst_f);
        }
        return;
    }

    track_idx_t track = cr->track();
    Tuplet *tuplet = cr->tuplet();

    if (src_f > dst_f) {
        //
        // make shorter and fill with rest
        //
        if (cr->is_chord()) {
            //
            // remove ties and tremolo between 2 notes
            //
            Chord *c = to_chord(cr);
            if (c->tremolo_two_chord()) {
                undo_remove_element(c->tremolo_two_chord());
            }
            for (Note *n : c->notes()) {
                if (Tie *tie = n->tie_for()) {
                    if (tie->tie_jump_points()) {
                        tie->undo_remove_ties_from_jump_points();
                    }
                    undo_remove_element(tie);
                }
                for (Spanner *sp : n->spanner_for()) {
                    if (sp->is_glissando() || sp->is_guitar_bend()) {
                        undo_remove_element(sp);
                    }
                }
            }
        }
        Fraction time_stretch = cr->staff()->time_stretch(cr->tick());
        std::vector<TDuration> d_list = to_duration_list(dst_f, true);
        if (d_list.empty()) {
            LOGD("Could not make durations for: %d/%d", dst_f.numerator(), dst_f.denominator());
            return;
        }
        undo_change_chord_rest_len(cr, d_list[0]);
        Fraction tick2 = cr->tick();
        for (unsigned i = 1; i < d_list.size(); ++i) {
            tick2 += actual_ticks(d_list[i - 1].ticks(), tuplet, time_stretch);
            TDuration d = d_list[i];
            set_rest(tick2, track, d.fraction(), (d.dots() > 0), tuplet);
        }
        if (fill_with_rest) {
            set_rest(cr->end_tick(), track, src_f - dst_f, false, tuplet);
        }

        return;
    }

    //
    // make longer
    //
    // split required len into Measures
    std::vector<Fraction> flist = split_gap_to_measure_boundaries(cr, dst_f);
    if (flist.empty()) {
        return;
    }

    Fraction tick = cr->tick();
    Fraction f = dst_f;
    ChordRest *cr1 = cr;
    Chord *oc = 0;
    Segment *s = cr->segment();

    bool first = true;
    for (const Fraction &f2 : flist) {
        if (!cr1) {
            expand_voice(s, track);
            cr1 = to_chord_rest(s->element(track));
        }

        f -= f2;
        make_gap(cr1->segment(), cr1->track(), f2, tuplet, first);

        if (cr->is_rest()) {
            Fraction time_stretch = cr1->staff()->time_stretch(cr1->tick());
            if (first) {
                std::vector<TDuration> d_list = to_duration_list(f2, true);
                undo_change_chord_rest_len(cr, d_list[0]);
                Fraction tick2 = cr->tick();
                for (unsigned i = 1; i < d_list.size(); ++i) {
                    tick2 += actual_ticks(d_list[i - 1].ticks(), tuplet, time_stretch);
                    TDuration d = d_list[i];
                    set_rest(tick2, track, d.fraction(), (d.dots() > 0), tuplet);
                }
            } else {
                set_rest(tick, track, f2, false, tuplet);
            }
            if (first) {
                first = false;
            }
            tick += actual_ticks(f2, tuplet, time_stretch);
        } else {
            std::vector<TDuration> d_list = to_duration_list(f2, true);
            Measure *measure = tick2measure(tick);
            Fraction etick = measure->tick();

            if (((tick - etick).ticks() % d_list[0].ticks().ticks()) == 0) {
                for (TDuration du : d_list) {
                    Chord *cc = nullptr;
                    if (oc) {
                        cc = oc;
                        oc = add_chord(tick, du, cc, true, tuplet);
                    } else {
                        cc = to_chord(cr);
                        undo_change_chord_rest_len(cr, du);
                        oc = cc;
                    }
                    if (oc && first) {
                        first = false;
                    }
                    if (oc) {
                        tick += oc->actual_ticks();
                    }
                }
            } else {
                for (size_t i = d_list.size(); i > 0; --i) { // loop probably needs to be in this reverse order
                    Chord *cc;
                    if (oc) {
                        cc = oc;
                        oc = add_chord(tick, d_list[i - 1], cc, true, tuplet);
                    } else {
                        cc = to_chord(cr);
                        undo_change_chord_rest_len(cr, d_list[i - 1]);
                        oc = cc;
                    }
                    if (first) {
                        first = false;
                    }
                    tick += oc->actual_ticks();
                }
            }
        }
        const Measure *m = cr1->measure();
        if (m->next_measure() == 0) {
            if (f.is_not_zero()) {
                append_measures(1);
            }
        }
        const Measure *m1 = m->next_measure();
        if (m1 == 0) {
            break;
        }
        s = m1->first(SegmentType::CHORD_REST);
        cr1 = to_chord_rest(s->element(track));
    }
    connect_ties();
}

} // namespace mu::engraving
