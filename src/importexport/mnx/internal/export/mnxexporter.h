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

#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef MNXDOM_SYSTEM
#include <mnxdom/mnxdom.h>
#else
#include "mnxdom.h"
#endif
#include "types/ret.h"

#include "engraving/types/types.h"

namespace mu::engraving {
class Chord;
class ChordRest;
class EID;
class EngravingItem;
class EngravingObject;
class Fermata;
class Hairpin;
class GraceNotesGroup;
class Instrument;
class Measure;
class Note;
class Part;
class Ottava;
class Score;
class Slur;
class Staff;
class Volta;
class TremoloTwoChord;
class Tuplet;
} // namespace mu::engraving

namespace mu::iex::mnxio {
struct MnxChordTargetPosition {
    mnx::FractionValue fraction;
    std::optional<unsigned> grace_index;
};

class MnxExporter {
public:
    MnxExporter(engraving::Score *s, bool export_beams = true, bool export_rest_positions = false)
        : score_(s), export_beams_(export_beams), export_rest_positions_(export_rest_positions) {}

    muse::Ret export_mnx();

    const mnx::Document &mnx_document() const { return mnx_document_; }

    const std::unordered_map<engraving::staff_idx_t, std::pair<size_t, int>> &staff_to_part_staff() const { return staff_to_part_staff_; }

    // utility functions
    engraving::EID get_or_assign_eid(engraving::EngravingObject *item);
    std::optional<mnx::sequence::Event> mnx_event_from_cr(const engraving::ChordRest *cr);
    size_t mnx_measure_index_from_measure(const engraving::Measure *measure) const;
    std::pair<size_t, int> mnx_part_staff_from_staff_idx(engraving::staff_idx_t staff_idx) const;
    static mnx::Fermata mnx_fermata_from_fermata(const engraving::Fermata *fermata);
    std::optional<MnxChordTargetPosition> mnx_chord_target_position(const engraving::Chord *chord, const engraving::Measure *measure);

private:
    enum class ContentContext { SEQUENCE, GRACE, TUPLET, TREMOLO };

    struct ExportContext {
        ExportContext(const engraving::Part *part_in, const engraving::Measure *measure_in, mnx::part::Measure mnx_measure_in,
                      engraving::staff_idx_t staff_idx_in, engraving::voice_idx_t voice_in, int mnx_part_staff_in)
            : part(part_in), measure(measure_in), mnx_measure(mnx_measure_in), staff_idx(staff_idx_in), voice(voice_in),
              mnx_part_staff(mnx_part_staff_in) {}

        const engraving::Part *part{};
        const engraving::Measure *measure{};
        mnx::part::Measure mnx_measure;
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
    void create_sequences(const engraving::Part *part, const engraving::Measure *measure, mnx::part::Measure &mnx_measure);
    void create_arpeggios(const engraving::Part *part, const engraving::Measure *measure, mnx::part::Measure &mnx_measure);
    void create_volta(const engraving::Volta *volta);
    void create_slur(const engraving::Slur *slur);
    void create_hairpin(const engraving::Hairpin *hairpin);
    void create_ottava(const engraving::Ottava *ottava);
    void append_content(mnx::sequence::SequenceContent content, ExportContext &ctx, const std::vector<engraving::ChordRest *> &chord_rests,
                        ContentContext context);
    void append_grace(mnx::sequence::SequenceContent content, ExportContext &ctx, engraving::GraceNotesGroup &grace_notes);
    void create_beam(ExportContext &ctx, engraving::ChordRest *chord_rest);
    size_t append_tuplet(mnx::sequence::SequenceContent, ExportContext &ctx, const std::vector<engraving::ChordRest *> &chord_rests,
                         size_t idx, engraving::ChordRest *chord_rest, const engraving::Tuplet *tuplet);
    size_t append_tremolo(mnx::sequence::SequenceContent content, ExportContext &ctx,
                          const std::vector<engraving::ChordRest *> &chord_rests, size_t idx, engraving::ChordRest *chord_rest);
    bool append_event(mnx::sequence::SequenceContent, ExportContext &ctx, engraving::ChordRest *chord_rest);
    bool create_rest(mnx::sequence::Event &mnx_event, engraving::ChordRest *chord_rest);
    bool create_notes(mnx::sequence::Event &mnx_event, engraving::ChordRest *chord_rest);
    void create_ties(mnx::sequence::NoteBase &mnx_note, engraving::Note *note);
    const engraving::Tuplet *find_top_tuplet(engraving::ChordRest *chord_rest, const ExportContext &ctx) const;
    void export_drumset_kit(const engraving::Part *part, const engraving::Instrument *instrument, mnx::Part &mnx_part);
    void create_markings(mnx::sequence::Event &mnx_event, engraving::ChordRest *cr);

    void export_spanners();

    engraving::Score *score_{};
    mnx::Document mnx_document_;

    // entity tracking
    std::unordered_map<const engraving::Measure *, size_t> meas_to_mnx_meas_;
    std::unordered_map<const engraving::ChordRest *, mnx::json_pointer> cr_to_mnx_event_;
    std::unordered_map<engraving::staff_idx_t, std::pair<size_t, int>> staff_to_part_staff_;
    std::vector<engraving::Staff *> exported_staves_;
    std::set<std::string> lyric_line_ids_; // this could be (ordered) map if we ever support lyric line metadata.

    bool export_beams_{true};
    bool export_rest_positions_{false};
};
} // namespace mu::iex::mnxio
