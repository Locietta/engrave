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

#ifdef MNXDOM_SYSTEM
#include <mnxdom/mnxdom.h>
#else
#include "mnxdom.h"
#endif

using namespace mu::engraving;

namespace mu::iex::mnxio {
namespace {
struct LyricLineInterval {
    engraving::Fraction start;
    engraving::Fraction end;
};
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

static bool tuplet_has_only_spaces_and_graces(const mnx::sequence::Tuplet &tuplet) {
    const auto &content = tuplet.content();
    for (const auto &item : content) {
        if (item.type() == mnx::sequence::Space::ContentTypeValue) {
            continue;
        }
        if (item.type() == mnx::sequence::Grace::ContentTypeValue) {
            continue;
        }
        if (item.type() == mnx::sequence::Tuplet::ContentTypeValue) {
            const auto nested = item.get<mnx::sequence::Tuplet>();
            if (!tuplet_has_only_spaces_and_graces(nested)) {
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

void MnxImporter::create_slur(const mnx::sequence::Slur &mnx_slur, engraving::ChordRest *start_cr) {
    ChordRest *target_cr = mnx_event_id_to_cr(mnx_slur.target());
    if (!target_cr) {
        LOGW() << "slur target was event with eventId " << mnx_slur.target() << " that was not mapped.";
        LOGW() << mnx_slur.dump(2);
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

    if (const auto line_type = mnx_slur.lineType()) {
        set_and_style_property(slur, Pid::SLUR_STYLE_TYPE, to_muse_score_slur_style_type(line_type.value()));
    }
    if (const auto side = mnx_slur.side()) {
        DirectionV slur_dir = side.value() == mnx::SlurTieSide::Up ? DirectionV::UP : DirectionV::DOWN;
        set_and_style_property(slur, Pid::SLUR_DIRECTION, slur_dir);
    } else if (const auto side_end = mnx_slur.sideEnd()) {
        DirectionV slur_dir = side_end.value() == mnx::SlurTieSide::Up ? DirectionV::UP : DirectionV::DOWN;
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

    const auto &line_order = mnx_document().getEntityMap().getLyricLineOrder();
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

void MnxImporter::create_lyrics(const mnx::sequence::Event &mnx_event, engraving::ChordRest *cr) {
    /// @todo import lyric line metadata (i.e., language code) somehow?
    if (const auto lyrics = mnx_event.lyrics()) {
        if (const auto lines = lyrics->lines()) {
            const auto &mnx_line_order = mnx_document().getEntityMap().getLyricLineOrder();
            const auto staff_it = lyric_line_to_verse_.find(cr->staff_idx());
            if (!mnx_line_order.empty()) {
                for (size_t line_index = 0; line_index < mnx_line_order.size(); line_index++) {
                    const auto it = lines->find(mnx_line_order[line_index]);
                    if (it == lines->end() || it->second.text().empty()) {
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
                    lyric->set_xml_text(String::from_std_string(it->second.text()));
                    lyric->set_syllabic(to_muse_score_lyrics_syllabic(it->second.type()));
                    /// @todo styled text when supported by MNX.
                    /// @todo word extension span, if mnx ever provides it
                    cr->add(lyric);
                }
                return;
            }
            for (const auto &[line_id, line] : *lines) {
                if (line.text().empty()) {
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
                lyric->set_xml_text(String::from_std_string(line.text()));
                lyric->set_syllabic(to_muse_score_lyrics_syllabic(line.type()));
                /// @todo import word extension span, if mnx ever provides it
                cr->add(lyric);
            }
        }
    }
}

//---------------------------------------------------------
//   updateLyricLineUsageForEvent
//   Track lyric line intervals per staff for verse assignment.
//---------------------------------------------------------

void MnxImporter::update_lyric_line_usage_for_event(const mnx::sequence::Event &event, const mnx::Sequence &sequence,
                                                    engraving::Measure *measure, engraving::track_idx_t cur_track_idx,
                                                    const mnx::FractionValue &start_tick) {
    const auto lyrics = event.lyrics();
    if (!lyrics) {
        return;
    }
    const auto lines = lyrics->lines();
    if (!lines) {
        return;
    }
    const staff_idx_t base_staff_idx = track2staff(cur_track_idx);
    const int event_staff = event.staff_or(sequence.staff());
    int cross_staff_move = event_staff - sequence.staff();
    // Cross-staff lyric events should reserve a verse on the staff where the event lands.
    staff_idx_t target_staff_idx = static_cast<staff_idx_t>(int(base_staff_idx) + cross_staff_move);
    if (!score_->staff(target_staff_idx)) {
        target_staff_idx = base_staff_idx;
    }
    const engraving::Fraction tick = measure->tick() + to_muse_score_fraction(start_tick);
    auto &line_usage = lyric_line_usage_[target_staff_idx];
    for (const auto &[line_id, line] : *lines) {
        if (line.text().empty()) {
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

void MnxImporter::create_ties(const mnx::Array<mnx::sequence::Tie> &ties, engraving::Note *start_note) {
    bool created_non_jump_tie = false;
    for (const mnx::sequence::Tie &mnx_tie : ties) {
        if (mnx_tie.targetType() == mnx::TieTargetType::CrossJump) {
            continue;
        }

        const bool is_lv = mnx_tie.lv() || !mnx_tie.target();
        Note *target_note = nullptr;
        if (!is_lv) {
            const auto target = mnx_tie.target();
            target_note = target ? mnx_note_id_to_note(target.value()) : nullptr;
            if (!target_note) {
                if (target) {
                    LOGW() << "Skipping tie to note with noteId " << target.value() << " that was not mapped.";
                    LOGW() << mnx_tie.dump(2);
                }
                continue;
            }
        }

        Tie *tie = is_lv ? Factory::create_laissez_vib(start_note) : Factory::create_tie(start_note);
        tie->set_start_note(start_note);
        tie->set_tick(start_note->tick());
        tie->set_track(start_note->track());
        start_note->set_tie_for(tie);
        DirectionV tie_dir = DirectionV::AUTO;
        if (const auto side = mnx_tie.side()) {
            tie_dir = side.value() == mnx::SlurTieSide::Up ? DirectionV::UP : DirectionV::DOWN;
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
        mnx::sequence::Tie tie;
        Note *target_note = nullptr;
    };

    std::vector<JumpTieTarget> jump_targets;
    for (const mnx::sequence::Tie &mnx_tie : ties) {
        if (mnx_tie.targetType() != mnx::TieTargetType::CrossJump) {
            continue;
        }

        const auto target = mnx_tie.target();
        Note *target_note = target ? mnx_note_id_to_note(target.value()) : nullptr;
        if (!target_note) {
            if (target) {
                LOGW() << "tie target was note with noteId " << target.value() << " that was not mapped.";
                LOGW() << mnx_tie.dump(2);
            }
            continue;
        }
        jump_targets.push_back(JumpTieTarget{mnx_tie, target_note});
    }

    // sort may not be strictly necessary
    std::sort(jump_targets.begin(), jump_targets.end(), [](const JumpTieTarget &a, const JumpTieTarget &b) {
        if (a.target_note->tick() != b.target_note->tick()) {
            return a.target_note->tick() < b.target_note->tick();
        }
        return a.target_note->track() < b.target_note->track();
    });

    for (const JumpTieTarget &jump_target : jump_targets) {
        const mnx::sequence::Tie &mnx_tie = jump_target.tie;
        Note *target_note = jump_target.target_note;

        DirectionV tie_dir = DirectionV::AUTO;
        if (const auto side = mnx_tie.side()) {
            tie_dir = side.value() == mnx::SlurTieSide::Up ? DirectionV::UP : DirectionV::DOWN;
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

void MnxImporter::create_accidentals(const mnx::sequence::Note &mnx_note, Note *note, Measure *measure) {
    const auto accidental_display = mnx_note.accidentalDisplay();
    bool force_accidental = accidental_display && accidental_display->force();
    bool show_accidental = accidental_display ? accidental_display->show() : false;
    bool has_enclosure = false;
    mnx::AccidentalEnclosureSymbol enclosure_symbol = mnx::AccidentalEnclosureSymbol::Parentheses;
    if (accidental_display) {
        if (const auto enclosure = accidental_display->enclosure()) {
            has_enclosure = true;
            enclosure_symbol = enclosure->symbol();
        }
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
        accidental->set_bracket(enclosure_symbol == mnx::AccidentalEnclosureSymbol::Parentheses ? AccidentalBracket::PARENTHESIS :
                                                                                                  AccidentalBracket::BRACKET);
    }
    note->add(accidental);
}

//---------------------------------------------------------
//   importRestProperties
//   Import rest properties.
//---------------------------------------------------------

void MnxImporter::import_rest_properties(const mnx::sequence::Rest &mnx_rest, Rest *rest) {
    // nothing to do in this function yet.
    static_cast<void>(mnx_rest);
    static_cast<void>(rest);

    /// @todo Override rest positioning if MuseScore exposes a straightforward staff-line override for rests.
}

//---------------------------------------------------------
//   emitGapRest
//   Emit a spacer (gap) rest for MNX space content.
//---------------------------------------------------------

Rest *MnxImporter::emit_gap_rest(Measure *measure, track_idx_t cur_track_idx, const mnx::FractionValue &start_tick,
                                 const mnx::FractionValue &duration, Tuplet *tuplet_to_add) {
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

Note *MnxImporter::create_note(const mnx::sequence::Note &mnx_note, Chord *chord, Staff *base_staff, const Fraction &tick,
                               int ottava_displacement, track_idx_t cur_track_idx) {
    Note *note = Factory::create_note(chord);
    note->set_track(cur_track_idx);
    auto pitch = mnx_note.pitch();
    NoteVal nval = to_muse_score_note_val(pitch, base_staff->concert_key(tick), ottava_displacement);
    // calcTransposed accounts for MNX transposeWritten.
    NoteVal nval_transposed = to_muse_score_note_val(pitch.calcTransposed(), base_staff->key(tick), ottava_displacement);
    nval.tpc2 = nval_transposed.tpc2;
    note->set_nval(nval);
    chord->add(note);
    mnx_note_to_note_.emplace(mnx_note.pointer().to_string(), note);
    return note;
}

//---------------------------------------------------------
//   createTuplet
//   Create a MuseScore tuplet from an MNX tuplet container.
//---------------------------------------------------------

Tuplet *MnxImporter::create_tuplet(const mnx::sequence::Tuplet &mnx_tuplet, Measure *measure, track_idx_t cur_track_idx,
                                   const mnx::FractionValue &start_tick) {
    TDuration base_len = to_muse_score_duration(mnx_tuplet.outer().duration());
    mnx::FractionValue ratio_divisor = mnx_tuplet.outer() / mnx_tuplet.inner().duration();
    if (!base_len.is_valid() || ratio_divisor.remainder() != 0) {
        LOGE() << "Unable to import tuplet at " << mnx_tuplet.pointer().to_string();
        LOGE() << mnx_tuplet.dump(2);
        return nullptr;
    }
    Fraction tuplet_ratio = Fraction(mnx_tuplet.inner().multiple(), ratio_divisor.quotient());

    Tuplet *t = Factory::create_tuplet(measure);
    t->set_track(cur_track_idx);
    t->set_tick(measure->tick() + to_muse_score_fraction(start_tick));
    t->set_ratio(tuplet_ratio);
    t->set_base_len(base_len);
    Fraction f = base_len.fraction() * tuplet_ratio.denominator();
    t->set_ticks(f.reduced());
    // options
    t->set_number_type(to_muse_score_tuplet_number_type(mnx_tuplet.showNumber()));
    t->set_bracket_type(to_muse_score_tuplet_bracket_type(mnx_tuplet.bracket()));
    return t;
}

//---------------------------------------------------------
//   createTremolo
//   Create a two-note tremolo from an MNX MultiNoteTremolo.
//---------------------------------------------------------

void MnxImporter::create_tremolo(const mnx::sequence::MultiNoteTremolo &mnx_tremolo, Measure *measure, track_idx_t cur_track_idx,
                                 const mnx::FractionValue &start_tick, const mnx::FractionValue &end_tick) {
    const auto start_tick2 = start_tick + (end_tick - start_tick) / 2;
    engraving::Chord *c1 = measure->find_chord(measure->tick() + to_muse_score_fraction(start_tick), cur_track_idx);
    engraving::Chord *c2 = measure->find_chord(measure->tick() + to_muse_score_fraction(start_tick2), cur_track_idx);
    IF_ASSERT_FAILED(c1 && c2 && c1->ticks() == c2->ticks()) {
        LOGE() << "Unable to import tremolo at " << mnx_tremolo.pointer().to_string();
        LOGE() << mnx_tremolo.dump(2);
        return;
    }
    int tremolo_beams_num = int(TremoloType::C8) - 1 + mnx_tremolo.marks();
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

ChordRest *MnxImporter::import_event(const mnx::sequence::Event &event, track_idx_t cur_track_idx, Measure *measure,
                                     const mnx::FractionValue &start_tick, const mnx::FractionValue &actual_duration,
                                     const std::stack<Tuplet *> &active_tuplets, TremoloTwoChord *active_tremolo) {
    const TDuration d = to_muse_score_duration(event.duration());
    if (!d.is_valid()) {
        LOGW() << "Given ChordRest duration not supported in MuseScore";
        return nullptr;
    }

    const engraving::Fraction event_tick = to_muse_score_fraction(start_tick);
    Segment *segment = measure->get_segment_r(SegmentType::CHORD_REST, event_tick);
    mnx::Sequence sequence = event.getSequence();

    ChordRest *cr = nullptr;
    const int event_staff = event.staff_or(sequence.staff());
    int cross_staff_move = event_staff - sequence.staff();
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
        LOGE() << "Event " << event.pointer().to_string() << " has invalid staff " << event_staff << ".";
        return nullptr;
    }

    if (const auto &mnx_rest = event.rest()) {
        Rest *rest = Factory::create_rest(segment, d);
        cr = to_chord_rest(rest);
    } else {
        const auto &notes = event.notes();
        const auto &kit_notes = event.kitNotes();
        const int ottava_displacement = mnx_document().getEntityMap().getOttavaShift(event);
        if ((notes && !notes->empty()) || (kit_notes && !kit_notes->empty())) {
            engraving::Chord *chord = Factory::create_chord(segment);
            if (notes && !notes->empty()) {
                for (size_t i = 0; i < event.notes()->size(); i++) {
                    create_note(notes->at(i), chord, base_staff, segment->tick(), ottava_displacement, cur_track_idx);
                }
            }
            if (kit_notes && !kit_notes->empty()) {
                const auto part_it = staff_to_mnx_part_.find(staff_idx);
                const Drumset *drumset = target_staff->part()->instrument()->drumset();
                for (const auto &kit_note : kit_notes.value()) {
                    int midi_pitch = -1;
                    if (part_it != staff_to_mnx_part_.end()) {
                        auto kit_it = mnx_kit_component_to_midi_.find({part_it->second, kit_note.kitComponent()});
                        if (kit_it != mnx_kit_component_to_midi_.end()) {
                            midi_pitch = kit_it->second;
                        }
                    }
                    if (!pitch_is_valid(midi_pitch)) {
                        LOGW() << "Kit note has unknown kit component \"" << kit_note.kitComponent() << "\" at "
                               << kit_note.pointer().to_string();
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
                    mnx_note_to_note_.emplace(kit_note.pointer().to_string(), note);
                }
            }
            if (chord->notes().empty()) {
                LOGW() << "Event " << event.pointer().to_string() << " has no valid notes.";
                delete chord;
                return nullptr;
            }
            if (const auto stem_dir = event.stemDirection()) {
                chord->set_stem_direction(stem_dir.value() == mnx::StemDirection::Up ? DirectionV::UP : DirectionV::DOWN);
            }
            if (use_beams_ && d.hooks() > 0 && !mnx_document().getEntityMap().tryGetBeam(event)) {
                chord->set_beam_mode(BeamMode::NONE);
            }
            cr = to_chord_rest(chord);
        } else {
            LOGW() << "Event " << event.pointer().to_string() << " is neither rest nor chord.";
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
    if (!event.isGrace()) {
        segment->add(cr);
        if (!active_tuplets.empty()) {
            DO_ASSERT(active_tuplets.top());
            if (active_tuplets.top()) {
                active_tuplets.top()->add(cr);
            }
        }
        if (active_tremolo) {
            active_tremolo->add(cr);
        }
    }
    mnx_event_to_cr_.emplace(event.pointer().to_string(), cr);
    return cr;
}

//---------------------------------------------------------
//   importNonGraceEvents
//   Import non-grace events and return true if any ChordRest was created.
//---------------------------------------------------------

bool MnxImporter::import_non_grace_events(const mnx::Sequence &sequence, Measure *measure, track_idx_t cur_track_idx,
                                          GraceNeighborsMap &grace_neighbors) {
    bool inserted_cr = false;
    std::stack<Tuplet *> active_tuplets;
    TremoloTwoChord *active_tremolo = nullptr;

    ChordRest *last_cr = nullptr;
    std::vector<std::string> pending_next;

    mnx::util::SequenceWalkHooks hooks;
    hooks.onFullMeasure = [&](const mnx::Sequence &, const mnx::sequence::FullMeasureRest &mnx_full_measure_rest,
                              const mnx::FractionValue &start_tick, const mnx::FractionValue &, mnx::util::SequenceWalkContext &) {
        Segment *segment = measure->get_segment_r(SegmentType::CHORD_REST, to_muse_score_fraction(start_tick));
        Rest *rest = Factory::create_rest(segment, TDuration(DurationType::V_MEASURE));
        rest->set_duration_type(TDuration(DurationType::V_MEASURE));
        rest->set_track(cur_track_idx);
        Staff *staff = score_->staff(track2staff(cur_track_idx));
        IF_ASSERT_FAILED(staff) {
            LOGE() << "Unable to resolve staff for full-measure rest at " << sequence.pointer().to_string();
            delete rest;
            return true;
        }
        rest->set_ticks(measure->stretched_len(staff));
        segment->add(rest);
        if (mnx_full_measure_rest.fermata()) {
            import_fermata(mnx_full_measure_rest.fermata().value(), rest);
        }
        last_cr = rest;
        inserted_cr = true;
        return true;
    };
    hooks.onItem = [&](const mnx::sequence::SequenceContentObject &item, mnx::util::SequenceWalkContext &ctx) {
        if (item.type() == mnx::sequence::Grace::ContentTypeValue) {
            /// @todo refactor this if MuseScore allows grace notes to be normal.
            const auto grace = item.get<mnx::sequence::Grace>();
            const std::string key = grace.pointer().to_string();
            grace_neighbors[key] = {last_cr, nullptr}; // store prev neighbor
            pending_next.push_back(key);
            return mnx::util::SequenceWalkControl::SkipChildren;
        } else if (item.type() == mnx::sequence::Tuplet::ContentTypeValue) {
            const auto mnx_tuplet = item.get<mnx::sequence::Tuplet>();
            if (tuplet_has_only_spaces_and_graces(mnx_tuplet)) {
                // MuseScore does not like Tuplets that contain only gap Rests,
                // so replace the entire thing with a single gap rest.
                TDuration base_len = to_muse_score_duration(mnx_tuplet.outer().duration());
                const Fraction total = base_len.fraction() * mnx_tuplet.outer().multiple();
                DO_ASSERT(active_tuplets.empty());
                emit_gap_rest(measure, cur_track_idx, ctx.elapsedTime, to_mnx_fraction_value(total.reduced()), nullptr);
                active_tuplets.push(nullptr);
                return mnx::util::SequenceWalkControl::SkipChildren;
            }
            if (Tuplet *t = create_tuplet(mnx_tuplet, measure, cur_track_idx, ctx.elapsedTime)) {
                if (!active_tuplets.empty()) {
                    DO_ASSERT(active_tuplets.top());
                    if (active_tuplets.top()) {
                        active_tuplets.top()->add(t); // reparent tuplet
                    }
                }
                active_tuplets.push(t);
            }
        } else if (item.type() == mnx::sequence::MultiNoteTremolo::ContentTypeValue) {
            const auto mnx_tremolo = item.get<mnx::sequence::MultiNoteTremolo>();
            auto content = mnx_tremolo.content();
            if (content.size() != 2) {
                LOGE() << "Tremolo at " << mnx_tremolo.pointer().to_string() << " has " << content.size()
                       << " events and cannot be imported.";
                LOGE() << mnx_tremolo.dump(2);
                return mnx::util::SequenceWalkControl::SkipChildren;
            }
            using MnxEv = mnx::sequence::Event;
            if (content[0].type() != MnxEv::ContentTypeValue || content[1].type() != MnxEv::ContentTypeValue) {
                LOGE() << "Tremolo at " << mnx_tremolo.pointer().to_string() << " contains other content than events";
                LOGE() << mnx_tremolo.dump(2);
                return mnx::util::SequenceWalkControl::SkipChildren;
            }
        } else if (item.type() == mnx::sequence::Space::ContentTypeValue) {
            // Note that if we are inside a tuplet (ctx.timeRatio != 1), we must emit a gap rest here.
            // For now, though, we emit a gap rest for *any* spacer, since that seems to be the intent of them.
            const auto mnx_space = item.get<mnx::sequence::Space>();
            emit_gap_rest(measure, cur_track_idx, ctx.elapsedTime, mnx_space.duration(),
                          active_tuplets.empty() ? nullptr : active_tuplets.top());
        }
        return mnx::util::SequenceWalkControl::Continue;
    };
    hooks.onEvent = [&](const mnx::sequence::Event &event, const mnx::FractionValue &start_tick, const mnx::FractionValue &actual_duration,
                        [[maybe_unused]] mnx::util::SequenceWalkContext &ctx) {
        IF_ASSERT_FAILED(!ctx.inGrace) {
            LOGE() << "Encountered grace when processing non-grace.";
            return true;
        }
        update_lyric_line_usage_for_event(event, sequence, measure, cur_track_idx, start_tick);
        if (ChordRest *cr = import_event(event, cur_track_idx, measure, start_tick, actual_duration, active_tuplets, active_tremolo)) {
            if (event.fermata()) {
                import_fermata(event.fermata().value(), cr);
            }
            last_cr = cr;
            inserted_cr = true;
            for (const auto &key : pending_next) {
                auto it = grace_neighbors.find(key);
                if (it != grace_neighbors.end()) {
                    it->second.second = cr; // store next neighbor
                }
            }
            pending_next.clear();
        }
        return true;
    };
    hooks.onAfterItem = [&](const mnx::sequence::SequenceContentObject &item, mnx::util::SequenceWalkContext &ctx) {
        if (item.type() == mnx::sequence::Tuplet::ContentTypeValue) {
            active_tuplets.pop();
        } else if (item.type() == mnx::sequence::MultiNoteTremolo::ContentTypeValue) {
            const auto mnx_tremolo = item.get<mnx::sequence::MultiNoteTremolo>();
            const auto start_time = ctx.elapsedTime - (mnx_tremolo.outer() * ctx.timeRatio);
            create_tremolo(mnx_tremolo, measure, cur_track_idx, start_time, ctx.elapsedTime);
        }
    };

    mnx::util::walkSequenceContent(sequence, hooks);

    return inserted_cr;
}

//---------------------------------------------------------
//   importGraceEvents
//   Import grace-note events using neighbor context.
//---------------------------------------------------------

void MnxImporter::import_grace_events(const mnx::Sequence &sequence, Measure *measure, track_idx_t cur_track_idx,
                                      const GraceNeighborsMap &grace_neighbors) {
    mnx::util::SequenceWalkHooks hooks;
    hooks.onEvent = [&](const mnx::sequence::Event &event, const mnx::FractionValue &start_tick, const mnx::FractionValue &actual_duration,
                        mnx::util::SequenceWalkContext &ctx) {
        if (ctx.inGrace) {
            if (event.rest()) {
                LOGW() << "encountered unsupported grace note rest at " << event.pointer().to_string();
                return true;
            }
            auto grace = event.container<mnx::sequence::Grace>();
            auto [left_neighbor, right_neighbor] = muse::value(grace_neighbors, grace.pointer().to_string());
            const bool use_right = right_neighbor && right_neighbor->is_chord();
            const bool use_left = !use_right && left_neighbor && left_neighbor->is_chord();
            if (use_right || use_left) {
                if (ChordRest *cr = import_event(event, cur_track_idx, measure, start_tick, actual_duration, {}, nullptr)) {
                    engraving::Chord *gc = to_chord(cr);
                    TDuration d = gc->duration_type();
                    if (use_right && grace.slash() && grace.content().size() == 1) {
                        gc->set_note_type(engraving::NoteType::ACCIACCATURA);
                    } else {
                        gc->set_note_type(dura_type_to_grace_note_type(d.type(), use_left));
                        gc->set_show_stem_slash(grace.slash());
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
                    if (event.fermata()) {
                        DO_ASSERT_X(grace_parent, "no parent found for grace note");
                        if (grace_parent) {
                            import_fermata(event.fermata().value(), grace_parent);
                        }
                    }
                }
            }
        }
        return true;
    };

    mnx::util::walkSequenceContent(sequence, hooks);
}

//---------------------------------------------------------
//   importSequences
//   Import all sequences for a part measure into a MuseScore measure.
//---------------------------------------------------------

void MnxImporter::import_sequences(const mnx::Part &mnx_part, const mnx::part::Measure &part_measure, Measure *measure) {
    auto &sequence_tracks = part_measure_sequence_tracks_[part_measure.pointer().to_string()];
    sequence_tracks.clear();
    std::vector<std::vector<track_idx_t>> staff_voice_maps(mnx_part.staves());

    // pass1: import non-grace-note events to ChordRest
    for (const auto &sequence : part_measure.sequences()) {
        if (sequence.staff() > mnx_part.staves()) {
            LOGE() << "Sequence " << sequence.pointer().to_string() << " specifies non-existent staff " << sequence.staff()
                   << " for MNX part at " << mnx_part.pointer().to_string() << ".";
            continue;
        }
        const staff_idx_t cur_staff_idx =
            muse::value(mnx_part_staff_to_staff_, std::make_pair(mnx_part.calcArrayIndex(), sequence.staff()), muse::nidx);
        IF_ASSERT_FAILED(cur_staff_idx != muse::nidx) {
            LOGE() << "Sequence " << sequence.pointer().to_string() << " specifies unmapped staff " << sequence.staff()
                   << " for MNX part at " << mnx_part.pointer().to_string() << ".";
            return;
        }
        auto &staff_voice_map = staff_voice_maps[static_cast<size_t>(sequence.staff() - 1)];
        const track_idx_t voice_id = staff_voice_map.size();
        if (voice_id >= ::mu::engraving::voices) {
            LOGW() << "Part measure " << part_measure.pointer().to_string() << " contains too many voices for staff " << sequence.staff()
                   << ". This sequence is skipped.";
            continue;
        }
        const track_idx_t cur_track_idx = staff2track(cur_staff_idx, voice_id);
        GraceNeighborsMap grace_neighbors;
        if (import_non_grace_events(sequence, measure, cur_track_idx, grace_neighbors)) {
            import_grace_events(sequence, measure, cur_track_idx,
                                grace_neighbors); // if MuseScore refactors graces, maybe we don't need this.
            sequence_tracks.emplace(sequence.pointer().to_string(), cur_track_idx);
            staff_voice_map.push_back(voice_id);
        }
    }

    // fill in measures as needed with rests.
    for (int staff_num = 1; staff_num <= mnx_part.staves(); staff_num++) {
        staff_idx_t staff_idx = mnx_part_staff_to_staff_idx(mnx_part, staff_num);
        measure->check_measure(staff_idx);
    }
}

// Whether a dynamic renders any text of its own, as opposed to being a bare hairpin.
// DynamicAccent overrides calcHasImmediateText() to account for its prefix, suffix, and
// residual value, but iterating a dynamics array yields the base class by value, so the
// override only runs if the concrete type is recovered first.
static bool calc_has_immediate_text(const mnx::part::DynamicGroupBase &mnx_dynamic) {
    if (mnx_dynamic.type() == mnx::part::DynamicAccent::ContentTypeValue) {
        return mnx_dynamic.get<mnx::part::DynamicAccent>().calcHasImmediateText();
    }
    return mnx_dynamic.calcHasImmediateText();
}

// Describes an MNX dynamic in the terms toMuseScoreDynamicType() understands, so that a
// dynamic MuseScore's text parser did not recognize can still be classified from what MNX
// actually said about it.
static MnxDynamicMapping mnx_dynamic_mapping(const mnx::part::DynamicGroupBase &mnx_dynamic,
                                             const std::optional<mnx::part::DynamicAccent> &mnx_accent) {
    MnxDynamicMapping mapping;
    mapping.value = mnx_dynamic.value();
    if (mnx_accent) {
        mapping.is_accent = true;
        mapping.residual_value = mnx_accent->residualValue();
        mapping.accent_prefix = mnx_accent->accentPrefix();
        mapping.accent_suffix = mnx_accent->accentSuffix();
    }
    return mapping;
}

staff_idx_t MnxImporter::resolve_dynamic_staff(const mnx::Part &mnx_part, const mnx::part::DynamicGroupBase &mnx_dynamic) {
    const auto explicit_staff = mnx_dynamic.staff();
    if (explicit_staff) {
        return muse::value(mnx_part_staff_to_staff_, std::make_pair(mnx_part.calcArrayIndex(), *explicit_staff), muse::nidx);
    }

    // Orient only decides which staff to anchor to when MNX does not provide an
    // explicit staff number.
    const int staff_num = [&]() {
        switch (mnx_dynamic.orient()) {
            case mnx::MultiStaffOrientation::Above: return 1;
            case mnx::MultiStaffOrientation::Below: return mnx_part.staves();
            case mnx::MultiStaffOrientation::Between: return 1;
            case mnx::MultiStaffOrientation::Auto: return 1;
        }
        return 1;
    }();

    return muse::value(mnx_part_staff_to_staff_, std::make_pair(mnx_part.calcArrayIndex(), staff_num), muse::nidx);
}

void MnxImporter::apply_dynamic_orient(EngravingItem *item, const mnx::Part &mnx_part, mnx::MultiStaffOrientation orient) {
    IF_ASSERT_FAILED(item) { return; }

    switch (orient) {
        case mnx::MultiStaffOrientation::Above:
            set_and_style_property(item, Pid::DIRECTION, DirectionV::UP);
            set_and_style_property(item, Pid::CENTER_BETWEEN_STAVES, AutoOnOff::OFF);
            break;
        case mnx::MultiStaffOrientation::Below:
            set_and_style_property(item, Pid::DIRECTION, DirectionV::DOWN);
            set_and_style_property(item, Pid::CENTER_BETWEEN_STAVES, AutoOnOff::OFF);
            break;
        case mnx::MultiStaffOrientation::Between:
            set_and_style_property(item, Pid::DIRECTION, DirectionV::AUTO);
            if (mnx_part.staves() > 1) {
                set_and_style_property(item, Pid::CENTER_BETWEEN_STAVES, AutoOnOff::ON);
            }
            break;
        case mnx::MultiStaffOrientation::Auto: break;
    }
}

static VoiceAssignment resolve_dynamic_voice_assignment(const mnx::part::DynamicGroupBase &mnx_dynamic, bool use_voice_assignment) {
    if (mnx_dynamic.voice() && use_voice_assignment) {
        return VoiceAssignment::CURRENT_VOICE_ONLY;
    }
    if (mnx_dynamic.staff()) {
        return VoiceAssignment::ALL_VOICE_IN_STAFF;
    }
    return VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
}

void MnxImporter::create_dynamic(const mnx::part::DynamicGroupBase &mnx_dynamic, Segment *segment, const mnx::Part &mnx_part,
                                 track_idx_t cur_track_idx, bool use_voice_assignment) {
    IF_ASSERT_FAILED(segment) { return; }

    Dynamic *dyn = Factory::create_dynamic(segment);
    dyn->set_track(cur_track_idx);

    // An accent spells itself out as accentPrefix + value + accentSuffix + residualValue.
    // Only DynamicAccent carries those, and iterating a dynamics array yields the base class
    // by value, so the concrete type has to be recovered before they can be read.
    const auto mnx_accent = mnx_dynamic.type() == mnx::part::DynamicAccent::ContentTypeValue ?
                                std::make_optional(mnx_dynamic.get<mnx::part::DynamicAccent>()) :
                                std::nullopt;
    const std::optional<mnx::DynamicValue> residual_value = mnx_accent ? mnx_accent->residualValue() : std::nullopt;

    String dynamic_text = String::from_std_string(mnx_dynamic.prefix_or({}));
    auto append_glyph = [&](const std::string &glyph_name) {
        if (!glyph_name.empty()) {
            dynamic_text += u"<sym>" + String::from_std_string(glyph_name) + u"</sym>";
        }
    };
    // Prefer glyphs over value. MNX can now spell out every dynamic MuseScore has a type for,
    // so glyphs no longer stand in for a value MNX cannot express, but they still describe the
    // exact representation the source document uses and override the default one derived from
    // the value. Only when they are absent do we spell the dynamic out ourselves.
    if (const auto glyphs = mnx_dynamic.glyphs(); glyphs && !glyphs->empty()) {
        if (!dynamic_text.empty()) {
            dynamic_text += u" ";
        }
        for (const auto &glyph : glyphs.value()) {
            append_glyph(glyph);
        }
    } else if (mnx_dynamic.value() || residual_value) {
        if (!dynamic_text.empty()) {
            dynamic_text += u" ";
        }
        auto append_value_glyphs = [&](std::optional<mnx::DynamicValue> val) {
            if (val) {
                for (const auto &glyph_name : to_muse_score_dynamic_glyph_names(val.value())) {
                    append_glyph(glyph_name);
                }
            }
        };
        // Spelling the dynamic with SMuFL glyphs rather than letters lets MuseScore
        // recognize the combinations it knows (it compares against its own glyph strings),
        // and leaves recoverable glyphs behind for the ones it does not.
        if (mnx_accent) {
            append_glyph(to_muse_score_dynamic_glyph_name(mnx_accent->accentPrefix()));
        }
        append_value_glyphs(mnx_dynamic.value());
        if (mnx_accent) {
            append_glyph(to_muse_score_dynamic_glyph_name(mnx_accent->accentSuffix()));
        }
        append_value_glyphs(residual_value);
    }

    if (mnx_dynamic.suffix()) {
        if (!dynamic_text.empty()) {
            dynamic_text += u" ";
        }
        dynamic_text += String::from_std_string(mnx_dynamic.suffix().value());
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
        if (const auto type = to_muse_score_dynamic_type(mnx_dynamic_mapping(mnx_dynamic, mnx_accent))) {
            dyn->set_dynamic_type(type.value());
        }
    }
    apply_dynamic_orient(dyn, mnx_part, mnx_dynamic.orient());
    dyn->set_voice_assignment(resolve_dynamic_voice_assignment(mnx_dynamic, use_voice_assignment));
    segment->add(dyn);
}

void MnxImporter::create_hairpin(const mnx::part::DynamicGradual &mnx_hairpin, Segment *segment, const mnx::Part &mnx_part,
                                 track_idx_t cur_track_idx, bool use_voice_assignment) {
    if (!segment) {
        return;
    }

    const auto end_measure = mnx_document().getEntityMap().tryGet<mnx::global::Measure>(mnx_hairpin.end().measure());
    if (!end_measure.has_value()) {
        LOGW() << "hairpin end measure not found for " << mnx_hairpin.pointer().to_string();
        return;
    }
    Measure *target_measure = try_mnx_measure_to_measure(end_measure->calcArrayIndex());
    if (!target_measure) {
        LOGW() << "hairpin end measure could not be resolved to a MuseScore measure for " << mnx_hairpin.pointer().to_string();
        return;
    }

    const Fraction end_tick = target_measure->tick() + to_muse_score_fraction(mnx_hairpin.end().position().fraction());
    if (end_tick <= segment->tick()) {
        LOGW() << "hairpin end tick is not after start tick for " << mnx_hairpin.pointer().to_string();
        return;
    }

    // A hairpin may finish on a different staff of the same part than it started on.
    track_idx_t end_track_idx = cur_track_idx;
    if (const auto staff_end = mnx_hairpin.staffEnd()) {
        if (staff_end.value() >= 1 && staff_end.value() <= mnx_part.staves()) {
            end_track_idx = staff2track(mnx_part_staff_to_staff_idx(mnx_part, staff_end.value()), cur_track_idx % ::mu::engraving::voices);
        } else {
            LOGW() << "hairpin staffEnd " << staff_end.value() << " is out of range for " << mnx_hairpin.pointer().to_string();
        }
    }

    Hairpin *hairpin = Factory::create_hairpin(score_->dummy());
    hairpin->set_hairpin_type(mnx_hairpin.wedgeType() == mnx::DynamicWedgeType::Increasing ? HairpinType::CRESC_HAIRPIN :
                                                                                             HairpinType::DIM_HAIRPIN);
    hairpin->set_track(cur_track_idx);
    hairpin->set_track2(end_track_idx);
    hairpin->set_tick(segment->tick());
    hairpin->set_tick2(end_tick);

    apply_dynamic_orient(hairpin, mnx_part, mnx_hairpin.orient());
    hairpin->set_voice_assignment(resolve_dynamic_voice_assignment(mnx_hairpin, use_voice_assignment));

    score_->add_element(hairpin);
}

std::optional<track_idx_t> MnxImporter::resolve_voice_track(const mnx::part::Measure &mnx_measure, staff_idx_t staff_idx,
                                                            const std::string &mnx_voice_id) {
    const auto measure_it = part_measure_sequence_tracks_.find(mnx_measure.pointer().to_string());
    if (measure_it == part_measure_sequence_tracks_.end()) {
        return std::nullopt;
    }

    const auto part = mnx_measure.getEnclosingElement<mnx::Part>();
    IF_ASSERT_FAILED(part) {
        LOGE() << "part not found for measure " << mnx_measure.pointer().to_string();
        return std::nullopt;
    }

    for (const auto &sequence : mnx_measure.sequences()) {
        if (mnx_part_staff_to_staff_idx(part.value(), sequence.staff()) != staff_idx) {
            continue;
        }

        const auto sequence_voice = sequence.voice();
        if (!sequence_voice || sequence_voice.value() != mnx_voice_id) {
            continue;
        }

        const auto sequence_it = measure_it->second.find(sequence.pointer().to_string());
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

void MnxImporter::create_dynamics(const mnx::part::Measure &mnx_measure, Measure *measure) {
    const auto part = mnx_measure.getEnclosingElement<mnx::Part>();
    if (const auto mnx_dynamics = mnx_measure.dynamics()) {
        for (const auto &mnx_dynamic : mnx_dynamics.value()) {
            staff_idx_t staff_idx = resolve_dynamic_staff(part.value(), mnx_dynamic);
            IF_ASSERT_FAILED(staff_idx != muse::nidx) {
                LOGE() << "staff idx not found for part " << part->pointer().to_string();
                continue;
            }
            const track_idx_t staff_track_idx = staff2track(staff_idx);
            bool use_voice_assignment = false;
            track_idx_t cur_track_idx = staff_track_idx;
            if (const auto mnx_voice_id = mnx_dynamic.voice()) {
                if (const auto resolved_track = resolve_voice_track(mnx_measure, staff_idx, mnx_voice_id.value())) {
                    cur_track_idx = resolved_track.value();
                    use_voice_assignment = true;
                }
            }

            Fraction r_tick = to_muse_score_fraction(mnx_dynamic.position().fraction());
            Segment *s = measure->get_chord_rest_or_time_tick_segment(measure->tick() + r_tick);

            if (calc_has_immediate_text(mnx_dynamic)) {
                create_dynamic(mnx_dynamic, s, part.value(), cur_track_idx, use_voice_assignment);
            }

            if (mnx_dynamic.type() == mnx::part::DynamicGradual::ContentTypeValue) {
                const auto mnx_hairpin = mnx_dynamic.get<mnx::part::DynamicGradual>();
                create_hairpin(mnx_hairpin, s, part.value(), cur_track_idx, use_voice_assignment);
            }
        }
    }
}

//---------------------------------------------------------
//   createOttavas
//   Create MuseScore ottava spanners from MNX ottavas.
//---------------------------------------------------------

void MnxImporter::create_ottavas(const mnx::part::Measure &mnx_measure, Measure *measure) {
    const auto part = mnx_measure.getEnclosingElement<mnx::Part>();
    if (const auto mnx_ottavas = mnx_measure.ottavas()) {
        for (const auto &mnx_ottava : mnx_ottavas.value()) {
            staff_idx_t staff_idx =
                muse::value(mnx_part_staff_to_staff_, std::make_pair(part->calcArrayIndex(), mnx_ottava.staff()), muse::nidx);
            IF_ASSERT_FAILED(staff_idx != muse::nidx) {
                LOGE() << "staff idx not found for part " << part->pointer().to_string();
                continue;
            }
            const auto mnx_end_measure = mnx_document().getEntityMap().get<mnx::global::Measure>(mnx_ottava.end().measure());
            Measure *end_measure = mnx_measure_to_measure(mnx_end_measure.calcArrayIndex());
            const Fraction end_pos = to_muse_score_fraction(mnx_ottava.end().position().fraction());
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
            ottava->set_tick(measure->tick() + to_muse_score_r_tick(mnx_ottava.position()));
            ottava->set_tick2(end_tick);
            const OttavaType ottava_type = to_muse_score_ottava_type(mnx_ottava.value());
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
}

//---------------------------------------------------------
//   createBeams
//   Apply MuseScore beam modes based on MNX beam definitions.
//---------------------------------------------------------

void MnxImporter::create_beams(const mnx::part::Measure &mnx_measure) {
    if (const auto beams = mnx_measure.beams()) {
        for (const auto &beam : beams.value()) {
            const auto events = beam.events();
            std::vector<ChordRest *> beam_elements;
            DirectionV beam_direction = DirectionV::AUTO;
            for (size_t x = 0; x < events.size(); x++) {
                const auto &event_id = events[x];
                const auto mnx_event = mnx_document().getEntityMap().tryGet<mnx::sequence::Event>(event_id);
                if (beam_direction == DirectionV::AUTO && mnx_event) {
                    if (const auto stem_dir = mnx_event->stemDirection()) {
                        beam_direction = stem_dir.value() == mnx::StemDirection::Up ? DirectionV::UP : DirectionV::DOWN;
                    }
                }
                ChordRest *cr = mnx_event_id_to_cr(event_id);
                IF_ASSERT_FAILED(cr) {
                    LOGE() << "encountered unmapped event " << event_id << " in beam " << beam.pointer().to_string();
                    LOGE() << beam.dump(2);
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
                    const auto mode = to_muse_score_beam_mode(mnx_document().getEntityMap().getBeamStartLevel(event_id));
                    cr->set_beam_mode(mode);
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
}

//---------------------------------------------------------
//   createArpeggios
//   Create MuseScore arepeggio markings from MNX arpeggios and nonArpeggios.
//---------------------------------------------------------

void MnxImporter::create_arpeggios(const mnx::part::Measure &mnx_measure) {
    const auto part = mnx_measure.getEnclosingElement<mnx::Part>();

    auto process_arpeggio = [&](const auto &mnx_arpeggio) -> void {
        using MnxArpeggioType = std::decay_t<decltype(mnx_arpeggio)>;
        static_assert(std::is_base_of_v<mnx::part::ArpeggioBase, MnxArpeggioType>, "arpeggio must derive from mnx::part::ArpeggioBase");

        const mnx::part::ArpeggioBase &arp_base = mnx_arpeggio;
        Note *note_start = mnx_note_id_to_note(arp_base.span().start());
        Note *note_end = mnx_note_id_to_note(arp_base.span().end());
        IF_ASSERT_FAILED(note_start && note_end) {
            LOGE() << "Start note " << arp_base.span().start() << " or end note " << arp_base.span().end() << " not mapped.";
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
            LOGE() << "Start note " << arp_base.span().start() << " or end note " << arp_base.span().end()
                   << " belongs to an unmapped staff.";
            return;
        }
        if (*start_part_index != part->calcArrayIndex() || *end_part_index != part->calcArrayIndex()) {
            LOGW() << "skipping arpeggio span " << arp_base.span().start() << " to " << arp_base.span().end()
                   << " because MNX arpeggios cannot cross part boundaries.";
            return;
        }
        if (note_start->tick() != note_end->tick()) {
            LOGW() << "Start note " << arp_base.span().start() << " or end note " << arp_base.span().end()
                   << " are not vertically aligned.";
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
        // leave the graceArpeggio check here in case we support grace arpeggios in the future
        if (grace_arpeggio && chord_top != chord_bot) {
            LOGW() << "skipping arpeggio spanning multiple grace chords at " << chord_top->tick().to_string();
            return;
        }

        // MNX targets individual notes, but MuseScore must target entire chords

        const Note *chord_top_note = chord_top->up_note();
        const Note *chord_bot_note = chord_bot->down_note();
        const bool targets_chord_extremes =
            (note_start == chord_top_note && note_end == chord_bot_note) || (note_end == chord_top_note && note_start == chord_bot_note);
        if (!targets_chord_extremes) {
            LOGW() << "arpeggio span " << arp_base.span().start() << " to " << arp_base.span().end()
                   << " does not target the top and bottom notes of its chord span at " << chord_top->tick().to_string();
        }
        if (chord_top->arpeggio()) {
            LOGW() << "arpeggio already exists on top chord";
            return;
        }

        Arpeggio *arpeggio = Factory::create_arpeggio(chord_top);
        arpeggio->set_track(chord_top->track());
        if constexpr (std::is_same_v<MnxArpeggioType, mnx::part::NonArpeggio>) {
            arpeggio->set_arpeggio_type(ArpeggioType::BRACKET);
        } else {
            arpeggio->set_arpeggio_type(ArpeggioType::NORMAL);
            if (mnx_arpeggio.arrow()) {
                switch (mnx_arpeggio.direction()) {
                    case mnx::MarkingUpDownAuto::Down: arpeggio->set_arpeggio_type(ArpeggioType::DOWN); break;
                    case mnx::MarkingUpDownAuto::Up: arpeggio->set_arpeggio_type(ArpeggioType::UP); break;
                    case mnx::MarkingUpDownAuto::Auto: break;
                }
            }
        }
        if (grace_arpeggio) {
            arpeggio->set_span(1);
        } else {
            arpeggio->set_span(int(chord_bot->track() + 1 - chord_top->track()));
            Segment *s = chord_top->segment();
            for (track_idx_t track = chord_top->track(); track <= chord_bot->track(); ++track) {
                EngravingItem *item = s->element(track);
                if (item && item->is_chord()) {
                    to_chord(item)->set_span_arpeggio(arpeggio);
                }
            }
        }
        chord_top->set_arpeggio(arpeggio);
    };

    if (const auto mnx_arpeggios = mnx_measure.arpeggios()) {
        for (const auto &arpeggio : mnx_arpeggios.value()) {
            process_arpeggio(arpeggio);
        }
    }
    if (const auto mnx_non_arpeggios = mnx_measure.nonArpeggios()) {
        for (const auto &non_arpeggio : mnx_non_arpeggios.value()) {
            process_arpeggio(non_arpeggio);
        }
    }
}

//---------------------------------------------------------
//   processSequencePass2
//   Second pass over a sequence for lyrics, accidentals, slurs, ties, and rest positions.
//---------------------------------------------------------

void MnxImporter::process_sequence_pass2(const mnx::Sequence &sequence, Measure *measure) {
    mnx::util::SequenceWalkHooks hooks;
    hooks.onEvent = [&](const mnx::sequence::Event &event, const mnx::FractionValue &, const mnx::FractionValue &,
                        mnx::util::SequenceWalkContext &ctx) {
        ChordRest *cr = muse::value(mnx_event_to_cr_, event.pointer().to_string());
        if (!cr) {
            LOGW() << "event " << event.pointer().to_string() << " is not mapped, possibly due to too many sequences in measure.";
            return true;
        }
        if (!ctx.inGrace) {
            create_lyrics(event, cr);
        }
        if (const auto rest = event.rest(); rest && cr->is_rest()) {
            import_rest_properties(rest.value(), to_rest(cr));
        }
        if (const auto slurs = event.slurs()) {
            for (const auto &slur : slurs.value()) {
                create_slur(slur, cr);
            }
        }
        if (const auto notes = event.notes()) {
            for (const auto &mnx_note : notes.value()) {
                Note *note = muse::value(mnx_note_to_note_, mnx_note.pointer().to_string());
                IF_ASSERT_FAILED(note) {
                    LOGE() << "note is not mapped: " << mnx_note.pointer().to_string();
                    LOGE() << mnx_note.dump(2);
                    continue;
                }
                create_accidentals(mnx_note, note, measure);
                if (const auto ties = mnx_note.ties()) {
                    create_ties(ties.value(), note);
                }
            }
        }
        if (const auto kit_notes = event.kitNotes()) {
            for (const auto &kit_note : kit_notes.value()) {
                Note *note = muse::value(mnx_note_to_note_, kit_note.pointer().to_string());
                IF_ASSERT_FAILED(note) {
                    LOGE() << "kit note is not mapped: " << kit_note.pointer().to_string();
                    LOGE() << kit_note.dump(2);
                    continue;
                }
                if (const auto ties = kit_note.ties()) {
                    create_ties(ties.value(), note);
                }
            }
        }
        return true;
    };

    mnx::util::walkSequenceContent(sequence, hooks);
}

//---------------------------------------------------------
//   createMeasureRepeats
//   Create MuseScore measure repeats from MNX measure repeats.
//---------------------------------------------------------

void MnxImporter::create_measure_repeats(const mnx::Part &mnx_part) {
    /// @todo Import MeasureRepeat counter, displayNumber, and staffPosition, none of which
    /// has a per-element home in MuseScore. counter is the iteration the player is on, which
    /// MuseScore derives from the repeat's place in the run under score-wide styles.
    /// displayNumber asks whether `number` itself is drawn above the glyph; MuseScore always
    /// draws it for 2- and 4-bar repeats and gates 1-bar repeats on the score-wide
    /// Sid::oneMeasureRepeatShow1. staffPosition is computed from the staff's middle line.
    for (const auto &part_measure : mnx_part.measures()) {
        const auto mnx_measure_repeat = part_measure.measureRepeat();
        if (!mnx_measure_repeat) {
            continue;
        }

        const int num_measures = mnx_measure_repeat->number();
        // MuseScore only draws a glyph for 1, 2, and 4 measure repeats.
        if (num_measures != 1 && num_measures != 2 && num_measures != 4) {
            LOGW() << "Skipping measure repeat of " << num_measures << " measures at " << mnx_measure_repeat->pointer().to_string()
                   << "; MuseScore supports 1, 2, or 4.";
            continue;
        }

        // MuseScore anchors the element in the center measure of an odd-numbered group, or
        // the last measure of the first half of an even-numbered one.
        const int anchor_offset = (num_measures % 2) ? num_measures / 2 : num_measures / 2 - 1;

        for (int offset = 0; offset < num_measures; offset++) {
            Measure *measure = try_mnx_measure_to_measure(part_measure.calcArrayIndex() + offset);
            if (!measure) {
                LOGW() << "Measure repeat at " << mnx_measure_repeat->pointer().to_string() << " extends past the end of the score.";
                break;
            }

            for (int staff_num = 1; staff_num <= mnx_part.staves(); staff_num++) {
                const staff_idx_t staff_idx = mnx_part_staff_to_staff_idx(mnx_part, staff_num);
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
    // pass1: create ChordRests and clefs
    for (const auto &mnx_part : mnx_document().parts()) {
        for (const auto &part_measure : mnx_part.measures()) {
            Measure *measure = mnx_measure_to_measure(part_measure.calcArrayIndex());
            import_sequences(mnx_part, part_measure, measure);
            if (const auto mnx_clefs = part_measure.clefs()) {
                create_clefs(mnx_part, mnx_clefs.value(), measure);
            }
        }
    }
    build_lyric_line_verse_map();
    // Measure repeats replace the content pass1 created, so they must follow it.
    for (const auto &mnx_part : mnx_document().parts()) {
        create_measure_repeats(mnx_part);
    }
    // pass2: add accidentals, beams, dynamics, ottavas, slurs, and ties
    for (const auto &mnx_part : mnx_document().parts()) {
        for (const auto &part_measure : mnx_part.measures()) {
            Measure *measure = mnx_measure_to_measure(part_measure.calcArrayIndex());
            create_arpeggios(part_measure);
            create_dynamics(part_measure, measure);
            create_ottavas(part_measure, measure);
            create_beams(part_measure);
            for (const auto &sequence : part_measure.sequences()) {
                process_sequence_pass2(sequence, measure);
            }
        }
    }
}
} // namespace mu::iex::mnxio
