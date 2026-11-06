/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "reset.h"

#include <vector>

#include "../dom/engravingitem.h"
#include "../dom/layoutbreak.h"
#include "../dom/measure.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/spacer.h"
#include "editsystemlocks.h"
#include "global/containers.h"
#include "log.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving;

//---------------------------------------------------------
//    For use with Score::scanElements.
//    Reset positions and autoplacement for the given
//    element.
//---------------------------------------------------------

static void reset_element_position(EngravingItem *e) {
    if (e->generated()) {
        return;
    }

    e->undo_reset_property(Pid::AUTOPLACE);
    e->undo_reset_property(Pid::OFFSET);
    e->undo_reset_property(Pid::LEADING_SPACE);
    if (e->is_spanner()) {
        e->undo_reset_property(Pid::OFFSET2);
    }
}

static void reset_text_properties(EngravingItem *e) {
    if (e->generated() || !e->is_text_base()) {
        return;
    }

    static const std::vector<Pid> text_style_to_reset{
        Pid::FONT_FACE,      Pid::FONT_SIZE,      Pid::FONT_STYLE,  Pid::SIZE_SPATIUM_DEPENDENT, Pid::FRAME_TYPE,  Pid::TEXT_LINE_SPACING,
        Pid::FRAME_FG_COLOR, Pid::FRAME_BG_COLOR, Pid::FRAME_WIDTH, Pid::FRAME_PADDING,          Pid::FRAME_ROUND, Pid::ALIGN};

    for (Pid pid : text_style_to_reset) {
        // TODO: use undoResetProperty: https://github.com/musescore/MuseScore/issues/16516
        // But for now, we'll use resetPropety since undoResetProperty leads to various problems
        e->reset_property(pid);
    }
}

void Reset::reset_to_default_layout(Transaction &tx, Score *score) {
    TRACEFUNC;

    StyleIdSet dont_reset_these_styles{Sid::LYRICS_PLACEMENT,
                                       Sid::REPEAT_BAR_TIPS,
                                       Sid::START_BARLINE_SINGLE,
                                       Sid::START_BARLINE_MULTIPLE,
                                       Sid::DIVIDER_LEFT,
                                       Sid::DIVIDER_RIGHT_Y,
                                       Sid::USE_STRAIGHT_NOTE_FLAGS,
                                       Sid::MR_NUMBER_SERIES,
                                       Sid::MR_NUMBER_EVERY_X_MEASURES,
                                       Sid::MR_NUMBER_SERIES_WITH_PARENTHESES,
                                       Sid::ONE_MEASURE_REPEAT_SHOW1,
                                       Sid::FOUR_MEASURE_REPEAT_SHOW_EXTENDERS,
                                       Sid::USE_WIDE_BEAMS,
                                       Sid::HAIRPIN_PLACEMENT,
                                       Sid::PEDAL_PLACEMENT,
                                       Sid::TRILL_PLACEMENT,
                                       Sid::VIBRATO_PLACEMENT,
                                       Sid::HARMONY_PLACEMENT,
                                       Sid::ROMAN_NUMERAL_PLACEMENT,
                                       Sid::NASHVILLE_NUMBER_PLACEMENT,
                                       Sid::HARMONY_VOICE_LITERAL,
                                       Sid::HARMONY_VOICING,
                                       Sid::HARMONY_DURATION,
                                       Sid::CAPO_POSITION,
                                       Sid::FRET_NUM_POS,
                                       Sid::FRET_PLACEMENT,
                                       Sid::FRET_STRINGS,
                                       Sid::FRET_FRETS,
                                       Sid::FRET_NUT,
                                       Sid::FRET_ORIENTATION,
                                       Sid::SHOW_PAGE_NUMBER,
                                       Sid::SHOW_PAGE_NUMBER_ONE,
                                       Sid::PAGE_NUMBER_ODD_EVEN,
                                       Sid::SHOW_MEASURE_NUMBER,
                                       Sid::SHOW_MEASURE_NUMBER_ONE,
                                       Sid::MEASURE_NUMBER_INTERVAL,
                                       Sid::MEASURE_NUMBER_SYSTEM,
                                       Sid::MEASURE_NUMBER_PLACEMENT_MODE,
                                       Sid::GEN_CLEF,
                                       Sid::HIDE_TAB_CLEF_AFTER_FIRST,
                                       Sid::GEN_KEYSIG,
                                       Sid::GEN_COURTESY_TIMESIG,
                                       Sid::GEN_COURTESY_KEYSIG,
                                       Sid::GEN_COURTESY_CLEF,
                                       Sid::SWING_RATIO,
                                       Sid::SWING_UNIT,
                                       Sid::CHORD_SYMBOL_SPELLING,
                                       Sid::AUTOMATIC_CAPITALIZATION,
                                       Sid::LOWER_CASE_MINOR_CHORDS,
                                       Sid::LOWER_CASE_BASS_NOTES,
                                       Sid::ALL_CAPS_NOTE_NAMES,
                                       Sid::CHORD_STYLE,
                                       Sid::CHORDS_XML_FILE,
                                       Sid::CHORD_DESCRIPTION_FILE,
                                       Sid::CONCERT_PITCH,
                                       Sid::CREATE_MULTI_MEASURE_RESTS,
                                       Sid::MIN_EMPTY_MEASURES,
                                       Sid::HIDE_EMPTY_STAVES,
                                       Sid::DONT_HIDE_STAVES_IN_FIRST_SYSTEM,
                                       Sid::ENABLE_INDENTATION_ON_FIRST_SYSTEM,
                                       Sid::FIRST_SYSTEM_INDENTATION_VALUE,
                                       Sid::HIDE_INSTRUMENT_NAME_IF_ONE_INSTRUMENT,
                                       Sid::GATE_TIME,
                                       Sid::TENUTO_GATE_TIME,
                                       Sid::STACCATO_GATE_TIME,
                                       Sid::SLUR_GATE_TIME,
                                       Sid::SECTION_PAUSE,
                                       Sid::SHOW_HEADER,
                                       Sid::HEADER_FIRST_PAGE,
                                       Sid::HEADER_ODD_EVEN,
                                       Sid::EVEN_HEADER_L,
                                       Sid::EVEN_HEADER_C,
                                       Sid::EVEN_HEADER_R,
                                       Sid::ODD_HEADER_L,
                                       Sid::ODD_HEADER_C,
                                       Sid::ODD_HEADER_R,
                                       Sid::SHOW_FOOTER,
                                       Sid::FOOTER_FIRST_PAGE,
                                       Sid::FOOTER_ODD_EVEN,
                                       Sid::EVEN_FOOTER_L,
                                       Sid::EVEN_FOOTER_C,
                                       Sid::EVEN_FOOTER_R,
                                       Sid::ODD_FOOTER_L,
                                       Sid::ODD_FOOTER_C,
                                       Sid::ODD_FOOTER_R,
                                       Sid::TUPLET_OUT_OF_STAFF,
                                       Sid::TUPLET_DIRECTION,
                                       Sid::TUPLET_BRACKET_TYPE,
                                       Sid::DYNAMICS_PLACEMENT,
                                       Sid::TEXT_LINE_PLACEMENT,
                                       Sid::HARP_PEDAL_DIAGRAM_PLACEMENT,
                                       Sid::HARP_PEDAL_TEXT_DIAGRAM_PLACEMENT,
                                       Sid::EXPRESSION_PLACEMENT,
                                       Sid::TUPLET_NUMBER_TYPE,
                                       Sid::MM_REST_SHOW_MEASURE_NUMBER_RANGE,
                                       Sid::MM_REST_RANGE_BRACKET_TYPE,
                                       Sid::MM_REST_RANGE_V_PLACEMENT,
                                       Sid::STAFF_TEXT_PLACEMENT,
                                       Sid::REPEAT_LEFT_PLACEMENT,
                                       Sid::REPEAT_RIGHT_PLACEMENT,
                                       Sid::INSTRUMENT_CHANGE_PLACEMENT,
                                       Sid::STICKING_PLACEMENT,
                                       Sid::LET_RING_PLACEMENT,
                                       Sid::PALM_MUTE_PLACEMENT,
                                       Sid::PAGE_WIDTH,
                                       Sid::PAGE_HEIGHT,
                                       Sid::PAGE_PRINTABLE_WIDTH,
                                       Sid::PAGE_EVEN_TOP_MARGIN,
                                       Sid::PAGE_EVEN_BOTTOM_MARGIN,
                                       Sid::PAGE_EVEN_LEFT_MARGIN,
                                       Sid::PAGE_ODD_TOP_MARGIN,
                                       Sid::PAGE_ODD_BOTTOM_MARGIN,
                                       Sid::PAGE_ODD_LEFT_MARGIN,
                                       Sid::PAGE_TWOSIDED,
                                       Sid::SPATIUM,
                                       Sid::CONCERT_PITCH,
                                       Sid::CREATE_MULTI_MEASURE_RESTS};

    auto reset_position_and_text_properties = [](EngravingItem *e) {
        reset_element_position(e);
        reset_text_properties(e);
    };

    reset_measures_layout(tx, score);
    score->scan_elements(reset_position_and_text_properties);
    reset_all_styles(tx, score, dont_reset_these_styles);
    EditSystemLocks::undo_remove_all_locks(tx, score);
}

void Reset::reset_text_style_overrides(Score *score) {
    TRACEFUNC;

    score->scan_elements(reset_text_properties);
}

void Reset::reset_all_styles(Transaction &, Score *score, const StyleIdSet &except_these_ones) {
    TRACEFUNC;

    int begin_idx = int(Sid::NOSTYLE) + 1;
    int end_idx = int(Sid::STYLES);

    StyleIdSet styles_to_reset;

    for (int idx = begin_idx; idx < end_idx; idx++) {
        Sid style_id = Sid(idx);
        if (!muse::contains(except_these_ones, style_id)) {
            styles_to_reset.insert(style_id);
        }
    }

    score->reset_style_values(styles_to_reset);
}

//---------------------------------------------------------
//   resetMeasuresLayout
//   Removes system/page breaks and spacers
//---------------------------------------------------------

void Reset::reset_measures_layout(Transaction &, Score *score) {
    TRACEFUNC;

    std::vector<EngravingItem *> items_to_remove;

    for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
        if (mb->is_measure()) {
            mb->undo_reset_property(Pid::USER_STRETCH);

            for (const MStaff *staff : to_measure(mb)->mstaves()) {
                if (Spacer *spacer = staff->vspacer_down()) {
                    items_to_remove.push_back(spacer);
                }

                if (Spacer *spacer = staff->vspacer_up()) {
                    items_to_remove.push_back(spacer);
                }
            }
        }

        if (!mb->line_break() && !mb->page_break()) {
            continue;
        }

        for (EngravingItem *item : mb->el()) {
            if (item->is_layout_break() && !to_layout_break(item)->is_section_break()) {
                items_to_remove.push_back(item);
            }
        }
    }

    for (EngravingItem *item : items_to_remove) {
        score->undo_remove_element(item);
    }
}

void Reset::reset_all_positions(Transaction &, Score *score) {
    TRACEFUNC;

    score->scan_elements(reset_element_position);
}
