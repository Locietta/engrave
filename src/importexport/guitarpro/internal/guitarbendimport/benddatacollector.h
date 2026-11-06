/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "guitarbendimporttypes.h"
#include "splitchord/benddatacollectorsplitchord.h"
#include <engraving/types/pitchvalue.h>
#include <engraving/types/types.h>

namespace mu::engraving {
struct Note;
struct Chord;
struct Score;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
struct BendDataCollector {

    BendDataCollector(mu::engraving::Score *score);
    void store_bend_data(const mu::engraving::Note *note, const mu::engraving::PitchValues &pitch_values);
    BendDataContext collect_bend_data_context();

private:
    mu::engraving::Score *score_ = nullptr;
    std::unordered_map<mu::engraving::track_idx_t,
                       std::map<mu::engraving::Fraction, std::unordered_map<const mu::engraving::Note *, ImportedBendInfo>>>
        bend_info_for_note_;

    std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, tied_chords_bend_data_chunk_t>>
        regrouped_data_by_tied_chords_;

    std::unique_ptr<BendDataCollectorSplitChord> bend_data_collector_split_chord_;

    // converts bendInfoForNote_ to regroupedData_, leaves bendInfoForNote_ empty
    // bendInfoForNote_ is needed to store separate bend data for each note, while regroupedData_ stores in format, comfortoble for import
    void regroup_bend_data_by_tied_chords();
    void fill_bend_data_context(BendDataContext &bend_data_ctx);

    // if first chord in chunk has more segments than tied notes, move segments forward to other notes
    void move_segments_to_tied_notes(tied_chords_bend_data_chunk_t &data_chunk, ImportedBendInfo &data_for_first_note, size_t note_idx);
};
} // namespace mu::iex::guitarpro
