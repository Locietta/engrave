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

#pragma once

#include "global/containers.h"

#include "engraving/types/types.h"

namespace mu::engraving {
static constexpr size_t voices = 4;

inline constexpr track_idx_t staff2track(staff_idx_t staff_idx, voice_idx_t voice_idx = 0) {
    return staff_idx != muse::nidx ? staff_idx * ::mu::engraving::voices + voice_idx : muse::nidx;
}

inline constexpr staff_idx_t track2staff(track_idx_t track) { return track != muse::nidx ? track / ::mu::engraving::voices : muse::nidx; }

inline constexpr voice_idx_t track2voice(track_idx_t track) { return track != muse::nidx ? track % ::mu::engraving::voices : muse::nidx; }

inline constexpr track_idx_t track_zero_voice(track_idx_t track) {
    return track != muse::nidx ? (track / ::mu::engraving::voices) * ::mu::engraving::voices : muse::nidx;
}

inline constexpr bool is_up_voice(voice_idx_t voice_idx) { return !(voice_idx & 1); }

inline constexpr bool is_down_voice(voice_idx_t voice_idx) { return voice_idx & 1; }

static constexpr int max_headers = 3;
static constexpr int max_footers = 3;

// UNIVERSAL UNITS
static constexpr double inch = 25.4; // millimitres per inch
static constexpr double ppi = 72.0;  // typographical points per inch

// INTERNAL DRAWING UNITS
static constexpr double dpi = 1200;
static constexpr double dpmm = ::mu::engraving::dpi / ::mu::engraving::inch;

static constexpr double ui_icons_default_font_size = 12.0;

static constexpr int max_staves = 4;

static constexpr char mime_symbol_format[] = "application/musescore/symbol";
static constexpr char mime_symbol_list_format[] = "application/musescore/symbollist";
static constexpr char mime_staff_list_format[] = "application/musescore/stafflist";

static constexpr int invalid_string_index = -1; // no ordinal for a physical string (0 = topmost in instrument)
static constexpr int invalid_fret_index = -1;   // no ordinal for a fret

static constexpr muse::ID invalid_id = 0;

//---------------------------------------------------------
//   TransposeDirection
//---------------------------------------------------------

enum class TransposeDirection : char { UP, DOWN, CLOSEST, UNKNOWN };

//---------------------------------------------------------
//   TransposeMode
//---------------------------------------------------------

enum class TransposeMode : char { TO_KEY, BY_INTERVAL, DIATONICALLY, UNKNOWN };

//---------------------------------------------------------
//   SelectType
//---------------------------------------------------------

enum class SelectType : char { SINGLE, RANGE, ADD, REPLACE };

//---------------------------------------------------------
//   OffsetType
//---------------------------------------------------------

enum class OffsetType : unsigned char {
    ABS,    ///< offset in point units
    SPATIUM ///< offset in staff space units
};

//---------------------------------------------------------
//   MScoreError
//---------------------------------------------------------

enum class MsError : unsigned char {
    MS_NO_ERROR,
    NO_NOTE_SELECTED,
    NO_CHORD_REST_SELECTED,
    NO_LYRICS_SELECTED,
    NO_NOTE_REST_SELECTED,
    NO_FLIPPABLE_SELECTED,
    NO_MEASURE_SELECTED,
    NO_STAFF_SELECTED,
    NO_NOTE_FIGUREDBASS_SELECTED,
    NO_NOTE_REST_HARMONY_SELECTED,
    CANNOT_INSERT_TUPLET,
    CANNOT_SPLIT_TUPLET,
    CANNOT_SPLIT_MEASURE_FIRST_BEAT,
    CANNOT_SPLIT_MEASURE_TUPLET,
    INSUFFICIENT_MEASURES,
    CANNOT_SPLIT_MEASURE_REPEAT,
    CANNOT_SPLIT_MEASURE_TOO_SHORT,
    CANNOT_REMOVE_TIME_TUPLET,
    CANNOT_REMOVE_TIME_MEASURE_REPEAT,
    NO_DEST,
    SOURCE_PARTIAL_TUPLET,
    DEST_TUPLET,
    TUPLET_CROSSES_BAR,
    DEST_LOCAL_TIME_SIGNATURE,
    SOURCE_PARTIAL_TREMOLO,
    DEST_TREMOLO,
    NO_MIME,
    DEST_NO_CR,
    CANNOT_CHANGE_LOCAL_TIMESIG_MEASURE_NOT_EMPTY,
    CANNOT_CHANGE_LOCAL_TIMESIG_HAS_EXCERPTS,
    CORRUPTED_MEASURE,
    CANNOT_REMOVE_KEY_SIG,
    CANNOT_JOIN_MEASURE_STAFFTYPE_CHANGE,
    CANNOT_REPEAT_SELECTION,
    TRANSPOSE_NO_FRET_DIAGRAM,
    CANNOT_EXPLODE_IMPLODE_LOCAL_TIMESIG,
};

/// \cond PLUGIN_API \private \endcond
struct MScoreError {
    MsError no;
    const char *group;
    const char *txt;
};

//---------------------------------------------------------
//   MScore
//    MuseScore application object
//---------------------------------------------------------

class MScore {
    static int h_raster_value, v_raster_value;
    static bool vertical_orientation_value;

public:

    static MsError error;
    static bool error_is_warning;

    static void register_ui_types();

    static double h_raster() { return h_raster_value; }

    static double v_raster() { return v_raster_value; }

    static void set_h_raster(int val) { h_raster_value = val; }

    static void set_v_raster(int val) { v_raster_value = val; }

    static void set_nudge_step(double val) { nudge_step = val; }

    static void set_nudge_step10(double val) { nudge_step10 = val; }

    static void set_nudge_step50(double val) { nudge_step50 = val; }

    static bool vertical_orientation() { return vertical_orientation_value; }

    static void set_vertical_orientation(bool val) { vertical_orientation_value = val; }

    static bool warn_pitch_range;
    static bool warn_guitar_bends;
    static int pedal_events_min_ticks;

    static double nudge_step;
    static double nudge_step10;
    static double nudge_step50;

    // #ifndef NDEBUG
    static bool no_horizontal_stretch;
    static bool no_vertical_stretch;
    static bool use_fallback_font;
    // #endif
    static bool debug_mode;
    static bool test_mode;
    static bool test_write_style_to_score;

    static bool save_template_mode;
    static bool no_gui;

    static bool no_images;

    static bool pdf_printing;
    static bool svg_printing;

    static double vertical_page_gap;
    static double horizontal_page_gap_even;
    static double horizontal_page_gap_odd;

    static void set_error(MsError e, bool warning = false) {
        error = e;
        error_is_warning = warning;
    }

    static std::string error_to_string(MsError err);
};
} // namespace mu::engraving
