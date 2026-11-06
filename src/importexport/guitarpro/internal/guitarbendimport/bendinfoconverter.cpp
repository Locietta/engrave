/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#include "bendinfoconverter.h"

#include <engraving/dom/note.h>

using namespace mu::engraving;

namespace mu::iex::guitarpro {
ImportedBendInfo BendInfoConverter::fill_bend_info(const Note *note, const PitchValues &pitch_values) {
    enum class PitchValuesDiff {
        NONE,
        SAME_PITCH_AND_TIME,
        SAME_PITCH_DIFF_TIME,
        INCREASED_PITCH_SAME_TIME,
        INCREASED_PITCH_DIFF_TIME,
        DECREASED_PITCH_DIFF_TIME
    };

    using pvd = PitchValuesDiff;

    ImportedBendInfo imported_info;
    PitchValuesDiff current_pitch_diff;
    PitchValuesDiff previous_pitch_diff = PitchValuesDiff::NONE;

    auto pitch_diff = [](const PitchValue &prev, const PitchValue &cur) {
        if (prev.pitch == cur.pitch) {
            return prev.time == cur.time ? pvd::SAME_PITCH_AND_TIME : pvd::SAME_PITCH_DIFF_TIME;
        }

        if (prev.time == cur.time) {
            return pvd::INCREASED_PITCH_SAME_TIME;
        }

        return prev.pitch < cur.pitch ? pvd::INCREASED_PITCH_DIFF_TIME : pvd::DECREASED_PITCH_DIFF_TIME;
    };

    auto add_prebend_or_hold = [&imported_info](const PitchValue &start) {
        imported_info.time_offset_from_start = start.time;
        imported_info.pitch_offset_from_start = start.pitch;
        if (start.pitch > 0) {
            imported_info.type = BendType::PREBEND;
        }
    };

    for (size_t i = 0; i < pitch_values.size() - 1; i++) {
        current_pitch_diff = pitch_diff(pitch_values[i], pitch_values[i + 1]);
        if (current_pitch_diff == pvd::SAME_PITCH_AND_TIME) {
            continue;
        }

        if (current_pitch_diff == previous_pitch_diff && !imported_info.segments.empty()) {
            BendSegment &last_segment = imported_info.segments.back();
            last_segment.middle_time = pitch_values[i + 1].time; // there is no "hold" in bend, cause both segments go same direction
            last_segment.end_time = pitch_values[i + 1].time;
            last_segment.end_pitch = pitch_values[i + 1].pitch;
            continue;
        }

        switch (current_pitch_diff) {
            case pvd::NONE:
            case pvd::SAME_PITCH_AND_TIME: break;

            case pvd::SAME_PITCH_DIFF_TIME:
            case pvd::INCREASED_PITCH_SAME_TIME: {
                if (imported_info.segments.empty()) {
                    add_prebend_or_hold(pitch_values[i]);
                } else {
                    BendSegment &last_segment = imported_info.segments.back();
                    last_segment.middle_time = last_segment.end_time;
                    last_segment.end_time = pitch_values[i + 1].time;
                    last_segment.end_pitch = pitch_values[i + 1].pitch;
                }

                break;
            }

            case pvd::INCREASED_PITCH_DIFF_TIME:
            case pvd::DECREASED_PITCH_DIFF_TIME: {
                if (imported_info.segments.empty()) {
                    add_prebend_or_hold(pitch_values[i]);
                }

                BendSegment new_segment;
                new_segment.start_pitch = pitch_values[i].pitch;
                new_segment.end_pitch = pitch_values[i + 1].pitch;
                new_segment.start_time = pitch_values[i].time;
                new_segment.middle_time = pitch_values[i + 1].time;
                new_segment.end_time = pitch_values[i + 1].time;
                imported_info.segments.push_back(new_segment);
                break;
            }
        }

        previous_pitch_diff = current_pitch_diff;
    }

    imported_info.note = note;
    if (!imported_info.segments.empty()) {
        imported_info.segments.back().end_time = PitchValue::max_time;
    }

    for (auto &seg : imported_info.segments) {
        if (seg.end_time - seg.middle_time == 1) {
            seg.middle_time = seg.end_time;
        }
    }

    if (imported_info.segments.size() == 1 && imported_info.type == BendType::NORMAL_BEND &&
        imported_info.segments.front().pitch_diff() == ::mu::iex::guitarpro::gp_pitch_per_quartertone) {
        imported_info.type = BendType::SLIGHT_BEND;
    }

    return imported_info;
}
} // namespace mu::iex::guitarpro
