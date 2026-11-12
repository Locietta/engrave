// SPDX-License-Identifier: GPL-3.0-only
#include "normalize.h"

#include <algorithm>

#include "dom/chord.h"
#include "dom/clef.h"
#include "dom/glissando.h"
#include "dom/guitarbend.h"
#include "dom/keysig.h"
#include "dom/layoutbreak.h"
#include "dom/lyrics.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/noteline.h"
#include "dom/ornament.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/slur.h"
#include "dom/spannermap.h"
#include "dom/staff.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "dom/trill.h"
#include "dom/utils.h"
#include "editing/addremoveelement.h"
#include "editing/transaction/transaction.h"
#include "notes.h"

using namespace mu::engraving;

void mu::engraving::normalize_measure_notes(Measure &value, bool include_hidden_staves) {
    Measure *measure = &value;
    Score &dom = *measure->score();
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        const Staff *staff = dom.staff(staff_idx);
        if (!include_hidden_staves && !staff->show()) {
            continue;
        }

        AccidentalState as; // list of already set accidentals for this measure
        // initAccidentalState
        {
            as.init(staff->key_sig_event(measure->tick()));

            // Trills may carry an accidental into this measure that requires a force-restate
            int ticks = measure->tick().ticks();
            auto spanners = dom.spanner_map().find_overlapping(ticks, ticks, true);
            for (auto iter : spanners) {
                Spanner *spanner = iter.value;
                if (spanner->staff_idx() != staff_idx || !spanner->is_trill() || spanner->tick() == measure->tick() ||
                    spanner->tick2() == measure->tick()) {
                    continue;
                }
                Ornament *ornament = to_trill(spanner)->ornament();
                Note *trill_note = ornament ? ornament->note_above() : nullptr;
                if (trill_note && trill_note->accidental() && ornament->show_accidental() == OrnamentShowAccidental::DEFAULT) {
                    int line = abs_step(trill_note->tpc(), trill_note->epitch());
                    as.set_force_restate_accidental(line, true);
                }
            }
        }

        const TrackRange track_range = staff->part()->track_range();
        track_idx_t main_track = staff_idx * ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (segment.is_just_type(SegmentType::KEY_SIG)) {
                KeySig *ks = item_cast<KeySig *>(segment.element(main_track));
                if (ks) {
                    Fraction tick = segment.tick();
                    as.init(staff->key_sig_event(tick));
                }
            } else if (segment.is_just_type(SegmentType::CHORD_REST)) {
                for (track_idx_t t = track_range.start_track; t < track_range.end_track; ++t) {
                    Chord *chord = item_cast<Chord *>(segment.element(t), CastMode::MAYBE_BAD); // maybe Rest
                    if (chord) {
                        chord->cmd_update_notes(&as, staff_idx);
                    }
                }
            }
        }
    }
}

static void normalize_grace_tracks(Measure *measure, const Score &dom) {
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (!segment.is_just_type(SegmentType::CHORD_REST)) {
                continue;
            }

            for (track_idx_t t = start_track; t < end_track; ++t) {
                ChordRest *cr = segment.cr(t);
                if (!cr) {
                    continue;
                }

                if (cr->is_chord()) {
                    Chord *chord = to_chord(cr);
                    for (Chord *c : chord->grace_notes()) {
                        c->set_track(t);
                    }
                }
            }
        }
    }
}

static void remove_and_add_begin_segments(Measure *measure) {
    std::vector<Segment *> segs_to_sort;
    for (Segment &seg : measure->segments()) {
        if (seg.rtick() != Fraction(0, 1) || seg.is_chord_rest_type() || seg.is_end_bar_line_type()) {
            continue;
        }

        segs_to_sort.push_back(&seg);
    }

    // Re-add the segments. They will be placed in their correct positions
    for (Segment *seg : segs_to_sort) {
        measure->segments().remove(seg);
    }
    for (Segment *seg : segs_to_sort) {
        measure->add(seg);
    }
}

static void normalize_segments(Measure *measure, Score &score) {

    // Move segments between measure which need to move

    // Move key and time signature segments to the correct measure.
    // Depending on configuration, this could mean moving segments at the end of this measure to the start of the following measure
    // or moving segments from the start of the following measure to the end of this measure

    // Loop through clef, key, time sigs in this bar.  If any are found and shouldn't be in this bar, move to the next

    auto change_applies_to_repeat_and_continuation = [&](const Segment &seg) -> bool {
        // Check if the change applies to the beginning of the repeat section as well as the continuation
        const std::vector<Measure *> measures = find_following_repeat_measures(measure, false);
        for (const Measure *repeat_measure : measures) {
            if (repeat_measure == measure->next_measure()) {
                continue;
            }
            const Fraction start_tick = repeat_measure->tick();
            for (track_idx_t track = 0; track < score.nstaves(); track += ::mu::engraving::voices) {
                const Staff *staff = score.staff(track2staff(track));

                if (seg.is_key_sig_type()) {
                    const Key continuation_key = staff->key(seg.tick());
                    const Key repeat_key = staff->key(start_tick);
                    const Key key_before_continuation = staff->key(seg.tick() - Fraction::eps());
                    return key_before_continuation != continuation_key && continuation_key == repeat_key;
                }

                if (seg.is_time_sig_type()) {
                    const TimeSig *continuation_time_sig = staff->time_sig(seg.tick());
                    const TimeSig *repeat_time_sig = staff->time_sig(start_tick);
                    const TimeSig *time_sig_before_continuation = staff->time_sig(seg.tick() - Fraction::eps());
                    return continuation_time_sig && repeat_time_sig && time_sig_before_continuation &&
                           time_sig_before_continuation->sig() != continuation_time_sig->sig() &&
                           continuation_time_sig->sig() == repeat_time_sig->sig();
                }

                if (seg.is_clef_type()) {
                    const ClefType continuation_clef = staff->clef(seg.tick());
                    const ClefType repeat_clef = staff->clef(start_tick);
                    const ClefType clef_before_continuation = staff->clef(seg.tick() - Fraction::eps());
                    if (clef_before_continuation != continuation_clef && continuation_clef == repeat_clef) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    auto clef_seg_barline_position = [&](const Segment &seg) -> ClefToBarlinePosition {
        for (EngravingItem *el : seg.elist()) {
            if (!el || !el->is_clef()) {
                continue;
            }
            const Clef *clef = to_clef(el);
            if (clef->clef_to_barline_position() != ClefToBarlinePosition::AUTO) {
                return clef->clef_to_barline_position();
            }
        }
        return ClefToBarlinePosition::AUTO;
    };

    MeasureBase *next_mb = measure ? measure->next() : nullptr;

    if (!next_mb || !next_mb->is_measure()) {
        return;
    }

    Measure *next_measure = to_measure(next_mb);

    const bool sigs_should_be_in_this_measure =
        ((measure->repeat_end() && score.style().style_b(Sid::CHANGES_BEFORE_BARLINE_REPEATS)) ||
         (measure->repeat_jump() && score.style().style_b(Sid::CHANGES_BEFORE_BARLINE_OTHER_JUMPS)));
    std::vector<Segment *> segs_to_remove;

    std::vector<Segment *> segs_to_move_to_next_measure;
    for (Segment &seg : measure->segments()) {
        if (seg.tick() != measure->end_tick() || seg.is_chord_rest_type()) {
            continue;
        }

        // Move clefs at the end of this measure into the next measure
        if (seg.is_clef_type()) {
            ClefToBarlinePosition pos = clef_seg_barline_position(seg);
            // Clef position explicitly set by user
            if (pos == ClefToBarlinePosition::BEFORE) {
                continue;
            } else if (pos == ClefToBarlinePosition::AFTER) {
                segs_to_move_to_next_measure.push_back(&seg);
                continue;
            }

            // AUTO on normal barline
            if (!measure->repeat_end()) {
                continue;
            }

            // Otherwise, pos is AUTO
            // Place changes before barline & clefs before repeats style settings
            if ((!sigs_should_be_in_this_measure && !change_applies_to_repeat_and_continuation(seg)) ||
                (measure->repeat_end() && !score.style().style_b(Sid::PLACE_CLEFS_BEFORE_REPEATS))) {
                segs_to_move_to_next_measure.push_back(&seg);
                continue;
            }
        }

        // Move key sigs and time sigs at the end of this measure into the next measure
        if ((seg.is_key_sig_type() || seg.is_time_sig_type()) && !seg.header() && !seg.trailer() &&
            (!sigs_should_be_in_this_measure || !change_applies_to_repeat_and_continuation(seg))) {
            if (next_measure && next_measure->find_segment_r(seg.segment_type(), Fraction(0, 1))) {
                segs_to_remove.push_back(&seg);
                continue;
            }
            segs_to_move_to_next_measure.push_back(&seg);
        }
    }

    // No next measure, so these segments are useless
    if (!next_measure) {
        for (Segment *seg : segs_to_move_to_next_measure) {
            score.do_undo_remove_element(seg);
        }
        return;
    }

    std::vector<Segment *> segs_to_move_to_this_measure;
    for (Segment &seg : next_measure->segments()) {
        if (seg.tick() != next_measure->tick() || seg.is_chord_rest_type()) {
            continue;
        }

        // Move clefs at the beginning of the next measure into this measure
        if (seg.is_clef_type()) {
            ClefToBarlinePosition pos = clef_seg_barline_position(seg);
            // Clef position explicitly set by user
            if (pos == ClefToBarlinePosition::BEFORE) {
                segs_to_move_to_this_measure.push_back(&seg);
                continue;
            } else if (pos == ClefToBarlinePosition::AFTER) {
                continue;
            }

            if (!measure->repeat_end()) {
                segs_to_move_to_this_measure.push_back(&seg);
                continue;
            }

            // Otherwise, pos is AUTO
            // Place changes before barline & clefs before repeats style settings
            if ((sigs_should_be_in_this_measure && change_applies_to_repeat_and_continuation(seg)) || !measure->repeat_end() ||
                score.style().style_b(Sid::PLACE_CLEFS_BEFORE_REPEATS)) {
                segs_to_move_to_this_measure.push_back(&seg);
            }
        }

        // Move key sigs and time sigs at the start of the next measure to the end of this measure
        if ((seg.is_time_sig_type() || seg.is_key_sig_type()) && !seg.header() && !seg.trailer() && sigs_should_be_in_this_measure &&
            change_applies_to_repeat_and_continuation(seg)) {
            if (measure->find_segment_r(seg.segment_type(), measure->ticks())) {
                segs_to_remove.push_back(&seg);
                continue;
            }
            segs_to_move_to_this_measure.push_back(&seg);
        }
    }

    for (Segment *seg : segs_to_remove) {
        // Don't add duplicate segs to a measure
        score.do_undo_remove_element(seg);
    }

    for (Segment *seg : segs_to_move_to_next_measure) {

        seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, false);
        measure->score()->undo(new ChangeSegmentParent(seg, next_measure, Fraction(0, 1)));
    }

    for (Segment *seg : segs_to_move_to_this_measure) {

        seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, true);
        measure->score()->undo(new ChangeSegmentParent(seg, measure, measure->ticks()));
    }

    // Check end of measure changes for top and underlying measures
    measure->check_end_of_measure_change();

    // Sort segments at start of next measure
    remove_and_add_begin_segments(next_measure);

    // Sort segments at start of first measure
    Measure *prev_measure = measure->prev_measure();
    if (prev_measure && prev_measure == score.first_measure()) {
        remove_and_add_begin_segments(prev_measure);
    }
}

int mu::engraving::normalize_measure_number(Measure &value, int measure_number) {
    Measure *m = &value;
    measure_number += m->measure_number_offset();
    m->set_measure_number(measure_number);
    if (!m->exclude_from_numbering()) {
        ++measure_number;
    }

    const LayoutBreak *layout_break = m->section_break_element();
    if (layout_break && layout_break->start_with_measure_one()) {
        measure_number = 0;
    }

    return measure_number;
}

Fraction mu::engraving::normalize_lyric_ticks(Lyrics &lyrics) {
    // Cleanup must see the original melisma duration before tick repair can
    // clear it; it also converts BEGIN/MIDDLE syllables to SINGLE/END.
    if (lyrics.need_remove_invalid_segments()) lyrics.remove_invalid_segments();
    Lyrics *item = &lyrics;
    auto is_end_melisma = [item]() { return item->ticks().is_not_zero(); };

    Fraction lyrics_line_ticks = Lyrics::temp_melisma_ticks;

    // Update the end tick
    if (is_end_melisma()) {
        const track_idx_t track = item->track();

        const Segment *const start_segment = item->segment();
        const Fraction start_tick = start_segment->tick();

        // if lyrics has a temporary one-chord melisma, interpret as 0 ticks (just its own chord)
        Fraction item_end_tick = item->ticks() == Lyrics::temp_melisma_ticks ? start_tick : item->end_tick();
        Fraction end_tick = item_end_tick;

        const Segment *end_segment = start_segment;
        while (end_segment && end_segment->tick() < end_tick) {
            end_segment = end_segment->next_cr(track, true);
        }
        if (!end_segment) {
            // user probably deleted measures at end of score, leaving this melisma too long
            // set endSegment to last segment and reset lyricsEndTick to trigger FIXUP code below
            end_segment = item->score()->last_segment();
            end_tick = Fraction(-1, 1);
        }

        if (!end_segment) return Fraction(0, 1);
        EngravingItem *end_segment_element = end_segment->element(track);
        if (end_segment->tick() == end_tick && end_segment_element && end_segment_element->is_chord()) {
            // everything is OK if we have reached a chord at right tick on right track
            // advance to next CR after duration of note, or last segment if no next CR
            const Segment *end_chord_seg = end_segment;
            const Chord *end_chord = to_chord(end_segment_element);

            end_segment = end_chord_seg->next_cr(track, false);

            if (!end_segment || end_segment->tick() > end_chord->end_tick()) {
                end_segment = end_chord_seg;
                while (end_segment && end_segment->tick() < end_chord->end_tick()) {
                    end_segment = end_segment->next_cr(muse::nidx, true);
                }
            }
        } else {
            // FIXUP - lyrics tick count not valid
            // this happens if edits to score have removed the original end segment
            // so let's fix it here
            // endSegment is already pointing to segment past endTick (or to last segment)
            // we should shorten the lyrics tick count to make this work
            const Segment *ns = end_segment;
            const Segment *ps = end_segment->prev1(SegmentType::CHORD_REST);
            while (ps && ps != start_segment) {
                EngravingItem *pe = ps->element(track);
                // we're looking for an actual chord on this track
                if (pe && pe->is_chord()) {
                    break;
                }
                end_segment = ps;
                ps = ps->prev1(SegmentType::CHORD_REST);
            }

            if (!ps || ps == start_segment) {
                // either there is no valid previous CR, or the previous CR is the one the lyric starts on
                // we don't want to make the melisma longer arbitrarily, but there is a possibility that the next
                // CR won't extend the melisma, so let's check it
                ps = ns;
                end_segment = ps->next_cr(track, false);
                EngravingItem *e = end_segment ? end_segment->element(track) : nullptr;

                // check to make sure we have a chord
                if (!e || !e->is_chord() || ps->tick() > item_end_tick) {
                    // nope, nothing to do but set ticks to 0
                    // this will result in no melisma
                    item->undo_change_property(Pid::LYRIC_TICKS, Fraction::from_ticks(0));
                } else {
                    item->undo_change_property(Pid::LYRIC_TICKS, ps->tick() - start_tick);
                }
            } else {
                item->undo_change_property(Pid::LYRIC_TICKS, ps->tick() - start_tick);
            }
        }

        if (end_segment) {
            // Lyrics::_ticks points to the beginning of the last spanned segment,
            // but the line shall include it:
            // include the duration of this last segment in the melisma duration
            lyrics_line_ticks = end_segment->tick() - start_tick;
        } else {
            lyrics_line_ticks = item->score()->end_tick() - start_tick;
        }
    }

    return lyrics_line_ticks;
}

void mu::engraving::invalidate_normalization(Score &score) {
    auto &state = score.normalize_state;
    if (state.running || score.master_score()->cmd_state().locked()) return;
    state.dirty = true;
}

static void normalize_glissando_style(Glissando &glissando) {
    const bool harp = glissando.staff()->part()->instrument_id(glissando.tick()) == "harp";
    const auto previous = glissando.is_harp_gliss();
    glissando.set_is_harp_gliss(harp);

    // Preserve the source's styled/explicit choice when an instrument changes.
    if (previous.has_value() && previous != harp && glissando.is_styled(Pid::GLISS_STYLE)) {
        glissando.undo_change_property(Pid::GLISS_STYLE, glissando.property_default(Pid::GLISS_STYLE), PropertyFlags::STYLED);
    }
    if (harp && glissando.glissando_style() != GlissandoStyle::DIATONIC && glissando.glissando_style() != GlissandoStyle::CHROMATIC) {
        glissando.undo_change_property(Pid::GLISS_STYLE, GlissandoStyle::DIATONIC, glissando.property_flags(Pid::GLISS_STYLE));
    }
}

static void normalize_note_spanner(Spanner &spanner) {
    if (!spanner.start_element()) spanner.compute_start_element();
    if (!spanner.end_element()) spanner.compute_end_element();
    if (spanner.start_element()) spanner.set_tick(spanner.start_element()->tick());
    if (spanner.end_element()) spanner.set_tick2(spanner.end_element()->tick());
    if (spanner.is_glissando() && spanner.staff()) normalize_glissando_style(*to_glissando(&spanner));
}

static void normalize_chord_anchors(Chord &chord) {
    for (Chord *grace : chord.grace_notes()) normalize_chord_anchors(*grace);
    for (Note *note : chord.notes()) {
        if (auto *tie = note->tie_for()) tie->set_tick(chord.tick());
        // Note-owned lines do not live in Score::spanner_map(). Include incoming
        // lines too: a partial line may have only its end note in this score.
        for (const auto *lines : {&note->spanner_for(), &note->spanner_back()}) {
            for (Spanner *spanner : *lines) {
                if (spanner->is_glissando() || spanner->is_guitar_bend() || spanner->is_note_line()) normalize_note_spanner(*spanner);
            }
        }
    }
}

static void normalize_anchors(Score &score, bool include_hidden_staves) {
    if (!include_hidden_staves && score.all_staves_invisible()) return;
    // Temporal setters can reindex the registry, so retain a stable work list.
    std::vector<Spanner *> spanners;
    for (const auto &[tick, spanner] : score.spanner_map().map()) spanners.push_back(spanner);
    for (Spanner *spanner : spanners) {
        if (!include_hidden_staves && spanner->staff() && !spanner->staff()->show()) continue;
        if (spanner->is_slur()) {
            Slur *slur = to_slur(spanner);
            if (slur->track2() == muse::nidx) slur->set_track2(slur->track());
            if (!slur->start_cr() || !slur->start_cr()->measure()) continue;
            if (slur->ticks().is_zero() && !slur->start_cr()->is_grace() && !(slur->end_cr() && slur->end_cr()->is_grace()) &&
                !slur->is_incoming() && !slur->is_outgoing() && !slur->is_tapping_half_slur())
                continue;
            if (!slur->end_cr()) {
                slur->set_end_element(slur->start_cr());
                slur->set_tick2(slur->tick());
            }
        } else if (spanner->is_glissando() || spanner->is_guitar_bend() || spanner->is_note_line()) {
            normalize_note_spanner(*spanner);
        } else {
            spanner->compute_start_element();
            spanner->compute_end_element();
        }
    }
    std::vector<Spanner *> moved;
    for (const auto &[tick, spanner] : score.spanner_map().map()) {
        if (tick != spanner->tick().ticks()) moved.push_back(spanner);
    }
    for (Spanner *spanner : moved) {
        score.spanner_map().remove_spanner(spanner);
        score.spanner_map().add_spanner(spanner);
    }
    score.spanner_map().update();
}

static bool needs_normalization(const Score &score, const NormalizeOptions &options) {
    const auto &state = score.normalize_state;
    return !state.running && (state.dirty || state.rebuild_midi_mapping || score.need_update_ticks_and_time_sig_map() ||
                              (options.include_hidden_staves && !state.includes_hidden_staves));
}

static void prepare_timeline(Score &score) {
    // Whole-score fallback until all mutation categories have bounded closures.
    int number = 0;
    for (MeasureBase *base = score.first(); base; base = base->next()) {
        if (base->is_measure())
            number = normalize_measure_number(*to_measure(base), number);
        else if (auto *section = base->section_break_element(); section && section->start_with_measure_one())
            number = 0;
    }
    score.update_ticks_and_time_sig_map(false);
    if (score.is_master() && score.normalize_state.rebuild_midi_mapping) {
        auto *master = score.master_score();
        master->rebuild_midi_mapping();
    }
}

static void prepare_source_anchors(Score &score, const NormalizeOptions &options) {
    for (Measure *measure = score.first_measure(); measure; measure = measure->next_measure()) {
        if (!options.include_hidden_staves && score.all_staves_invisible()) continue;
        normalize_grace_tracks(measure, score);
        for (const Segment &segment : measure->segments()) {
            for (EngravingItem *item : segment.elist()) {
                if (!item || !item->is_chord_rest()) continue;
                auto *cr = to_chord_rest(item);
                if (cr->staff_move() || cr->stored_staff_move()) cr->check_staff_move_validity();
                if (item->is_chord() && (options.include_hidden_staves || item->staff()->show())) {
                    normalize_chord_anchors(*to_chord(item));
                }
            }
        }
    }
    normalize_anchors(score, options.include_hidden_staves);
}

static void normalize_tab_fretting(Chord &chord) {
    for (Chord *grace : chord.grace_notes()) normalize_tab_fretting(*grace);
    if (chord.staff()->is_tab_staff(chord.tick())) {
        for (Note *note : chord.notes()) note->update_fretting_for_ties_and_bends();
    }
}

static void normalize_content(Score &score, const NormalizeOptions &options) {
    if (options.include_hidden_staves || !score.all_staves_invisible()) {
        for (Measure *measure = score.first_measure(); measure; measure = measure->next_measure()) {
            for (const Segment &segment : measure->segments()) {
                for (EngravingItem *item : segment.elist()) {
                    if (item && item->is_chord() && (options.include_hidden_staves || item->staff()->show()))
                        normalize_cross_measure(*to_chord(item), score.style().style_b(Sid::CROSS_MEASURE_VALUES));
                }
            }
            measure->connect_tremolo();
            normalize_measure_notes(*measure, options.include_hidden_staves);
            for (const Segment &segment : measure->segments()) {
                for (auto *item : segment.elist()) {
                    if (item && item->is_chord() && (options.include_hidden_staves || item->staff()->show()))
                        normalize_tab_fretting(*to_chord(item));
                }
            }
            normalize_segments(measure, score);
            for (const Segment &segment : measure->segments()) {
                for (EngravingItem *item : segment.elist()) {
                    if (!item || !item->is_chord_rest() || (!options.include_hidden_staves && !item->staff()->show())) continue;
                    for (Lyrics *lyrics : to_chord_rest(item)->lyrics()) normalize_lyric_ticks(*lyrics);
                }
            }
        }
    }
}

static void normalize_scores(MasterScore &master, const std::vector<Score *> &scores, NormalizeOptions options) {
    std::vector<Score *> work;
    for (auto *score : scores)
        if (needs_normalization(*score, options)) work.push_back(score);
    if (work.empty()) return;
    ReplayScope replay(*master.transaction_manager(), options.mode != NormalizeMode::TRANSACTION);
    for (auto *score : work) score->normalize_state.running = true;
    try {
        // All linked timelines/anchors are current before repeat identity is used
        // by signature placement. Neither phase needs display measures.
        for (auto *score : work) prepare_timeline(*score);
        for (auto *score : work) prepare_source_anchors(*score, options);
        master.invalidate_repeat_list();
        master.update_repeat_list(false);
        // Tie-jump cleanup can mark lyrics for repair or remove partial slurs.
        // Do it before content normalization, in the same recording transaction.
        if (options.mode == NormalizeMode::TRANSACTION && master.transaction_manager()->current_transaction()) {
            master.update_repeat_list(true);
            for (auto *score : work) score->spanner_map().update();
        }
        for (auto *score : work) normalize_content(*score, options);
        for (auto *score : work) {
            if (score->need_update_ticks_and_time_sig_map()) score->update_ticks_and_time_sig_map(false);
        }
        master.invalidate_repeat_list();
        master.update_repeat_list(false);
        for (auto *score : work) {
            auto &state = score->normalize_state;
            state.dirty = false;
            state.rebuild_midi_mapping = false;
            state.includes_hidden_staves = options.include_hidden_staves;
            state.running = false;
        }
    } catch (...) {
        for (auto *score : work) {
            score->normalize_state.dirty = true;
            score->normalize_state.running = false;
        }
        throw;
    }
}

void mu::engraving::normalize_document(MasterScore &master, NormalizeOptions options) {
    std::vector<Score *> scores;
    for (auto *score : master.score_list()) scores.push_back(score);
    normalize_scores(master, scores, options);
}

void mu::engraving::normalize_score(Score &score, NormalizeOptions options) {
    std::vector<Score *> scores{score.master_score()};
    if (!score.is_master()) scores.push_back(&score);
    normalize_scores(*score.master_score(), scores, options);
}

void mu::engraving::prepare_semantic_edit(Score &score) { normalize_document(*score.master_score(), {.include_hidden_staves = true}); }
