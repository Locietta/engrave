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

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "engraving/types/types.h"
#include "model/mnx_model.h"
#include "types/ret.h"

namespace mu::engraving {
struct Chord;
struct ChordRest;
struct EID;
struct EngravingItem;
struct EngravingObject;
struct Fermata;
struct Hairpin;
struct GraceNotesView;
struct Instrument;
struct Measure;
struct Note;
struct Part;
struct Ottava;
struct Score;
struct Slur;
struct Staff;
struct Volta;
struct TremoloTwoChord;
struct Tuplet;
} // namespace mu::engraving

namespace mu::iex::mnxio {
struct MnxChordTargetPosition {
    model::Fraction fraction;
    std::optional<unsigned> grace_index;
};

struct MnxExporter {
    MnxExporter(engraving::Score *s, bool export_beams = true, bool export_rest_positions = false)
        : score_(s), export_beams_(export_beams), export_rest_positions_(export_rest_positions) {
        root_.mnx.version = model::schema_version;
    }

    muse::Ret export_mnx();

    // The exported document; model::write_document serializes it.
    const model::Root &document() const { return root_; }

    const std::unordered_map<engraving::staff_idx_t, std::pair<size_t, int>> &staff_to_part_staff() const { return staff_to_part_staff_; }

    // utility functions
    engraving::EID get_or_assign_eid(engraving::EngravingObject *item);
    size_t mnx_measure_index_from_measure(const engraving::Measure *measure) const;
    std::pair<size_t, int> mnx_part_staff_from_staff_idx(engraving::staff_idx_t staff_idx) const;
    static model::Fermata mnx_fermata_from_fermata(const engraving::Fermata *fermata);
    std::optional<MnxChordTargetPosition> mnx_chord_target_position(const engraving::Chord *chord, const engraving::Measure *measure);

private:
    enum class ContentContext { SEQUENCE, GRACE, TUPLET, TREMOLO };

    struct ExportContext {
        ExportContext(const engraving::Part *part_in, const engraving::Measure *measure_in, model::PartMeasure &mnx_measure_in,
                      engraving::staff_idx_t staff_idx_in, engraving::voice_idx_t voice_in, int mnx_part_staff_in)
            : part(part_in), measure(measure_in), mnx_measure(mnx_measure_in), staff_idx(staff_idx_in), voice(voice_in),
              mnx_part_staff(mnx_part_staff_in) {}

        const engraving::Part *part{};
        const engraving::Measure *measure{};
        model::PartMeasure &mnx_measure;
        engraving::staff_idx_t staff_idx{};
        engraving::voice_idx_t voice{};
        int mnx_part_staff{}; // 1-based staff number within part.
        std::vector<const engraving::Tuplet *> tuplet_stack;
        std::unordered_set<const engraving::ChordRest *> grace_before_emitted;
        std::unordered_set<const engraving::ChordRest *> grace_after_emitted;
    };

    void create_global();
    bool create_parts();
    void create_layout(const std::vector<engraving::Staff *> &staves, const std::string &layout_id);
    void create_sequences(const engraving::Part *part, const engraving::Measure *measure, model::PartMeasure &mnx_measure);
    void create_arpeggios(const engraving::Part *part, const engraving::Measure *measure, model::PartMeasure &mnx_measure);
    void create_volta(const engraving::Volta *volta);
    void create_slur(const engraving::Slur *slur);
    void create_hairpin(const engraving::Hairpin *hairpin);
    void create_ottava(const engraving::Ottava *ottava);
    // `Content` is a sequence or tuplet content list (std::vector<model::SequenceItem>) or the
    // event list of a grace note or tremolo (std::vector<model::Event>).
    template <class Content>
    void append_content(Content &content, ExportContext &ctx, const std::vector<engraving::ChordRest *> &chord_rests,
                        ContentContext context);
    void append_grace(std::vector<model::SequenceItem> &content, ExportContext &ctx, engraving::GraceNotesView grace_notes);
    void create_beam(ExportContext &ctx, engraving::ChordRest *chord_rest);
    // These return the index of the last chord/rest they consumed, or nothing when the container
    // cannot be exported (the chord/rest is then exported on its own).
    std::optional<size_t> append_tuplet(std::vector<model::SequenceItem> &content, ExportContext &ctx,
                                        const std::vector<engraving::ChordRest *> &chord_rests, size_t idx,
                                        engraving::ChordRest *chord_rest, const engraving::Tuplet *tuplet);
    std::optional<size_t> append_tremolo(std::vector<model::SequenceItem> &content, ExportContext &ctx,
                                         const std::vector<engraving::ChordRest *> &chord_rests, size_t idx,
                                         engraving::ChordRest *chord_rest);
    template <class Content>
    bool append_event(Content &content, ExportContext &ctx, engraving::ChordRest *chord_rest);
    bool create_rest(model::Event &mnx_event, engraving::ChordRest *chord_rest);
    bool create_notes(model::Event &mnx_event, engraving::ChordRest *chord_rest);
    void create_ties(std::optional<std::vector<model::Tie>> &mnx_ties, engraving::Note *note);
    const engraving::Tuplet *find_top_tuplet(engraving::ChordRest *chord_rest, const ExportContext &ctx) const;
    void export_drumset_kit(const engraving::Part *part, const engraving::Instrument *instrument, model::Part &mnx_part);
    void create_markings(model::Event &mnx_event, engraving::ChordRest *cr);
    // The exported event of a chord/rest; valid once all content is exported.
    model::Event *mnx_event_from_cr(const engraving::ChordRest *cr);

    void export_spanners();

    engraving::Score *score_{};
    model::Root root_;

    // entity tracking
    std::unordered_map<const engraving::Measure *, size_t> meas_to_mnx_meas_;
    std::unordered_map<const engraving::ChordRest *, std::string> cr_to_mnx_event_; // event ids
    ankerl::unordered_dense::map<std::string, model::Event *> events_by_id_;        // built after the content is complete
    std::unordered_map<engraving::staff_idx_t, std::pair<size_t, int>> staff_to_part_staff_;
    std::vector<engraving::Staff *> exported_staves_;
    std::set<int> lyric_verses_; // 1-based verses with lyrics; the lyric line ids, in verse order

    bool export_beams_{true};
    bool export_rest_positions_{false};
};

// Sets an optional property, leaving it absent when the value is its default.
template <class T>
void set_unless_default(std::optional<T> &field, const T &value, const T &default_value) {
    if (value == default_value) {
        field.reset();
    } else {
        field = value;
    }
}

// The value of an optional property, created if absent.
template <class T>
T &ensure(std::optional<T> &field) {
    if (!field) {
        field.emplace();
    }
    return *field;
}
} // namespace mu::iex::mnxio