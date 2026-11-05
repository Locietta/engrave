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

#include <engraving/types/pitchvalue.h>
#include <engraving/types/types.h>

#include "guitarbendimporttypes.h"
#include "splitchord/benddatacollectorsplitchord.h"

namespace mu::engraving {
class Note;
class Chord;
class Score;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
class BendDataCollector {
public:

    BendDataCollector(mu::engraving::Score *score);
    void storeBendData(const mu::engraving::Note *note, const mu::engraving::PitchValues &pitchValues);
    BendDataContext collectBendDataContext();

private:
    mu::engraving::Score *score_ = nullptr;
    std::unordered_map<mu::engraving::track_idx_t,
                       std::map<mu::engraving::Fraction, std::unordered_map<const mu::engraving::Note *, ImportedBendInfo>>>
        bendInfoForNote_;

    std::unordered_map<mu::engraving::track_idx_t, std::map<mu::engraving::Fraction, tied_chords_bend_data_chunk_t>>
        regroupedDataByTiedChords_;

    std::unique_ptr<BendDataCollectorSplitChord> bendDataCollectorSplitChord_;

    // converts bendInfoForNote_ to regroupedData_, leaves bendInfoForNote_ empty
    // bendInfoForNote_ is needed to store separate bend data for each note, while regroupedData_ stores in format, comfortoble for import
    void regroupBendDataByTiedChords();
    void fillBendDataContext(BendDataContext &bendDataCtx);

    // if first chord in chunk has more segments than tied notes, move segments forward to other notes
    void moveSegmentsToTiedNotes(tied_chords_bend_data_chunk_t &dataChunk, ImportedBendInfo &dataForFirstNote, size_t noteIdx);
};
} // namespace mu::iex::guitarpro
