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
#pragma once

#include <map>
#include <variant>
#include <stack>
#include <unordered_map>

#include "engraving/types/propertyvalue.h"
#include "engraving/types/types.h"

#ifdef MNXDOM_SYSTEM
#include <mnxdom/mnxdom.h>
#else
#include "mnxdom.h"
#endif

namespace mu::engraving {
struct Articulation;
struct Chord;
struct ChordRest;
struct Dynamic;
struct EngravingObject;
struct EngravingItem;
struct Fermata;
struct Instrument;
struct Hairpin;
struct Measure;
struct Note;
struct Part;
struct Rest;
struct Score;
struct Segment;
struct Staff;
struct TremoloTwoChord;
struct Tuplet;

enum class Pid : short;
} // namespace mu::engraving

namespace mu::iex::mnxio {
struct MnxImporter {
    MnxImporter(engraving::Score *s, mnx::Document &&doc) : score_(s), mnx_document_(std::move(doc)) {}

    void import_mnx();

    mnx::Document &mnx_document() { return mnx_document_; }

    const mnx::Document &mnx_document() const { return mnx_document_; }

    engraving::Score *score() const { return score_; }

private:
    using GraceNeighborsMap = std::unordered_map<std::string, std::pair<engraving::ChordRest *, engraving::ChordRest *>>;

    // settings
    void import_settings();

    // parts
    void import_parts();
    void create_staff(engraving::Part *part, const mnx::Part &mnx_part, int staff_num);

    // brackets and barlines
    void import_brackets();

    // global measures
    void import_global_measures();
    void build_lyric_line_verse_map();
    void create_key_sig(engraving::Measure *measure, int key_fifths); // positive = sharps; negative = flats
    void create_time_sig(engraving::Measure *measure, const mnx::TimeSignature &time_sig);
    void set_barline(engraving::Measure *measure, const mnx::global::Barline &barline);
    void create_volta(engraving::Measure *measure, const mnx::global::Ending &ending);
    void create_jump_or_marker(engraving::Measure *measure, const mnx::FractionValue &location,
                               std::variant<engraving::JumpType, engraving::MarkerType> type,
                               const std::optional<std::string> glyph_name = std::nullopt);
    void create_tempo_mark(engraving::Measure *measure, const mnx::global::Tempo &tempo);

    // part measures
    void import_part_measures();
    void import_sequences(const mnx::Part &mnx_part, const mnx::part::Measure &part_measure, engraving::Measure *measure);
    bool import_non_grace_events(const mnx::Sequence &sequence, engraving::Measure *measure, engraving::track_idx_t cur_track_idx,
                                 GraceNeighborsMap &grace_neighbors);
    void update_lyric_line_usage_for_event(const mnx::sequence::Event &event, const mnx::Sequence &sequence, engraving::Measure *measure,
                                           engraving::track_idx_t cur_track_idx, const mnx::FractionValue &start_tick);
    void import_grace_events(const mnx::Sequence &sequence, engraving::Measure *measure, engraving::track_idx_t cur_track_idx,
                             const GraceNeighborsMap &grace_neighbors);
    engraving::ChordRest *import_event(const mnx::sequence::Event &event, engraving::track_idx_t, engraving::Measure *measure,
                                       const mnx::FractionValue &start_tick, const mnx::FractionValue &actual_duration,
                                       const std::stack<engraving::Tuplet *> &active_tuplets, engraving::TremoloTwoChord *active_tremolo);
    engraving::Tuplet *create_tuplet(const mnx::sequence::Tuplet &mnx_tuplet, engraving::Measure *measure,
                                     engraving::track_idx_t cur_track_idx, const mnx::FractionValue &start_tick);
    void create_tremolo(const mnx::sequence::MultiNoteTremolo &mnx_tremolo, engraving::Measure *measure,
                        engraving::track_idx_t cur_track_idx, const mnx::FractionValue &start_tick, const mnx::FractionValue &end_tick);
    void process_sequence_pass2(const mnx::Sequence &sequence, engraving::Measure *measure);
    void create_slur(const mnx::sequence::Slur &mnx_slur, engraving::ChordRest *start_cr);
    void create_lyrics(const mnx::sequence::Event &mnx_event, engraving::ChordRest *cr);
    void create_ties(const mnx::Array<mnx::sequence::Tie> &ties, engraving::Note *start_note);
    void create_accidentals(const mnx::sequence::Note &mnx_note, engraving::Note *note, engraving::Measure *measure);
    void import_rest_properties(const mnx::sequence::Rest &mnx_rest, engraving::Rest *rest);
    engraving::Rest *emit_gap_rest(engraving::Measure *measure, engraving::track_idx_t cur_track_idx, const mnx::FractionValue &start_tick,
                                   const mnx::FractionValue &duration, engraving::Tuplet *tuplet_to_add);
    engraving::Note *create_note(const mnx::sequence::Note &mnx_note, engraving::Chord *chord, engraving::Staff *base_staff,
                                 const engraving::Fraction &tick, int ottava_displacement, engraving::track_idx_t cur_track_idx);
    void create_arpeggios(const mnx::part::Measure &mnx_measure);
    void create_beams(const mnx::part::Measure &mnx_measure);
    void create_clefs(const mnx::Part &mnx_part, const mnx::Array<mnx::part::PositionedClef> &mnx_clefs, engraving::Measure *measure);
    void create_dynamics(const mnx::part::Measure &mnx_measure, engraving::Measure *measure);
    void create_dynamic(const mnx::part::DynamicGroupBase &mnx_dynamic, engraving::Segment *segment, const mnx::Part &mnx_part,
                        engraving::track_idx_t cur_track_idx, bool use_voice_assignment);
    void create_measure_repeats(const mnx::Part &mnx_part);
    void create_ottavas(const mnx::part::Measure &mnx_measure, engraving::Measure *measure);

    // markings
    engraving::Articulation *add_articulation(engraving::ChordRest *cr, const mnx::sequence::EventMarkingBase &marking,
                                              engraving::SymId sym_id, const char *name);
    void add_fermata(engraving::Segment *seg, const mnx::Fermata &mnx_fermata, engraving::track_idx_t track);
    void import_markings(const mnx::sequence::Event &mnx_event, engraving::ChordRest *cr, const engraving::Fraction &event_end_tick,
                         engraving::Measure *measure = nullptr);
    void import_bow_direction(const mnx::sequence::BowDirection &bow_direction, engraving::ChordRest *cr);
    void import_accent(const mnx::sequence::Accent &accent, engraving::ChordRest *cr);
    void import_breath(const mnx::sequence::BreathMark &breath, engraving::ChordRest *cr, const engraving::Fraction &event_end_tick,
                       engraving::Measure *measure = nullptr);
    void import_soft_accent(const mnx::sequence::SoftAccent &soft_accent, engraving::ChordRest *cr);
    void import_spiccato(const mnx::sequence::Spiccato &spiccato, engraving::ChordRest *cr);
    void import_staccatissimo(const mnx::sequence::Staccatissimo &staccatissimo, engraving::ChordRest *cr);
    void import_staccato(const mnx::sequence::Staccato &staccato, engraving::ChordRest *cr);
    void import_stress(const mnx::sequence::Stress &stress, engraving::ChordRest *cr);
    void import_strong_accent(const mnx::sequence::StrongAccent &strong_accent, engraving::ChordRest *cr);
    void import_tenuto(const mnx::sequence::Tenuto &tenuto, engraving::ChordRest *cr);
    void import_tremolo(const mnx::sequence::SingleNoteTremolo &tremolo, engraving::ChordRest *cr);
    void import_unstress(const mnx::sequence::Unstress &unstress, engraving::ChordRest *cr);
    void import_fermata(const mnx::Fermata &mnx_fermata, engraving::ChordRest *cr);

    // utility funcs
    engraving::staff_idx_t mnx_part_staff_to_staff_idx(const mnx::Part &mnx_part, int staff_num);
    engraving::staff_idx_t
    mnx_layout_staff_to_staff_idx(const mnx::layout::Staff &mnx_staff); // returns the first corresponding part staff found, or muse::nidx
    engraving::Measure *mnx_measure_to_measure(const size_t mnx_meas_idx);
    /// #mnxMeasureToMeasure for indices that may legitimately not resolve, such as one past
    /// the end of the score. Returns nullptr instead of throwing, so the caller can warn and
    /// carry on rather than aborting the import.
    engraving::Measure *try_mnx_measure_to_measure(const size_t mnx_meas_idx);
    engraving::ChordRest *mnx_event_id_to_cr(const std::string &event_id);
    engraving::Note *mnx_note_id_to_note(const std::string &note_id);
    static void set_and_style_property(engraving::EngravingObject *e, engraving::Pid id, engraving::PropertyValue v);
    engraving::Fraction mnx_measure_pos_to_tick(const mnx::MeasureRhythmicPosition &meas_pos);
    engraving::staff_idx_t resolve_dynamic_staff(const mnx::Part &mnx_part, const mnx::part::DynamicGroupBase &mnx_dynamic);
    std::optional<engraving::track_idx_t> resolve_voice_track(const mnx::part::Measure &mnx_measure, engraving::staff_idx_t staff_idx,
                                                              const std::string &mnx_voice_id);
    void apply_dynamic_orient(engraving::EngravingItem *item, const mnx::Part &part, mnx::MultiStaffOrientation orient);
    void create_hairpin(const mnx::part::DynamicGradual &mnx_hairpin, engraving::Segment *segment, const mnx::Part &mnx_part,
                        engraving::track_idx_t cur_track_idx, bool use_voice_assignment);

    // ordered map avoids need for hash on std::pair
    std::map<std::pair<size_t, int>, engraving::staff_idx_t> mnx_part_staff_to_staff_;
    std::unordered_map<engraving::staff_idx_t, size_t> staff_to_mnx_part_;
    std::unordered_map<size_t, engraving::Fraction> mnx_meas_to_tick_;
    std::unordered_map<engraving::staff_idx_t, std::unordered_map<std::string, std::pair<engraving::Fraction, engraving::Fraction>>>
        lyric_line_usage_;
    std::unordered_map<engraving::staff_idx_t, std::unordered_map<std::string, int>> lyric_line_to_verse_;
    std::unordered_map<std::string, std::unordered_map<std::string, engraving::track_idx_t>> part_measure_sequence_tracks_;

    struct GroupBarlineOverrideSpan {
        engraving::staff_idx_t start_staff{};
        engraving::staff_idx_t end_staff{};
        mnx::StaffGroupBarlineOverride barline_override{};
    };

    std::vector<GroupBarlineOverrideSpan> group_barline_overrides_;
    // event tracking
    std::unordered_map<std::string, engraving::ChordRest *> mnx_event_to_cr_; // key is json_pointer, since event.id() is optional.
    std::unordered_map<std::string, engraving::Note *> mnx_note_to_note_;     // key is json_pointer, since event.id() is optional.
    std::map<std::pair<size_t, std::string>, int> mnx_kit_component_to_midi_;

    bool use_beams_{};              // if true, only events in mnx beams arrays should be beamed.
    bool use_accidental_display_{}; // if true, all accidentals are force hide or show.

    engraving::Score *score_{};
    mnx::Document mnx_document_;
};
} // namespace mu::iex::mnxio
