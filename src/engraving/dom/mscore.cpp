/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "mscore.h"

#include "figuredbass.h"
#include "hairpin.h"
#include "ottava.h"
#include "score.h"
#include "spanner.h"
#include "volta.h"

namespace mu::engraving {
bool MScore::debug_mode = false;
bool MScore::test_mode = false;
bool MScore::test_write_style_to_score = true;

bool MScore::no_horizontal_stretch = false;
bool MScore::no_vertical_stretch = false;
bool MScore::use_fallback_font = true;

bool MScore::save_template_mode = false;
bool MScore::no_gui = false;

int MScore::v_raster_value;
int MScore::h_raster_value;
bool MScore::vertical_orientation_value = false;
double MScore::vertical_page_gap = 0.35 * ::mu::engraving::dpmm;
double MScore::horizontal_page_gap_even = 0.07 * ::mu::engraving::dpmm;
double MScore::horizontal_page_gap_odd = 3.5 * ::mu::engraving::dpmm;

bool MScore::warn_pitch_range;
bool MScore::warn_guitar_bends;
int MScore::pedal_events_min_ticks;

double MScore::nudge_step;
double MScore::nudge_step10;
double MScore::nudge_step50;

bool MScore::no_images = false;
bool MScore::pdf_printing = false;
bool MScore::svg_printing = false;

extern void init_drumset();

MsError MScore::error{MsError::MS_NO_ERROR};
bool MScore::error_is_warning = false;

void MScore::register_ui_types() {}

std::string MScore::error_to_string(MsError err) {
    switch (err) {
        case MsError::MS_NO_ERROR: return "MS_NO_ERROR";
        case MsError::NO_NOTE_SELECTED: return "NO_NOTE_SELECTED";
        case MsError::NO_CHORD_REST_SELECTED: return "NO_CHORD_REST_SELECTED";
        case MsError::NO_LYRICS_SELECTED: return "NO_LYRICS_SELECTED";
        case MsError::NO_NOTE_REST_SELECTED: return "NO_NOTE_REST_SELECTED";
        case MsError::NO_FLIPPABLE_SELECTED: return "NO_FLIPPABLE_SELECTED";
        case MsError::NO_MEASURE_SELECTED: return "NO_MEASURE_SELECTED";
        case MsError::NO_STAFF_SELECTED: return "NO_STAFF_SELECTED";
        case MsError::NO_NOTE_FIGUREDBASS_SELECTED: return "NO_NOTE_FIGUREDBASS_SELECTED";
        case MsError::NO_NOTE_REST_HARMONY_SELECTED: return "NO_NOTE_REST_HARMONY_SELECTED";
        case MsError::CANNOT_INSERT_TUPLET: return "CANNOT_INSERT_TUPLET";
        case MsError::CANNOT_SPLIT_TUPLET: return "CANNOT_SPLIT_TUPLET";
        case MsError::CANNOT_SPLIT_MEASURE_FIRST_BEAT: return "CANNOT_SPLIT_MEASURE_FIRST_BEAT";
        case MsError::CANNOT_SPLIT_MEASURE_TUPLET: return "CANNOT_SPLIT_MEASURE_TUPLET";
        case MsError::INSUFFICIENT_MEASURES: return "INSUFFICIENT_MEASURES";
        case MsError::CANNOT_SPLIT_MEASURE_REPEAT: return "CANNOT_SPLIT_MEASURE_REPEAT";
        case MsError::CANNOT_SPLIT_MEASURE_TOO_SHORT: return "CANNOT_SPLIT_MEASURE_TOO_SHORT";
        case MsError::CANNOT_REMOVE_TIME_TUPLET: return "CANNOT_REMOVE_TIME_TUPLET";
        case MsError::CANNOT_REMOVE_TIME_MEASURE_REPEAT: return "CANNOT_REMOVE_TIME_MEASURE_REPEAT";
        case MsError::NO_DEST: return "NO_DEST";
        case MsError::SOURCE_PARTIAL_TUPLET: return "SOURCE_PARTIAL_TUPLET";
        case MsError::DEST_TUPLET: return "DEST_TUPLET";
        case MsError::TUPLET_CROSSES_BAR: return "TUPLET_CROSSES_BAR";
        case MsError::DEST_LOCAL_TIME_SIGNATURE: return "DEST_LOCAL_TIME_SIGNATURE";
        case MsError::SOURCE_PARTIAL_TREMOLO: return "SOURCE_PARTIAL_TREMOLO";
        case MsError::DEST_TREMOLO: return "DEST_TREMOLO";
        case MsError::NO_MIME: return "NO_MIME";
        case MsError::DEST_NO_CR: return "DEST_NO_CR";
        case MsError::CANNOT_CHANGE_LOCAL_TIMESIG_MEASURE_NOT_EMPTY: return "CANNOT_CHANGE_LOCAL_TIMESIG_MEASURE_NOT_EMPTY";
        case MsError::CANNOT_CHANGE_LOCAL_TIMESIG_HAS_EXCERPTS: return "CANNOT_CHANGE_LOCAL_TIMESIG_HAS_EXCERPTS";
        case MsError::CORRUPTED_MEASURE: return "CORRUPTED_MEASURE";
        case MsError::CANNOT_REMOVE_KEY_SIG: return "CANNOT_REMOVE_KEY_SIG";
        case MsError::CANNOT_JOIN_MEASURE_STAFFTYPE_CHANGE: return "CANNOT_JOIN_MEASURE_STAFFTYPE_CHANGE";
        case MsError::CANNOT_REPEAT_SELECTION: return "CANNOT_REPEAT_SELECTION";
        case MsError::TRANSPOSE_NO_FRET_DIAGRAM: return "TRANSPOSE_NO_FRET_DIAGRAM";
        case MsError::CANNOT_EXPLODE_IMPLODE_LOCAL_TIMESIG: return "CANNOT_EXPLODE_IMPLODE_LOCAL_TIMESIG";
    }

    return {};
}
} // namespace mu::engraving
