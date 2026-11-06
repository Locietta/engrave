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
#include "diveinfoconverter.h"

#include <engraving/dom/note.h>

#include <cmath>

using namespace mu::engraving;

namespace mu::iex::guitarpro {
static std::vector<SegmentData> classify_dive_segments(const PitchValues &pitch_values) {
    enum class Dir { NONE, UP, DOWN };

    std::vector<SegmentData> segments;
    Dir prev_dir = Dir::NONE;
    size_t seg_start_idx = 0;

    for (size_t i = 0; i + 1 < pitch_values.size(); ++i) {
        const int dp = pitch_values[i + 1].pitch - pitch_values[i].pitch;
        if (dp == 0) {
            continue;
        }

        const int qt = gp_pitch_to_quarter_tones(dp);
        if (qt == 0) {
            continue;
        }

        Dir cur_dir = dp > 0 ? Dir::UP : Dir::DOWN;

        if (cur_dir == prev_dir && !segments.empty()) {
            SegmentData &last = segments.back();
            last.quarter_tones = gp_pitch_to_quarter_tones(pitch_values[i + 1].pitch - pitch_values[seg_start_idx].pitch);
            last.end_factor = static_cast<double>(pitch_values[i + 1].time) / PitchValue::max_time;
            continue;
        }

        seg_start_idx = i;
        SegmentData seg;
        seg.quarter_tones = qt;
        seg.start_factor = static_cast<double>(pitch_values[i].time) / PitchValue::max_time;
        seg.end_factor = static_cast<double>(pitch_values[i + 1].time) / PitchValue::max_time;
        segments.push_back(std::move(seg));

        prev_dir = cur_dir;
    }

    return segments;
}

int gp_pitch_to_quarter_tones(int gp_pitch) {
    return static_cast<int>(std::lround(static_cast<float>(gp_pitch) / ::mu::iex::guitarpro::gp_pitch_per_quartertone));
}

ImportedDiveInfo DiveInfoConverter::from_pitch_values(const PitchValues &pitch_values, bool is_continued_whammy) {
    ImportedDiveInfo info;

    if (pitch_values.size() < 2) {
        return info;
    }

    const int origin_pitch = pitch_values.front().pitch;
    const int dest_pitch = pitch_values.back().pitch;
    const int origin_quarter_tones = gp_pitch_to_quarter_tones(origin_pitch);

    std::vector<SegmentData> chain = classify_dive_segments(pitch_values);

    if (is_continued_whammy && dest_pitch == origin_pitch) {
        return info;
    } else if (!is_continued_whammy && origin_pitch != 0 && dest_pitch == origin_pitch) {
        info.type = DiveType::PRE_DIVE;
        info.quarter_tones = -gp_pitch_to_quarter_tones(origin_pitch);
        info.end_factor = 1.0;
        return info;
    } else if (!is_continued_whammy && origin_pitch != 0) {
        info.type = DiveType::PRE_DIVE;
        info.quarter_tones = -origin_quarter_tones;
        info.end_factor = 1.0;
        info.grace_dive_segments = std::move(chain);
        return info;
    } else if (chain.empty()) {
        return info;
    }

    info.type = DiveType::DIVE;
    info.grace_dive_segments = std::move(chain);

    if (info.grace_dive_segments.size() == 1) {
        SegmentData &only = info.grace_dive_segments.front();
        only.quarter_tones = gp_pitch_to_quarter_tones(dest_pitch - origin_pitch);
        only.start_factor = static_cast<double>(pitch_values.front().time) / PitchValue::max_time;
        only.end_factor = static_cast<double>(pitch_values.back().time) / PitchValue::max_time;
    }

    return info;
}

ImportedDiveInfo DiveInfoConverter::fill_dive_info(const Note *note, const PitchValues &pitch_values, bool is_continued_whammy) {
    if (!note || pitch_values.size() < 2) {
        return {};
    }

    ImportedDiveInfo info = from_pitch_values(pitch_values, is_continued_whammy);

    if (info.type == DiveType::NONE) {
        return info;
    }

    if (info.type == DiveType::DIVE && info.grace_dive_segments.empty()) {
        info.type = DiveType::NONE;
        return info;
    }

    info.note = note;
    return info;
}
} // namespace mu::iex::guitarpro
