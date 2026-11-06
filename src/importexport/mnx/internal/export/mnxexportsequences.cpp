/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include "mnxexporter.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <cstdlib>
#include <utility>
#include <vector>

#include "engraving/dom/accidental.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/key.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tiejumppointlist.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/editing/transpose.h"

#include "internal/shared/mnxtypesconv.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
//---------------------------------------------------------
//   exportAccidentalDetails
//   export accidental details into accidentalDisplay
//---------------------------------------------------------

static void export_accidental_details(mnx::sequence::Note &mnx_note, const Note *note) {
    if (Accidental *acc = note->accidental()) {
        bool force = acc->role() == AccidentalRole::USER;
        const AccidentalBracket bracket = acc->bracket();
        if (force || bracket != AccidentalBracket::NONE) {
            auto acc_disp = mnx_note.ensure_accidentalDisplay(acc->visible());
            acc_disp.set_force(force);
            if (bracket != AccidentalBracket::NONE) {
                auto mnx_acci_bracket = bracket == AccidentalBracket::PARENTHESIS ? mnx::AccidentalEnclosureSymbol::Parentheses :
                                                                                    mnx::AccidentalEnclosureSymbol::Brackets;
                auto enclosure = acc_disp.ensure_enclosure(mnx_acci_bracket);
            }
        }
    }
}

//---------------------------------------------------------
//   calcWrittenDiatonicDelta
//   calculates if the written pitch matches the expected
//   pitch from transposition and returns the enharmonic
//   delta if not
//---------------------------------------------------------

static int calc_written_diatonic_delta(const Note *note) {
    const Staff *staff = note->staff();
    Interval transpose = staff->transpose(note->tick());
    if (transpose.is_zero()) {
        return 0;
    }
    transpose.flip();

    const int concert_pitch = note->pitch();
    const int written_pitch = note->pitch() + transpose.chromatic;
    const int concert_tpc = note->tpc1();
    const int written_tpc = note->tpc2();
    IF_ASSERT_FAILED(tpc_is_valid(concert_tpc) && tpc_is_valid(written_tpc)) { return 0; }

    int expected_written_tpc = Transpose::transpose_tpc(concert_tpc, transpose, true);
    IF_ASSERT_FAILED(tpc2pitch(expected_written_tpc) == tpc2pitch(written_tpc)) {
        LOGW() << "Skipping written pitch override with mismatched chromatic pitch:"
               << " concert=" << tpc_user_name(concert_tpc, concert_pitch, true).to_std_string()
               << " transposed=" << tpc_user_name(written_tpc, written_pitch, true).to_std_string()
               << " expectedTransposed=" << tpc_user_name(expected_written_tpc, written_pitch, true).to_std_string();
        return 0;
    }
    int delta = abs_step(written_tpc, written_pitch) - abs_step(expected_written_tpc, written_pitch);

    static constexpr int max_delta = ::mu::engraving::step_delta_octave / 2;
    if (std::abs(delta) > max_delta) {
        LOGW() << "Skipping written pitch override with large diatonic delta:"
               << " concert=" << tpc_user_name(concert_tpc, concert_pitch, true).to_std_string()
               << " transposed=" << tpc_user_name(written_tpc, written_pitch, true).to_std_string() << " delta=" << delta;
        return 0;
    }

    return delta;
}

//---------------------------------------------------------
//   createLyrics
//   export lyrics on a chord/rest into mnxEvent
//---------------------------------------------------------

static void create_lyrics(mnx::sequence::Event &mnx_event, const ChordRest *cr, std::set<std::string> &lyric_line_ids) {
    IF_ASSERT_FAILED(cr) { return; }

    const auto lyrics = cr->lyrics();
    if (lyrics.empty()) {
        return;
    }

    auto mnx_lyrics = mnx_event.ensure_lyrics();
    auto mnx_lines = mnx_lyrics.ensure_lines();

    for (const auto &lyric : lyrics) {
        const std::string line_id = std::to_string(lyric->verse() + 1);
        lyric_line_ids.insert(line_id);
        auto mnx_line = mnx_lines.append(line_id, lyric->plain_text().to_std_string());
        mnx_line.set_type(to_mnx_lyric_line_type(lyric->syllabic()));
        /// @todo export styled text when supported by MNX.
        /// @todo export word extension span when supported by MNX.
    }
}

//---------------------------------------------------------
//   createMarking
//   export a single marking from an Articulation instance
//---------------------------------------------------------

static std::optional<mnx::sequence::EventMarkingBase> create_marking(const Articulation *a, mnx::sequence::EventMarkings mnx_markings) {
    const SymId sym = a->sym_id();

    switch (sym) {
        case SymId::ARTIC_ACCENT_ABOVE:
        case SymId::ARTIC_ACCENT_BELOW: return mnx_markings.ensure_accent();
        case SymId::STRINGS_DOWN_BOW:
        case SymId::STRINGS_DOWN_BOW_TURNED: return mnx_markings.ensure_bowDirection(mnx::MarkingUpDown::Down);
        case SymId::STRINGS_UP_BOW:
        case SymId::STRINGS_UP_BOW_TURNED: return mnx_markings.ensure_bowDirection(mnx::MarkingUpDown::Up);
        case SymId::ARTIC_SOFT_ACCENT_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_BELOW: return mnx_markings.ensure_softAccent();
        case SymId::ARTIC_STACCATO_ABOVE:
        case SymId::ARTIC_STACCATO_BELOW: return mnx_markings.ensure_staccato();
        case SymId::ARTIC_STACCATISSIMO_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_BELOW: return mnx_markings.ensure_staccatissimo();
        case SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_STROKE_BELOW: return mnx_markings.ensure_spiccato();
        case SymId::ARTIC_STRESS_ABOVE:
        case SymId::ARTIC_STRESS_BELOW: return mnx_markings.ensure_stress();
        case SymId::ARTIC_UNSTRESS_ABOVE:
        case SymId::ARTIC_UNSTRESS_BELOW: return mnx_markings.ensure_unstress();
        case SymId::ARTIC_MARCATO_ABOVE:
        case SymId::ARTIC_MARCATO_BELOW: return mnx_markings.ensure_strongAccent();
        case SymId::ARTIC_TENUTO_ABOVE:
        case SymId::ARTIC_TENUTO_BELOW: return mnx_markings.ensure_tenuto();
        default: break;
    }

    return std::nullopt;
}

//---------------------------------------------------------
//   processAnnotations
//   template function to process annotations for both
//   events and full measure rests
//---------------------------------------------------------

template <typename EventType>
void static process_annotations(EventType &mnx_event, ChordRest *cr) {
    // Grace notes are parented to their main chord, not directly to a segment.
    // Segment-anchored annotations such as fermatas belong to the main chord's
    // segment and must not be re-exported from each grace note event.
    if (cr->is_grace()) {
        return;
    }

    if (Segment *seg = cr->segment()) {
        for (EngravingItem *ann : seg->annotations()) {
            if (!ann || ann->track() != cr->track()) {
                continue;
            }
            if (ann->is_fermata()) {
                const Fermata *fermata = to_fermata(ann);
                IF_ASSERT_FAILED(fermata) { continue; }
                mnx_event.set_fermata(MnxExporter::mnx_fermata_from_fermata(fermata));
                /// @note MNX currently can only store one barline fermata per measure stack.
                /// To keep the loop clean for future expansion, this code deliberately
                /// exports the last one found. We can revisit this if/when MNX provides
                /// more clarity around barline fermatas.
            }
        }
    }
}

//---------------------------------------------------------
//   createMarkings
//   export articulations, breath marks, single-note
//   tremolo
//---------------------------------------------------------

void MnxExporter::create_markings(mnx::sequence::Event &mnx_event, ChordRest *cr) {
    IF_ASSERT_FAILED(cr) { return; }

    if (cr->is_chord()) {
        const Chord *chord = to_chord(cr);
        for (Articulation *a : chord->articulations()) {
            IF_ASSERT_FAILED(a) { continue; }
            auto mnx_markings = mnx_event.ensure_markings();
            if (auto marking = create_marking(a, mnx_markings)) {
                marking->set_or_clear_orient(to_mnx_orientation(a->anchor()));
            }
        }

        if (const TremoloSingleChord *trem = chord->tremolo_single_chord()) {
            const int marks = int(trem->tremolo_type()) - int(TremoloType::R8) + 1;
            DO_ASSERT(marks > 0);
            if (marks > 0) {
                auto mnx_markings = mnx_event.ensure_markings();
                mnx_markings.ensure_tremolo(marks);
            }
        }
    }

    if (Breath *breath = cr->has_breath_mark()) {
        auto mnx_markings = mnx_event.ensure_markings();
        auto mnx_breath = mnx_markings.ensure_breath();
        mnx_breath.set_or_clear_orient(to_mnx_orientation(breath->placement()));
        if (const auto breath_sym = to_mnx_breath_mark_sym(breath->sym_id())) {
            mnx_breath.set_symbol(breath_sym.value());
        }
    }

    process_annotations(mnx_event, cr);
}

//---------------------------------------------------------
//   tupletContainsChordRest
//---------------------------------------------------------

static bool tuplet_contains_chord_rest(const Tuplet *tuplet, const ChordRest *chord_rest) {
    IF_ASSERT_FAILED(tuplet && chord_rest) { return false; }

    for (const DurationElement *element : tuplet->elements()) {
        if (!element) {
            continue;
        }
        if (element->is_chord_rest()) {
            if (to_chord_rest(element) == chord_rest) {
                return true;
            }
            continue;
        }
        if (element->is_tuplet()) {
            if (tuplet_contains_chord_rest(to_tuplet(element), chord_rest)) {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   firstTupletChordRest
//---------------------------------------------------------

static ChordRest *first_tuplet_chord_rest(const Tuplet *tuplet) {
    IF_ASSERT_FAILED(tuplet) { return nullptr; }

    for (DurationElement *element : tuplet->elements()) {
        if (!element) {
            continue;
        }
        if (element->is_chord_rest()) {
            return to_chord_rest(element);
        }
        if (element->is_tuplet()) {
            if (ChordRest *nested = first_tuplet_chord_rest(to_tuplet(element))) {
                return nested;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   createTies
//   emits MNX ties for the given note
//---------------------------------------------------------

void MnxExporter::create_ties(mnx::sequence::NoteBase &mnx_note, Note *note) {
    IF_ASSERT_FAILED(note) { return; }

    auto append_tie = [&](Note *target_note, const Tie *dir_tie, bool is_cross_jump) {
        IF_ASSERT_FAILED(target_note) {
            LOGW() << "Skipping tie with missing target note.";
            return;
        }

        auto mnx_tie = mnx_note.ensure_ties().append();
        mnx_tie.set_target(get_or_assign_eid(target_note).to_std_string());
        if (is_cross_jump) {
            mnx_tie.set_targetType(mnx::TieTargetType::CrossJump);
        } else {
            const Fraction expected_tick = note->tick() + note->chord()->ticks();
            if (target_note->tick() == expected_tick) {
                if (target_note->track() == note->track()) {
                    mnx_tie.set_targetType(mnx::TieTargetType::NextNote);
                } else {
                    mnx_tie.set_targetType(mnx::TieTargetType::CrossVoice);
                }
            } else {
                mnx_tie.set_targetType(mnx::TieTargetType::Arpeggio);
            }
        }

        if (dir_tie && dir_tie->slur_direction() != DirectionV::AUTO) {
            mnx_tie.set_side(dir_tie->up() ? mnx::SlurTieSide::Up : mnx::SlurTieSide::Down);
        }
    };

    Tie *tie_for = note->tie_for();
    if (tie_for && !tie_for->is_partial_tie()) {
        if (tie_for->is_laissez_vib()) {
            auto mnx_tie = mnx_note.ensure_ties().append();
            mnx_tie.set_lv(true);
            if (tie_for->slur_direction() != DirectionV::AUTO) {
                mnx_tie.set_side(tie_for->up() ? mnx::SlurTieSide::Up : mnx::SlurTieSide::Down);
            }
            return;
        } else {
            append_tie(tie_for->end_note(), tie_for, false);
        }
    }

    const TieJumpPointList *jump_points = note->tie_jump_points();
    if (!jump_points || jump_points->empty()) {
        return;
    }

    for (const TieJumpPoint *jump_point : *jump_points) {
        if (!jump_point || !jump_point->active() || jump_point->following_note()) {
            continue;
        }

        append_tie(jump_point->note(), jump_point->end_tie(), true);
    }
}

//---------------------------------------------------------
//   createNotes
//   emits MNX notes for the given chord
//   returns true when appended
//---------------------------------------------------------

bool MnxExporter::create_notes(mnx::sequence::Event &mnx_event, ChordRest *chord_rest) {
    using MnxNote = mnx::sequence::Note;
    using MnxKitNote = mnx::sequence::KitNote;

    IF_ASSERT_FAILED(chord_rest->is_chord()) { return false; }

    auto append_kit_note = [&](mnx::Array<MnxKitNote> &mnx_kit_notes, Note *note) -> bool {
        const int pitch = note->pitch();
        if (!pitch_is_valid(pitch)) {
            LOGW() << "Skipping kit note with invalid MIDI pitch: " << pitch;
            return false;
        }
        const std::string kit_id = "drum-midi-" + std::to_string(pitch);
        auto mnx_kit_note = mnx_kit_notes.append(kit_id);
        mnx_kit_note.set_id(get_or_assign_eid(note).to_std_string());
        create_ties(mnx_kit_note, note);
        return true;
    };

    auto append_note = [&](mnx::Array<MnxNote> &mnx_notes, Note *note) -> bool {
        const auto pitch = to_mnx_pitch(note);
        if (!pitch) {
            LOGW() << "Skipping note with unsupported pitch.";
            return false;
        }
        auto mnx_note = mnx_notes.append(*pitch);
        mnx_note.set_id(get_or_assign_eid(note).to_std_string());
        create_ties(mnx_note, note);
        export_accidental_details(mnx_note, note);
        const int delta = calc_written_diatonic_delta(note);
        if (delta != 0) {
            mnx_note.ensure_written().set_diatonicDelta(delta);
        }
        return true;
    };

    const Chord *chord = to_chord(chord_rest);
    const std::vector<Note *> &chord_notes = chord->notes();
    IF_ASSERT_FAILED(!chord_notes.empty()) {
        LOGW() << "Skipping chord event with no notes.";
        return false;
    }

    const Staff *staff = chord->staff();
    const bool is_drumset = staff && staff->is_drum_staff(chord->tick());
    if (is_drumset) {
        auto mnx_kit_notes = mnx_event.ensure_kitNotes();
        bool has_note = false;
        for (Note *note : chord_notes) {
            has_note = append_kit_note(mnx_kit_notes, note) || has_note;
        }
        if (!has_note) {
            LOGW() << "Skipping chord event with no valid kit notes.";
            return false;
        }
        return true;
    }

    auto mnx_notes = mnx_event.ensure_notes();
    bool has_note = false;
    for (Note *note : chord_notes) {
        has_note = append_note(mnx_notes, note) || has_note;
    }
    if (!has_note) {
        LOGW() << "Skipping chord event with no convertible notes.";
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   createRest
//   emits a MNX rest for the given chord/rest
//   returns true when appended
//---------------------------------------------------------

bool MnxExporter::create_rest(mnx::sequence::Event &mnx_event, ChordRest *chord_rest) {
    IF_ASSERT_FAILED(chord_rest->is_rest()) { return false; }
    auto mnx_rest = mnx_event.ensure_rest();

    if (export_rest_positions_) {
        if (Rest *rest = to_rest(chord_rest)) {
            const Staff *staff = rest->staff();
            if (staff && rest->ldata() && rest->ldata()->is_set_pos()) {
                const double line_dist = staff->line_distance(rest->tick());
                const double staff_step = line_dist * rest->spatium() * 0.5; // half-space
                if (staff_step > 0.0) {
                    const int middle_line = staff->middle_line(rest->tick());
                    const double y = rest->pos().y();
                    const int line_index = static_cast<int>(std::lround(y / staff_step));
                    mnx_rest.set_staffPosition(middle_line - line_index);
                } else {
                    LOGW() << "Skipping MNX rest staffPosition export; invalid staff step.";
                }
            } else {
                LOGW() << "Skipping MNX rest staffPosition export; missing layout position or staff.";
            }
        }
    }

    return true;
}

//---------------------------------------------------------
//   createBeam
//   emits MNX beams for a single primary beam group
//---------------------------------------------------------

void MnxExporter::create_beam(ExportContext &ctx, ChordRest *chord_rest) {
    IF_ASSERT_FAILED(chord_rest) { return; }

    Beam *beam = chord_rest->beam();
    if (!beam) {
        return;
    }
    const std::vector<ChordRest *> &elements = beam->elements();
    if (elements.empty() || elements.front() != chord_rest) {
        return;
    }
    IF_ASSERT_FAILED(elements.front()->measure() == ctx.measure) { return; }
    /// @todo Handle beams that cross system breaks, rather than just mirroring layout segments.
    /// This work is deferred on the chance that it may become much simpler if
    /// MuseScore also implements beams across system breaks.

    auto mnx_beams = ctx.mnx_measure.ensure_beams();

    enum class BeamAction { BEGIN, CONTINUE, END, FORWARD_HOOK, BACKWARD_HOOK };

    auto append_beam_level = [&](auto &&self, mnx::Array<mnx::part::Beam> &mnx_beam_array, const std::vector<ChordRest *> &beam_elements,
                                 size_t start_idx, size_t end_idx, int level) -> void {
        auto beam_action_for_level = [&](size_t idx) -> std::optional<BeamAction> {
            int prev_beams = -1;
            int current_beams = beam_elements[idx]->beams();
            int next_beams = -1;

            BeamMode current_beam_mode = beam_elements[idx]->beam_mode();
            BeamMode next_beam_mode = BeamMode::AUTO;

            if (idx > start_idx) {
                prev_beams = beam_elements[idx - 1]->beams();
            }
            if (idx + 1 <= end_idx) {
                next_beams = beam_elements[idx + 1]->beams();
                next_beam_mode = beam_elements[idx + 1]->beam_mode();
            }

            auto participates = [&](int beams) { return beams >= level; };

            const bool cur_part = participates(current_beams);
            const bool prev_part = participates(prev_beams);
            const bool next_part = participates(next_beams);

            // Non-participants can never begin/continue/end/hook at this level.
            if (!cur_part) {
                return std::nullopt;
            }

            // Forced begins from BeamMode (but still only on participants)
            const bool forced_begin =
                (current_beam_mode == BeamMode::BEGIN16 && level > 1) || (current_beam_mode == BeamMode::BEGIN32 && level > 2);

            const bool forced_end =
                (next_beam_mode == BeamMode::BEGIN16 && level > 1) || (next_beam_mode == BeamMode::BEGIN32 && level > 2);

            // Isolated at this level => hook
            if (!prev_part && !next_part) {
                // Direction heuristic: point toward musical flow
                if (idx == end_idx) {
                    return BeamAction::BACKWARD_HOOK;
                }
                return BeamAction::FORWARD_HOOK;
            }

            if (!prev_part || forced_begin) {
                return BeamAction::BEGIN;
            }

            if (!next_part || forced_end) {
                return BeamAction::END;
            }

            // Fully surrounded at this level
            return BeamAction::CONTINUE;
        };

        struct BeamRange {
            mnx::part::Beam beam;
            size_t start_idx = 0;
            size_t end_idx = 0;
        };

        std::optional<size_t> current_start;
        std::optional<mnx::part::Beam> current_beam;
        std::vector<BeamRange> ranges;

        for (size_t idx = start_idx; idx <= end_idx; ++idx) {
            const auto action = beam_action_for_level(idx);
            if (!action) {
                continue;
            }
            const std::string event_id = get_or_assign_eid(beam_elements[idx]).to_std_string();

            if (action == BeamAction::BEGIN) {
                current_beam = mnx_beam_array.append();
                current_start = idx;
                current_beam->events().push_back(event_id);
            } else if (action == BeamAction::CONTINUE) {
                IF_ASSERT_FAILED(current_beam) { continue; }
                current_beam->events().push_back(event_id);
            } else if (action == BeamAction::END) {
                IF_ASSERT_FAILED(current_beam && current_start) { continue; }
                current_beam->events().push_back(event_id);
                ranges.push_back(BeamRange{current_beam.value(), current_start.value(), idx});
                current_beam.reset();
                current_start.reset();
            } else if (action == BeamAction::FORWARD_HOOK || action == BeamAction::BACKWARD_HOOK) {
                auto hook_beam = mnx_beam_array.append();
                hook_beam.events().push_back(event_id);
                /// @note For now, we do not set leave hook direction auto, because the user cannot override it.
                /// If we ever need to we can base it on BeamAction::ForwardHook & BeamAction::BackwardHook.
            }
        }

        for (auto &range : ranges) {
            bool has_nested = false;
            for (size_t idx = range.start_idx; idx <= range.end_idx; ++idx) {
                if (beam_elements[idx]->beams() >= level + 1) {
                    has_nested = true;
                    break;
                }
            }
            if (!has_nested) {
                continue;
            }
            auto nested = range.beam.ensure_beams();
            self(self, nested, beam_elements, range.start_idx, range.end_idx, level + 1);
        }
    };

    append_beam_level(append_beam_level, mnx_beams, elements, 0, elements.size() - 1, 1);
}

//---------------------------------------------------------
//   appendEvent
//   emits a single MNX event (duration + rest/notes)
//   returns true when appended
//---------------------------------------------------------

bool MnxExporter::append_event(mnx::sequence::SequenceContent content, ExportContext &ctx, ChordRest *chord_rest) {
    const TDuration duration = chord_rest->duration_type();
    IF_ASSERT_FAILED(duration.type() != DurationType::V_MEASURE) {
        LOGW() << "appendEvent received V_MEASURE duration; full-measure rests must be exported via sequence.fullMeasure.";
        return false;
    }
    const bool is_rest = chord_rest->is_rest();

    if (is_rest && (!chord_rest->visible() || to_rest(chord_rest)->is_gap())) {
        /// @todo Revisit doing this for `!visible()` if MNX adds support for explicit rest visibility.
        const Fraction gap_ticks = chord_rest->ticks();
        const mnx::FractionValue gap_duration(static_cast<mnx::FractionValue::NumType>(gap_ticks.numerator()),
                                              static_cast<mnx::FractionValue::NumType>(gap_ticks.denominator()));
        content.appendSpace(gap_duration);
        return true;
    }

    const auto note_value = to_mnx_note_value(duration);
    if (!note_value) {
        LOGW() << "Skipping ChordRest with unsupported MNX duration type: " << static_cast<int>(duration.type());
        return false;
    }
    auto mnx_event = content.appendEvent(note_value->base, note_value->dots);

    mnx_event.set_id(get_or_assign_eid(chord_rest).to_std_string());
    create_lyrics(mnx_event, chord_rest, lyric_line_ids_);
    create_markings(mnx_event, chord_rest);
    if (chord_rest->staff_move() != 0) {
        const int cross_staff = ctx.mnx_part_staff + chord_rest->staff_move();
        if (cross_staff >= 1 && cross_staff <= static_cast<int>(ctx.part->nstaves())) {
            mnx_event.set_staff(cross_staff);
        } else {
            LOGW() << "Skipping cross staff to staff not contained within part.";
        }
    }
    if (chord_rest->is_chord()) {
        DirectionV stem_dir = chord_rest->beam() ? chord_rest->beam()->direction() : DirectionV::AUTO;
        if (stem_dir == DirectionV::AUTO) {
            stem_dir = to_chord(chord_rest)->stem_direction();
        }
        if (stem_dir != DirectionV::AUTO) {
            using MnxDir = mnx::StemDirection;
            mnx_event.set_stemDirection(stem_dir == DirectionV::UP ? MnxDir::Up : MnxDir::Down);
        }
    }
    /// @note slurs are created in exportSpanners

    const bool success = is_rest ? create_rest(mnx_event, chord_rest) : create_notes(mnx_event, chord_rest);

    if (success) {
        cr_to_mnx_event_.emplace(chord_rest, mnx_event.pointer());
        if (export_beams_) {
            create_beam(ctx, chord_rest);
        }
    } else {
        content.erase(content.size() - 1);
    }

    return success;
}

//---------------------------------------------------------
//   appendGrace
//   emit a grace container and recurse into its content
//---------------------------------------------------------

void MnxExporter::append_grace(mnx::sequence::SequenceContent content, ExportContext &ctx, GraceNotesGroup &grace_notes) {
    if (grace_notes.empty()) {
        return;
    }

    // Emit separate Grace containers whenever slash visibility changes so runs with
    // identical stem-slash settings stay together.
    for (size_t start = 0; start < grace_notes.size();) {
        const bool slash = grace_notes[start]->show_stem_slash();
        size_t end = start + 1;
        while (end < grace_notes.size() && grace_notes[end]->show_stem_slash() == slash) {
            ++end;
        }

        auto mnx_grace = content.appendGrace();
        mnx_grace.set_slash(slash);
        /// @todo Grace note playback type has no obvious mapping from MuseScore. Revisit as appropriate.

        std::vector<ChordRest *> grace_chord_rests;
        grace_chord_rests.reserve(end - start);
        for (size_t i = start; i < end; ++i) {
            grace_chord_rests.push_back(grace_notes[i]);
        }

        append_content(mnx_grace.content(), ctx, grace_chord_rests, ContentContext::GRACE);
        start = end;
    }
}

//---------------------------------------------------------
//   findTopTuplet
//   find highest tuplet not already on the stack
//---------------------------------------------------------

const Tuplet *MnxExporter::find_top_tuplet(ChordRest *chord_rest, const ExportContext &ctx) const {
    const Tuplet *tuplet = chord_rest ? chord_rest->tuplet() : nullptr;
    const Tuplet *top_most = nullptr;
    for (const Tuplet *cursor = tuplet; cursor; cursor = cursor->tuplet()) {
        const bool active_tuplet = std::find(ctx.tuplet_stack.begin(), ctx.tuplet_stack.end(), cursor) != ctx.tuplet_stack.end();
        if (!active_tuplet) {
            top_most = cursor;
        }
    }
    return top_most;
}

//---------------------------------------------------------
//   appendTuplet
//   start a tuplet container and recurse into its content
//   returns last processed index
//---------------------------------------------------------

size_t MnxExporter::append_tuplet(mnx::sequence::SequenceContent content, ExportContext &ctx, const std::vector<ChordRest *> &chord_rests,
                                  size_t idx, ChordRest *chord_rest, const Tuplet *tuplet) {
    IF_ASSERT_FAILED(tuplet) { return idx; }

    std::vector<ChordRest *> tuplet_chord_rests;
    size_t last_tuplet_idx = idx;
    for (size_t scan = idx; scan < chord_rests.size(); ++scan) {
        ChordRest *scan_cr = chord_rests[scan];
        if (tuplet_contains_chord_rest(tuplet, scan_cr)) {
            tuplet_chord_rests.push_back(scan_cr);
            last_tuplet_idx = scan;
        } else if (!tuplet_chord_rests.empty()) {
            break;
        }
    }

    const auto base_note_value = to_mnx_note_value(tuplet->base_len());
    const Fraction ratio = tuplet->ratio();
    const bool ratio_valid = ratio.numerator() > 0 && ratio.denominator() > 0;
    if (tuplet_chord_rests.empty() || !base_note_value || !ratio_valid) {
        if (!tuplet_chord_rests.empty()) {
            LOGW() << "Skipping tuplet with unsupported MNX base note value or ratio.";
        }
        return idx;
    }

    if (chord_rest->is_chord()) {
        const Chord *chord = to_chord(chord_rest);
        if (ctx.grace_before_emitted.insert(chord_rest).second) {
            append_grace(content, ctx, chord->grace_notes_before());
        }
    }

    auto inner = mnx::NoteValueQuantity::make(static_cast<unsigned>(ratio.numerator()), *base_note_value);
    auto outer = mnx::NoteValueQuantity::make(static_cast<unsigned>(ratio.denominator()), *base_note_value);
    auto mnx_tuplet = content.appendTuplet(inner, outer);
    mnx_tuplet.set_or_clear_showNumber(to_mnx_tuplet_number_type(tuplet->number_type()));
    mnx_tuplet.set_or_clear_bracket(to_mnx_tuplet_bracket_type(tuplet->bracket_type()));
    /// @todo add `showValue` if MuseScore supports showing note values on tuplet relation text.

    ctx.tuplet_stack.push_back(tuplet);
    append_content(mnx_tuplet.content(), ctx, tuplet_chord_rests, ContentContext::TUPLET);
    ctx.tuplet_stack.pop_back();
    const ChordRest *last_tuplet_cr = tuplet_chord_rests.back();
    if (last_tuplet_cr && last_tuplet_cr->is_chord()) {
        if (ctx.grace_after_emitted.insert(last_tuplet_cr).second) {
            append_grace(content, ctx, to_chord(last_tuplet_cr)->grace_notes_after());
        }
    }
    return last_tuplet_idx; // shift index to last idx in tuplet
}

//---------------------------------------------------------
//   appendTremolo
//   start a tremolo container and recurse into its content
//   returns last processed index
//---------------------------------------------------------

size_t MnxExporter::append_tremolo(mnx::sequence::SequenceContent content, ExportContext &ctx, const std::vector<ChordRest *> &chord_rests,
                                   size_t idx, ChordRest *chord_rest) {
    Chord *chord = chord_rest->is_chord() ? to_chord(chord_rest) : nullptr;
    const TremoloTwoChord *tremolo = chord ? chord->tremolo_two_chord() : nullptr;
    IF_ASSERT_FAILED(chord && tremolo) {
        LOGW() << "Skipping ChordRest that is not part of a tremolo.";
        return idx;
    }

    IF_ASSERT_FAILED(tremolo->chord1() == chord_rest) {
        LOGW() << "Skipping tremolo with unexpected first chord.";
        return idx;
    }

    Chord *chord2 = tremolo->chord2();
    IF_ASSERT_FAILED(chord2) {
        LOGW() << "Skipping tremolo with missing second chord.";
        return idx;
    }

    if (!chord->grace_notes_after().empty() || !chord2->grace_notes_before().empty()) {
        LOGW() << "Skipping tremolo with grace notes inside tremolo content.";
        return idx;
    }

    if (idx + 1 >= chord_rests.size() || chord_rests[idx + 1] != chord2) {
        LOGW() << "Skipping tremolo with non-adjacent chord events.";
        return idx;
    }

    const TremoloType tremolo_type = tremolo->tremolo_type();
    const int marks = static_cast<int>(tremolo_type) - static_cast<int>(TremoloType::C8) + 1;
    if (marks <= 0) {
        LOGW() << "Skipping tremolo with unsupported tremolo type.";
        return idx;
    }

    TDuration tremolo_duration = tremolo->duration_type();
    if (tremolo_duration.is_valid()) {
        tremolo_duration = tremolo_duration.shift_retain_dots(1); // +1 divides the duration by 2.
    }
    if (!tremolo_duration.is_valid()) {
        LOGW() << "Skipping 2-note tremolo with invalide duration typew.";
        return idx;
    }
    const auto tremolo_note_value = to_mnx_note_value(tremolo_duration);
    if (!tremolo_note_value) {
        LOGW() << "Skipping tremolo with unsupported MNX duration type.";
        return idx;
    }

    auto outer = mnx::NoteValueQuantity::make(2, *tremolo_note_value);
    auto mnx_tremolo = content.appendMultiNoteTremolo(marks, outer);
    /// @todo Perhaps export tremolo individual duration if MNX provides clarity about it.

    std::vector<ChordRest *> tremolo_chord_rests{chord_rest, chord2};
    if (ctx.grace_before_emitted.insert(chord_rest).second) {
        append_grace(content, ctx, chord->grace_notes_before());
    }
    append_content(mnx_tremolo.content(), ctx, tremolo_chord_rests, ContentContext::TREMOLO);
    if (ctx.grace_after_emitted.insert(chord2).second) {
        append_grace(content, ctx, chord2->grace_notes_after());
    }
    return idx + 1; // shift index to last idx in tremolo
}

//---------------------------------------------------------
//   appendContent
//   walk chord/rest events into MNX content
//---------------------------------------------------------

void MnxExporter::append_content(mnx::sequence::SequenceContent content, ExportContext &ctx, const std::vector<ChordRest *> &chord_rests,
                                 ContentContext context) {
    for (size_t idx = 0; idx < chord_rests.size(); ++idx) {
        ChordRest *chord_rest = chord_rests[idx];
        IF_ASSERT_FAILED(chord_rest) {
            LOGW() << "Skipping null ChordRest while exporting MNX content.";
            continue;
        }

        const bool in_grace = context == ContentContext::GRACE;
        const bool is_grace = chord_rest->is_grace();
        IF_ASSERT_FAILED((is_grace && in_grace) || (!is_grace && !in_grace)) {
            LOGW() << "Skipping grace note content with unexpected grace context.";
            continue;
        }

        const bool in_tremolo = context == ContentContext::TREMOLO;

        if (!in_grace) {
            if (chord_rest->tuplet()) {
                const Tuplet *top_most = find_top_tuplet(chord_rest, ctx);
                if (top_most && first_tuplet_chord_rest(top_most) == chord_rest) {
                    const size_t last_idx = append_tuplet(content, ctx, chord_rests, idx, chord_rest, top_most);
                    if (last_idx >= idx) {
                        idx = last_idx;
                        continue;
                    }
                    IF_ASSERT_FAILED(last_idx < idx) { LOGW() << "Invalid index returned by appendTuplet."; }
                }
            }
            if (chord_rest->is_chord() && to_chord(chord_rest)->tremolo_two_chord()) {
                if (!in_tremolo) {
                    const Chord *chord = to_chord(chord_rest);
                    const TremoloTwoChord *tremolo = chord->tremolo_two_chord();
                    if (tremolo && tremolo->chord1() == chord_rest) {
                        const size_t last_idx = append_tremolo(content, ctx, chord_rests, idx, chord_rest);
                        if (last_idx >= idx) {
                            idx = last_idx;
                            continue;
                        }
                        IF_ASSERT_FAILED(last_idx < idx) { LOGW() << "Invalid index returned by appendTremolo."; }
                    }
                }
            }
        }

        // Tremolos manage their own grace content. Grace notes cannot have grace notes.
        // Tuplet boundaries are handled via graceBeforeEmitted/graceAfterEmitted in appendTuplet.
        if (!in_tremolo && !in_grace && chord_rest->is_chord()) {
            if (ctx.grace_before_emitted.insert(chord_rest).second) {
                append_grace(content, ctx, to_chord(chord_rest)->grace_notes_before());
            }
        }

        const bool event_appended = append_event(content, ctx, chord_rest);

        if (event_appended && !in_tremolo && !in_grace && chord_rest->is_chord()) {
            if (ctx.grace_after_emitted.insert(chord_rest).second) {
                append_grace(content, ctx, to_chord(chord_rest)->grace_notes_after());
            }
        }
    }
}

//---------------------------------------------------------
//   createSequences
//---------------------------------------------------------

void MnxExporter::create_sequences(const Part *part, const Measure *measure, mnx::part::Measure &mnx_measure) {
    const size_t staves = part->nstaves();
    const track_idx_t part_start_track = part->track_range().start_track;
    auto mnx_sequences = mnx_measure.sequences();

    for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
        for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
            const track_idx_t cur_track_idx = part_start_track + ::mu::engraving::voices * staff_idx + voice;
            std::vector<ChordRest *> chord_rests;

            for (Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
                EngravingItem *item = segment->element(cur_track_idx);
                if (!item) {
                    continue;
                }
                chord_rests.push_back(to_chord_rest(item));
            }

            if (chord_rests.empty()) {
                continue;
            }

            auto mnx_sequence = mnx_sequences.append();
            if (staves > 1) {
                mnx_sequence.set_staff(static_cast<int>(staff_idx + 1));
            }
            mnx_sequence.set_voice(make_mnx_voice_id_from_track(mnx_sequence.staff(), cur_track_idx));

            if (chord_rests.size() == 1) {
                ChordRest *only_cr = chord_rests.front();
                if (only_cr->is_rest()) {
                    Rest *rest = to_rest(only_cr);
                    if (rest->duration_type().is_measure()) {
                        if (rest->visible() && !rest->is_gap()) {
                            auto full_measure = mnx_sequence.ensure_fullMeasure();
                            if (export_rest_positions_ && rest->staff() && rest->ldata() && rest->ldata()->is_set_pos()) {
                                const double line_dist = rest->staff()->line_distance(rest->tick());
                                const double staff_step = line_dist * rest->spatium() * 0.5; // half-space
                                if (staff_step > 0.0) {
                                    const int middle_line = rest->staff()->middle_line(rest->tick());
                                    const double y = rest->pos().y();
                                    const int line_index = static_cast<int>(std::lround(y / staff_step));
                                    full_measure.set_staffPosition(middle_line - line_index);
                                } else {
                                    LOGW() << "Skipping MNX fullMeasure staffPosition export; invalid staff step.";
                                }
                            }
                            process_annotations(full_measure, rest);
                        } else {
                            /// @todo If MNX adds explicit rest visibility, export hidden (non-gap) full-measure rests; keep omitting gap rests.
                            // Hidden/gap measure rests should not generate a sequence.
                            mnx_sequences.erase(mnx_sequences.size() - 1);
                        }
                        continue;
                    }
                }
            }

            ExportContext ctx(part, measure, mnx_measure, static_cast<staff_idx_t>(staff_idx), voice, mnx_sequence.staff());
            append_content(mnx_sequence.content(), ctx, chord_rests, ContentContext::SEQUENCE);
        }
    }

    // Avoid cluttering the output with unnecessary full-measure rests.
    // Keep a solitary full-measure sequence only when the full measure rest
    // carries additional information, such as a staff position or fermata.
    if (mnx_sequences.size() == 1) {
        auto only_sequence = mnx_sequences.at(0);
        const auto full_measure = only_sequence.fullMeasure();
        if (full_measure && only_sequence.content().empty() && full_measure->empty()) {
            mnx_sequences.erase(0);
        }
    }
}
} // namespace mu::iex::mnxio
