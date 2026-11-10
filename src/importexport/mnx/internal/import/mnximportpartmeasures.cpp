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
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "engraving/dom/accidental.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/bracketitem.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/hook.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/laissezvib.h" // IWYU pragma: keep
#include "engraving/dom/lyrics.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteval.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/partialtie.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stem.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tiejumppointlist.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/dom/volta.h"
#include "engraving/editing/editmeasurerepeat.h"
#include "engraving/editing/transaction/transaction.h"
#include "internal/shared/mnxtypesconv.h"
#include "log.h"
#include "mnximporter.h"
#include "model/music.h"
#include "model/walk.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
namespace {
struct LyricLineInterval {
    engraving::Fraction start;
    engraving::Fraction end;
};

// The lines of an event's lyrics in line id order, which is how they used to be read.
std::vector<const std::pair<std::string, model::EventLyricLine> *> sorted_lines(const model::Lyrics &lyrics) {
    std::vector<const std::pair<std::string, model::EventLyricLine> *> lines;
    if (lyrics.lines) {
        for (const auto &line : *lyrics.lines) {
            lines.push_back(&line);
        }
    }
    std::ranges::stable_sort(lines, {}, [](const auto *line) -> const std::string & { return line->first; });
    return lines;
}

const model::EventLyricLine *find_line(const model::Lyrics &lyrics, std::string_view line_id) {
    if (lyrics.lines) {
        for (const auto &[id, line] : *lyrics.lines) {
            if (id == line_id) {
                return &line;
            }
        }
    }
    return nullptr;
}

model::Rational to_rational(const Fraction &fraction) { return model::Rational(fraction.numerator(), fraction.denominator()); }
} // namespace

//---------------------------------------------------------
//   intervalsOverlap
//   Return true if lyric intervals overlap.
//---------------------------------------------------------

static bool intervals_overlap(const LyricLineInterval &a, const LyricLineInterval &b) { return !(a.end < b.start || b.end < a.start); }

//---------------------------------------------------------
//   tupletHasOnlySpacesAndGraces
//   Detect tuplets that contain only spaces and grace containers.
//---------------------------------------------------------

static bool tuplet_has_only_spaces_and_graces(const model::Tuplet &tuplet) {
    for (const auto &item : tuplet.content) {
        if (std::holds_alternative<model::Space>(item) || std::holds_alternative<model::Grace>(item)) {
            continue;
        }
        if (const auto *nested = std::get_if<model::Tuplet>(&item)) {
            if (!tuplet_has_only_spaces_and_graces(*nested)) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   createSlur
//   Create a MuseScore slur from an MNX slur attached to start chord/rest.
//---------------------------------------------------------

void MnxImporter::create_slur(const model::Slur &mnx_slur, engraving::ChordRest *start_cr) {
    ChordRest *target_cr = mnx_event_id_to_cr(mnx_slur.target);
    if (!target_cr) {
        LOGW() << "slur target was event with eventId " << std::string(mnx_slur.target) << " that was not mapped.";
        return;
    }
    Slur *slur = Factory::create_slur(score_->dummy());
    slur->set_track(start_cr->track());
    slur->set_track2(target_cr->track());
    slur->set_start_element(start_cr);
    slur->set_end_element(target_cr);
    slur->set_tick(start_cr->tick());
    slur->set_tick2(target_cr->tick());
    score_->add_element(slur);

    if (const auto line_type = mnx_slur.line_type) {
        set_and_style_property(slur, Pid::SLUR_STYLE_TYPE, to_muse_score_slur_style_type(line_type.value()));
    }
    if (const auto side = mnx_slur.side) {
        DirectionV slur_dir = side.value() == model::SlurSide::UP ? DirectionV::UP : DirectionV::DOWN;
        set_and_style_property(slur, Pid::SLUR_DIRECTION, slur_dir);
    } else if (const auto side_end = mnx_slur.side_end) {
        DirectionV slur_dir = side_end.value() == model::SlurSide::UP ? DirectionV::UP : DirectionV::DOWN;
        set_and_style_property(slur, Pid::SLUR_DIRECTION, slur_dir);
    }
    /// @todo implement side and sideEnd in opposite directions, if/when MuseScore supports it.
    /// @todo endNote and startNote are not supported by MuseScore (yet?)
}

//---------------------------------------------------------
//   buildLyricLineVerseMap
//   Map MNX lyric line ids to MuseScore verse numbers per staff.
//---------------------------------------------------------

void MnxImporter::build_lyric_line_verse_map() {
    lyric_line_to_verse_.clear();

    const auto &line_order = index().lyric_line_order;
    std::unordered_map<std::string, size_t> line_order_index;
    line_order_index.reserve(line_order.size());
    for (size_t i = 0; i < line_order.size(); ++i) {
        line_order_index.emplace(line_order[i], i);
    }

    const bool has_line_order = !line_order_index.empty();
    for (auto &[staff_idx, line_usage] : lyric_line_usage_) {
        struct LineEntry {
            std::string id;
            LyricLineInterval interval;
        };

        std::vector<LineEntry> entries;
        entries.reserve(line_usage.size());
        for (const auto &[line_id, usage] : line_usage) {
            entries.push_back(LineEntry{line_id, {usage.first, usage.second}});
        }

        std::sort(entries.begin(), entries.end(), [&](const LineEntry &a, const LineEntry &b) {
            if (has_line_order) {
                const auto a_it = line_order_index.find(a.id);
                const auto b_it = line_order_index.find(b.id);
                const bool a_in = a_it != line_order_index.end();
                const bool b_in = b_it != line_order_index.end();
                if (a_in && b_in && a_it->second != b_it->second) {
                    return a_it->second < b_it->second;
                }
                if (a_in != b_in) {
                    return a_in;
                }
            }
            if (a.interval.start < b.interval.start) {
                return true;
            }
            if (b.interval.start < a.interval.start) {
                return false;
            }
            return a.id < b.id;
        });

        std::vector<std::vector<LyricLineInterval>> verse_intervals;
        auto &line_to_verse = lyric_line_to_verse_[staff_idx];
        for (const auto &entry : entries) {
            size_t verse_index = 0;
            for (; verse_index < verse_intervals.size(); ++verse_index) {
                bool overlaps = false;
                for (const auto &interval : verse_intervals[verse_index]) {
                    if (intervals_overlap(interval, entry.interval)) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps) {
                    break;
                }
            }
            if (verse_index == verse_intervals.size()) {
                verse_intervals.emplace_back();
            }
            verse_intervals[verse_index].push_back(entry.interval);
            line_to_verse.emplace(entry.id, static_cast<int>(verse_index));
        }
    }
}

//---------------------------------------------------------
//   createLyrics
//   Create MuseScore lyrics for a chord/rest from an MNX event.
//---------------------------------------------------------

void MnxImporter::create_lyrics(const model::Event &mnx_event, engraving::ChordRest *cr) {
    /// @todo import lyric line metadata (i.e., language code) somehow?
    if (!mnx_event.lyrics || !mnx_event.lyrics->lines) {
        return;
    }
    const model::Lyrics &lyrics = *mnx_event.lyrics;
    const auto &mnx_line_order = index().lyric_line_order;
    const auto staff_it = lyric_line_to_verse_.find(cr->staff_idx());
    if (!mnx_line_order.empty()) {
        for (size_t line_index = 0; line_index < mnx_line_order.size(); line_index++) {
            const model::EventLyricLine *line = find_line(lyrics, mnx_line_order[line_index]);
            if (!line || line->text.empty()) {
                continue;
            }
            int verse = static_cast<int>(line_index);
            if (staff_it != lyric_line_to_verse_.end()) {
                const auto mapped = staff_it->second.find(mnx_line_order[line_index]);
                if (mapped != staff_it->second.end()) {
                    verse = mapped->second;
                }
            }
            Lyrics *lyric = Factory::create_lyrics(cr);
            lyric->set_track(cr->track());
            lyric->set_verse(verse);
            lyric->set_xml_text(String::from_std_string(line->text));
            lyric->set_syllabic(to_muse_score_lyrics_syllabic(line->type_or_default()));
            /// @todo styled text when supported by MNX.
            /// @todo word extension span, if mnx ever provides it
            cr->add(lyric);
        }
        return;
    }
    for (const auto *entry : sorted_lines(lyrics)) {
        const auto &[line_id, line] = *entry;
        if (line.text.empty()) {
            continue;
        }
        Lyrics *lyric = Factory::create_lyrics(cr);
        lyric->set_track(cr->track());
        int verse = 0;
        if (staff_it != lyric_line_to_verse_.end()) {
            const auto mapped = staff_it->second.find(line_id);
            if (mapped != staff_it->second.end()) {
                verse = mapped->second;
            }
        }
        lyric->set_verse(verse);
        lyric->set_xml_text(String::from_std_string(line.text));
        lyric->set_syllabic(to_muse_score_lyrics_syllabic(line.type_or_default()));
        /// @todo import word extension span, if mnx ever provides it
        cr->add(lyric);
    }
}

//---------------------------------------------------------
//   updateLyricLineUsageForEvent
//   Track lyric line intervals per staff for verse assignment.
//---------------------------------------------------------

void MnxImporter::update_lyric_line_usage_for_event(const model::Event &event, const model::Sequence &sequence, engraving::Measure *measure,
                                                    engraving::track_idx_t cur_track_idx, const model::Rational &start_tick) {
    if (!event.lyrics || !event.lyrics->lines) {
        return;
    }
    const staff_idx_t base_staff_idx = track2staff(cur_track_idx);
    const int sequence_staff = sequence.staff_or_default();
    const int event_staff = event.staff.value_or(sequence_staff);
    int cross_staff_move = event_staff - sequence_staff;
    // Cross-staff lyric events should reserve a verse on the staff where the event lands.
    staff_idx_t target_staff_idx = static_cast<staff_idx_t>(int(base_staff_idx) + cross_staff_move);
    if (!score_->staff(target_staff_idx)) {
        target_staff_idx = base_staff_idx;
    }
    const engraving::Fraction tick = measure->tick() + to_muse_score_fraction(start_tick);
    auto &line_usage = lyric_line_usage_[target_staff_idx];
    for (const auto &[line_id, line] : *event.lyrics->lines) {
        if (line.text.empty()) {
            continue;
        }
        auto it = line_usage.find(line_id);
        if (it == line_usage.end()) {
            line_usage.emplace(line_id, std::make_pair(tick, tick));
        } else {
            if (tick < it->second.first) {
                it->second.first = tick;
            }
            if (it->second.second < tick) {
                it->second.second = tick;
            }
        }
    }
}

//---------------------------------------------------------
//   createTies
//   Create MuseScore ties (including laissez vibrer and jump ties) from MNX tie data.
//---------------------------------------------------------

void MnxImporter::create_ties(const std::vector<model::Tie> &ties, engraving::Note *start_note) {
    bool created_non_jump_tie = false;
    for (const model::Tie &mnx_tie : ties) {
        if (mnx_tie.target_type == model::TieTargetType::CROSS_JUMP) {
            continue;
        }

        const bool is_lv = mnx_tie.lv_or_default() || !mnx_tie.target;
        Note *target_note = nullptr;
        if (!is_lv) {
            target_note = mnx_note_id_to_note(*mnx_tie.target);
            if (!target_note) {
                LOGW() << "Skipping tie to note with noteId " << std::string(*mnx_tie.target) << " that was not mapped.";
                continue;
            }
        }

        Tie *tie = is_lv ? Factory::create_laissez_vib(start_note) : Factory::create_tie(start_note);
        tie->set_start_note(start_note);
        tie->set_tick(start_note->tick());
        tie->set_track(start_note->track());
        start_note->set_tie_for(tie);
        DirectionV tie_dir = DirectionV::AUTO;
        if (const auto side = mnx_tie.side) {
            tie_dir = side.value() == model::SlurSide::UP ? DirectionV::UP : DirectionV::DOWN;
            set_and_style_property(tie, Pid::SLUR_DIRECTION, tie_dir);
        }
        if (!is_lv) {
            tie->set_end_note(target_note);
            tie->set_tick2(target_note->tick());
            tie->set_track2(target_note->track());
            target_note->set_tie_back(tie);
        }
        created_non_jump_tie = true;
        break;
    }

    Tie *start_tie = start_note->tie_for();
    if (created_non_jump_tie && start_tie && start_tie->is_laissez_vib()) {
        return;
    }

    struct JumpTieTarget {
        const model::Tie *tie = nullptr;
        Note *target_note = nullptr;
    };

    std::vector<JumpTieTarget> jump_targets;
    for (const model::Tie &mnx_tie : ties) {
        if (mnx_tie.target_type != model::TieTargetType::CROSS_JUMP || !mnx_tie.target) {
            continue;
        }

        Note *target_note = mnx_note_id_to_note(*mnx_tie.target);
        if (!target_note) {
            LOGW() << "tie target was note with noteId " << std::string(*mnx_tie.target) << " that was not mapped.";
            continue;
        }
        jump_targets.push_back(JumpTieTarget{&mnx_tie, target_note});
    }

    // sort may not be strictly necessary
    std::sort(jump_targets.begin(), jump_targets.end(), [](const JumpTieTarget &a, const JumpTieTarget &b) {
        if (a.target_note->tick() != b.target_note->tick()) {
            return a.target_note->tick() < b.target_note->tick();
        }
        return a.target_note->track() < b.target_note->track();
    });

    for (const JumpTieTarget &jump_target : jump_targets) {
        const model::Tie &mnx_tie = *jump_target.tie;
        Note *target_note = jump_target.target_note;

        DirectionV tie_dir = DirectionV::AUTO;
        if (const auto side = mnx_tie.side) {
            tie_dir = side.value() == model::SlurSide::UP ? DirectionV::UP : DirectionV::DOWN;
        }
        if (!start_tie || start_tie->is_partial_tie()) {
            PartialTie *tie = Factory::create_partial_tie(start_note);
            tie->set_start_note(start_note);
            tie->set_tick(start_note->tick());
            tie->set_track(start_note->track());
            start_note->set_tie_for(tie);
            start_tie = tie;
            set_and_style_property(tie, Pid::SLUR_DIRECTION, tie_dir);
        }

        TieJumpPointList *jump_points = start_tie->tie_jump_points();
        IF_ASSERT_FAILED(jump_points) { continue; }
        const int jump_point_idx = static_cast<int>(jump_points->size());
        TieJumpPoint *jump_point = new TieJumpPoint(target_note, true, jump_point_idx, false);
        jump_points->add(jump_point);
        jump_points->undo_add_tie_to_score(jump_point);
        if (Tie *end_tie = jump_point->end_tie()) {
            set_and_style_property(end_tie, Pid::SLUR_DIRECTION, tie_dir);
        }
    }
}

//---------------------------------------------------------
//   createAccidentals
//   Apply MNX accidental display to a MuseScore note.
//---------------------------------------------------------

void MnxImporter::create_accidentals(const model::Note &mnx_note, Note *note, Measure *measure) {
    const auto &accidental_display = mnx_note.accidental_display;
    bool force_accidental = accidental_display && accidental_display->force_or_default();
    bool show_accidental = accidental_display ? accidental_display->show : false;
    bool has_enclosure = false;
    model::AccidentalEnclosureSymbol enclosure_symbol = model::AccidentalEnclosureSymbol::PARENTHESES;
    if (accidental_display && accidental_display->enclosure) {
        has_enclosure = true;
        enclosure_symbol = accidental_display->enclosure->symbol;
    }

    if (!force_accidental) {
        if (!use_accidental_display_) {
            return;
        }
        IF_ASSERT_FAILED(measure) { return; }
        AccidentalVal acc_val = tpc2alter(note->tpc());
        AccidentalVal current_val = measure->find_accidental(note);
        // autoShow mirrors the default accidental state at this note before forcing any override.
        bool auto_show = acc_val != current_val;
        if (auto_show == show_accidental) {
            return;
        }
    }

    AccidentalVal acc_val = tpc2alter(note->tpc());
    AccidentalType acc_type = Accidental::value2subtype(acc_val);
    Accidental *accidental = Factory::create_accidental(note);
    accidental->set_accidental_type(acc_type);
    accidental->set_role(AccidentalRole::USER);
    accidental->set_visible(show_accidental);
    if (show_accidental && has_enclosure) {
        accidental->set_bracket(enclosure_symbol == model::AccidentalEnclosureSymbol::PARENTHESES ? AccidentalBracket::PARENTHESIS :
                                                                                                    AccidentalBracket::BRACKET);
    }
    note->add(accidental);
}

//---------------------------------------------------------
//   importRestProperties
//   Import rest properties.
//---------------------------------------------------------

void MnxImporter::import_rest_properties([[maybe_unused]] const model::Rest &mnx_rest, [[maybe_unused]] Rest *rest) {
    // nothing to do in this function yet.
    /// @todo Override rest positioning if MuseScore exposes a straightforward staff-line override for rests.
}

//---------------------------------------------------------
//   emitGapRest
//   Emit a spacer (gap) rest for MNX space content.
//---------------------------------------------------------

Rest *MnxImporter::emit_gap_rest(Measure *measure, track_idx_t cur_track_idx, const model::Rational &start_tick,
                                 const model::Rational &duration, Tuplet *tuplet_to_add) {
    Segment *segment = measure->get_segment_r(SegmentType::CHORD_REST, to_muse_score_fraction(start_tick));
    TDuration d(to_muse_score_fraction(duration));
    if (!d.is_valid()) {
        return nullptr;
    }
    Rest *rest = Factory::create_rest(segment, d);
    rest->set_duration_type(d);
    rest->set_gap(true);
    rest->set_track(cur_track_idx);
    rest->set_ticks(rest->actual_duration_type().fraction());
    segment->add(rest);
    if (tuplet_to_add) {
        tuplet_to_add->add(rest);
    }
    return rest;
}

//---------------------------------------------------------
//   createNote
//   Create a MuseScore note from an MNX note, applying transposition.
//---------------------------------------------------------

Note *MnxImporter::create_note(const model::Note &mnx_note, const model::EventEntry &entry, Chord *chord, Staff *base_staff,
                               const Fraction &tick, track_idx_t cur_track_idx) {
    Note *note = Factory::create_note(chord);
    note->set_track(cur_track_idx);
    NoteVal nval = to_muse_score_note_val(model::spelled(mnx_note.pitch), base_staff->concert_key(tick), entry.ottava_shift);
    // The written pitch accounts for the part transposition and MNX "written".
    const model::Key *concert_key = model::current_key(root(), index(), entry.measure);
    const model::SpelledPitch written =
        model::written_pitch(mnx_note, part_transpositions_.at(entry.part), concert_key ? concert_key->fifths : 0);
    NoteVal nval_transposed = to_muse_score_note_val(written, base_staff->key(tick), entry.ottava_shift);
    nval.tpc2 = nval_transposed.tpc2;
    note->set_nval(nval);
    chord->add(note);
    mnx_note_to_note_.emplace(&mnx_note, note);
    return note;
}

//---------------------------------------------------------
//   createTuplet
//   Create a MuseScore tuplet from an MNX tuplet container.
//---------------------------------------------------------

Tuplet *MnxImporter::create_tuplet(const model::Tuplet &mnx_tuplet, Measure *measure, track_idx_t cur_track_idx,
                                   const model::Rational &start_tick) {
    TDuration base_len = to_muse_score_duration(mnx_tuplet.outer.duration);
    const model::Rational ratio_divisor = model::duration_of(mnx_tuplet.outer) / model::duration_of(mnx_tuplet.inner.duration);
    if (!base_len.is_valid() || ratio_divisor.denominator != 1) {
        LOGE() << "Unable to import tuplet at tick " << to_muse_score_fraction(start_tick).to_string() << " of measure "
               << measure->measure_index();
        return nullptr;
    }
    Fraction tuplet_ratio = Fraction(mnx_tuplet.inner.multiple, static_cast<int>(ratio_divisor.numerator));

    Tuplet *t = Factory::create_tuplet(measure);
    t->set_track(cur_track_idx);
    t->set_tick(measure->tick() + to_muse_score_fraction(start_tick));
    t->set_ratio(tuplet_ratio);
    t->set_base_len(base_len);
    Fraction f = base_len.fraction() * tuplet_ratio.denominator();
    t->set_ticks(f.reduced());
    // options
    t->set_number_type(to_muse_score_tuplet_number_type(mnx_tuplet.show_number_or_default()));
    t->set_bracket_type(to_muse_score_tuplet_bracket_type(mnx_tuplet.bracket_or_default()));
    return t;
}

//---------------------------------------------------------
//   createTremolo
//   Create a two-note tremolo from an MNX MultiNoteTremolo.
//---------------------------------------------------------

void MnxImporter::create_tremolo(const model::MultiNoteTremolo &mnx_tremolo, Measure *measure, track_idx_t cur_track_idx,
                                 const model::Rational &start_tick, const model::Rational &end_tick) {
    const model::Rational start_tick2 = start_tick + (end_tick - start_tick) / model::Rational(2);
    engraving::Chord *c1 = measure->find_chord(measure->tick() + to_muse_score_fraction(start_tick), cur_track_idx);
    engraving::Chord *c2 = measure->find_chord(measure->tick() + to_muse_score_fraction(start_tick2), cur_track_idx);
    IF_ASSERT_FAILED(c1 && c2 && c1->ticks() == c2->ticks()) {
        LOGE() << "Unable to import tremolo at tick " << to_muse_score_fraction(start_tick).to_string() << " of measure "
               << measure->measure_index();
        return;
    }
    int tremolo_beams_num = int(TremoloType::C8) - 1 + mnx_tremolo.marks;
    tremolo_beams_num = std::clamp(tremolo_beams_num, int(TremoloType::C8), int(TremoloType::C256));
    if (tremolo_beams_num <= c1->duration_type().hooks()) {
        return; // no tremolo is possible
    }

    Fraction d = c1->ticks() / 2;
    c1->set_duration_type(d.reduced());
    c1->set_ticks(c1->actual_duration_type().fraction());
    c2->set_duration_type(c1->duration_type());
    c2->set_ticks(c1->ticks());

    if (use_beams_ && c1->duration_type().hooks() > 0) {
        /// @todo revisit this when tremolo beaming is addressed by MNX
        // When useBeams_ is true, the importer explicitly sets every beam from MNX beam arrays,
        // leaving everything else unbeamed. However, MNX does not include tremolo events in those
        // arrays. This code beams each tremolo separately, as implied by the current MNX spec,
        // but there is an open issue about this with MNX.
        c1->set_beam_mode(BeamMode::BEGIN);
        c2->set_beam_mode(BeamMode::END);
    }

    TremoloTwoChord *tremolo = Factory::create_tremolo_two_chord(c1);
    tremolo->set_tremolo_type(TremoloType(tremolo_beams_num));
    tremolo->set_track(cur_track_idx);
    tremolo->set_visible(c1->notes().front()->visible());
    tremolo->set_chords(c1, c2);
    c1->set_tremolo_two_chord(tremolo);
}

//---------------------------------------------------------
//   importEvent
//   Return the created ChordRest when importing an MNX event (nullptr if skipped).
//---------------------------------------------------------

ChordRest *MnxImporter::import_event(const model::Event &event, track_idx_t cur_track_idx, Measure *measure,
                                     const model::Rational &start_tick, const model::Rational &actual_duration,
                                     const std::stack<Tuplet *> &active_tuplets, TremoloTwoChord *active_tremolo) {
    const TDuration d = to_muse_score_duration(event.duration);
    if (!d.is_valid()) {
        LOGW() << "Given ChordRest duration not supported in MuseScore";
        return nullptr;
    }

    const model::EventEntry &entry = index().entry(event);
    const engraving::Fraction event_tick = to_muse_score_fraction(start_tick);
    Segment *segment = measure->get_segment_r(SegmentType::CHORD_REST, event_tick);
    const model::Sequence &sequence = root().parts[entry.part].measures[entry.measure].sequences[entry.sequence];

    ChordRest *cr = nullptr;
    const int sequence_staff = sequence.staff_or_default();
    const int event_staff = event.staff.value_or(sequence_staff);
    int cross_staff_move = event_staff - sequence_staff;
    staff_idx_t staff_idx = track2staff(cur_track_idx);
    Staff *base_staff = score_->staff(staff_idx);
    Staff *target_staff = base_staff;
    if (cross_staff_move != 0) {
        const staff_idx_t target_staff_idx_candidate = static_cast<staff_idx_t>(int(staff_idx) + cross_staff_move);
        Staff *candidate_staff = score_->staff(target_staff_idx_candidate);
        const TrackRange base_part_track_range = base_staff->part()->track_range();
        const bool can_use_candidate = candidate_staff && candidate_staff->visible() &&
                                       candidate_staff->is_linked() == base_staff->is_linked() &&
                                       staff2track(staff_idx) >= base_part_track_range.start_track &&
                                       staff2track(target_staff_idx_candidate) < base_part_track_range.end_track &&
                                       candidate_staff->staff_type(event_tick)->group() == base_staff->staff_type(event_tick)->group();
        if (can_use_candidate) {
            target_staff = candidate_staff;
        } else {
            cross_staff_move = 0;
        }
    }
    IF_ASSERT_FAILED(base_staff && target_staff) {
        LOGE() << "Event " << entry.pointer << " has invalid staff " << event_staff << ".";
        return nullptr;
    }

    if (event.rest) {
        Rest *rest = Factory::create_rest(segment, d);
        cr = to_chord_rest(rest);
    } else {
        const auto &notes = event.notes;
        const auto &kit_notes = event.kit_notes;
        if ((notes && !notes->empty()) || (kit_notes && !kit_notes->empty())) {
            engraving::Chord *chord = Factory::create_chord(segment);
            if (notes) {
                for (const auto &mnx_note : *notes) {
                    create_note(mnx_note, entry, chord, base_staff, segment->tick(), cur_track_idx);
                }
            }
            if (kit_notes && !kit_notes->empty()) {
                const auto part_it = staff_to_mnx_part_.find(staff_idx);
                const Drumset *drumset = target_staff->part()->instrument()->drumset();
                for (const auto &kit_note : *kit_notes) {
                    int midi_pitch = -1;
                    if (part_it != staff_to_mnx_part_.end()) {
                        auto kit_it = mnx_kit_component_to_midi_.find({part_it->second, std::string(kit_note.kit_component)});
                        if (kit_it != mnx_kit_component_to_midi_.end()) {
                            midi_pitch = kit_it->second;
                        }
                    }
                    if (!pitch_is_valid(midi_pitch)) {
                        LOGW() << "Kit note has unknown kit component \"" << std::string(kit_note.kit_component) << "\" at "
                               << entry.pointer;
                        continue;
                    }
                    NoteVal nval;
                    nval.pitch = midi_pitch;
                    if (drumset && drumset->is_valid(midi_pitch)) {
                        nval.head_group = drumset->note_head(midi_pitch);
                    }
                    engraving::Note *note = Factory::create_note(chord);
                    note->set_track(cur_track_idx);
                    note->set_nval(nval);
                    chord->add(note);
                    mnx_kit_note_to_note_.emplace(&kit_note, note);
                }
            }
            if (chord->notes().empty()) {
                LOGW() << "Event " << entry.pointer << " has no valid notes.";
                delete chord;
                return nullptr;
            }
            if (const auto stem_dir = event.stem_direction) {
                chord->set_stem_direction(stem_dir.value() == model::StemDirection::UP ? DirectionV::UP : DirectionV::DOWN);
            }
            if (use_beams_ && d.hooks() > 0 && !(event.id && index().find_beam(*event.id))) {
                chord->set_beam_mode(BeamMode::NONE);
            }
            cr = to_chord_rest(chord);
        } else {
            LOGW() << "Event " << entry.pointer << " is neither rest nor chord.";
            return nullptr;
        }
    }
    cr->set_duration_type(d);
    cr->set_staff_move(cross_staff_move);
    cr->set_track(cur_track_idx);
    if (cr->duration_type().is_measure()) {
        cr->set_ticks(measure->stretched_len(base_staff)); // baseStaff because that's the staff the cr 'belongs to'
    } else {
        cr->set_ticks(cr->actual_duration_type().fraction());
    }
    import_markings(event, cr, to_muse_score_fraction(start_tick + actual_duration), measure);
    if (entry.container != model::EventContainer::GRACE) {
        segment->add(cr);
        if (!active_tuplets.empty() && active_tuplets.top()) {
            active_tuplets.top()->add(cr);
        }
        if (active_tremolo) {
            active_tremolo->add(cr);
        }
    }
    mnx_event_to_cr_.emplace(&event, cr);
    return cr;
}

//---------------------------------------------------------
//   importNonGraceEvents
//   Import non-grace events and return true if any ChordRest was created.
//---------------------------------------------------------

bool MnxImporter::import_non_grace_events(const model::Sequence &sequence, const model::Rational &bar_length, Measure *measure,
                                          track_idx_t cur_track_idx, GraceNeighborsMap &grace_neighbors) {
    bool inserted_cr = false;
    std::stack<Tuplet *> active_tuplets;
    TremoloTwoChord *active_tremolo = nullptr;

    ChordRest *last_cr = nullptr;
    std::vector<const model::Grace *> pending_next;

    model::WalkHooks hooks;
    hooks.on_full_measure = [&](const model::FullMeasureRest &mnx_full_measure_rest, const model::Rational &start_tick,
                                const model::Rational &, model::WalkContext &) {
        Segment *segment = measure->get_segment_r(SegmentType::CHORD_REST, to_muse_score_fraction(start_tick));
        Rest *rest = Factory::create_rest(segment, TDuration(DurationType::V_MEASURE));
        rest->set_duration_type(TDuration(DurationType::V_MEASURE));
        rest->set_track(cur_track_idx);
        Staff *staff = score_->staff(track2staff(cur_track_idx));
        IF_ASSERT_FAILED(staff) {
            LOGE() << "Unable to resolve staff for full-measure rest in measure " << measure->measure_index();
            delete rest;
            return true;
        }
        rest->set_ticks(measure->stretched_len(staff));
        segment->add(rest);
        if (mnx_full_measure_rest.fermata) {
            import_fermata(*mnx_full_measure_rest.fermata, rest);
        }
        last_cr = rest;
        inserted_cr = true;
        return true;
    };
    hooks.on_item = [&](const model::SequenceItem &item, model::WalkContext &ctx) {
        if (const auto *grace = std::get_if<model::Grace>(&item)) {
            /// @todo refactor this if MuseScore allows grace notes to be normal.
            grace_neighbors[grace] = {last_cr, nullptr}; // store prev neighbor
            pending_next.push_back(grace);
            return model::WalkControl::SKIP_CHILDREN;
        } else if (const auto *mnx_tuplet = std::get_if<model::Tuplet>(&item)) {
            if (tuplet_has_only_spaces_and_graces(*mnx_tuplet)) {
                // MuseScore does not like Tuplets that contain only gap Rests,
                // so replace the entire thing with a single gap rest.
                TDuration base_len = to_muse_score_duration(mnx_tuplet->outer.duration);
                const Fraction total = base_len.fraction() * mnx_tuplet->outer.multiple;
                DO_ASSERT(active_tuplets.empty());
                emit_gap_rest(measure, cur_track_idx, ctx.elapsed, to_rational(total.reduced()), nullptr);
                active_tuplets.push(nullptr);
                return model::WalkControl::SKIP_CHILDREN;
            }
            // A tuplet MuseScore cannot represent stays on the stack as null, so on_after_item
            // pops its own entry; its events are imported without a tuplet.
            Tuplet *t = create_tuplet(*mnx_tuplet, measure, cur_track_idx, ctx.elapsed);
            if (t && !active_tuplets.empty() && active_tuplets.top()) {
                active_tuplets.top()->add(t); // reparent tuplet
            }
            active_tuplets.push(t);
        } else if (const auto *mnx_tremolo = std::get_if<model::MultiNoteTremolo>(&item)) {
            if (mnx_tremolo->content.size() != 2) {
                LOGE() << "Tremolo in measure " << measure->measure_index() << " has " << mnx_tremolo->content.size()
                       << " events and cannot be imported.";
                return model::WalkControl::SKIP_CHILDREN;
            }
        } else if (const auto *mnx_space = std::get_if<model::Space>(&item)) {
            // Note that if we are inside a tuplet (ctx.ratio != 1), we must emit a gap rest here.
            // For now, though, we emit a gap rest for *any* spacer, since that seems to be the intent of them.
            emit_gap_rest(measure, cur_track_idx, ctx.elapsed, model::Rational(mnx_space->duration),
                          active_tuplets.empty() ? nullptr : active_tuplets.top());
        }
        return model::WalkControl::CONTINUE;
    };
    hooks.on_event = [&](const model::Event &event, const model::Rational &start_tick, const model::Rational &actual_duration,
                         model::WalkContext &ctx) {
        IF_ASSERT_FAILED(!ctx.in_grace) {
            LOGE() << "Encountered grace when processing non-grace.";
            return true;
        }
        update_lyric_line_usage_for_event(event, sequence, measure, cur_track_idx, start_tick);
        if (ChordRest *cr = import_event(event, cur_track_idx, measure, start_tick, actual_duration, active_tuplets, active_tremolo)) {
            if (event.fermata) {
                import_fermata(*event.fermata, cr);
            }
            last_cr = cr;
            inserted_cr = true;
            for (const auto *grace : pending_next) {
                auto it = grace_neighbors.find(grace);
                if (it != grace_neighbors.end()) {
                    it->second.second = cr; // store next neighbor
                }
            }
            pending_next.clear();
        }
        return true;
    };
    hooks.on_after_item = [&](const model::SequenceItem &item, model::WalkContext &ctx) {
        if (std::holds_alternative<model::Tuplet>(item)) {
            active_tuplets.pop();
        } else if (const auto *mnx_tremolo = std::get_if<model::MultiNoteTremolo>(&item)) {
            const model::Rational start_time = ctx.elapsed - model::duration_of(mnx_tremolo->outer) * ctx.ratio;
            create_tremolo(*mnx_tremolo, measure, cur_track_idx, start_time, ctx.elapsed);
        }
    };

    model::walk_sequence(sequence, bar_length, hooks);

    return inserted_cr;
}

//---------------------------------------------------------
//   importGraceEvents
//   Import grace-note events using neighbor context.
//---------------------------------------------------------

void MnxImporter::import_grace_events(const model::Sequence &sequence, const model::Rational &bar_length, Measure *measure,
                                      track_idx_t cur_track_idx, const GraceNeighborsMap &grace_neighbors) {
    const model::Grace *current_grace = nullptr;
    model::WalkHooks hooks;
    hooks.on_item = [&](const model::SequenceItem &item, model::WalkContext &) {
        if (const auto *grace = std::get_if<model::Grace>(&item)) {
            current_grace = grace; // its events follow
        }
        return model::WalkControl::CONTINUE;
    };
    hooks.on_event = [&](const model::Event &event, const model::Rational &start_tick, const model::Rational &actual_duration,
                         model::WalkContext &ctx) {
        if (!ctx.in_grace || !current_grace) {
            return true;
        }
        if (event.rest) {
            LOGW() << "encountered unsupported grace note rest at " << index().entry(event).pointer;
            return true;
        }
        const model::Grace &grace = *current_grace;
        auto [left_neighbor, right_neighbor] = muse::value(grace_neighbors, &grace);
        const bool use_right = right_neighbor && right_neighbor->is_chord();
        const bool use_left = !use_right && left_neighbor && left_neighbor->is_chord();
        if (use_right || use_left) {
            if (ChordRest *cr = import_event(event, cur_track_idx, measure, start_tick, actual_duration, {}, nullptr)) {
                engraving::Chord *gc = to_chord(cr);
                TDuration d = gc->duration_type();
                if (use_right && grace.slash_or_default() && grace.content.size() == 1) {
                    gc->set_note_type(engraving::NoteType::ACCIACCATURA);
                } else {
                    gc->set_note_type(dura_type_to_grace_note_type(d.type(), use_left));
                    gc->set_show_stem_slash(grace.slash_or_default());
                }
                Chord *grace_parent = nullptr;
                if (use_right) {
                    grace_parent = to_chord(right_neighbor);
                    gc->set_grace_index(grace_parent->grace_notes_before().size());
                    grace_parent->add(gc);
                } else if (use_left) {
                    grace_parent = to_chord(left_neighbor);
                    gc->set_grace_index(0);
                    grace_parent->add(gc);
                }
                if (event.fermata) {
                    DO_ASSERT_X(grace_parent, "no parent found for grace note");
                    if (grace_parent) {
                        import_fermata(*event.fermata, grace_parent);
                    }
                }
            }
        }
        return true;
    };

    model::walk_sequence(sequence, bar_length, hooks);
}

//---------------------------------------------------------
//   importSequences
//   Import all sequences for a part measure into a MuseScore measure.
//---------------------------------------------------------

void MnxImporter::import_sequences(size_t part_index, size_t measure_index, Measure *measure) {
    const model::Part &mnx_part = root().parts[part_index];
    const model::PartMeasure &part_measure = mnx_part.measures[measure_index];
    const model::Rational bar_length = model::measure_length(root(), index(), static_cast<std::uint32_t>(measure_index));
    auto &sequence_tracks = part_measure_sequence_tracks_[{part_index, measure_index}];
    sequence_tracks.clear();
    const int staves = mnx_part.staves_or_default();
    std::vector<std::vector<track_idx_t>> staff_voice_maps(static_cast<size_t>(staves));

    // pass1: import non-grace-note events to ChordRest
    for (size_t sequence_index = 0; sequence_index < part_measure.sequences.size(); ++sequence_index) {
        const model::Sequence &sequence = part_measure.sequences[sequence_index];
        const int sequence_staff = sequence.staff_or_default();
        const staff_idx_t cur_staff_idx = mnx_part_staff_to_staff_idx(part_index, sequence_staff);
        auto &staff_voice_map = staff_voice_maps[static_cast<size_t>(sequence_staff - 1)];
        const track_idx_t voice_id = staff_voice_map.size();
        if (voice_id >= ::mu::engraving::voices) {
            LOGW() << "Part " << part_index << " measure " << measure_index << " contains too many voices for staff " << sequence_staff
                   << ". Sequence " << sequence_index << " is skipped.";
            continue;
        }
        const track_idx_t cur_track_idx = staff2track(cur_staff_idx, voice_id);
        GraceNeighborsMap grace_neighbors;
        if (import_non_grace_events(sequence, bar_length, measure, cur_track_idx, grace_neighbors)) {
            import_grace_events(sequence, bar_length, measure, cur_track_idx,
                                grace_neighbors); // if MuseScore refactors graces, maybe we don't need this.
            sequence_tracks.emplace(&sequence, cur_track_idx);
            staff_voice_map.push_back(voice_id);
        }
    }

    // fill in measures as needed with rests.
    for (int staff_num = 1; staff_num <= staves; staff_num++) {
        staff_idx_t staff_idx = mnx_part_staff_to_staff_idx(part_index, staff_num);
        measure->check_measure(staff_idx);
    }
}

// Whether a dynamic renders any text of its own, as opposed to being a bare hairpin. An accent
// also spells out its prefix, suffix and residual value.
static bool calc_has_immediate_text(const model::DynamicGroup &mnx_dynamic) {
    const bool text = mnx_dynamic.value || !mnx_dynamic.prefix.value_or("").empty() || !mnx_dynamic.suffix.value_or("").empty() ||
                      (mnx_dynamic.glyphs && !mnx_dynamic.glyphs->empty());
    if (mnx_dynamic.type == model::DynamicGroupType::ACCENT) {
        return text || mnx_dynamic.accent_prefix_or_default() != model::DynamicPrefix::NONE ||
               mnx_dynamic.accent_suffix_or_default() != model::DynamicSuffix::NONE || mnx_dynamic.residual_value;
    }
    return text;
}

// Describes an MNX dynamic in the terms toMuseScoreDynamicType() understands, so that a
// dynamic MuseScore's text parser did not recognize can still be classified from what MNX
// actually said about it.
static MnxDynamicMapping mnx_dynamic_mapping(const model::DynamicGroup &mnx_dynamic) {
    MnxDynamicMapping mapping;
    mapping.value = mnx_dynamic.value;
    if (mnx_dynamic.type == model::DynamicGroupType::ACCENT) {
        mapping.is_accent = true;
        mapping.residual_value = mnx_dynamic.residual_value;
        mapping.accent_prefix = mnx_dynamic.accent_prefix_or_default();
        mapping.accent_suffix = mnx_dynamic.accent_suffix_or_default();
    }
    return mapping;
}

staff_idx_t MnxImporter::resolve_dynamic_staff(size_t part_index, const model::DynamicGroup &mnx_dynamic) {
    if (const auto explicit_staff = mnx_dynamic.staff) {
        return muse::value(mnx_part_staff_to_staff_, std::make_pair(part_index, *explicit_staff), muse::nidx);
    }

    // Orient only decides which staff to anchor to when MNX does not provide an
    // explicit staff number.
    const int staff_num = [&]() {
        switch (mnx_dynamic.orient_or_default()) {
            case model::MultiStaffOrientation::ABOVE: return 1;
            case model::MultiStaffOrientation::BELOW: return root().parts[part_index].staves_or_default();
            case model::MultiStaffOrientation::BETWEEN: return 1;
            case model::MultiStaffOrientation::AUTO: return 1;
        }
        return 1;
    }();

    return muse::value(mnx_part_staff_to_staff_, std::make_pair(part_index, staff_num), muse::nidx);
}

void MnxImporter::apply_dynamic_orient(EngravingItem *item, size_t part_index, model::MultiStaffOrientation orient) {
    IF_ASSERT_FAILED(item) { return; }

    switch (orient) {
        case model::MultiStaffOrientation::ABOVE:
            set_and_style_property(item, Pid::DIRECTION, DirectionV::UP);
            set_and_style_property(item, Pid::CENTER_BETWEEN_STAVES, AutoOnOff::OFF);
            break;
        case model::MultiStaffOrientation::BELOW:
            set_and_style_property(item, Pid::DIRECTION, DirectionV::DOWN);
            set_and_style_property(item, Pid::CENTER_BETWEEN_STAVES, AutoOnOff::OFF);
            break;
        case model::MultiStaffOrientation::BETWEEN:
            set_and_style_property(item, Pid::DIRECTION, DirectionV::AUTO);
            if (root().parts[part_index].staves_or_default() > 1) {
                set_and_style_property(item, Pid::CENTER_BETWEEN_STAVES, AutoOnOff::ON);
            }
            break;
        case model::MultiStaffOrientation::AUTO: break;
    }
}

static VoiceAssignment resolve_dynamic_voice_assignment(const model::DynamicGroup &mnx_dynamic, bool use_voice_assignment) {
    if (mnx_dynamic.voice && use_voice_assignment) {
        return VoiceAssignment::CURRENT_VOICE_ONLY;
    }
    if (mnx_dynamic.staff) {
        return VoiceAssignment::ALL_VOICE_IN_STAFF;
    }
    return VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
}

void MnxImporter::create_dynamic(const model::DynamicGroup &mnx_dynamic, Segment *segment, size_t part_index, track_idx_t cur_track_idx,
                                 bool use_voice_assignment) {
    IF_ASSERT_FAILED(segment) { return; }

    Dynamic *dyn = Factory::create_dynamic(segment);
    dyn->set_track(cur_track_idx);

    // An accent spells itself out as accentPrefix + value + accentSuffix + residualValue.
    const bool is_accent = mnx_dynamic.type == model::DynamicGroupType::ACCENT;
    const std::optional<model::DynamicValue> residual_value = is_accent ? mnx_dynamic.residual_value : std::nullopt;

    String dynamic_text = String::from_std_string(mnx_dynamic.prefix.value_or(""));
    auto append_glyph = [&](const std::string &glyph_name) {
        if (!glyph_name.empty()) {
            dynamic_text += u"<sym>" + String::from_std_string(glyph_name) + u"</sym>";
        }
    };
    // Prefer glyphs over value. MNX can now spell out every dynamic MuseScore has a type for,
    // so glyphs no longer stand in for a value MNX cannot express, but they still describe the
    // exact representation the source document uses and override the default one derived from
    // the value. Only when they are absent do we spell the dynamic out ourselves.
    if (mnx_dynamic.glyphs && !mnx_dynamic.glyphs->empty()) {
        if (!dynamic_text.empty()) {
            dynamic_text += u" ";
        }
        for (const auto &glyph : *mnx_dynamic.glyphs) {
            append_glyph(glyph);
        }
    } else if (mnx_dynamic.value || residual_value) {
        if (!dynamic_text.empty()) {
            dynamic_text += u" ";
        }
        auto append_value_glyphs = [&](std::optional<model::DynamicValue> val) {
            if (val) {
                for (const auto &glyph_name : to_muse_score_dynamic_glyph_names(val.value())) {
                    append_glyph(glyph_name);
                }
            }
        };
        // Spelling the dynamic with SMuFL glyphs rather than letters lets MuseScore
        // recognize the combinations it knows (it compares against its own glyph strings),
        // and leaves recoverable glyphs behind for the ones it does not.
        if (is_accent) {
            append_glyph(to_muse_score_dynamic_glyph_name(mnx_dynamic.accent_prefix_or_default()));
        }
        append_value_glyphs(mnx_dynamic.value);
        if (is_accent) {
            append_glyph(to_muse_score_dynamic_glyph_name(mnx_dynamic.accent_suffix_or_default()));
        }
        append_value_glyphs(residual_value);
    }

    if (mnx_dynamic.suffix) {
        if (!dynamic_text.empty()) {
            dynamic_text += u" ";
        }
        dynamic_text += String::from_std_string(*mnx_dynamic.suffix);
    }

    dyn->set_dynamic_type(dynamic_text);
    /// @todo If mnx adds an "other" type, we should leave our type as other and remove
    /// this if statement.
    // MuseScore classifies by spelling, so a marking it renders but cannot name (a ligature
    // such as "pppp", say) arrives as OTHER. Reclassify from what MNX said about the dynamic,
    // but only when those components name a MuseScore type exactly. A spelling MuseScore has
    // no type for, such as "sfzp" or "ffz", stays OTHER rather than being filed under a type
    // that drops half of it.
    if (dyn->dynamic_type() == DynamicType::OTHER) {
        if (const auto type = to_muse_score_dynamic_type(mnx_dynamic_mapping(mnx_dynamic))) {
            dyn->set_dynamic_type(type.value());
        }
    }
    apply_dynamic_orient(dyn, part_index, mnx_dynamic.orient_or_default());
    dyn->set_voice_assignment(resolve_dynamic_voice_assignment(mnx_dynamic, use_voice_assignment));
    segment->add(dyn);
}

void MnxImporter::create_hairpin(const model::DynamicGroup &mnx_hairpin, Segment *segment, size_t part_index, track_idx_t cur_track_idx,
                                 bool use_voice_assignment) {
    // build_index requires a gradual dynamic to have an end and a wedge type.
    if (!segment || !mnx_hairpin.end || !mnx_hairpin.wedge_type) {
        return;
    }

    const size_t end_measure_index = index().measure_ids.at(mnx_hairpin.end->measure);
    Measure *target_measure = try_mnx_measure_to_measure(end_measure_index);
    if (!target_measure) {
        LOGW() << "hairpin end measure " << end_measure_index << " could not be resolved to a MuseScore measure.";
        return;
    }

    const Fraction end_tick = target_measure->tick() + to_muse_score_r_tick(mnx_hairpin.end->position);
    if (end_tick <= segment->tick()) {
        LOGW() << "hairpin end tick is not after start tick at " << segment->tick().to_string();
        return;
    }

    // A hairpin may finish on a different staff of the same part than it started on.
    track_idx_t end_track_idx = cur_track_idx;
    if (const auto staff_end = mnx_hairpin.staff_end) {
        end_track_idx = staff2track(mnx_part_staff_to_staff_idx(part_index, *staff_end), cur_track_idx % ::mu::engraving::voices);
    }

    Hairpin *hairpin = Factory::create_hairpin(score_->dummy());
    hairpin->set_hairpin_type(*mnx_hairpin.wedge_type == model::WedgeType::INCREASING ? HairpinType::CRESC_HAIRPIN :
                                                                                        HairpinType::DIM_HAIRPIN);
    hairpin->set_track(cur_track_idx);
    hairpin->set_track2(end_track_idx);
    hairpin->set_tick(segment->tick());
    hairpin->set_tick2(end_tick);

    apply_dynamic_orient(hairpin, part_index, mnx_hairpin.orient_or_default());
    hairpin->set_voice_assignment(resolve_dynamic_voice_assignment(mnx_hairpin, use_voice_assignment));

    score_->add_element(hairpin);
}

std::optional<track_idx_t> MnxImporter::resolve_voice_track(size_t part_index, size_t measure_index, staff_idx_t staff_idx,
                                                            const std::string &mnx_voice_id) {
    const auto measure_it = part_measure_sequence_tracks_.find({part_index, measure_index});
    if (measure_it == part_measure_sequence_tracks_.end()) {
        return std::nullopt;
    }

    for (const auto &sequence : root().parts[part_index].measures[measure_index].sequences) {
        if (mnx_part_staff_to_staff_idx(part_index, sequence.staff_or_default()) != staff_idx) {
            continue;
        }
        if (!sequence.voice || *sequence.voice != mnx_voice_id) {
            continue;
        }
        const auto sequence_it = measure_it->second.find(&sequence);
        if (sequence_it != measure_it->second.end()) {
            return sequence_it->second;
        }
        break;
    }

    return std::nullopt;
}

//---------------------------------------------------------
//   createDynamics
//   Create MuseScore dynamics from MNX part-measure dynamics.
//---------------------------------------------------------

void MnxImporter::create_dynamics(size_t part_index, size_t measure_index, Measure *measure) {
    const auto &mnx_dynamics = root().parts[part_index].measures[measure_index].dynamics;
    if (!mnx_dynamics) {
        return;
    }
    for (const auto &mnx_dynamic : *mnx_dynamics) {
        staff_idx_t staff_idx = resolve_dynamic_staff(part_index, mnx_dynamic);
        IF_ASSERT_FAILED(staff_idx != muse::nidx) {
            LOGE() << "staff idx not found for part " << part_index;
            continue;
        }
        const track_idx_t staff_track_idx = staff2track(staff_idx);
        bool use_voice_assignment = false;
        track_idx_t cur_track_idx = staff_track_idx;
        if (const auto &mnx_voice_id = mnx_dynamic.voice) {
            if (const auto resolved_track = resolve_voice_track(part_index, measure_index, staff_idx, *mnx_voice_id)) {
                cur_track_idx = resolved_track.value();
                use_voice_assignment = true;
            }
        }

        Fraction r_tick = to_muse_score_r_tick(mnx_dynamic.position);
        Segment *s = measure->get_chord_rest_or_time_tick_segment(measure->tick() + r_tick);

        if (calc_has_immediate_text(mnx_dynamic)) {
            create_dynamic(mnx_dynamic, s, part_index, cur_track_idx, use_voice_assignment);
        }

        if (mnx_dynamic.type == model::DynamicGroupType::GRADUAL) {
            create_hairpin(mnx_dynamic, s, part_index, cur_track_idx, use_voice_assignment);
        }
    }
}

//---------------------------------------------------------
//   createOttavas
//   Create MuseScore ottava spanners from MNX ottavas.
//---------------------------------------------------------

void MnxImporter::create_ottavas(size_t part_index, const model::PartMeasure &mnx_measure, Measure *measure) {
    if (!mnx_measure.ottavas) {
        return;
    }
    for (const auto &mnx_ottava : *mnx_measure.ottavas) {
        staff_idx_t staff_idx =
            muse::value(mnx_part_staff_to_staff_, std::make_pair(part_index, mnx_ottava.staff_or_default()), muse::nidx);
        IF_ASSERT_FAILED(staff_idx != muse::nidx) {
            LOGE() << "staff idx not found for part " << part_index;
            continue;
        }
        Measure *end_measure = mnx_measure_to_measure(index().measure_ids.at(mnx_ottava.end.measure));
        const Fraction end_pos = to_muse_score_r_tick(mnx_ottava.end.position);
        const Fraction end_tick = end_measure->tick() + end_pos;
        bool ends_on_barline = false;
        if (Measure *end_plus1 = end_measure->next_measure()) {
            ends_on_barline = end_plus1->tick() == end_tick;
        }
        /// @todo map ottava.voice() to a relative track other than 0, if MuseScore decides to implements it.
        track_idx_t cur_track_idx = staff2track(staff_idx);

        Ottava *ottava = Factory::create_ottava(score_->dummy());
        ottava->set_track(cur_track_idx);
        ottava->set_track2(cur_track_idx);
        ottava->set_tick(measure->tick() + to_muse_score_r_tick(mnx_ottava.position));
        ottava->set_tick2(end_tick);
        const OttavaType ottava_type = to_muse_score_ottava_type(mnx_ottava.value);
        set_and_style_property(ottava, Pid::OTTAVA_TYPE, int(ottava_type));
        if (!ends_on_barline) {
            // ottavas in MNX include any event that starts on the endTick
            ChordRest *end_cr = nullptr;
            for (track_idx_t voice_idx = 0; voice_idx < ::mu::engraving::voices; voice_idx++) {
                ChordRest *cr = score_->find_cr(end_tick, cur_track_idx + voice_idx);
                if (!cr) {
                    continue;
                }
                if (!end_cr) {
                    end_cr = cr;
                } else if (end_cr->end_tick() > cr->end_tick()) {
                    end_cr = cr;
                }
            }
            if (end_cr) {
                ottava->set_end_element(end_cr);
                ottava->set_tick2(end_cr->end_tick());
            }
        }
        score_->add_element(ottava);
    }
}

//---------------------------------------------------------
//   createBeams
//   Apply MuseScore beam modes based on MNX beam definitions.
//---------------------------------------------------------

void MnxImporter::create_beams(const model::PartMeasure &mnx_measure) {
    if (!mnx_measure.beams) {
        return;
    }
    for (const auto &beam : *mnx_measure.beams) {
        const auto &events = beam.events;
        std::vector<ChordRest *> beam_elements;
        DirectionV beam_direction = DirectionV::AUTO;
        for (size_t x = 0; x < events.size(); x++) {
            const auto &event_id = events[x];
            const model::EventEntry *mnx_event = index().find_event(event_id);
            if (beam_direction == DirectionV::AUTO && mnx_event) {
                if (const auto stem_dir = mnx_event->event->stem_direction) {
                    beam_direction = stem_dir.value() == model::StemDirection::UP ? DirectionV::UP : DirectionV::DOWN;
                }
            }
            ChordRest *cr = mnx_event_id_to_cr(event_id);
            IF_ASSERT_FAILED(cr) {
                LOGE() << "encountered unmapped event " << std::string(event_id) << " in a beam";
                continue;
            }
            beam_elements.push_back(cr);
            if (beam_direction == DirectionV::AUTO && cr->is_chord()) {
                beam_direction = to_chord(cr)->stem_direction();
            }
            if (events.size() == 1) {
                cr->set_beam_mode(BeamMode::NONE); // MuseScore does not have singleton beams
            } else if (x == 0) {
                cr->set_beam_mode(BeamMode::BEGIN);
            } else if (x == events.size() - 1) {
                cr->set_beam_mode(BeamMode::END);
            } else {
                const model::BeamEntry *entry = index().find_beam(event_id);
                cr->set_beam_mode(to_muse_score_beam_mode(entry ? entry->start_level : 0));
            }
        }
        if (beam_elements.size() > 1) {
            Beam *ms_beam = Factory::create_beam(score_);
            ms_beam->set_track(beam_elements.front()->track());
            ms_beam->set_direction(beam_direction);
            for (ChordRest *cr : beam_elements) {
                ms_beam->add(cr);
            }
        }
    }
}

//---------------------------------------------------------
//   createArpeggios
//   Create MuseScore arepeggio markings from MNX arpeggios and nonArpeggios.
//---------------------------------------------------------

void MnxImporter::create_arpeggios(size_t part_index, const model::PartMeasure &mnx_measure) {
    auto process_arpeggio = [&](const auto &mnx_arpeggio) -> void {
        using MnxArpeggioType = std::decay_t<decltype(mnx_arpeggio)>;
        static_assert(std::is_same_v<MnxArpeggioType, model::Arpeggio> || std::is_same_v<MnxArpeggioType, model::NonArpeggio>);

        const model::IdPair &span = mnx_arpeggio.span;
        const std::string span_start(span.start);
        const std::string span_end(span.end);
        Note *note_start = mnx_note_id_to_note(span.start);
        Note *note_end = mnx_note_id_to_note(span.end);
        IF_ASSERT_FAILED(note_start && note_end) {
            LOGE() << "Start note " << span_start << " or end note " << span_end << " not mapped.";
            return;
        }
        const auto mnx_part_index_for_note = [this](const Note *note) -> std::optional<size_t> {
            const auto part_it = staff_to_mnx_part_.find(track2staff(note->track()));
            if (part_it == staff_to_mnx_part_.end()) {
                return std::nullopt;
            }
            return part_it->second;
        };
        const std::optional<size_t> start_part_index = mnx_part_index_for_note(note_start);
        const std::optional<size_t> end_part_index = mnx_part_index_for_note(note_end);
        IF_ASSERT_FAILED(start_part_index && end_part_index) {
            LOGE() << "Start note " << span_start << " or end note " << span_end << " belongs to an unmapped staff.";
            return;
        }
        if (*start_part_index != part_index || *end_part_index != part_index) {
            LOGW() << "skipping arpeggio span " << span_start << " to " << span_end
                   << " because MNX arpeggios cannot cross part boundaries.";
            return;
        }
        if (note_start->tick() != note_end->tick()) {
            LOGW() << "Start note " << span_start << " or end note " << span_end << " are not vertically aligned.";
            return;
        }

        Chord *chord_top{};
        Chord *chord_bot{};
        if (note_end->track() <= note_start->track()) {
            chord_top = note_end->chord();
            chord_bot = note_start->chord();
        } else {
            chord_top = note_start->chord();
            chord_bot = note_end->chord();
        }
        IF_ASSERT_FAILED(chord_top && chord_bot) {
            LOGE() << "failed to determining top/bottom notes for arpeggio";
            return;
        }

        const bool grace_arpeggio = chord_top->is_grace() || chord_bot->is_grace();
        if (grace_arpeggio) {
            /// @note hopefully we can lift this requirement someday
            LOGW() << "skipping arpeggio on grace note (not supported by MuseScore)";
            return;
        }

        // MNX targets individual notes, but MuseScore must target entire chords

        const Note *chord_top_note = chord_top->up_note();
        const Note *chord_bot_note = chord_bot->down_note();
        const bool targets_chord_extremes =
            (note_start == chord_top_note && note_end == chord_bot_note) || (note_end == chord_top_note && note_start == chord_bot_note);
        if (!targets_chord_extremes) {
            LOGW() << "arpeggio span " << span_start << " to " << span_end
                   << " does not target the top and bottom notes of its chord span at " << chord_top->tick().to_string();
        }
        if (chord_top->arpeggio()) {
            LOGW() << "arpeggio already exists on top chord";
            return;
        }

        Arpeggio *arpeggio = Factory::create_arpeggio(chord_top);
        arpeggio->set_track(chord_top->track());
        if constexpr (std::is_same_v<MnxArpeggioType, model::NonArpeggio>) {
            arpeggio->set_arpeggio_type(ArpeggioType::BRACKET);
        } else {
            arpeggio->set_arpeggio_type(ArpeggioType::NORMAL);
            if (mnx_arpeggio.arrow_or_default()) {
                switch (mnx_arpeggio.direction_or_default()) {
                    case model::UpDownAuto::DOWN: arpeggio->set_arpeggio_type(ArpeggioType::DOWN); break;
                    case model::UpDownAuto::UP: arpeggio->set_arpeggio_type(ArpeggioType::UP); break;
                    case model::UpDownAuto::AUTO: break;
                }
            }
        }
        arpeggio->set_span(int(chord_bot->track() + 1 - chord_top->track()));
        Segment *s = chord_top->segment();
        for (track_idx_t track = chord_top->track(); track <= chord_bot->track(); ++track) {
            EngravingItem *item = s->element(track);
            if (item && item->is_chord()) {
                to_chord(item)->set_span_arpeggio(arpeggio);
            }
        }
        chord_top->set_arpeggio(arpeggio);
    };

    if (mnx_measure.arpeggios) {
        for (const auto &arpeggio : *mnx_measure.arpeggios) {
            process_arpeggio(arpeggio);
        }
    }
    if (mnx_measure.non_arpeggios) {
        for (const auto &non_arpeggio : *mnx_measure.non_arpeggios) {
            process_arpeggio(non_arpeggio);
        }
    }
}

//---------------------------------------------------------
//   processSequencePass2
//   Second pass over a sequence for lyrics, accidentals, slurs, ties, and rest positions.
//---------------------------------------------------------

void MnxImporter::process_sequence_pass2(const model::Sequence &sequence, const model::Rational &bar_length, Measure *measure) {
    model::WalkHooks hooks;
    hooks.on_event = [&](const model::Event &event, const model::Rational &, const model::Rational &, model::WalkContext &ctx) {
        ChordRest *cr = muse::value(mnx_event_to_cr_, &event);
        if (!cr) {
            LOGW() << "event " << index().entry(event).pointer << " is not mapped, possibly due to too many sequences in measure.";
            return true;
        }
        if (!ctx.in_grace) {
            create_lyrics(event, cr);
        }
        if (event.rest && cr->is_rest()) {
            import_rest_properties(*event.rest, to_rest(cr));
        }
        if (event.slurs) {
            for (const auto &slur : *event.slurs) {
                create_slur(slur, cr);
            }
        }
        if (event.notes) {
            for (const auto &mnx_note : *event.notes) {
                Note *note = muse::value(mnx_note_to_note_, &mnx_note);
                IF_ASSERT_FAILED(note) {
                    LOGE() << "note of event " << index().entry(event).pointer << " is not mapped";
                    continue;
                }
                create_accidentals(mnx_note, note, measure);
                if (mnx_note.ties) {
                    create_ties(*mnx_note.ties, note);
                }
            }
        }
        if (event.kit_notes) {
            for (const auto &kit_note : *event.kit_notes) {
                Note *note = muse::value(mnx_kit_note_to_note_, &kit_note);
                IF_ASSERT_FAILED(note) {
                    LOGE() << "kit note of event " << index().entry(event).pointer << " is not mapped";
                    continue;
                }
                if (kit_note.ties) {
                    create_ties(*kit_note.ties, note);
                }
            }
        }
        return true;
    };

    model::walk_sequence(sequence, bar_length, hooks);
}

//---------------------------------------------------------
//   createMeasureRepeats
//   Create MuseScore measure repeats from MNX measure repeats.
//---------------------------------------------------------

void MnxImporter::create_measure_repeats(size_t part_index) {
    /// @todo Import MeasureRepeat counter, displayNumber, and staffPosition, none of which
    /// has a per-element home in MuseScore. counter is the iteration the player is on, which
    /// MuseScore derives from the repeat's place in the run under score-wide styles.
    /// displayNumber asks whether `number` itself is drawn above the glyph; MuseScore always
    /// draws it for 2- and 4-bar repeats and gates 1-bar repeats on the score-wide
    /// Sid::oneMeasureRepeatShow1. staffPosition is computed from the staff's middle line.
    const model::Part &mnx_part = root().parts[part_index];
    for (size_t measure_index = 0; measure_index < mnx_part.measures.size(); ++measure_index) {
        const auto &mnx_measure_repeat = mnx_part.measures[measure_index].measure_repeat;
        if (!mnx_measure_repeat) {
            continue;
        }

        const int num_measures = mnx_measure_repeat->number;
        // MuseScore only draws a glyph for 1, 2, and 4 measure repeats.
        if (num_measures != 1 && num_measures != 2 && num_measures != 4) {
            LOGW() << "Skipping measure repeat of " << num_measures << " measures in part " << part_index << " at measure " << measure_index
                   << "; MuseScore supports 1, 2, or 4.";
            continue;
        }

        // MuseScore anchors the element in the center measure of an odd-numbered group, or
        // the last measure of the first half of an even-numbered one.
        const int anchor_offset = (num_measures % 2) ? num_measures / 2 : num_measures / 2 - 1;

        for (int offset = 0; offset < num_measures; offset++) {
            Measure *measure = try_mnx_measure_to_measure(measure_index + static_cast<size_t>(offset));
            if (!measure) {
                LOGW() << "Measure repeat in part " << part_index << " at measure " << measure_index
                       << " extends past the end of the score.";
                break;
            }

            for (int staff_num = 1; staff_num <= mnx_part.staves_or_default(); staff_num++) {
                const staff_idx_t staff_idx = mnx_part_staff_to_staff_idx(part_index, staff_num);
                const track_idx_t track = staff2track(staff_idx);

                // Clear the rests pass1 left behind before taking over the measure.
                score_->make_gap(measure->first(SegmentType::CHORD_REST), track, measure->stretched_len(score_->staff(staff_idx)), 0);

                if (offset == anchor_offset) {
                    EditMeasureRepeat::add_measure_repeat(score_->transaction_manager()->current_or_dummy_transaction(), score_,
                                                          measure->tick(), track, num_measures);
                } else {
                    // The rest of the group carries undisplayed full-measure rests.
                    score_->add_rest(measure->tick(), track, TDuration(DurationType::V_MEASURE), 0);
                }
                measure->set_measure_repeat_count(offset + 1, staff_idx);
            }
        }
    }
}

//---------------------------------------------------------
//   importPartMeasures
//   Import all part measures with two-pass processing.
//---------------------------------------------------------

void MnxImporter::import_part_measures() {
    lyric_line_usage_.clear();
    part_measure_sequence_tracks_.clear();
    // build_index guarantees each part has one measure per global measure.
    // pass1: create ChordRests and clefs
    for (size_t part_index = 0; part_index < root().parts.size(); ++part_index) {
        const model::Part &mnx_part = root().parts[part_index];
        for (size_t measure_index = 0; measure_index < mnx_part.measures.size(); ++measure_index) {
            Measure *measure = mnx_measure_to_measure(measure_index);
            import_sequences(part_index, measure_index, measure);
            if (const auto &mnx_clefs = mnx_part.measures[measure_index].clefs) {
                create_clefs(part_index, *mnx_clefs, measure);
            }
        }
    }
    build_lyric_line_verse_map();
    // Measure repeats replace the content pass1 created, so they must follow it.
    for (size_t part_index = 0; part_index < root().parts.size(); ++part_index) {
        create_measure_repeats(part_index);
    }
    // pass2: add accidentals, beams, dynamics, ottavas, slurs, and ties
    for (size_t part_index = 0; part_index < root().parts.size(); ++part_index) {
        const model::Part &mnx_part = root().parts[part_index];
        for (size_t measure_index = 0; measure_index < mnx_part.measures.size(); ++measure_index) {
            const model::PartMeasure &part_measure = mnx_part.measures[measure_index];
            Measure *measure = mnx_measure_to_measure(measure_index);
            const model::Rational bar_length = model::measure_length(root(), index(), static_cast<std::uint32_t>(measure_index));
            create_arpeggios(part_index, part_measure);
            create_dynamics(part_index, measure_index, measure);
            create_ottavas(part_index, part_measure, measure);
            create_beams(part_measure);
            for (const auto &sequence : part_measure.sequences) {
                process_sequence_pass2(sequence, bar_length, measure);
            }
        }
    }
}
} // namespace mu::iex::mnxio
