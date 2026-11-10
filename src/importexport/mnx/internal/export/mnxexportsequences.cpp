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
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "engraving/dom/accidental.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/key.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
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
#include "mnxexporter.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
//---------------------------------------------------------
//   exportAccidentalDetails
//   export accidental details into accidentalDisplay
//---------------------------------------------------------

static void export_accidental_details(model::Note &mnx_note, const Note *note) {
    if (Accidental *acc = note->accidental()) {
        bool force = acc->role() == AccidentalRole::USER;
        const AccidentalBracket bracket = acc->bracket();
        if (force || bracket != AccidentalBracket::NONE) {
            auto &acc_disp = ensure(mnx_note.accidental_display);
            acc_disp.show = acc->visible();
            acc_disp.force = force;
            if (bracket != AccidentalBracket::NONE) {
                acc_disp.enclosure = model::AccidentalEnclosure{.symbol = bracket == AccidentalBracket::PARENTHESIS ?
                                                                              model::AccidentalEnclosureSymbol::PARENTHESES :
                                                                              model::AccidentalEnclosureSymbol::BRACKETS};
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

static void create_lyrics(model::Event &mnx_event, const ChordRest *cr, std::set<std::string> &lyric_line_ids) {
    IF_ASSERT_FAILED(cr) { return; }

    const auto lyrics = cr->lyrics();
    if (lyrics.empty()) {
        return;
    }

    auto &mnx_lines = ensure(ensure(mnx_event.lyrics).lines);

    for (const auto &lyric : lyrics) {
        const std::string line_id = std::to_string(lyric->verse() + 1);
        lyric_line_ids.insert(line_id);
        model::EventLyricLine mnx_line;
        mnx_line.text = lyric->plain_text().to_std_string();
        set_unless_default(mnx_line.type, to_mnx_lyric_line_type(lyric->syllabic()), model::EventLyricLineType::WHOLE);
        mnx_lines.emplace_back(line_id, std::move(mnx_line));
        /// @todo export styled text when supported by MNX.
        /// @todo export word extension span when supported by MNX.
    }
}

//---------------------------------------------------------
//   createMarking
//   export a single marking from an Articulation instance
//---------------------------------------------------------

static void create_marking(const Articulation *a, model::EventMarkings &mnx_markings) {
    const model::Orientation orient = to_mnx_orientation(a->anchor());
    auto mark = [&](auto &marking) { set_unless_default(ensure(marking).orient, orient, model::Orientation::AUTO); };
    auto bow = [&](model::UpDown direction) {
        auto &bow_direction = ensure(mnx_markings.bow_direction);
        bow_direction.direction = direction;
        set_unless_default(bow_direction.orient, orient, model::Orientation::AUTO);
    };

    switch (a->sym_id()) {
        case SymId::ARTIC_ACCENT_ABOVE:
        case SymId::ARTIC_ACCENT_BELOW: mark(mnx_markings.accent); break;
        case SymId::STRINGS_DOWN_BOW:
        case SymId::STRINGS_DOWN_BOW_TURNED: bow(model::UpDown::DOWN); break;
        case SymId::STRINGS_UP_BOW:
        case SymId::STRINGS_UP_BOW_TURNED: bow(model::UpDown::UP); break;
        case SymId::ARTIC_SOFT_ACCENT_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_BELOW: mark(mnx_markings.soft_accent); break;
        case SymId::ARTIC_STACCATO_ABOVE:
        case SymId::ARTIC_STACCATO_BELOW: mark(mnx_markings.staccato); break;
        case SymId::ARTIC_STACCATISSIMO_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_BELOW: mark(mnx_markings.staccatissimo); break;
        case SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_STROKE_BELOW: mark(mnx_markings.spiccato); break;
        case SymId::ARTIC_STRESS_ABOVE:
        case SymId::ARTIC_STRESS_BELOW: mark(mnx_markings.stress); break;
        case SymId::ARTIC_UNSTRESS_ABOVE:
        case SymId::ARTIC_UNSTRESS_BELOW: mark(mnx_markings.unstress); break;
        case SymId::ARTIC_MARCATO_ABOVE:
        case SymId::ARTIC_MARCATO_BELOW: mark(mnx_markings.strong_accent); break;
        case SymId::ARTIC_TENUTO_ABOVE:
        case SymId::ARTIC_TENUTO_BELOW: mark(mnx_markings.tenuto); break;
        default: break;
    }
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
                mnx_event.fermata = MnxExporter::mnx_fermata_from_fermata(fermata);
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

void MnxExporter::create_markings(model::Event &mnx_event, ChordRest *cr) {
    IF_ASSERT_FAILED(cr) { return; }

    if (cr->is_chord()) {
        const Chord *chord = to_chord(cr);
        for (Articulation *a : chord->articulations()) {
            IF_ASSERT_FAILED(a) { continue; }
            create_marking(a, ensure(mnx_event.markings));
        }

        if (const TremoloSingleChord *trem = chord->tremolo_single_chord()) {
            const int marks = int(trem->tremolo_type()) - int(TremoloType::R8) + 1;
            DO_ASSERT(marks > 0);
            if (marks > 0) {
                ensure(mnx_event.markings).tremolo = model::TremoloSingle{.marks = marks};
            }
        }
    }

    if (Breath *breath = cr->has_breath_mark()) {
        auto &mnx_breath = ensure(ensure(mnx_event.markings).breath);
        set_unless_default(mnx_breath.orient, to_mnx_orientation(breath->placement()), model::Orientation::AUTO);
        if (const auto breath_sym = to_mnx_breath_mark_sym(breath->sym_id())) {
            mnx_breath.symbol = breath_sym.value();
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

void MnxExporter::create_ties(std::optional<std::vector<model::Tie>> &mnx_ties, Note *note) {
    IF_ASSERT_FAILED(note) { return; }

    auto append_tie = [&](Note *target_note, const Tie *dir_tie, bool is_cross_jump) {
        IF_ASSERT_FAILED(target_note) {
            LOGW() << "Skipping tie with missing target note.";
            return;
        }

        model::Tie mnx_tie;
        mnx_tie.target = model::Id(get_or_assign_eid(target_note).to_std_string());
        if (is_cross_jump) {
            mnx_tie.target_type = model::TieTargetType::CROSS_JUMP;
        } else {
            const Fraction expected_tick = note->tick() + note->chord()->ticks();
            if (target_note->tick() == expected_tick) {
                if (target_note->track() == note->track()) {
                    mnx_tie.target_type = model::TieTargetType::NEXT_NOTE;
                } else {
                    mnx_tie.target_type = model::TieTargetType::CROSS_VOICE;
                }
            } else {
                mnx_tie.target_type = model::TieTargetType::ARPEGGIO;
            }
        }

        if (dir_tie && dir_tie->slur_direction() != DirectionV::AUTO) {
            mnx_tie.side = dir_tie->up() ? model::SlurSide::UP : model::SlurSide::DOWN;
        }
        ensure(mnx_ties).push_back(std::move(mnx_tie));
    };

    Tie *tie_for = note->tie_for();
    if (tie_for && !tie_for->is_partial_tie()) {
        if (tie_for->is_laissez_vib()) {
            model::Tie mnx_tie;
            mnx_tie.lv = true;
            if (tie_for->slur_direction() != DirectionV::AUTO) {
                mnx_tie.side = tie_for->up() ? model::SlurSide::UP : model::SlurSide::DOWN;
            }
            ensure(mnx_ties).push_back(std::move(mnx_tie));
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

bool MnxExporter::create_notes(model::Event &mnx_event, ChordRest *chord_rest) {
    IF_ASSERT_FAILED(chord_rest->is_chord()) { return false; }

    auto append_kit_note = [&](std::vector<model::KitNote> &mnx_kit_notes, Note *note) -> bool {
        const int pitch = note->pitch();
        if (!pitch_is_valid(pitch)) {
            LOGW() << "Skipping kit note with invalid MIDI pitch: " << pitch;
            return false;
        }
        model::KitNote mnx_kit_note;
        mnx_kit_note.kit_component = model::Id("drum-midi-" + std::to_string(pitch));
        mnx_kit_note.id = model::Id(get_or_assign_eid(note).to_std_string());
        create_ties(mnx_kit_note.ties, note);
        mnx_kit_notes.push_back(std::move(mnx_kit_note));
        return true;
    };

    auto append_note = [&](std::vector<model::Note> &mnx_notes, Note *note) -> bool {
        const auto pitch = to_mnx_pitch(note);
        if (!pitch) {
            LOGW() << "Skipping note with unsupported pitch.";
            return false;
        }
        model::Note mnx_note;
        mnx_note.pitch = *pitch;
        mnx_note.id = model::Id(get_or_assign_eid(note).to_std_string());
        create_ties(mnx_note.ties, note);
        export_accidental_details(mnx_note, note);
        const int delta = calc_written_diatonic_delta(note);
        if (delta != 0) {
            mnx_note.written = model::Written{.diatonic_delta = delta};
        }
        mnx_notes.push_back(std::move(mnx_note));
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
        auto &mnx_kit_notes = ensure(mnx_event.kit_notes);
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

    auto &mnx_notes = ensure(mnx_event.notes);
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

// The staff position of a laid-out rest, if it has one.
static std::optional<int> rest_staff_position(const Rest *rest) {
    const Staff *staff = rest->staff();
    if (!staff || !rest->ldata() || !rest->ldata()->is_set_pos()) {
        LOGW() << "Skipping MNX rest staffPosition export; missing layout position or staff.";
        return std::nullopt;
    }
    const double line_dist = staff->line_distance(rest->tick());
    const double staff_step = line_dist * rest->spatium() * 0.5; // half-space
    if (staff_step <= 0.0) {
        LOGW() << "Skipping MNX rest staffPosition export; invalid staff step.";
        return std::nullopt;
    }
    const int middle_line = staff->middle_line(rest->tick());
    const double y = rest->pos().y();
    const int line_index = static_cast<int>(std::lround(y / staff_step));
    return middle_line - line_index;
}

bool MnxExporter::create_rest(model::Event &mnx_event, ChordRest *chord_rest) {
    IF_ASSERT_FAILED(chord_rest->is_rest()) { return false; }
    auto &mnx_rest = ensure(mnx_event.rest);

    if (export_rest_positions_) {
        if (Rest *rest = to_rest(chord_rest)) {
            mnx_rest.staff_position = rest_staff_position(rest);
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

    auto &mnx_beams = ensure(ctx.mnx_measure.beams);

    enum class BeamAction { BEGIN, CONTINUE, END, FORWARD_HOOK, BACKWARD_HOOK };

    auto append_beam_level = [&](auto &&self, std::vector<model::Beam> &mnx_beam_array, const std::vector<ChordRest *> &beam_elements,
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

        // Beams are referred to by index: appending to the array moves its elements.
        struct BeamRange {
            size_t beam = 0;
            size_t start_idx = 0;
            size_t end_idx = 0;
        };

        std::optional<size_t> current_start;
        std::optional<size_t> current_beam;
        std::vector<BeamRange> ranges;

        for (size_t idx = start_idx; idx <= end_idx; ++idx) {
            const auto action = beam_action_for_level(idx);
            if (!action) {
                continue;
            }
            const model::Id event_id(get_or_assign_eid(beam_elements[idx]).to_std_string());

            if (action == BeamAction::BEGIN) {
                current_beam = mnx_beam_array.size();
                mnx_beam_array.emplace_back();
                current_start = idx;
                mnx_beam_array[*current_beam].events.push_back(event_id);
            } else if (action == BeamAction::CONTINUE) {
                IF_ASSERT_FAILED(current_beam) { continue; }
                mnx_beam_array[*current_beam].events.push_back(event_id);
            } else if (action == BeamAction::END) {
                IF_ASSERT_FAILED(current_beam && current_start) { continue; }
                mnx_beam_array[*current_beam].events.push_back(event_id);
                ranges.push_back(BeamRange{current_beam.value(), current_start.value(), idx});
                current_beam.reset();
                current_start.reset();
            } else if (action == BeamAction::FORWARD_HOOK || action == BeamAction::BACKWARD_HOOK) {
                mnx_beam_array.emplace_back().events.push_back(event_id);
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
            self(self, ensure(mnx_beam_array[range.beam].beams), beam_elements, range.start_idx, range.end_idx, level + 1);
        }
    };

    append_beam_level(append_beam_level, mnx_beams, elements, 0, elements.size() - 1, 1);
}

//---------------------------------------------------------
//   appendEvent
//   emits a single MNX event (duration + rest/notes)
//   returns true when appended
//---------------------------------------------------------

template <class Content>
bool MnxExporter::append_event(Content &content, ExportContext &ctx, ChordRest *chord_rest) {
    const TDuration duration = chord_rest->duration_type();
    IF_ASSERT_FAILED(duration.type() != DurationType::V_MEASURE) {
        LOGW() << "appendEvent received V_MEASURE duration; full-measure rests must be exported via sequence.fullMeasure.";
        return false;
    }
    const bool is_rest = chord_rest->is_rest();

    if (is_rest && (!chord_rest->visible() || to_rest(chord_rest)->is_gap())) {
        /// @todo Revisit doing this for `!visible()` if MNX adds support for explicit rest visibility.
        if constexpr (std::is_same_v<Content, std::vector<model::SequenceItem>>) {
            content.emplace_back(model::Space{.duration = to_mnx_fraction(chord_rest->ticks())});
            return true;
        } else {
            LOGW() << "Skipping a gap inside a grace note or tremolo, which MNX cannot hold.";
            return false;
        }
    }

    const auto note_value = to_mnx_note_value(duration);
    if (!note_value) {
        LOGW() << "Skipping ChordRest with unsupported MNX duration type: " << static_cast<int>(duration.type());
        return false;
    }
    model::Event mnx_event;
    mnx_event.duration = *note_value;

    const std::string event_id = get_or_assign_eid(chord_rest).to_std_string();
    mnx_event.id = model::Id(event_id);
    create_lyrics(mnx_event, chord_rest, lyric_line_ids_);
    create_markings(mnx_event, chord_rest);
    if (chord_rest->staff_move() != 0) {
        const int cross_staff = ctx.mnx_part_staff + chord_rest->staff_move();
        if (cross_staff >= 1 && cross_staff <= static_cast<int>(ctx.part->nstaves())) {
            mnx_event.staff = cross_staff;
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
            mnx_event.stem_direction = stem_dir == DirectionV::UP ? model::StemDirection::UP : model::StemDirection::DOWN;
        }
    }
    /// @note slurs are created in exportSpanners

    const bool success = is_rest ? create_rest(mnx_event, chord_rest) : create_notes(mnx_event, chord_rest);

    if (success) {
        content.emplace_back(std::move(mnx_event));
        cr_to_mnx_event_.emplace(chord_rest, event_id);
        if (export_beams_) {
            create_beam(ctx, chord_rest);
        }
    }

    return success;
}

//---------------------------------------------------------
//   appendGrace
//   emit a grace container and recurse into its content
//---------------------------------------------------------

void MnxExporter::append_grace(std::vector<model::SequenceItem> &content, ExportContext &ctx, GraceNotesGroup &grace_notes) {
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

        model::Grace mnx_grace;
        mnx_grace.slash = slash;
        /// @todo Grace note playback type has no obvious mapping from MuseScore. Revisit as appropriate.

        std::vector<ChordRest *> grace_chord_rests;
        grace_chord_rests.reserve(end - start);
        for (size_t i = start; i < end; ++i) {
            grace_chord_rests.push_back(grace_notes[i]);
        }

        append_content(mnx_grace.content, ctx, grace_chord_rests, ContentContext::GRACE);
        content.emplace_back(std::move(mnx_grace));
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

size_t MnxExporter::append_tuplet(std::vector<model::SequenceItem> &content, ExportContext &ctx,
                                  const std::vector<ChordRest *> &chord_rests, size_t idx, ChordRest *chord_rest, const Tuplet *tuplet) {
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

    model::Tuplet mnx_tuplet;
    mnx_tuplet.inner = model::NoteValueQuantity{.duration = *base_note_value, .multiple = ratio.numerator()};
    mnx_tuplet.outer = model::NoteValueQuantity{.duration = *base_note_value, .multiple = ratio.denominator()};
    set_unless_default(mnx_tuplet.show_number, to_mnx_tuplet_number_type(tuplet->number_type()), model::TupletDisplaySetting::INNER);
    set_unless_default(mnx_tuplet.bracket, to_mnx_tuplet_bracket_type(tuplet->bracket_type()), model::YesNoAuto::AUTO);
    /// @todo add `showValue` if MuseScore supports showing note values on tuplet relation text.

    ctx.tuplet_stack.push_back(tuplet);
    append_content(mnx_tuplet.content, ctx, tuplet_chord_rests, ContentContext::TUPLET);
    ctx.tuplet_stack.pop_back();
    content.emplace_back(std::move(mnx_tuplet));
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

size_t MnxExporter::append_tremolo(std::vector<model::SequenceItem> &content, ExportContext &ctx,
                                   const std::vector<ChordRest *> &chord_rests, size_t idx, ChordRest *chord_rest) {
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

    model::MultiNoteTremolo mnx_tremolo;
    mnx_tremolo.marks = marks;
    mnx_tremolo.outer = model::NoteValueQuantity{.duration = *tremolo_note_value, .multiple = 2};
    /// @todo Perhaps export tremolo individual duration if MNX provides clarity about it.

    std::vector<ChordRest *> tremolo_chord_rests{chord_rest, chord2};
    if (ctx.grace_before_emitted.insert(chord_rest).second) {
        append_grace(content, ctx, chord->grace_notes_before());
    }
    append_content(mnx_tremolo.content, ctx, tremolo_chord_rests, ContentContext::TREMOLO);
    content.emplace_back(std::move(mnx_tremolo));
    if (ctx.grace_after_emitted.insert(chord2).second) {
        append_grace(content, ctx, chord2->grace_notes_after());
    }
    return idx + 1; // shift index to last idx in tremolo
}

//---------------------------------------------------------
//   appendContent
//   walk chord/rest events into MNX content
//---------------------------------------------------------

template <class Content>
void MnxExporter::append_content(Content &content, ExportContext &ctx, const std::vector<ChordRest *> &chord_rests,
                                 ContentContext context) {
    // Grace notes and tremolos hold events only; tuplets, tremolos and grace notes go in sequence
    // and tuplet content.
    constexpr bool full_content = std::is_same_v<Content, std::vector<model::SequenceItem>>;
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

        if constexpr (full_content) {
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
        }

        const bool event_appended = append_event(content, ctx, chord_rest);

        if constexpr (full_content) {
            if (event_appended && !in_tremolo && !in_grace && chord_rest->is_chord()) {
                if (ctx.grace_after_emitted.insert(chord_rest).second) {
                    append_grace(content, ctx, to_chord(chord_rest)->grace_notes_after());
                }
            }
        } else {
            std::ignore = event_appended;
        }
    }
}

//---------------------------------------------------------
//   createSequences
//---------------------------------------------------------

void MnxExporter::create_sequences(const Part *part, const Measure *measure, model::PartMeasure &mnx_measure) {
    const size_t staves = part->nstaves();
    const track_idx_t part_start_track = part->track_range().start_track;
    auto &mnx_sequences = mnx_measure.sequences;

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

            auto &mnx_sequence = mnx_sequences.emplace_back();
            if (staves > 1) {
                mnx_sequence.staff = static_cast<int>(staff_idx + 1);
            }
            mnx_sequence.voice = make_mnx_voice_id_from_track(mnx_sequence.staff_or_default(), cur_track_idx);

            if (chord_rests.size() == 1) {
                ChordRest *only_cr = chord_rests.front();
                if (only_cr->is_rest()) {
                    Rest *rest = to_rest(only_cr);
                    if (rest->duration_type().is_measure()) {
                        if (rest->visible() && !rest->is_gap()) {
                            auto &full_measure = ensure(mnx_sequence.full_measure);
                            if (export_rest_positions_) {
                                full_measure.staff_position = rest_staff_position(rest);
                            }
                            process_annotations(full_measure, rest);
                        } else {
                            /// @todo If MNX adds explicit rest visibility, export hidden (non-gap) full-measure rests; keep omitting gap rests.
                            // Hidden/gap measure rests should not generate a sequence.
                            mnx_sequences.pop_back();
                        }
                        continue;
                    }
                }
            }

            ExportContext ctx(part, measure, mnx_measure, static_cast<staff_idx_t>(staff_idx), voice, mnx_sequence.staff_or_default());
            append_content(mnx_sequence.content, ctx, chord_rests, ContentContext::SEQUENCE);
        }
    }

    // Avoid cluttering the output with unnecessary full-measure rests.
    // Keep a solitary full-measure sequence only when the full measure rest
    // carries additional information, such as a staff position or fermata.
    if (mnx_sequences.size() == 1) {
        const auto &only_sequence = mnx_sequences.front();
        const auto &full_measure = only_sequence.full_measure;
        if (full_measure && only_sequence.content.empty() && !full_measure->id && !full_measure->fermata && !full_measure->staff_position &&
            !full_measure->visual_duration) {
            mnx_sequences.clear();
        }
    }
}
} // namespace mu::iex::mnxio
