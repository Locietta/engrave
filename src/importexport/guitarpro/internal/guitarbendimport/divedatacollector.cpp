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
#include "divedatacollector.h"

#include <engraving/dom/chord.h>
#include <engraving/dom/note.h>

#include "diveinfoconverter.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
void DiveDataCollector::collect_dive_data(const Chord *chord, const PitchValues &pitch_values) {
    if (pitch_values.size() < 2) {
        return;
    }

    const track_idx_t track = chord->track();
    const Fraction tick = chord->tick();

    bool has_preceding_whammy = false;
    if (auto it = last_whammy_by_track_.find(track); it != last_whammy_by_track_.end()) {
        const auto &prev = it->second;
        has_preceding_whammy = (prev.dest_pitch == pitch_values.front().pitch) && (prev.end_tick == tick);
    }
    last_whammy_by_track_[track] = {pitch_values.back().pitch, tick + chord->actual_ticks()};

    for (const Note *note : chord->notes()) {
        if (note->display_fret() == Note::DisplayFretOption::HIDE) {
            continue;
        }
        const bool is_continued_whammy = has_preceding_whammy && pitch_values.front().pitch != 0 && note->tie_back();
        ImportedDiveInfo info = DiveInfoConverter::fill_dive_info(note, pitch_values, is_continued_whammy);
        if (info.type == DiveType::NONE || !info.note) {
            continue;
        }

        const size_t note_idx = muse::index_of(chord->notes(), note);

        const bool is_compound_pre_dive = (info.type == DiveType::PRE_DIVE);

        if (is_compound_pre_dive) {
            ctx_.pre_dive_data[track][tick][note_idx].quarter_tones = info.quarter_tones;
            if (info.grace_dive_segments.empty()) {
                continue;
            }
            // Compound PRE_DIVE: route grace segments through the same path as DIVE
        }

        if (info.grace_dive_segments.size() == 1 && note->tie_for()) {
            ctx_.tied_notes_dives_data[track][tick][note_idx] = std::move(info.grace_dive_segments.front());
            continue;
        }

        GraceNotesImportInfo &grace_info = ctx_.grace_after_dive_data[track][tick][note_idx];

        if (note->tie_for() && !info.grace_dive_segments.empty()) {
            grace_info.last_note_data.should_move_tie = true;
            grace_info.last_note_data.end_factor = info.grace_dive_segments.back().end_factor;
            if (info.grace_dive_segments.size() > 1) {
                info.grace_dive_segments.pop_back();
            }
        }

        grace_info.data = std::move(info.grace_dive_segments);
    }
}
} // namespace mu::iex::guitarpro
