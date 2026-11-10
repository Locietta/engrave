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
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "engraving/types/propertyvalue.h"
#include "engraving/types/types.h"
#include "model/document.h"
#include "model/layout.h"

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
    // `doc` must come from model::load_document, so every lookup below succeeds.
    MnxImporter(engraving::Score *s, model::Document &&doc) : score_(s), document_(std::move(doc)) {}

    void import_mnx();

    const model::Document &document() const { return document_; }

    engraving::Score *score() const { return score_; }

private:
    using GraceNeighborsMap = std::unordered_map<const model::Grace *, std::pair<engraving::ChordRest *, engraving::ChordRest *>>;

    const model::Root &root() const { return document_.root; }

    const model::Index &index() const { return document_.index; }

    // settings
    void import_settings();

    // parts
    void import_parts();
    void create_staff(engraving::Part *part, size_t part_index, int staff_num);

    // brackets and barlines
    void import_brackets();

    // global measures
    void import_global_measures();
    void build_lyric_line_verse_map();
    void create_key_sig(engraving::Measure *measure, int key_fifths); // positive = sharps; negative = flats
    void create_time_sig(engraving::Measure *measure, const model::Time &time_sig);
    void set_barline(engraving::Measure *measure, const model::Barline &barline);
    void create_volta(engraving::Measure *measure, const model::Ending &ending);
    // MuseScore cannot place jumps and markers within a measure, so their MNX location is unused.
    void create_jump_or_marker(engraving::Measure *measure, std::variant<engraving::JumpType, engraving::MarkerType> type,
                               const std::optional<std::string> &glyph_name = std::nullopt);
    void create_tempo_mark(engraving::Measure *measure, const model::Tempo &tempo);

    // part measures
    void import_part_measures();
    void import_sequences(size_t part_index, size_t measure_index, engraving::Measure *measure);
    // `bar_length` is the length of the measure, which a full-measure rest takes.
    bool import_non_grace_events(const model::Sequence &sequence, const model::Rational &bar_length, engraving::Measure *measure,
                                 engraving::track_idx_t cur_track_idx, GraceNeighborsMap &grace_neighbors);
    void update_lyric_line_usage_for_event(const model::Event &event, const model::Sequence &sequence, engraving::Measure *measure,
                                           engraving::track_idx_t cur_track_idx, const model::Rational &start_tick);
    void import_grace_events(const model::Sequence &sequence, const model::Rational &bar_length, engraving::Measure *measure,
                             engraving::track_idx_t cur_track_idx, const GraceNeighborsMap &grace_neighbors);
    engraving::ChordRest *import_event(const model::Event &event, engraving::track_idx_t, engraving::Measure *measure,
                                       const model::Rational &start_tick, const model::Rational &actual_duration,
                                       const std::stack<engraving::Tuplet *> &active_tuplets, engraving::TremoloTwoChord *active_tremolo);
    engraving::Tuplet *create_tuplet(const model::Tuplet &mnx_tuplet, engraving::Measure *measure, engraving::track_idx_t cur_track_idx,
                                     const model::Rational &start_tick);
    void create_tremolo(const model::MultiNoteTremolo &mnx_tremolo, engraving::Measure *measure, engraving::track_idx_t cur_track_idx,
                        const model::Rational &start_tick, const model::Rational &end_tick);
    void process_sequence_pass2(const model::Sequence &sequence, const model::Rational &bar_length, engraving::Measure *measure);
    void create_slur(const model::Slur &mnx_slur, engraving::ChordRest *start_cr);
    void create_lyrics(const model::Event &mnx_event, engraving::ChordRest *cr);
    void create_ties(const std::vector<model::Tie> &ties, engraving::Note *start_note);
    void create_accidentals(const model::Note &mnx_note, engraving::Note *note, engraving::Measure *measure);
    void import_rest_properties(const model::Rest &mnx_rest, engraving::Rest *rest);
    engraving::Rest *emit_gap_rest(engraving::Measure *measure, engraving::track_idx_t cur_track_idx, const model::Rational &start_tick,
                                   const model::Rational &duration, engraving::Tuplet *tuplet_to_add);
    engraving::Note *create_note(const model::Note &mnx_note, const model::EventEntry &entry, engraving::Chord *chord,
                                 engraving::Staff *base_staff, const engraving::Fraction &tick, engraving::track_idx_t cur_track_idx);
    void create_arpeggios(size_t part_index, const model::PartMeasure &mnx_measure);
    void create_beams(const model::PartMeasure &mnx_measure);
    void create_clefs(size_t part_index, const std::vector<model::PositionedClef> &mnx_clefs, engraving::Measure *measure);
    void create_dynamics(size_t part_index, size_t measure_index, engraving::Measure *measure);
    void create_dynamic(const model::DynamicGroup &mnx_dynamic, engraving::Segment *segment, size_t part_index,
                        engraving::track_idx_t cur_track_idx, bool use_voice_assignment);
    void create_measure_repeats(size_t part_index);
    void create_ottavas(size_t part_index, const model::PartMeasure &mnx_measure, engraving::Measure *measure);

    // markings
    engraving::Articulation *add_articulation(engraving::ChordRest *cr, model::Orientation orient, engraving::SymId sym_id,
                                              const char *name);
    void add_fermata(engraving::Segment *seg, const model::Fermata &mnx_fermata, engraving::track_idx_t track);
    void import_markings(const model::Event &mnx_event, engraving::ChordRest *cr, const engraving::Fraction &event_end_tick,
                         engraving::Measure *measure = nullptr);
    void import_bow_direction(const model::BowDirection &bow_direction, engraving::ChordRest *cr);
    void import_breath(const model::BreathMark &breath, engraving::ChordRest *cr, const engraving::Fraction &event_end_tick,
                       engraving::Measure *measure = nullptr);
    void import_strong_accent(const model::StrongAccent &strong_accent, engraving::ChordRest *cr);
    void import_tremolo(const model::TremoloSingle &tremolo, engraving::ChordRest *cr);
    void import_fermata(const model::Fermata &mnx_fermata, engraving::ChordRest *cr);

    // utility funcs
    engraving::staff_idx_t mnx_part_staff_to_staff_idx(size_t part_index, int staff_num);
    // The first part staff the layout staff shows, or muse::nidx.
    engraving::staff_idx_t mnx_layout_staff_to_staff_idx(const model::Staff &mnx_staff);
    engraving::Measure *mnx_measure_to_measure(size_t mnx_meas_idx);
    /// #mnxMeasureToMeasure for indices that may legitimately not resolve, such as one past
    /// the end of the score. Returns nullptr instead of throwing, so the caller can warn and
    /// carry on rather than aborting the import.
    engraving::Measure *try_mnx_measure_to_measure(size_t mnx_meas_idx);
    engraving::ChordRest *mnx_event_id_to_cr(std::string_view event_id);
    engraving::Note *mnx_note_id_to_note(std::string_view note_id);
    static void set_and_style_property(engraving::EngravingObject *e, engraving::Pid id, engraving::PropertyValue v);
    engraving::Fraction mnx_measure_pos_to_tick(const model::MeasureRhythmicPosition &meas_pos);
    engraving::staff_idx_t resolve_dynamic_staff(size_t part_index, const model::DynamicGroup &mnx_dynamic);
    std::optional<engraving::track_idx_t> resolve_voice_track(size_t part_index, size_t measure_index, engraving::staff_idx_t staff_idx,
                                                              const std::string &mnx_voice_id);
    void apply_dynamic_orient(engraving::EngravingItem *item, size_t part_index, model::MultiStaffOrientation orient);
    void create_hairpin(const model::DynamicGroup &mnx_hairpin, engraving::Segment *segment, size_t part_index,
                        engraving::track_idx_t cur_track_idx, bool use_voice_assignment);

    // ordered map avoids need for hash on std::pair
    std::map<std::pair<size_t, int>, engraving::staff_idx_t> mnx_part_staff_to_staff_;
    std::unordered_map<engraving::staff_idx_t, size_t> staff_to_mnx_part_;
    std::unordered_map<size_t, engraving::Fraction> mnx_meas_to_tick_;
    std::unordered_map<engraving::staff_idx_t, std::unordered_map<std::string, std::pair<engraving::Fraction, engraving::Fraction>>>
        lyric_line_usage_;
    std::unordered_map<engraving::staff_idx_t, std::unordered_map<std::string, int>> lyric_line_to_verse_;
    // (part, measure) -> the track each imported sequence went to
    std::map<std::pair<size_t, size_t>, std::unordered_map<const model::Sequence *, engraving::track_idx_t>> part_measure_sequence_tracks_;
    // Each part's transposition with keyFifthsFlipAt replaced by the value MuseScore can represent.
    std::vector<std::optional<model::PartTransposition>> part_transpositions_;

    struct GroupBarlineOverrideSpan {
        engraving::staff_idx_t start_staff{};
        engraving::staff_idx_t end_staff{};
        model::BarlineOverride barline_override{};
    };

    std::vector<GroupBarlineOverrideSpan> group_barline_overrides_;
    // event tracking
    std::unordered_map<const model::Event *, engraving::ChordRest *> mnx_event_to_cr_;
    std::unordered_map<const model::Note *, engraving::Note *> mnx_note_to_note_;
    std::unordered_map<const model::KitNote *, engraving::Note *> mnx_kit_note_to_note_;
    std::map<std::pair<size_t, std::string>, int> mnx_kit_component_to_midi_;

    bool use_beams_{};              // if true, only events in mnx beams arrays should be beamed.
    bool use_accidental_display_{}; // if true, all accidentals are force hide or show.

    engraving::Score *score_{};
    model::Document document_;
};
} // namespace mu::iex::mnxio