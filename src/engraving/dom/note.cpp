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

/**
 \file
 Implementation of classes Note and ShadowNote.
*/

#include "note.h"

#include <cassert>

#include "../editing/addremoveelement.h"
#include "../editing/editchord.h"
#include "../editing/editnote.h"
#include "../editing/editparentheses.h"
#include "../editing/noteinput.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"
#include "accidental.h"
#include "actionicon.h"
#include "articulation.h"
#include "bagpembell.h"
#include "barline.h"
#include "beam.h"
#include "chord.h"
#include "chordline.h"
#include "drumset.h"
#include "factory.h"
#include "fingering.h"
#include "glissando.h"
#include "guitarbend.h"
#include "iengravingconfiguration.h" // IWYU pragma: keep
#include "iengravingfont.h"
#include "laissezvib.h"
#include "linkedobjects.h"
#include "log.h"
#include "measure.h"
#include "notedot.h"
#include "noteline.h"
#include "parenthesis.h"
#include "part.h"
#include "partialtie.h"
#include "pitchspelling.h"
#include "rendering/score/horizontalspacing.h"
#include "score.h"
#include "segment.h"
#include "spanner.h"
#include "staff.h"
#include "stafftype.h"
#include "stringdata.h"
#include "tie.h"
#include "translation.h"
#include "types/typesconv.h"
#include "utils.h"
#include "volta.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

namespace mu::engraving {
//---------------------------------------------------------
//   noteHeads
//    notehead groups
//---------------------------------------------------------

//int(NoteHeadGroup::HEAD_GROUPS) - 1: "-1" is needed to prevent building CUSTOM_GROUP noteheads set, since it is built by users and keep a specific set of existing noteheads
static const SymId note_heads[2][int(NoteHeadGroup::HEAD_GROUPS) - 1][int(NoteHeadType::HEAD_TYPES)] = {
    {// down stem
     {SymId::NOTEHEAD_WHOLE, SymId::NOTEHEAD_HALF, SymId::NOTEHEAD_BLACK, SymId::NOTEHEAD_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_X_WHOLE, SymId::NOTEHEAD_X_HALF, SymId::NOTEHEAD_X_BLACK, SymId::NOTEHEAD_X_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_PLUS_WHOLE, SymId::NOTEHEAD_PLUS_HALF, SymId::NOTEHEAD_PLUS_BLACK, SymId::NOTEHEAD_PLUS_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_CIRCLE_X_WHOLE, SymId::NOTEHEAD_CIRCLE_X_HALF, SymId::NOTEHEAD_CIRCLE_X, SymId::NOTEHEAD_CIRCLE_X_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_WHOLE_WITH_X, SymId::NOTEHEAD_HALF_WITH_X, SymId::NOTEHEAD_VOID_WITH_X, SymId::NOTEHEAD_DOUBLE_WHOLE_WITH_X},
     {SymId::NOTEHEAD_TRIANGLE_UP_WHOLE, SymId::NOTEHEAD_TRIANGLE_UP_HALF, SymId::NOTEHEAD_TRIANGLE_UP_BLACK,
      SymId::NOTEHEAD_TRIANGLE_UP_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_TRIANGLE_DOWN_WHOLE, SymId::NOTEHEAD_TRIANGLE_DOWN_HALF, SymId::NOTEHEAD_TRIANGLE_DOWN_BLACK,
      SymId::NOTEHEAD_TRIANGLE_DOWN_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_SLASHED_WHOLE1, SymId::NOTEHEAD_SLASHED_HALF1, SymId::NOTEHEAD_SLASHED_BLACK1, SymId::NOTEHEAD_SLASHED_DOUBLE_WHOLE1},
     {SymId::NOTEHEAD_SLASHED_WHOLE2, SymId::NOTEHEAD_SLASHED_HALF2, SymId::NOTEHEAD_SLASHED_BLACK2, SymId::NOTEHEAD_SLASHED_DOUBLE_WHOLE2},
     {SymId::NOTEHEAD_DIAMOND_WHOLE, SymId::NOTEHEAD_DIAMOND_HALF, SymId::NOTEHEAD_DIAMOND_BLACK, SymId::NOTEHEAD_DIAMOND_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_DIAMOND_WHOLE_OLD, SymId::NOTEHEAD_DIAMOND_HALF_OLD, SymId::NOTEHEAD_DIAMOND_BLACK_OLD,
      SymId::NOTEHEAD_DIAMOND_DOUBLE_WHOLE_OLD},
     {SymId::NOTEHEAD_CIRCLED_WHOLE, SymId::NOTEHEAD_CIRCLED_HALF, SymId::NOTEHEAD_CIRCLED_BLACK, SymId::NOTEHEAD_CIRCLED_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_CIRCLED_WHOLE_LARGE, SymId::NOTEHEAD_CIRCLED_HALF_LARGE, SymId::NOTEHEAD_CIRCLED_BLACK_LARGE,
      SymId::NOTEHEAD_CIRCLED_DOUBLE_WHOLE_LARGE},
     {SymId::NOTEHEAD_LARGE_ARROW_UP_WHOLE, SymId::NOTEHEAD_LARGE_ARROW_UP_HALF, SymId::NOTEHEAD_LARGE_ARROW_UP_BLACK,
      SymId::NOTEHEAD_LARGE_ARROW_UP_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_WHOLE, SymId::NOTEHEAD_HALF, SymId::NOTEHEAD_BLACK, SymId::NOTEHEAD_DOUBLE_WHOLE_SQUARE},

     {SymId::NOTEHEAD_SLASH_WHITE_WHOLE, SymId::NOTEHEAD_SLASH_WHITE_HALF, SymId::NOTEHEAD_SLASH_HORIZONTAL_ENDS,
      SymId::NOTEHEAD_SLASH_WHITE_WHOLE},
     {SymId::NOTEHEAD_SLASH_DIAMOND_WHITE, SymId::NOTEHEAD_SLASH_DIAMOND_WHITE, SymId::NOTEHEAD_SLASH_HORIZONTAL_ENDS,
      SymId::NOTEHEAD_SLASH_WHITE_WHOLE},

     {SymId::NOTE_SHAPE_ROUND_WHITE, SymId::NOTE_SHAPE_ROUND_WHITE, SymId::NOTE_SHAPE_ROUND_BLACK, SymId::NOTE_SHAPE_ROUND_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_SQUARE_WHITE, SymId::NOTE_SHAPE_SQUARE_WHITE, SymId::NOTE_SHAPE_SQUARE_BLACK,
      SymId::NOTE_SHAPE_SQUARE_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_RIGHT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_RIGHT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_RIGHT_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_RIGHT_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_DIAMOND_WHITE, SymId::NOTE_SHAPE_DIAMOND_WHITE, SymId::NOTE_SHAPE_DIAMOND_BLACK,
      SymId::NOTE_SHAPE_DIAMOND_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_UP_WHITE, SymId::NOTE_SHAPE_TRIANGLE_UP_WHITE, SymId::NOTE_SHAPE_TRIANGLE_UP_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_UP_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_MOON_WHITE, SymId::NOTE_SHAPE_MOON_WHITE, SymId::NOTE_SHAPE_MOON_BLACK, SymId::NOTE_SHAPE_MOON_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_ROUND_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_ROUND_DOUBLE_WHOLE},

     {SymId::NOTEHEAD_HEAVY_X, SymId::NOTEHEAD_HEAVY_X, SymId::NOTEHEAD_HEAVY_X, SymId::NOTEHEAD_HEAVY_X},
     {SymId::NOTEHEAD_HEAVY_X_HAT, SymId::NOTEHEAD_HEAVY_X_HAT, SymId::NOTEHEAD_HEAVY_X_HAT, SymId::NOTEHEAD_HEAVY_X_HAT},

     {SymId::NOTE_SHAPE_KEYSTONE_WHITE, SymId::NOTE_SHAPE_KEYSTONE_WHITE, SymId::NOTE_SHAPE_KEYSTONE_BLACK,
      SymId::NOTE_SHAPE_KEYSTONE_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_QUARTER_MOON_WHITE, SymId::NOTE_SHAPE_QUARTER_MOON_WHITE, SymId::NOTE_SHAPE_QUARTER_MOON_BLACK,
      SymId::NOTE_SHAPE_QUARTER_MOON_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_WHITE, SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_WHITE, SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_BLACK,
      SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_MOON_LEFT_WHITE, SymId::NOTE_SHAPE_MOON_LEFT_WHITE, SymId::NOTE_SHAPE_MOON_LEFT_BLACK,
      SymId::NOTE_SHAPE_MOON_LEFT_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_ARROWHEAD_LEFT_WHITE, SymId::NOTE_SHAPE_ARROWHEAD_LEFT_WHITE, SymId::NOTE_SHAPE_ARROWHEAD_LEFT_BLACK,
      SymId::NOTE_SHAPE_ARROWHEAD_LEFT_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_DOUBLE_WHOLE},

     {SymId::NOTE_DO_WHOLE, SymId::NOTE_DO_HALF, SymId::NOTE_DO_BLACK, SymId::NO_SYM},
     {SymId::NOTE_DI_WHOLE, SymId::NOTE_DI_HALF, SymId::NOTE_DI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_RA_WHOLE, SymId::NOTE_RA_HALF, SymId::NOTE_RA_BLACK, SymId::NO_SYM},
     {SymId::NOTE_RE_WHOLE, SymId::NOTE_RE_HALF, SymId::NOTE_RE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_RI_WHOLE, SymId::NOTE_RI_HALF, SymId::NOTE_RI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_ME_WHOLE, SymId::NOTE_ME_HALF, SymId::NOTE_ME_BLACK, SymId::NO_SYM},
     {SymId::NOTE_MI_WHOLE, SymId::NOTE_MI_HALF, SymId::NOTE_MI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_FA_WHOLE, SymId::NOTE_FA_HALF, SymId::NOTE_FA_BLACK, SymId::NO_SYM},
     {SymId::NOTE_FI_WHOLE, SymId::NOTE_FI_HALF, SymId::NOTE_FI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_SE_WHOLE, SymId::NOTE_SE_HALF, SymId::NOTE_SE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_SO_WHOLE, SymId::NOTE_SO_HALF, SymId::NOTE_SO_BLACK, SymId::NO_SYM},
     {SymId::NOTE_LE_WHOLE, SymId::NOTE_LE_HALF, SymId::NOTE_LE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_LA_WHOLE, SymId::NOTE_LA_HALF, SymId::NOTE_LA_BLACK, SymId::NO_SYM},
     {SymId::NOTE_LI_WHOLE, SymId::NOTE_LI_HALF, SymId::NOTE_LI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_TE_WHOLE, SymId::NOTE_TE_HALF, SymId::NOTE_TE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_TI_WHOLE, SymId::NOTE_TI_HALF, SymId::NOTE_TI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_SI_WHOLE, SymId::NOTE_SI_HALF, SymId::NOTE_SI_BLACK, SymId::NO_SYM},

     {SymId::NOTE_A_SHARP_WHOLE, SymId::NOTE_A_SHARP_HALF, SymId::NOTE_A_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_A_WHOLE, SymId::NOTE_A_HALF, SymId::NOTE_A_BLACK, SymId::NO_SYM},
     {SymId::NOTE_A_FLAT_WHOLE, SymId::NOTE_A_FLAT_HALF, SymId::NOTE_A_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_B_SHARP_WHOLE, SymId::NOTE_B_SHARP_HALF, SymId::NOTE_B_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_B_WHOLE, SymId::NOTE_B_HALF, SymId::NOTE_B_BLACK, SymId::NO_SYM},
     {SymId::NOTE_B_FLAT_WHOLE, SymId::NOTE_B_FLAT_HALF, SymId::NOTE_B_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_C_SHARP_WHOLE, SymId::NOTE_C_SHARP_HALF, SymId::NOTE_C_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_C_WHOLE, SymId::NOTE_C_HALF, SymId::NOTE_C_BLACK, SymId::NO_SYM},
     {SymId::NOTE_C_FLAT_WHOLE, SymId::NOTE_C_FLAT_HALF, SymId::NOTE_C_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_D_SHARP_WHOLE, SymId::NOTE_D_SHARP_HALF, SymId::NOTE_D_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_D_WHOLE, SymId::NOTE_D_HALF, SymId::NOTE_D_BLACK, SymId::NO_SYM},
     {SymId::NOTE_D_FLAT_WHOLE, SymId::NOTE_D_FLAT_HALF, SymId::NOTE_D_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_E_SHARP_WHOLE, SymId::NOTE_E_SHARP_HALF, SymId::NOTE_E_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_E_WHOLE, SymId::NOTE_E_HALF, SymId::NOTE_E_BLACK, SymId::NO_SYM},
     {SymId::NOTE_E_FLAT_WHOLE, SymId::NOTE_E_FLAT_HALF, SymId::NOTE_E_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_F_SHARP_WHOLE, SymId::NOTE_F_SHARP_HALF, SymId::NOTE_F_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_F_WHOLE, SymId::NOTE_F_HALF, SymId::NOTE_F_BLACK, SymId::NO_SYM},
     {SymId::NOTE_F_FLAT_WHOLE, SymId::NOTE_F_FLAT_HALF, SymId::NOTE_F_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_G_SHARP_WHOLE, SymId::NOTE_G_SHARP_HALF, SymId::NOTE_G_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_G_WHOLE, SymId::NOTE_G_HALF, SymId::NOTE_G_BLACK, SymId::NO_SYM},
     {SymId::NOTE_G_FLAT_WHOLE, SymId::NOTE_G_FLAT_HALF, SymId::NOTE_G_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_H_WHOLE, SymId::NOTE_H_HALF, SymId::NOTE_H_BLACK, SymId::NO_SYM},
     {SymId::NOTE_H_SHARP_WHOLE, SymId::NOTE_H_SHARP_HALF, SymId::NOTE_H_SHARP_BLACK, SymId::NO_SYM},

     {SymId::NO_SYM, SymId::SWISS_RUDIMENTS_NOTEHEAD_HALF_FLAM, SymId::SWISS_RUDIMENTS_NOTEHEAD_BLACK_FLAM, SymId::NO_SYM},
     {SymId::NO_SYM, SymId::SWISS_RUDIMENTS_NOTEHEAD_HALF_DOUBLE, SymId::SWISS_RUDIMENTS_NOTEHEAD_BLACK_DOUBLE, SymId::NO_SYM}},
    {// up stem
     {SymId::NOTEHEAD_WHOLE, SymId::NOTEHEAD_HALF, SymId::NOTEHEAD_BLACK, SymId::NOTEHEAD_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_X_WHOLE, SymId::NOTEHEAD_X_HALF, SymId::NOTEHEAD_X_BLACK, SymId::NOTEHEAD_X_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_PLUS_WHOLE, SymId::NOTEHEAD_PLUS_HALF, SymId::NOTEHEAD_PLUS_BLACK, SymId::NOTEHEAD_PLUS_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_CIRCLE_X_WHOLE, SymId::NOTEHEAD_CIRCLE_X_HALF, SymId::NOTEHEAD_CIRCLE_X, SymId::NOTEHEAD_CIRCLE_X_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_WHOLE_WITH_X, SymId::NOTEHEAD_HALF_WITH_X, SymId::NOTEHEAD_VOID_WITH_X, SymId::NOTEHEAD_DOUBLE_WHOLE_WITH_X},
     {SymId::NOTEHEAD_TRIANGLE_UP_WHOLE, SymId::NOTEHEAD_TRIANGLE_UP_HALF, SymId::NOTEHEAD_TRIANGLE_UP_BLACK,
      SymId::NOTEHEAD_TRIANGLE_UP_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_TRIANGLE_DOWN_WHOLE, SymId::NOTEHEAD_TRIANGLE_DOWN_HALF, SymId::NOTEHEAD_TRIANGLE_DOWN_BLACK,
      SymId::NOTEHEAD_TRIANGLE_DOWN_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_SLASHED_WHOLE1, SymId::NOTEHEAD_SLASHED_HALF1, SymId::NOTEHEAD_SLASHED_BLACK1, SymId::NOTEHEAD_SLASHED_DOUBLE_WHOLE1},
     {SymId::NOTEHEAD_SLASHED_WHOLE2, SymId::NOTEHEAD_SLASHED_HALF2, SymId::NOTEHEAD_SLASHED_BLACK2, SymId::NOTEHEAD_SLASHED_DOUBLE_WHOLE2},
     {SymId::NOTEHEAD_DIAMOND_WHOLE, SymId::NOTEHEAD_DIAMOND_HALF, SymId::NOTEHEAD_DIAMOND_BLACK, SymId::NOTEHEAD_DIAMOND_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_DIAMOND_WHOLE_OLD, SymId::NOTEHEAD_DIAMOND_HALF_OLD, SymId::NOTEHEAD_DIAMOND_BLACK_OLD,
      SymId::NOTEHEAD_DIAMOND_DOUBLE_WHOLE_OLD},
     {SymId::NOTEHEAD_CIRCLED_WHOLE, SymId::NOTEHEAD_CIRCLED_HALF, SymId::NOTEHEAD_CIRCLED_BLACK, SymId::NOTEHEAD_CIRCLED_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_CIRCLED_WHOLE_LARGE, SymId::NOTEHEAD_CIRCLED_HALF_LARGE, SymId::NOTEHEAD_CIRCLED_BLACK_LARGE,
      SymId::NOTEHEAD_CIRCLED_DOUBLE_WHOLE_LARGE},
     // different from down, find source?
     {SymId::NOTEHEAD_LARGE_ARROW_DOWN_WHOLE, SymId::NOTEHEAD_LARGE_ARROW_DOWN_HALF, SymId::NOTEHEAD_LARGE_ARROW_DOWN_BLACK,
      SymId::NOTEHEAD_LARGE_ARROW_DOWN_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_WHOLE, SymId::NOTEHEAD_HALF, SymId::NOTEHEAD_BLACK, SymId::NOTEHEAD_DOUBLE_WHOLE_SQUARE},

     {SymId::NOTEHEAD_SLASH_WHITE_WHOLE, SymId::NOTEHEAD_SLASH_WHITE_HALF, SymId::NOTEHEAD_SLASH_HORIZONTAL_ENDS,
      SymId::NOTEHEAD_SLASH_WHITE_DOUBLE_WHOLE},
     {SymId::NOTEHEAD_SLASH_DIAMOND_WHITE, SymId::NOTEHEAD_SLASH_DIAMOND_WHITE, SymId::NOTEHEAD_SLASH_HORIZONTAL_ENDS,
      SymId::NOTEHEAD_SLASH_WHITE_DOUBLE_WHOLE},

     {SymId::NOTE_SHAPE_ROUND_WHITE, SymId::NOTE_SHAPE_ROUND_WHITE, SymId::NOTE_SHAPE_ROUND_BLACK, SymId::NOTE_SHAPE_ROUND_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_SQUARE_WHITE, SymId::NOTE_SHAPE_SQUARE_WHITE, SymId::NOTE_SHAPE_SQUARE_BLACK,
      SymId::NOTE_SHAPE_SQUARE_DOUBLE_WHOLE},
     // different from down
     {SymId::NOTE_SHAPE_TRIANGLE_LEFT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_LEFT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_LEFT_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_LEFT_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_DIAMOND_WHITE, SymId::NOTE_SHAPE_DIAMOND_WHITE, SymId::NOTE_SHAPE_DIAMOND_BLACK,
      SymId::NOTE_SHAPE_DIAMOND_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_UP_WHITE, SymId::NOTE_SHAPE_TRIANGLE_UP_WHITE, SymId::NOTE_SHAPE_TRIANGLE_UP_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_UP_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_MOON_WHITE, SymId::NOTE_SHAPE_MOON_WHITE, SymId::NOTE_SHAPE_MOON_BLACK, SymId::NOTE_SHAPE_MOON_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_ROUND_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_ROUND_DOUBLE_WHOLE},

     {SymId::NOTEHEAD_HEAVY_X, SymId::NOTEHEAD_HEAVY_X, SymId::NOTEHEAD_HEAVY_X, SymId::NOTEHEAD_HEAVY_X},
     {SymId::NOTEHEAD_HEAVY_X_HAT, SymId::NOTEHEAD_HEAVY_X_HAT, SymId::NOTEHEAD_HEAVY_X_HAT, SymId::NOTEHEAD_HEAVY_X_HAT},

     {SymId::NOTE_SHAPE_KEYSTONE_WHITE, SymId::NOTE_SHAPE_KEYSTONE_WHITE, SymId::NOTE_SHAPE_KEYSTONE_BLACK,
      SymId::NOTE_SHAPE_KEYSTONE_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_QUARTER_MOON_WHITE, SymId::NOTE_SHAPE_QUARTER_MOON_WHITE, SymId::NOTE_SHAPE_QUARTER_MOON_BLACK,
      SymId::NOTE_SHAPE_QUARTER_MOON_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_WHITE, SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_WHITE, SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_BLACK,
      SymId::NOTE_SHAPE_ISOSCELES_TRIANGLE_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_MOON_LEFT_WHITE, SymId::NOTE_SHAPE_MOON_LEFT_WHITE, SymId::NOTE_SHAPE_MOON_LEFT_BLACK,
      SymId::NOTE_SHAPE_MOON_LEFT_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_ARROWHEAD_LEFT_WHITE, SymId::NOTE_SHAPE_ARROWHEAD_LEFT_WHITE, SymId::NOTE_SHAPE_ARROWHEAD_LEFT_BLACK,
      SymId::NOTE_SHAPE_ARROWHEAD_LEFT_DOUBLE_WHOLE},
     {SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_WHITE, SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_BLACK,
      SymId::NOTE_SHAPE_TRIANGLE_ROUND_LEFT_DOUBLE_WHOLE},

     {SymId::NOTE_DO_WHOLE, SymId::NOTE_DO_HALF, SymId::NOTE_DO_BLACK, SymId::NO_SYM},
     {SymId::NOTE_DI_WHOLE, SymId::NOTE_DI_HALF, SymId::NOTE_DI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_RA_WHOLE, SymId::NOTE_RA_HALF, SymId::NOTE_RA_BLACK, SymId::NO_SYM},
     {SymId::NOTE_RE_WHOLE, SymId::NOTE_RE_HALF, SymId::NOTE_RE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_RI_WHOLE, SymId::NOTE_RI_HALF, SymId::NOTE_RI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_ME_WHOLE, SymId::NOTE_ME_HALF, SymId::NOTE_ME_BLACK, SymId::NO_SYM},
     {SymId::NOTE_MI_WHOLE, SymId::NOTE_MI_HALF, SymId::NOTE_MI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_FA_WHOLE, SymId::NOTE_FA_HALF, SymId::NOTE_FA_BLACK, SymId::NO_SYM},
     {SymId::NOTE_FI_WHOLE, SymId::NOTE_FI_HALF, SymId::NOTE_FI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_SE_WHOLE, SymId::NOTE_SE_HALF, SymId::NOTE_SE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_SO_WHOLE, SymId::NOTE_SO_HALF, SymId::NOTE_SO_BLACK, SymId::NO_SYM},
     {SymId::NOTE_LE_WHOLE, SymId::NOTE_LE_HALF, SymId::NOTE_LE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_LA_WHOLE, SymId::NOTE_LA_HALF, SymId::NOTE_LA_BLACK, SymId::NO_SYM},
     {SymId::NOTE_LI_WHOLE, SymId::NOTE_LI_HALF, SymId::NOTE_LI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_TE_WHOLE, SymId::NOTE_TE_HALF, SymId::NOTE_TE_BLACK, SymId::NO_SYM},
     {SymId::NOTE_TI_WHOLE, SymId::NOTE_TI_HALF, SymId::NOTE_TI_BLACK, SymId::NO_SYM},
     {SymId::NOTE_SI_WHOLE, SymId::NOTE_SI_HALF, SymId::NOTE_SI_BLACK, SymId::NO_SYM},

     {SymId::NOTE_A_SHARP_WHOLE, SymId::NOTE_A_SHARP_HALF, SymId::NOTE_A_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_A_WHOLE, SymId::NOTE_A_HALF, SymId::NOTE_A_BLACK, SymId::NO_SYM},
     {SymId::NOTE_A_FLAT_WHOLE, SymId::NOTE_A_FLAT_HALF, SymId::NOTE_A_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_B_SHARP_WHOLE, SymId::NOTE_B_SHARP_HALF, SymId::NOTE_B_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_B_WHOLE, SymId::NOTE_B_HALF, SymId::NOTE_B_BLACK, SymId::NO_SYM},
     {SymId::NOTE_B_FLAT_WHOLE, SymId::NOTE_B_FLAT_HALF, SymId::NOTE_B_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_C_SHARP_WHOLE, SymId::NOTE_C_SHARP_HALF, SymId::NOTE_C_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_C_WHOLE, SymId::NOTE_C_HALF, SymId::NOTE_C_BLACK, SymId::NO_SYM},
     {SymId::NOTE_C_FLAT_WHOLE, SymId::NOTE_C_FLAT_HALF, SymId::NOTE_C_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_D_SHARP_WHOLE, SymId::NOTE_D_SHARP_HALF, SymId::NOTE_D_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_D_WHOLE, SymId::NOTE_D_HALF, SymId::NOTE_D_BLACK, SymId::NO_SYM},
     {SymId::NOTE_D_FLAT_WHOLE, SymId::NOTE_D_FLAT_HALF, SymId::NOTE_D_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_E_SHARP_WHOLE, SymId::NOTE_E_SHARP_HALF, SymId::NOTE_E_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_E_WHOLE, SymId::NOTE_E_HALF, SymId::NOTE_E_BLACK, SymId::NO_SYM},
     {SymId::NOTE_E_FLAT_WHOLE, SymId::NOTE_E_FLAT_HALF, SymId::NOTE_E_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_F_SHARP_WHOLE, SymId::NOTE_F_SHARP_HALF, SymId::NOTE_F_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_F_WHOLE, SymId::NOTE_F_HALF, SymId::NOTE_F_BLACK, SymId::NO_SYM},
     {SymId::NOTE_F_FLAT_WHOLE, SymId::NOTE_F_FLAT_HALF, SymId::NOTE_F_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_G_SHARP_WHOLE, SymId::NOTE_G_SHARP_HALF, SymId::NOTE_G_SHARP_BLACK, SymId::NO_SYM},
     {SymId::NOTE_G_WHOLE, SymId::NOTE_G_HALF, SymId::NOTE_G_BLACK, SymId::NO_SYM},
     {SymId::NOTE_G_FLAT_WHOLE, SymId::NOTE_G_FLAT_HALF, SymId::NOTE_G_FLAT_BLACK, SymId::NO_SYM},
     {SymId::NOTE_H_WHOLE, SymId::NOTE_H_HALF, SymId::NOTE_H_BLACK, SymId::NO_SYM},
     {SymId::NOTE_H_SHARP_WHOLE, SymId::NOTE_H_SHARP_HALF, SymId::NOTE_H_SHARP_BLACK, SymId::NO_SYM},

     {SymId::NO_SYM, SymId::SWISS_RUDIMENTS_NOTEHEAD_HALF_FLAM, SymId::SWISS_RUDIMENTS_NOTEHEAD_BLACK_FLAM, SymId::NO_SYM},
     {SymId::NO_SYM, SymId::SWISS_RUDIMENTS_NOTEHEAD_HALF_DOUBLE, SymId::SWISS_RUDIMENTS_NOTEHEAD_BLACK_DOUBLE, SymId::NO_SYM}}};

//---------------------------------------------------------
//   noteHead
//---------------------------------------------------------

SymId Note::note_head(int direction, NoteHeadGroup group, NoteHeadType t) {
    return ::mu::engraving::note_heads[direction][int(group)][int(t)];
}

SymId Note::note_head(int direction, NoteHeadGroup group, NoteHeadType t, int tpc, Key key, NoteHeadScheme scheme) {
    // shortcut
    if (scheme == NoteHeadScheme::HEAD_NORMAL) {
        return ::mu::engraving::note_heads[direction][int(group)][int(t)];
    }
    // other schemes
    if (scheme == NoteHeadScheme::HEAD_PITCHNAME || scheme == NoteHeadScheme::HEAD_PITCHNAME_GERMAN ||
        scheme == NoteHeadScheme::HEAD_PITCHNAME_NO_ACCIDENTALS || scheme == NoteHeadScheme::HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS) {
        const bool no_accidentals =
            scheme == NoteHeadScheme::HEAD_PITCHNAME_NO_ACCIDENTALS || scheme == NoteHeadScheme::HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS;
        const bool german =
            scheme == NoteHeadScheme::HEAD_PITCHNAME_GERMAN || scheme == NoteHeadScheme::HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS;

        if (tpc == Tpc::TPC_A) {
            group = NoteHeadGroup::HEAD_A;
        } else if (tpc == Tpc::TPC_B) {
            if (german) {
                group = NoteHeadGroup::HEAD_H;
            } else {
                group = NoteHeadGroup::HEAD_B;
            }
        } else if (tpc == Tpc::TPC_C) {
            group = NoteHeadGroup::HEAD_C;
        } else if (tpc == Tpc::TPC_D) {
            group = NoteHeadGroup::HEAD_D;
        } else if (tpc == Tpc::TPC_E) {
            group = NoteHeadGroup::HEAD_E;
        } else if (tpc == Tpc::TPC_F) {
            group = NoteHeadGroup::HEAD_F;
        } else if (tpc == Tpc::TPC_G) {
            group = NoteHeadGroup::HEAD_G;
        } else if (tpc == Tpc::TPC_A_S) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_A;
            } else {
                group = NoteHeadGroup::HEAD_A_SHARP;
            }
        } else if (tpc == Tpc::TPC_B_S) {
            if (no_accidentals) {
                if (german) {
                    group = NoteHeadGroup::HEAD_H;
                } else {
                    group = NoteHeadGroup::HEAD_B;
                }
            } else {
                if (german) {
                    group = NoteHeadGroup::HEAD_H_SHARP;
                } else {
                    group = NoteHeadGroup::HEAD_B_SHARP;
                }
            }
        } else if (tpc == Tpc::TPC_C_S) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_C;
            } else {
                group = NoteHeadGroup::HEAD_C_SHARP;
            }
        } else if (tpc == Tpc::TPC_D_S) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_D;
            } else {
                group = NoteHeadGroup::HEAD_D_SHARP;
            }
        } else if (tpc == Tpc::TPC_E_S) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_E;
            } else {
                group = NoteHeadGroup::HEAD_E_SHARP;
            }
        } else if (tpc == Tpc::TPC_F_S) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_F;
            } else {
                group = NoteHeadGroup::HEAD_F_SHARP;
            }
        } else if (tpc == Tpc::TPC_G_S) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_G;
            } else {
                group = NoteHeadGroup::HEAD_G_SHARP;
            }
        } else if (tpc == Tpc::TPC_A_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_A;
            } else {
                group = NoteHeadGroup::HEAD_A_FLAT;
            }
        } else if (tpc == Tpc::TPC_B_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_B;
            } else {
                if (german) {
                    group = NoteHeadGroup::HEAD_B;
                } else {
                    group = NoteHeadGroup::HEAD_B_FLAT;
                }
            }
        } else if (tpc == Tpc::TPC_C_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_C;
            } else {
                group = NoteHeadGroup::HEAD_C_FLAT;
            }
        } else if (tpc == Tpc::TPC_D_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_D;
            } else {
                group = NoteHeadGroup::HEAD_D_FLAT;
            }
        } else if (tpc == Tpc::TPC_E_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_E;
            } else {
                group = NoteHeadGroup::HEAD_E_FLAT;
            }
        } else if (tpc == Tpc::TPC_F_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_F;
            } else {
                group = NoteHeadGroup::HEAD_F_FLAT;
            }
        } else if (tpc == Tpc::TPC_G_B) {
            if (no_accidentals) {
                group = NoteHeadGroup::HEAD_G;
            } else {
                group = NoteHeadGroup::HEAD_G_FLAT;
            }
        } else if (no_accidentals) {
            // for TPCs that don't have their own heads (i.e. with 2 or 3 accidentals),
            // fall back to using the corresponding ones without accidentals
            // rather than using the (smaller!) normal noteheads
            if (tpc == Tpc::TPC_A_SS || tpc == Tpc::TPC_A_SSS) {
                group = NoteHeadGroup::HEAD_A;
            } else if (tpc == Tpc::TPC_B_SS || tpc == Tpc::TPC_B_SSS) {
                if (german) {
                    group = NoteHeadGroup::HEAD_H;
                } else {
                    group = NoteHeadGroup::HEAD_B;
                }
            } else if (tpc == Tpc::TPC_C_SS || tpc == Tpc::TPC_C_SSS) {
                group = NoteHeadGroup::HEAD_C;
            } else if (tpc == Tpc::TPC_D_SS || tpc == Tpc::TPC_D_SSS) {
                group = NoteHeadGroup::HEAD_D;
            } else if (tpc == Tpc::TPC_E_SS || tpc == Tpc::TPC_E_SSS) {
                group = NoteHeadGroup::HEAD_E;
            } else if (tpc == Tpc::TPC_F_SS || tpc == Tpc::TPC_F_SSS) {
                group = NoteHeadGroup::HEAD_F;
            } else if (tpc == Tpc::TPC_G_SS || tpc == Tpc::TPC_G_SSS) {
                group = NoteHeadGroup::HEAD_G;
            } else if (tpc == Tpc::TPC_A_BB || tpc == Tpc::TPC_A_BBB) {
                group = NoteHeadGroup::HEAD_A;
            } else if (tpc == Tpc::TPC_B_BB || tpc == Tpc::TPC_B_BBB) {
                if (german) {
                    group = NoteHeadGroup::HEAD_H;
                } else {
                    group = NoteHeadGroup::HEAD_B;
                }
            } else if (tpc == Tpc::TPC_C_BB || tpc == Tpc::TPC_C_BBB) {
                group = NoteHeadGroup::HEAD_C;
            } else if (tpc == Tpc::TPC_D_BB || tpc == Tpc::TPC_D_BBB) {
                group = NoteHeadGroup::HEAD_D;
            } else if (tpc == Tpc::TPC_E_BB || tpc == Tpc::TPC_E_BBB) {
                group = NoteHeadGroup::HEAD_E;
            } else if (tpc == Tpc::TPC_F_BB || tpc == Tpc::TPC_F_BBB) {
                group = NoteHeadGroup::HEAD_F;
            } else if (tpc == Tpc::TPC_G_BB || tpc == Tpc::TPC_G_BBB) {
                group = NoteHeadGroup::HEAD_G;
            }
        }
    } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_4) {
        int degree = tpc2degree(tpc, key);
        switch (degree) {
            case 0:
            case 3: group = NoteHeadGroup::HEAD_FA; break;
            case 1:
            case 4: group = NoteHeadGroup::HEAD_SOL; break;
            case 2:
            case 5: group = NoteHeadGroup::HEAD_LA; break;
            case 6: group = NoteHeadGroup::HEAD_MI; break;
        }
    } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN || scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_FUNK ||
               scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_WALKER) {
        int degree = tpc2degree(tpc, key);
        switch (degree) {
            case 0:
                if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN) {
                    group = NoteHeadGroup::HEAD_DO;
                } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_FUNK) {
                    group = NoteHeadGroup::HEAD_DO_FUNK;
                } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_WALKER) {
                    group = NoteHeadGroup::HEAD_DO_WALKER;
                }
                break;
            case 1:
                if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN) {
                    group = NoteHeadGroup::HEAD_RE;
                } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_FUNK) {
                    group = NoteHeadGroup::HEAD_RE_FUNK;
                } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_WALKER) {
                    group = NoteHeadGroup::HEAD_RE_WALKER;
                }
                break;
            case 2: group = NoteHeadGroup::HEAD_MI; break;
            case 3: group = NoteHeadGroup::HEAD_FA; break;
            case 4: group = NoteHeadGroup::HEAD_SOL; break;
            case 5: group = NoteHeadGroup::HEAD_LA; break;
            case 6:
                if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN) {
                    group = NoteHeadGroup::HEAD_TI;
                } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_FUNK) {
                    group = NoteHeadGroup::HEAD_TI_FUNK;
                } else if (scheme == NoteHeadScheme::HEAD_SHAPE_NOTE_7_WALKER) {
                    group = NoteHeadGroup::HEAD_TI_WALKER;
                }
                break;
        }
    } else if (scheme == NoteHeadScheme::HEAD_SOLFEGE) {
        int degree = tpc2degree(tpc, key);
        int alteration = tpc2alter_by_key(tpc, key);
        if (degree == 0 && alteration == 0) {
            group = NoteHeadGroup::HEAD_DO_NAME;
        } else if (degree == 0 && alteration == 1) {
            group = NoteHeadGroup::HEAD_DI_NAME;
        } else if (degree == 1 && alteration == -1) {
            group = NoteHeadGroup::HEAD_RA_NAME;
        } else if (degree == 1 && alteration == 0) {
            group = NoteHeadGroup::HEAD_RE_NAME;
        } else if (degree == 1 && alteration == 1) {
            group = NoteHeadGroup::HEAD_RI_NAME;
        } else if (degree == 2 && alteration == -1) {
            group = NoteHeadGroup::HEAD_ME_NAME;
        } else if (degree == 2 && alteration == 0) {
            group = NoteHeadGroup::HEAD_MI_NAME;
        } else if (degree == 3 && alteration == 0) {
            group = NoteHeadGroup::HEAD_FA_NAME;
        } else if (degree == 3 && alteration == 1) {
            group = NoteHeadGroup::HEAD_FI_NAME;
        } else if (degree == 4 && alteration == -1) {
            group = NoteHeadGroup::HEAD_SE_NAME;
        } else if (degree == 4 && alteration == 0) {
            group = NoteHeadGroup::HEAD_SOL_NAME;
        } else if (degree == 4 && alteration == 1) {
            group = NoteHeadGroup::HEAD_SI_NAME;
        } else if (degree == 5 && alteration == -1) {
            group = NoteHeadGroup::HEAD_LE_NAME;
        } else if (degree == 5 && alteration == 0) {
            group = NoteHeadGroup::HEAD_LA_NAME;
        } else if (degree == 5 && alteration == 1) {
            group = NoteHeadGroup::HEAD_LI_NAME;
        } else if (degree == 6 && alteration == -1) {
            group = NoteHeadGroup::HEAD_TE_NAME;
        } else if (degree == 6 && alteration == 0) {
            group = NoteHeadGroup::HEAD_TI_NAME;
        }
    } else if (scheme == NoteHeadScheme::HEAD_SOLFEGE_FIXED) {
        Char step_name = tpc2step_name(tpc);
        if (step_name == u'C') {
            group = NoteHeadGroup::HEAD_DO_NAME;
        } else if (step_name == u'D') {
            group = NoteHeadGroup::HEAD_RE_NAME;
        } else if (step_name == u'E') {
            group = NoteHeadGroup::HEAD_MI_NAME;
        } else if (step_name == u'F') {
            group = NoteHeadGroup::HEAD_FA_NAME;
        } else if (step_name == u'G') {
            group = NoteHeadGroup::HEAD_SOL_NAME;
        } else if (step_name == u'A') {
            group = NoteHeadGroup::HEAD_LA_NAME;
        } else if (step_name == u'B') {
            group = NoteHeadGroup::HEAD_SI_NAME;
        }
    }
    return ::mu::engraving::note_heads[direction][int(group)][int(t)];
}

NoteHead::NoteHead(DummyParentOr<Note> parent) : Symbol(ElementType::NOTEHEAD, parent) {}

//---------------------------------------------------------
//   headGroup
//   used only when dropping a notehead from the palette
//   they are either half note, either double whole
//---------------------------------------------------------

NoteHeadGroup NoteHead::head_group() const {
    NoteHeadGroup group = NoteHeadGroup::HEAD_INVALID;
    for (int i = 0; i < int(NoteHeadGroup::HEAD_DO_WALKER); ++i) {
        if (::mu::engraving::note_heads[0][i][1] == sym_ || ::mu::engraving::note_heads[0][i][3] == sym_) {
            group = (NoteHeadGroup) i;
            break;
        }
    }
    return group;
}

//---------------------------------------------------------
//   Note
//---------------------------------------------------------

Note::Note(DummyParentOr<Chord> ch) : EngravingItem(ElementType::NOTE, ch, ElementFlag::MOVABLE) {
    play_events_.push_back(NoteEvent()); // add default play event
}

Note::~Note() {
    delete accidental_;
    muse::delete_all(el_);

    if (tie_for_ && tie_for_->parent() == this) {
        delete tie_for_;
    }

    muse::delete_all(dots_);
}

std::vector<Note *> Note::compound_notes() const {
    std::vector<Note *> elements;
    if (Note *note = first_tied_note()) {
        elements.push_back(note);
    }

    if (Note *note = last_tied_note()) {
        elements.push_back(note);
    }

    for (Spanner *e : spanner_for_) {
        elements.push_back(to_note(e->end_element()));
    }
    for (Spanner *e : spanner_back_) {
        elements.push_back(to_note(e->start_element()));
    }

    Beam *beam = chord()->beam();
    if (beam) {
        for (EngravingItem *e : beam->elements()) {
            if (e && e->is_note() && e != this) {
                elements.push_back(to_note(e));
            }
        }
    }

    return elements;
}

Note::Note(const Note &n, bool link) : EngravingItem(n, link) {
    if (link) {
        score()->undo(new Link(this, const_cast<Note *>(&n)));
    }
    subchannel_ = n.subchannel_;
    line_ = n.line_;
    fret_ = n.fret_;
    harmonic_fret_ = n.harmonic_fret_;
    display_fret_ = n.display_fret_;
    string_ = n.string_;
    fret_conflict_ = n.fret_conflict_;
    ghost_ = n.ghost_;
    dead_note_ = n.dead_note_;
    pitch_ = n.pitch_;
    cent_offset_ = n.cent_offset_;
    tpc_[0] = n.tpc_[0];
    tpc_[1] = n.tpc_[1];
    dots_hidden_ = n.dots_hidden_;
    hidden_ = n.hidden_;
    play_ = n.play_;
    tuning_ = n.tuning_;
    velo_type_ = n.velo_type_;
    user_velocity_ = n.user_velocity_;
    head_scheme_ = n.head_scheme_;
    head_group_ = n.head_group_;
    head_type_ = n.head_type_;
    user_mirror_ = n.user_mirror_;
    is_small_ = n.is_small_;
    user_dot_position_ = n.user_dot_position_;
    fixed_ = n.fixed_;
    fixed_line_ = n.fixed_line_;
    harmonic_ = n.harmonic_;
    has_parens_ = n.has_parens_;
    hide_generated_parens_ = n.hide_generated_parens_;

    if (n.accidental_) {
        add(new Accidental(*(n.accidental_)));
    }

    // types in _el: SYMBOL, IMAGE, FINGERING, TEXT, BEND
    const Staff *stf = staff();
    bool tab_fingering = stf ? stf->staff_type_for_element(this)->show_tab_fingering() : false;
    for (EngravingItem *e : n.el_) {
        if (e->is_fingering() && staff()->is_tab_staff(tick()) && !tab_fingering) { // tablature has no fingering
            continue;
        }
        EngravingItem *ce = e->clone();
        add(ce);
        if (link) {
            score()->undo(new Link(ce, const_cast<EngravingItem *>(e)));
        }
    }

    play_events_ = n.play_events_;

    if (n.laissez_vib()) {
        tie_for_ = Factory::copy_laissez_vib(*to_laissez_vib(n.tie_for_));
        tie_for_->set_ownership_parent(this);
        tie_for_->set_start_note(this);
        tie_for_->set_tick(tick());
        if (link) {
            score()->undo(new Link(tie_for_, n.tie_for_));
        }
    } else if (n.outgoing_partial_tie()) {
        set_tie_for(Factory::copy_partial_tie(*to_partial_tie(n.tie_for_)));
        tie_for_->set_ownership_parent(this);
        tie_for_->set_start_note(this);
        tie_for_->set_tick(tick());
        if (link) {
            score()->undo(new Link(tie_for_, n.tie_for_));
        }
    } else if (n.tie_for_) {
        tie_for_ = Factory::copy_tie(*n.tie_for_);
        tie_for_->set_start_note(this);
        tie_for_->set_tick(tick());
        tie_for_->set_end_note(0);
    }

    if (n.incoming_partial_tie()) {
        set_tie_back(Factory::copy_partial_tie(*to_partial_tie(n.tie_back_)));
        tie_back_->set_ownership_parent(this);
        tie_back_->set_end_note(this);
        tie_back_->set_tick(tick());
        if (link) {
            score()->undo(new Link(tie_back_, n.tie_back_));
        }
    }

    for (NoteDot *dot : n.dots_) {
        add(Factory::copy_note_dot(*dot));
    }
    mark_ = n.mark_;

    set_drop_target(false);
}

void Note::set_ownership_parent(DummyParentOr<Chord> ch) { EngravingItem::set_ownership_parent(ch); }

//---------------------------------------------------------
//   concertPitchIdx
//---------------------------------------------------------

inline int Note::concert_pitch_idx() const { return concert_pitch() ? 0 : 1; }

//---------------------------------------------------------
//   setPitch
//---------------------------------------------------------

void Note::set_pitch(int val, bool notify_about_changed) {
    assert(pitch_is_valid(val));
    if (pitch_ != val) {
        pitch_ = val;

        if (notify_about_changed) {}
    }
}

void Note::set_pitch(int pitch, int tpc1, int tpc2) {
    assert(tpc_is_valid(tpc1));
    assert(tpc_is_valid(tpc2));
    tpc_[0] = tpc1;
    tpc_[1] = tpc2;
    set_pitch(pitch);
}

//---------------------------------------------------------
//   tpc1default
//---------------------------------------------------------

int Note::tpc1default(int p) const {
    Key key = Key::C;
    if (staff() && chord()) {
        Fraction tick = chord()->tick();
        key = staff()->concert_key(tick);
    }
    return pitch2tpc(p, key, Prefer::NEAREST);
}

//---------------------------------------------------------
//   tpc2default
//---------------------------------------------------------

int Note::tpc2default(int p) const {
    Key key = Key::C;
    if (staff() && chord()) {
        Fraction tick = chord()->tick();
        key = staff()->key(tick);
        if (concert_pitch()) {
            Interval interval = origin_part()->instrument(tick)->transpose();
            if (!interval.is_zero()) {
                interval.flip();
                key = Transpose::transpose_key(key, interval);
            }
        }
    }
    return pitch2tpc(p - transposition(), key, Prefer::NEAREST);
}

//---------------------------------------------------------
//   setTpcFromPitch
//---------------------------------------------------------

void Note::set_tpc_from_pitch(Prefer prefer /* = Prefer::NEAREST */) {
    // works best if note is already added to score, otherwise we can't determine transposition or key
    Fraction tick = chord() ? chord()->tick() : Fraction(-1, 1);
    Interval v = staff() ? staff()->transpose(tick) : Interval();
    Key c_key = (staff() && chord()) ? staff()->concert_key(chord()->tick()) : Key::C;
    // set concert pitch tpc
    tpc_[0] = pitch2tpc(pitch_, c_key, prefer);
    // set transposed tpc
    if (v.is_zero()) {
        tpc_[1] = tpc_[0];
    } else {
        v.flip();
        tpc_[1] = Transpose::transpose_tpc(tpc_[0], v, true);
    }
    assert(tpc_is_valid(tpc_[0]));
    assert(tpc_is_valid(tpc_[1]));
}

//---------------------------------------------------------
//   setTpc
//---------------------------------------------------------

void Note::set_tpc(int v) {
    IF_ASSERT_FAILED(tpc_is_valid(v)) { return; }
    tpc_[concert_pitch_idx()] = v;
}

//---------------------------------------------------------
//   undoSetTpc
//    change the current tpc
//---------------------------------------------------------

void Note::undo_set_tpc(int v) {
    if (concert_pitch()) {
        if (v != tpc1()) {
            undo_change_property(Pid::TPC1, v);
        }
    } else {
        if (v != tpc2()) {
            undo_change_property(Pid::TPC2, v);
        }
    }
}

//---------------------------------------------------------
//   tpc
//---------------------------------------------------------

int Note::tpc() const { return tpc_[concert_pitch_idx()]; }

//---------------------------------------------------------
//   tpcUserName
//---------------------------------------------------------

String Note::tpc_user_name(const bool explicit_accidental, bool full) const {
    String pitch_name = engraving::tpc_user_name(tpc(), epitch() + ottave_capo_fret(), explicit_accidental, full);

    pitch_name = muse::mtrc("global/pitchName", pitch_name);

    if (fixed() && head_group() == NoteHeadGroup::HEAD_SLASH) {
        // see Note::accessibleInfo(), but we return what we have
        return pitch_name;
    }
    if (staff()->is_drum_staff(tick()) && origin_part()->instrument(tick())->drumset()) {
        // see Note::accessibleInfo(), but we return what we have
        return pitch_name;
    }
    if (staff()->is_tab_staff(tick())) {
        // no further translation
        return pitch_name;
    }

    String pitch_offset;
    if (tuning() != 0) {
        static constexpr size_t buffer_size = 50;
        char buffer[buffer_size];
        snprintf(buffer, buffer_size, "%+.3f", tuning());
        pitch_offset = String::from_ascii(buffer);
    }

    if (!concert_pitch() && transposition()) {
        String sounding_pitch = engraving::tpc_user_name(tpc1(), ppitch(), explicit_accidental);
        sounding_pitch = muse::mtrc("global/pitchName", sounding_pitch);
        return muse::mtrc("engraving", "%1 (sounding as %2%3)").arg(pitch_name, sounding_pitch, pitch_offset);
    }
    return pitch_name + pitch_offset;
}

//---------------------------------------------------------
//   transposeTpc
//    return transposed tpc
//    If in concertPitch mode return tpc for transposed view
//    else return tpc for concert pitch view.
//---------------------------------------------------------

int Note::transpose_tpc(int tpc) const {
    Fraction tick = chord() ? chord()->tick() : Fraction(-1, 1);
    Interval v = staff()->transpose(tick);
    if (v.is_zero()) {
        return tpc;
    }
    if (concert_pitch()) {
        v.flip();
        return Transpose::transpose_tpc(tpc, v, true);
    } else {
        return Transpose::transpose_tpc(tpc, v, true);
    }
}

int Note::playing_tpc() const {
    int result = tpc();

    if (!concert_pitch() && transposition()) {
        int tpc1 = this->tpc1();
        if (tpc1 == Tpc::TPC_INVALID) {
            result = transpose_tpc(result);
        } else {
            result = tpc1;
        }
    }

    int steps = ottave_capo_fret();
    if (steps != 0) {
        result = Transpose::transpose_tpc(result, Interval(steps), true);
    }

    return result;
}

//---------------------------------------------------------
//   noteHead
//---------------------------------------------------------

SymId Note::note_head() const {
    int up;
    NoteHeadType ht;
    if (chord()) {
        up = chord()->up();
        ht = chord()->duration_type().head_type();
    } else {
        up = 1;
        ht = NoteHeadType::HEAD_QUARTER;
    }
    if (head_type_ != NoteHeadType::HEAD_AUTO) {
        ht = head_type_;
    }

    const Staff *st = chord() ? chord()->staff() : nullptr;

    NoteHeadGroup head_group = head_group_;
    if (head_group_ == NoteHeadGroup::HEAD_CUSTOM) {
        if (st) {
            if (st->staff_type_for_element(chord())->is_drum_staff()) {
                Fraction t = chord()->tick();
                Instrument *inst = origin_part()->instrument(t);
                Drumset *d = inst->drumset();
                if (d) {
                    return d->note_heads(pitch_, ht);
                } else {
                    LOGD("no drumset");
                    return note_head(up, NoteHeadGroup::HEAD_NORMAL, ht);
                }
            } else {
                head_group = NoteHeadGroup::HEAD_NORMAL;
            }
        } else {
            return ldata()->cached_notehead_sym.value();
        }
    }

    Key key = Key::C;
    NoteHeadScheme scheme = head_scheme_;
    if (st) {
        Fraction tick = chord()->tick();
        if (tick >= Fraction(0, 1)) {
            key = st->key(tick);
            if (scheme == NoteHeadScheme::HEAD_AUTO) {
                scheme = st->staff_type_for_element(chord())->note_head_scheme();
            }
        }
    }
    if (scheme == NoteHeadScheme::HEAD_AUTO) {
        scheme = NoteHeadScheme::HEAD_NORMAL;
    }
    SymId t = note_head(up, head_group, ht, tpc(), key, scheme);
    if (t == SymId::NO_SYM) {
        LOGD("invalid notehead %d/%d", int(head_group), int(ht));
        t = note_head(up, NoteHeadGroup::HEAD_NORMAL, ht);
    }
    return t;
}

//---------------------------------------------------------
//   headWidth
//
//    returns the x of the symbol bbox. It is different from headWidth() because zero point could be different from leftmost bbox position.
//---------------------------------------------------------
double Note::bbox_right_pos() const {
    const auto &bbox = score()->engraving_font()->bbox(note_head(), mag_s());
    return bbox.right();
}

//---------------------------------------------------------
//   headBodyWidth
//
//    returns the width of the notehead "body". It is actual for slashed noteheads like -O-, where O is body.
//---------------------------------------------------------
double Note::head_body_width() const {
    const StaffType *st = staff_type();
    if (st && st->is_tab_staff()) {
        return tab_head_width(st);
    }
    return head_width() + 2 * bbox_x_shift();
}

//---------------------------------------------------------
//   outsideTieAttachX
//
//    returns the X-position for tie attachment for this particular notehead
//---------------------------------------------------------
double Note::outside_tie_attach_x(bool up) const {
    double xo = 0;

    // tab staff notes just use center of bounding box
    if (staff_type()->is_tab_staff()) {
        return x() + ((width() / 2) * mag());
    }
    // special cases:
    if (head_group_ == NoteHeadGroup::HEAD_SLASH) {
        xo = (up ? head_width() * 0.75 : head_width() * 0.25);
        if (chord()->duration_type().has_stem()) {
            // for quarters and halves, we can safely move a little bit outwards
            xo += spatium() * 0.13 * (chord()->up() ? -mag() : mag());
        }
        return x() + xo;
    }
    if (head_group_ == NoteHeadGroup::HEAD_SLASHED1 || head_group_ == NoteHeadGroup::HEAD_SLASHED2) {
        // just use the very center of the notehead
        return x() + ((head_body_width() / 2) * mag());
    }
    /* Noteheads do not have optical centers at this time, but here's the code
       to future-proof
    xo = symSmuflAnchor(noteHead(), SmuflAnchorId::opticalCenter).x() * mag();
    if (xo > 0) {
        return x() + xo;
    }
    */
    // try for average of cutouts
    if (up) {
        double x_ne = sym_smufl_anchor(note_head(), SmuflAnchorId::CUT_OUT_NE).x();
        double x_nw = sym_smufl_anchor(note_head(), SmuflAnchorId::CUT_OUT_NW).x();
        xo = ((x_ne + x_nw) / 2);
        if (x_ne < x_nw) {
            // musejazz is busted
            xo = 0;
        }
    } else {
        double x_se = sym_smufl_anchor(note_head(), SmuflAnchorId::CUT_OUT_SE).x();
        double x_sw = sym_smufl_anchor(note_head(), SmuflAnchorId::CUT_OUT_SW).x();
        xo = ((x_se + x_sw) / 2);
        if (x_se < x_sw) {
            xo = 0;
        }
    }
    if (xo > 0) {
        return x() + xo;
    }
    // no cutout, not a slash head, default to middle of notehead
    return x() + (head_width() / 2);
}

void Note::update_head_group(const NoteHeadGroup head_group) {
    NoteHeadGroup group = head_group;

    if (group == NoteHeadGroup::HEAD_INVALID) {
        LOGD("unknown notehead");
        group = NoteHeadGroup::HEAD_NORMAL;
    }

    if (group == head_group_) {
        return;
    }

    if (links()) {
        for (EngravingObject *score_element : *links()) {
            score_element->undo_change_property(Pid::HEAD_GROUP, group);
            Note *note = to_note(score_element);

            if (note->staff() && !note->staff()->is_drum_staff(chord()->tick()) && group == NoteHeadGroup::HEAD_CROSS) {
                score_element->undo_change_property(Pid::DEAD, true);
            }
        }
    } else {
        undo_change_property(Pid::HEAD_GROUP, group);
    }
}

//---------------------------------------------------------
//   headWidth
//
//    returns the width of the symbol bbox
//    or the width of the string representation of the fret mark
//---------------------------------------------------------

double Note::head_width() const { return sym_width(note_head()); }

//---------------------------------------------------------
//   bboxXShift
//
//    returns the x shift of the notehead bounding box
//---------------------------------------------------------
double Note::bbox_x_shift() const {
    const auto &bbox = score()->engraving_font()->bbox(note_head(), mag_s());
    return bbox.bottom_left().x();
}

//---------------------------------------------------------
//   noteheadCenterX
//
//    returns the x coordinate of the notehead center related to the basepoint of the notehead bbox
//---------------------------------------------------------
double Note::notehead_center_x() const { return score()->engraving_font()->width(note_head(), mag_s()) / 2 + bbox_x_shift(); }

//---------------------------------------------------------
//   tabHeadWidth
//---------------------------------------------------------

double Note::tab_head_width(const StaffType *tab) const {
    double val;
    if (tab && tab->is_tab_staff() && fret_ != ::mu::engraving::invalid_fret_index && string_ != ::mu::engraving::invalid_string_index) {
        Font f = tab->fret_font();
        f.set_point_size_f(tab->fret_font_size());
        val = FontMetrics::width(f, fret_string_) * mag_s();
    } else {
        val = head_width();
    }
    return val;
}

//---------------------------------------------------------
//   headHeight
//
//    returns the height of the notehead symbol
//    or the height of the string representation of the fret mark
//---------------------------------------------------------

double Note::head_height() const { return sym_height(note_head()); }

//---------------------------------------------------------
//   tabHeadHeight
//---------------------------------------------------------

double Note::tab_head_height(const StaffType *tab) const {
    if (tab && fret_ != ::mu::engraving::invalid_fret_index && string_ != ::mu::engraving::invalid_string_index) {
        return tab->fret_box_h() * mag_s();
    }
    return head_height();
}

//---------------------------------------------------------
//   stemDownNW
//---------------------------------------------------------

PointF Note::stem_down_nw() const { return sym_smufl_anchor(note_head(), SmuflAnchorId::STEM_DOWN_NW); }

//---------------------------------------------------------
//   stemUpSE
//---------------------------------------------------------

PointF Note::stem_up_se() const { return sym_smufl_anchor(note_head(), SmuflAnchorId::STEM_UP_SE); }

//---------------------------------------------------------
//   playTicks
///   Return total tick len of tied notes
//---------------------------------------------------------

int Note::play_ticks() const { return play_ticks_fraction().ticks(); }

//---------------------------------------------------------
//   playTicksFraction
///   Return total tick len of tied notes
//---------------------------------------------------------

Fraction Note::play_ticks_fraction() const {
    if (!tie_back_ && !tie_for_ && chord()) {
        return chord()->actual_ticks();
    }
    return last_tied_note()->chord()->end_tick() - first_tied_note()->chord()->tick();
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

void Note::add_spanner(Spanner *l) {
    EngravingItem *e = l->end_element();
    if (e && e->is_note()) {
        Note *note = to_note(e);
        note->add_spanner_back(l);
        bool is_note_anchored_text_line = l->is_note_line() && to_note_line(l)->enforce_min_length();
        if (l->is_glissando() || l->is_guitar_bend() || is_note_anchored_text_line) {
            note->chord()->set_ends_note_anchored_line(true);
        }
    }
    add_spanner_for(l);
}

//---------------------------------------------------------
//   removeSpanner
//---------------------------------------------------------

void Note::remove_spanner(Spanner *l) {
    Note *e = to_note(l->end_element());
    if (e && e->is_note()) {
        if (!e->remove_spanner_back(l)) {
            LOGD("Note::removeSpanner(%p): cannot remove spannerBack %s %p", this, l->type_name(), l);
            // abort();
        }
        if (l->is_glissando()) {
            e->chord()->update_ends_note_anchored_line();
        }
    }
    if (!remove_spanner_for(l)) {
        LOGD("Note(%p): cannot remove spannerFor %s %p", this, l->type_name(), l);
        // abort();
    }
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Note::add(EngravingItem *e) {
    if (e->ownership_parent() != this) {
        e->set_ownership_parent(this);
    }
    e->set_track(track());

    switch (e->type()) {
        case ElementType::NOTEDOT: dots_.push_back(to_note_dot(e)); break;
        case ElementType::BEND:
        case ElementType::FINGERING:
        case ElementType::IMAGE:
        case ElementType::TEXT: el_.push_back(e); break;
        case ElementType::SYMBOL: {
            el_.push_back(e);
        } break;
        case ElementType::LAISSEZ_VIB: {
            LaissezVib *lv = to_laissez_vib(e);
            lv->set_start_note(this);
            lv->set_tick(lv->start_note()->tick());
            set_tie_for(lv);
            break;
        }
        case ElementType::PARTIAL_TIE: {
            PartialTie *pt = to_partial_tie(e);
            pt->set_tick(tick());
            if (pt->is_outgoing()) {
                pt->set_start_note(this);
                set_tie_for(pt);
            } else {
                pt->set_end_note(this);
                set_tie_back(pt);
            }
            break;
        }
        case ElementType::TIE: {
            Tie *tie = to_tie(e);
            tie->set_start_note(this);
            tie->set_tick(tie->start_note()->tick());
            set_tie_for(tie);
            if (tie->end_note()) {
                tie->end_note()->set_tie_back(tie);
            }
        } break;
        case ElementType::ACCIDENTAL:
            accidental_ = to_accidental(e);
            cent_offset_ = Accidental::subtype2cent_offset(to_accidental(e)->accidental_type());
            break;
        case ElementType::TEXTLINE:
        case ElementType::NOTELINE:
        case ElementType::GLISSANDO:
        case ElementType::GUITAR_BEND: add_spanner(to_spanner(e)); break;
        default: EngravingItem::add(e); break;
    }
    trigger_layout();

    e->added();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Note::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::NOTEDOT: dots_.pop_back(); break;

        case ElementType::BEND:
        case ElementType::TEXT:
        case ElementType::IMAGE:
        case ElementType::FINGERING:
            if (!el_.remove(e)) {
                LOGD("Note::remove(): cannot find %s", e->type_name());
            }
            break;
        case ElementType::SYMBOL:
            if (!el_.remove(e)) {
                LOGD("Note::remove(): cannot find %s", e->type_name());
            }
            break;
        case ElementType::PARTIAL_TIE: {
            PartialTie *pt = to_partial_tie(e);
            assert((pt->is_outgoing() ? pt->start_note() : pt->end_note()) == this);
            if (pt->is_outgoing()) {
                set_tie_for(nullptr);
            } else {
                set_tie_back(nullptr);
                pt->set_jump_point(nullptr);
            }
            break;
        }
        case ElementType::LAISSEZ_VIB:
        case ElementType::TIE: {
            Tie *tie = to_tie(e);
            assert(tie->start_note() == this);
            set_tie_for(nullptr);
            tie->set_jump_point(nullptr);
            if (tie->end_note()) {
                tie->end_note()->set_tie_back(0);
            }
        } break;

        case ElementType::ACCIDENTAL:
            accidental_ = 0;
            cent_offset_ = 0;
            break;

        case ElementType::TEXTLINE:
        case ElementType::NOTELINE:
        case ElementType::GLISSANDO:
        case ElementType::GUITAR_BEND: remove_spanner(to_spanner(e)); break;

        default: EngravingItem::remove(e); break;
    }
    trigger_layout();
    e->removed();
}

//---------------------------------------------------------
//   isNoteName
//---------------------------------------------------------

bool Note::is_note_name() const {
    if (chord() && chord()->staff()) {
        const Staff *st = staff();
        NoteHeadScheme s = head_scheme_;
        if (s == NoteHeadScheme::HEAD_AUTO) {
            s = st->staff_type_for_element(this)->note_head_scheme();
        }
        return s == NoteHeadScheme::HEAD_PITCHNAME || s == NoteHeadScheme::HEAD_PITCHNAME_GERMAN ||
               s == NoteHeadScheme::HEAD_PITCHNAME_NO_ACCIDENTALS || s == NoteHeadScheme::HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS ||
               s == NoteHeadScheme::HEAD_SOLFEGE || s == NoteHeadScheme::HEAD_SOLFEGE_FIXED;
    }
    return false;
}

void Note::update_fretting_for_ties_and_bends() {
    Note *prev_note = nullptr;
    if (tie_back_) {
        prev_note = tie_back_->start_note();
    } else {
        GuitarBend *bend = bend_back();
        if (bend) {
            prev_note = bend->start_note();
        }
    }

    if (!prev_note) {
        return;
    }

    set_string(prev_note->string());
    set_fret(prev_note->fret());
}

bool Note::should_hide_fret() const {
    if (!tie_back() || should_force_show_fret() || !staff_type()->is_tab_staff()) {
        return false;
    }

    if (is_continuation_of_bend() && !rtick().is_zero()) {
        return true;
    }

    ShowTiedFret show_tied_fret = style().value(Sid::TAB_SHOW_TIED_FRET).value<ShowTiedFret>();
    if (show_tied_fret == ShowTiedFret::TIE_AND_FRET) {
        return false;
    }

    ParenthesizeTiedFret parenth_tied_fret = style().value(Sid::TAB_PARENTHESIZE_TIED_FRET).value<ParenthesizeTiedFret>();
    if (parenth_tied_fret == ParenthesizeTiedFret::NEVER || !rtick().is_zero()) {
        return true;
    }

    if (parenth_tied_fret == ParenthesizeTiedFret::START_OF_MEASURE) {
        return false;
    }

    const Measure *measure = find_measure();
    bool is_start_of_system = measure && measure->system() && measure->is_first_in_system();

    return !is_start_of_system;
}

bool Note::should_force_show_fret() const {
    if (!style().style_b(Sid::PARENTHESIZE_TIED_FRET_IF_ARTICULATION)) {
        return false;
    }

    Chord *ch = chord();
    if (!ch) {
        return false;
    }

    auto has_tremolo_bar = [&]() {
        for (EngravingItem *item : ch->segment()->annotations()) {
            if (item && item->is_tremolo_bar() && item->track() == track()) {
                return true;
            }
        }
        return false;
    };

    auto has_vibrato_line = [&]() {
        auto spanners = score()->spanner_map().find_overlapping(tick().ticks(), ch->end_tick().ticks());
        for (auto interval : spanners) {
            Spanner *sp = interval.value;
            if (sp->is_vibrato() && sp->start_element() == ch) {
                return true;
            }
        }
        return false;
    };

    const GuitarBend *bend_f = bend_for();
    bool start_unconnected_bend = bend_f && !bend_f->find_preceding_bend();
    bool starts_non_bend_spanner = !spanner_for().empty() && !bend_f;

    bool chord_has_dip = false;
    for (Note *note : chord()->notes()) {
        for (Spanner *sp : note->spanner_for()) {
            if (sp->is_guitar_bend() && to_guitar_bend(sp)->bend_type() == GuitarBendType::DIP) {
                chord_has_dip = true;
                break;
            }
        }
        if (chord_has_dip) {
            break;
        }
    }

    return !ch->articulations().empty() || ch->chord_line() || starts_non_bend_spanner || start_unconnected_bend || has_tremolo_bar() ||
           has_vibrato_line() || chord_has_dip;
}

void Note::set_visible(bool v) {
    EngravingItem::set_visible(v);
    if (!chord() || chord()->note_parentheses().empty()) {
        return;
    }

    const NoteParenthesisInfo *note_paren_info = chord()->find_note_parenthesis_info(this);

    if (!note_paren_info) {
        return;
    }

    const std::vector<Note *> &notes = note_paren_info->notes();
    bool visible = false;
    for (const Note *note : notes) {
        if (note->visible()) {
            visible = true;
            break;
        }
    }

    if (note_paren_info->left_paren()) {
        note_paren_info->left_paren()->set_visible(visible);
    }
    if (note_paren_info->right_paren()) {
        note_paren_info->right_paren()->set_visible(visible);
    }
}

bool Note::is_exact_unison(Note *other) { return other->pitch() == pitch_ && other->tpc() == tpc(); }

void Note::setup_after_read(const Fraction &ctx_tick, bool paste_mode) {
    // ensure sane values:
    pitch_ = std::clamp(pitch_, 0, 127);

    if (!tpc_is_valid(tpc_[0]) && !tpc_is_valid(tpc_[1])) {
        Key key = (staff() && chord()) ? staff()->key(chord()->tick()) : Key::C;
        int tpc = pitch2tpc(pitch_, key, Prefer::NEAREST);
        if (concert_pitch()) {
            tpc_[0] = tpc;
        } else {
            tpc_[1] = tpc;
        }
    }
    if (!(tpc_is_valid(tpc_[0]) && tpc_is_valid(tpc_[1]))) {
        Fraction tick = chord() ? chord()->tick() : Fraction(-1, 1);
        Interval v = staff() ? staff()->transpose(tick) : Interval();
        if (tpc_is_valid(tpc_[0])) {
            v.flip();
            if (v.is_zero()) {
                tpc_[1] = tpc_[0];
            } else {
                tpc_[1] = Transpose::transpose_tpc(tpc_[0], v, true);
            }
        } else {
            if (v.is_zero()) {
                tpc_[0] = tpc_[1];
            } else {
                tpc_[0] = Transpose::transpose_tpc(tpc_[1], v, true);
            }
        }
    }

    // check consistency of pitch, tpc1, tpc2, and transposition
    // see note in InstrumentChange::read() about a known case of tpc corruption produced in 2.0.x
    // but since there are other causes of tpc corruption (eg, https://musescore.org/en/node/74746)
    // including perhaps some we don't know about yet,
    // we will attempt to fix some problems here regardless of version

    if (staff() && !staff()->is_drum_staff(ctx_tick) && !paste_mode && !MScore::test_mode) {
        int tpc1_pitch = (tpc2pitch(tpc_[0]) + 12) % 12;
        int tpc2_pitch = (tpc2pitch(tpc_[1]) + 12) % 12;
        int sounding_pitch = pitch_ % 12;
        if (tpc1_pitch != sounding_pitch) {
            LOGD("bad tpc1 - soundingPitch = %d, tpc1 = %d", sounding_pitch, tpc1_pitch);
            pitch_ += tpc1_pitch - sounding_pitch;
        }
        if (staff()) {
            Interval v = staff()->transpose(ctx_tick);
            int written_pitch = (pitch_ - v.chromatic) % 12;
            if (tpc2_pitch != written_pitch) {
                LOGD("bad tpc2 - writtenPitch = %d, tpc2 = %d", written_pitch, tpc2_pitch);
                if (concert_pitch()) {
                    // assume we want to keep sounding pitch
                    // so fix written pitch (tpc only)
                    v.flip();
                    tpc_[1] = Transpose::transpose_tpc(tpc_[0], v, true);
                } else {
                    // assume we want to keep written pitch
                    // so fix sounding pitch (both tpc and pitch)
                    tpc_[0] = Transpose::transpose_tpc(tpc_[1], v, true);
                    pitch_ += tpc2_pitch - written_pitch;
                }
            }
        }
    }

    const StaffType *st = staff_type();
    if (st && st->is_tab_staff() && st->fret_use_text_style() && color() == configuration()->default_color()) {
        set_color(property_default(Pid::COLOR).value<Color>());
    }
}

//---------------------------------------------------------
//   transposition
//---------------------------------------------------------

int Note::transposition() const {
    Fraction tick = chord() ? chord()->tick() : Fraction(-1, 1);
    Part *origin = origin_part();
    return origin ? origin->instrument(tick)->transpose().chromatic : 0;
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Note::accept_drop(EditData &data) const {
    EngravingItem *e = data.drop_element;
    switch (e->type()) {
        case ElementType::GLISSANDO:
        case ElementType::GUITAR_BEND:
            for (auto ee : spanner_for_) {
                if (ee->is_glissando() || ee->is_guitar_bend()) {
                    return false;
                }
            }
            return true;
        case ElementType::FINGERING: return staff()->is_tab_staff(tick()) ? staff_type()->show_tab_fingering() : true;
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING:
        case ElementType::CHORDLINE:
        case ElementType::TEXT:
        case ElementType::ACCIDENTAL:
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET:
        case ElementType::NOTEHEAD:
        case ElementType::NOTE:
        case ElementType::TREMOLO_SINGLECHORD:
        case ElementType::TREMOLO_TWOCHORD:
        case ElementType::IMAGE:
        case ElementType::CHORD:
        case ElementType::SYMBOL:
        case ElementType::BAR_LINE:
        case ElementType::STICKING:
        case ElementType::BEND:
        case ElementType::FIGURED_BASS:
        case ElementType::LYRICS: return true;
        case ElementType::BAGPIPE_EMBELLISHMENT: return note_type() == NoteType::NORMAL;
        case ElementType::ACTION_ICON: {
            switch (to_action_icon(e)->action_type()) {
                case ActionIconType::ACCIACCATURA:
                case ActionIconType::APPOGGIATURA:
                case ActionIconType::GRACE4:
                case ActionIconType::GRACE16:
                case ActionIconType::GRACE32:
                case ActionIconType::GRACE8_AFTER:
                case ActionIconType::GRACE16_AFTER:
                case ActionIconType::GRACE32_AFTER:
                case ActionIconType::PARENTHESES:
                case ActionIconType::STANDARD_BEND:
                case ActionIconType::PRE_BEND:
                case ActionIconType::GRACE_NOTE_BEND:
                case ActionIconType::SLIGHT_BEND:
                case ActionIconType::DIVE:
                case ActionIconType::PRE_DIVE:
                case ActionIconType::DIP:
                case ActionIconType::SCOOP:
                case ActionIconType::NOTE_ANCHORED_LINE: return true;
                default: break;
            }
            break;
        }
        default:
            if (e->is_spanner()) {
                return e->type() != ElementType::TIE && e->type() != ElementType::PARTIAL_TIE;
            }
            break;
    }
    return to_chord_rest(chord())->accept_drop(data);
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Note::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;

    const Staff *st = staff();
    bool is_tablature = st->is_tab_staff(tick());
    bool tab_fingering = st->staff_type_for_element(this)->show_tab_fingering();
    Chord *ch = chord();

    switch (e->type()) {
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
            e->set_ownership_parent(this);
            score()->undo_add_element(e);
            return e;

        case ElementType::FINGERING:
            if (!is_tablature || tab_fingering) {
                e->set_ownership_parent(this);
                score()->undo_add_element(e);
                return e;
            } else {
                delete e;
            }
            return 0;

        case ElementType::LYRICS:
            e->set_ownership_parent(ch);
            e->set_track(track());
            score()->undo_add_element(e);
            return e;

        case ElementType::ACCIDENTAL: EditNote::change_accidental(score(), this, to_accidental(e)->accidental_type()); break;

        case ElementType::BEND:
            e->set_ownership_parent(this);
            e->set_track(track());
            score()->undo_add_element(e);
            return e;

        case ElementType::NOTEHEAD: {
            NoteHead *s = to_note_head(e);
            NoteHeadGroup group = s->head_group();
            if (group == NoteHeadGroup::HEAD_INVALID) {
                LOGD("unknown notehead");
                group = NoteHeadGroup::HEAD_NORMAL;
            }
            delete s;

            if (group != head_group_) {
                if (links()) {
                    for (EngravingObject *se : *links()) {
                        se->undo_change_property(Pid::HEAD_GROUP, group);
                        Note *note = to_note(se);
                        if (note->staff() && !note->staff()->is_drum_staff(ch->tick())) {
                            se->undo_change_property(Pid::DEAD, group == NoteHeadGroup::HEAD_CROSS);
                        }
                    }
                } else {
                    undo_change_property(Pid::HEAD_GROUP, group);
                    if (!staff()->is_drum_staff(ch->tick())) {
                        undo_change_property(Pid::DEAD, group == NoteHeadGroup::HEAD_CROSS);
                    }
                }
            }
        } break;

        case ElementType::ACTION_ICON: {
            switch (to_action_icon(e)->action_type()) {
                case ActionIconType::ACCIACCATURA: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::ACCIACCATURA, Constants::division / 2);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::APPOGGIATURA: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::APPOGGIATURA, Constants::division / 2);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::GRACE4: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::GRACE4, Constants::division);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::GRACE16: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::GRACE16, Constants::division / 4);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::GRACE32: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::GRACE32, Constants::division / 8);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::GRACE8_AFTER: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::GRACE8_AFTER, Constants::division / 2);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::GRACE16_AFTER: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::GRACE16_AFTER, Constants::division / 4);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case ActionIconType::GRACE32_AFTER: {
                    Note *note = score()->set_grace_note(ch, pitch(), NoteType::GRACE32_AFTER, Constants::division / 8);
                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }

                case ActionIconType::PARENTHESES: {
                    EditParentheses::add_parentheses_to_notes(tx, {this});
                    break;
                }
                case ActionIconType::STANDARD_BEND:
                case ActionIconType::SLIGHT_BEND:
                case ActionIconType::DIVE:
                case ActionIconType::DIP:
                case ActionIconType::SCOOP:
                    score()->add_guitar_bend(GuitarBend::bend_type_from_action_icon(to_action_icon(e)->action_type()), this);
                    break;
                case ActionIconType::PRE_BEND:
                case ActionIconType::GRACE_NOTE_BEND:
                case ActionIconType::PRE_DIVE: {
                    GuitarBendType type = GuitarBend::bend_type_from_action_icon(to_action_icon(e)->action_type());
                    GuitarBend *guitar_bend = score()->add_guitar_bend(type, this);
                    if (!guitar_bend) {
                        break;
                    }
                    Note *note = guitar_bend->start_note();
                    IF_ASSERT_FAILED(note) {
                        LOGE() << "not valid start note of the bend";
                        break;
                    }

                    score()->select(note, SelectType::SINGLE, 0);
                    break;
                }
                case mu::engraving::ActionIconType::NOTE_ANCHORED_LINE: score()->add_note_line();
                default: break;
            }
            return ch->drop(tx, data);
        }

        case ElementType::GUITAR_BEND: {
            GuitarBend *new_guitar_bend = score()->add_guitar_bend(to_guitar_bend(e)->bend_type(), this);
            delete e;
            return new_guitar_bend;
        }

        case ElementType::BAGPIPE_EMBELLISHMENT: {
            BagpipeEmbellishment *b = to_bagpipe_embellishment(e);
            BagpipeNoteList nl = b->resolve_note_list();
            // add grace notes in reverse order, as setGraceNote adds a grace note
            // before the current note
            for (int i = static_cast<int>(nl.size()) - 1; i >= 0; --i) {
                int p = BagpipeEmbellishment::bagpipe_noteinfo_list[nl.at(i)].pitch;
                score()->set_grace_note(ch, p, NoteType::GRACE32, Constants::division / 8);
            }
        }
            delete e;
            break;

        case ElementType::NOTE: {
            // calculate correct transposed tpc
            Note *n = to_note(e);
            const Segment *segment = ch->segment();
            Interval v = staff()->transpose(ch->tick());
            v.flip();
            n->set_tpc2(Transpose::transpose_tpc(n->tpc1(), v, true));
            // replace this note with new note
            n->set_ownership_parent(ch);
            if (this->tie_back()) {
                n->set_tie_back(this->tie_back());
                n->tie_back()->set_end_note(n);
                this->set_tie_back(nullptr);
            }
            // Set correct stem direction for drum staves
            const StaffGroup staff_group = st->staff_type(segment->tick())->group();
            DirectionV stem_direction = DirectionV::AUTO;
            if (staff_group == StaffGroup::PERCUSSION) {
                const Drumset *ds = origin_part()->instrument(segment->tick())->drumset();
                DO_ASSERT(ds);

                if (ds) {
                    stem_direction = ds->stem_direction(n->note_val().pitch);
                }
            }
            ch->set_stem_direction(stem_direction);

            score()->undo_remove_element(this);
            score()->undo_add_element(n);
            return n;
        } break;

        case ElementType::GLISSANDO: {
            for (auto ee : spanner_for_) {
                if (ee->is_glissando()) {
                    LOGD("there is already a glissando");
                    delete e;
                    return 0;
                }
            }

            Glissando *gliss = to_glissando(e);
            EngravingItem *end_el = gliss->end_element();
            Note *final_note = end_el && end_el->is_note() ? to_note(end_el) : SLine::guess_final_note(this);
            if (final_note) {
                // init glissando data
                gliss->set_start_element(this);
                gliss->set_end_element(final_note);
                gliss->set_tick(ch->tick());
                gliss->set_tick2(final_note->chord()->tick());
                gliss->set_track(track());
                gliss->set_track2(final_note->track());
                // in TAB, use straight line with no text
                if (staff()->is_tab_staff(final_note->chord()->tick())) {
                    gliss->set_glissando_type(GlissandoType::STRAIGHT);
                    gliss->set_show_text(false);
                }
                gliss->set_ownership_parent(this);

                const Sid style_id = gliss->get_property_style(Pid::GLISS_STYLE);
                if (gliss->is_styled(Pid::GLISS_STYLE) && score()->style().is_default(style_id)) {
                    const GlissandoStyle instrument_style = origin_part()->instrument(gliss->tick())->glissando_style();
                    if (instrument_style != gliss->glissando_style()) {
                        gliss->set_glissando_style(instrument_style);
                        gliss->set_property_flags(Pid::GLISS_STYLE, PropertyFlags::UNSTYLED);
                    }
                }

                score()->undo_add_element(e);
            } else {
                LOGD("no segment for second note of glissando found");
                delete e;
                return 0;
            }
        } break;

        case ElementType::CHORD: {
            Chord *c = to_chord(e);
            Note *n = c->up_note();
            DirectionV dir = c->stem_direction();
            track_idx_t t = track(); // (staff2track(staffIdx()) + n->voice());
            score()->select(0, SelectType::SINGLE, 0);
            NoteVal nval;
            nval.pitch = n->pitch();
            nval.head_group = n->head_group();
            const ChordRest *cr = nullptr;
            if (data.modifiers & SHIFT_MODIFIER) {
                // add note to chord
                NoteInput::add_note(tx, score(), ch, nval);
            } else {
                // replace current chord
                Segment *seg = score()->set_note_rest(ch->segment(), t, nval, score()->input_state().duration().fraction(), dir);
                cr = seg ? to_chord_rest(seg->element(t)) : nullptr;
            }
            if (cr) {
                NoteInput::next_input_pos(tx, score(), cr, false);
            }
            delete e;
        } break;

        case ElementType::CHORDLINE: to_chord_line(e)->set_note(this); return ch->drop(tx, data);

        default:
            Spanner *spanner;
            if (e->is_spanner() && (spanner = to_spanner(e))->anchor() == Spanner::Anchor::NOTE) {
                spanner->set_ownership_parent(this);
                spanner->set_start_element(this);
                spanner->set_tick(tick());
                spanner->set_track(track());
                spanner->set_track2(track());
                spanner->compute_end_element();
                score()->undo_add_element(spanner);
                return e;
            }
            return ch->drop(tx, data);
    }
    return 0;
}

//---------------------------------------------------------
//   dotIsUp
//---------------------------------------------------------

bool Note::dot_is_up() const {
    if (dots_.empty()) {
        return true;
    }
    if (user_dot_position_ == DirectionV::AUTO) {
        return dots_[0]->y() < spatium() * .1;
    } else {
        return user_dot_position_ == DirectionV::UP;
    }
}

static bool has_altered_unison(Note *note) {
    const auto &chord_notes = note->chord()->notes();
    AccidentalVal acc_val = tpc2alter(note->tpc());
    int abs_line = abs_step(note->tpc(), note->epitch());
    return std::find_if(chord_notes.begin(), chord_notes.end(), [note, acc_val, abs_line](Note *n) {
               return n != note && !n->hidden() && abs_step(n->tpc(), n->epitch()) == abs_line && tpc2alter(n->tpc()) != acc_val;
           }) != chord_notes.end();
}

//---------------------------------------------------------
//   updateAccidental
//    set _accidental and _line depending on tpc
//---------------------------------------------------------

void Note::update_accidental(AccidentalState *as) {
    if (dead_note() && configuration()->keep_dead_notes_unchanged_on_transpose()) {
        if (accidental_) {
            score()->undo_remove_element(accidental_);
        }
        int e_abs_line = abs_step(tpc(), epitch());
        as->set_accidental_val(e_abs_line, tpc2alter(tpc()), false);
        update_rel_line(e_abs_line, true);
        return;
    }

    int abs_line = abs_step(tpc(), epitch());

    // Ensure centOffset_ and microtonal accidental match (they can mismatch when switching from TAB)
    if (muse::real_is_null(cent_offset_)) {
        if (accidental_ && !muse::real_is_null(Accidental::subtype2cent_offset(accidental_->accidental_type()))) {
            score()->undo_remove_element(accidental_);
        }
    } else {
        if (accidental_) {
            bool correct = muse::real_is_equal(Accidental::subtype2cent_offset(accidental_->accidental_type()), cent_offset_);
            if (!correct) {
                accidental_->undo_change_property(Pid::ACCIDENTAL_TYPE, static_cast<int>(Accidental::cent_offset2_subtype(cent_offset_)));
            }
        } else {
            AccidentalType acc_type = Accidental::value2_microtonal_subtype(tpc2alter(tpc()), quarter_tone_offset());
            update_line();
            EditNote::change_accidental(score(), this, acc_type);
        }
    }

    // don't touch accidentals that don't concern tpc such as
    // quarter tones
    if (!(accidental_ && Accidental::is_microtonal(accidental_->accidental_type()))) {
        // calculate accidental
        AccidentalType acci = AccidentalType::NONE;

        AccidentalVal acc_val = tpc2alter(tpc());
        bool error = false;
        int e_abs_line = abs_step(tpc(), epitch());
        AccidentalVal abs_line_acc_val = as->accidental_val(e_abs_line, error);
        if (error) {
            LOGD("error accidentalVal()");
            return;
        }
        if ((acc_val != abs_line_acc_val) || hidden() || as->tie_context(e_abs_line) || as->force_restate_accidental(e_abs_line)) {
            as->set_accidental_val(e_abs_line, acc_val, tie_back_ != 0 && accidental_ == 0);
            acci = Accidental::value2subtype(acc_val);
            // if previous tied note has same tpc, don't show accidental
            if (tie_back_non_partial() && tie_back_->start_note()->tpc1() == tpc1()) {
                acci = AccidentalType::NONE;
            } else if (acci == AccidentalType::NONE) {
                acci = AccidentalType::NATURAL;
            }
        } else if (has_altered_unison(this)) {
            if ((acci = Accidental::value2subtype(acc_val)) == AccidentalType::NONE) {
                acci = AccidentalType::NATURAL;
            }
        }
        if (acci != AccidentalType::NONE && !hidden_) {
            if (accidental_ == 0) {
                Accidental *a = Factory::create_accidental(this);
                a->set_accidental_type(acci);
                a->set_visible(visible());
                score()->undo_add_element(a);
            } else if (accidental_->accidental_type() != acci) {
                Accidental *a = accidental_->clone();
                a->set_ownership_parent(this);
                a->set_accidental_type(acci);
                score()->undo_change_element(accidental_, a);
            }
        } else {
            if (accidental_) {
                // remove this if it was AUTO:
                if (accidental_->role() == AccidentalRole::AUTO) {
                    score()->undo_remove_element(accidental_);
                } else {
                    // keep it, but update type if needed
                    acci = Accidental::value2subtype(acc_val);
                    if (acci == AccidentalType::NONE) {
                        acci = AccidentalType::NATURAL;
                    }
                    if (accidental_->accidental_type() != acci) {
                        Accidental *a = accidental_->clone();
                        a->set_ownership_parent(this);
                        a->set_accidental_type(acci);
                        score()->undo_change_element(accidental_, a);
                    }
                }
            }
        }
    } else {
        // microtonal accidentals playback as naturals
        // in 1.X, they had no effect on accidental state of measure
        // ultimately, they should probably get their own state
        // for now, at least change state to natural, so subsequent notes playback as might be expected
        // this is an incompatible change, but better to break it for 2.0 than wait until later
        AccidentalVal acc_val = Accidental::subtype2value(accidental_->accidental_type());
        as->set_accidental_val(abs_line, acc_val, tie_back_ != 0 && accidental_ == 0);
    }

    as->set_force_restate_accidental(abs_line, false);
    update_rel_line(abs_line, true);
}

//---------------------------------------------------------
//   noteType
//---------------------------------------------------------

NoteType Note::note_type() const { return chord()->note_type(); }

//---------------------------------------------------------
//   noteTypeUserName
//---------------------------------------------------------

String Note::note_type_user_name() const {
    switch (note_type()) {
        case NoteType::ACCIACCATURA: return muse::mtrc("engraving", "Acciaccatura");
        case NoteType::APPOGGIATURA: return muse::mtrc("engraving", "Appoggiatura");
        case NoteType::GRACE8_AFTER:
        case NoteType::GRACE16_AFTER:
        case NoteType::GRACE32_AFTER: return muse::mtrc("engraving", "Grace note after");
        case NoteType::GRACE4:
        case NoteType::GRACE16:
        case NoteType::GRACE32: return muse::mtrc("engraving", "Grace note before");
        default: return muse::mtrc("engraving", "Note");
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Note::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);

    for (EngravingItem *e : el_) {
        e->scan_elements(func);
    }
    for (Spanner *sp : spanner_for_) {
        sp->scan_elements(func);
    }

    if (accidental_) {
        func(accidental_);
    }
    for (NoteDot *dot : dots_) {
        func(dot);
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Note::set_track(track_idx_t val) {
    EngravingItem::set_track(val);
    if (tie_for_non_partial()) {
        tie_for_->set_track(val);
        for (SpannerSegment *seg : tie_for_->spanner_segments()) {
            seg->set_track(val);
        }
    }
    if (tie_back_non_partial()) {
        tie_back_->set_track2(val);
    }
    if (laissez_vib() || outgoing_partial_tie()) {
        tie_for_->set_track(val);
        tie_for_->set_track2(val);
        for (SpannerSegment *seg : tie_for_->spanner_segments()) {
            seg->set_track(val);
        }
    }
    if (incoming_partial_tie()) {
        tie_back_->set_track(val);
        tie_back_->set_track2(val);
        for (SpannerSegment *seg : tie_back_->spanner_segments()) {
            seg->set_track(val);
        }
    }
    for (Spanner *s : spanner_for_) {
        s->set_track(val);
    }
    for (Spanner *s : spanner_back_) {
        s->set_track2(val);
    }
    for (EngravingItem *e : el_) {
        e->set_track(val);
    }
    if (accidental_) {
        accidental_->set_track(val);
    }
    if (!chord()) { // if note is dragged with shift+ctrl
        return;
    }
    for (NoteDot *dot : dots_) {
        dot->set_track(val);
    }
}

//---------------------------------------------------------
//    reset
//---------------------------------------------------------

void Note::reset() {
    undo_change_property(Pid::OFFSET, PointF());
    undo_reset_property(Pid::LEADING_SPACE);
    chord()->undo_change_property(Pid::OFFSET, PropertyValue::from_value(PointF()));
    chord()->undo_change_property(Pid::STEM_DIRECTION, PropertyValue::from_value<DirectionV>(DirectionV::AUTO));
    set_override_bend_visibility_rules(false);
    set_hide_generated_parens(false);
}

float Note::user_velocity_fraction() const { return user_velocity_ / 127.f; }

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Note::mag() const {
    double m = chord() ? chord()->mag() : 1.0;
    if (is_small_) {
        m *= style().style_d(Sid::SMALL_NOTE_MAG);
    }
    return m;
}

//---------------------------------------------------------
//   elementBase
//---------------------------------------------------------
EngravingItem *Note::element_base() const { return chord(); }

//---------------------------------------------------------
//   setSmall
//---------------------------------------------------------

void Note::set_small(bool val) { is_small_ = val; }

GuitarBend *Note::bend_for() const {
    /// Returns bend or dive going forward from this note. In the edge case of having both, returns the bend.
    GuitarBend *dive_for = nullptr;

    for (Spanner *sp : spanner_for_) {
        if (!sp->is_guitar_bend()) {
            continue;
        }

        GuitarBend *bend = to_guitar_bend(sp);
        if (bend->bend_type() == GuitarBendType::SCOOP) {
            continue;
        }

        if (!bend->is_dive()) {
            return bend;
        } else {
            dive_for = bend;
        }
    }

    return dive_for;
}

GuitarBend *Note::bend_back() const {
    /// Returns bend or dive going back from this note. In the edge case of having both, returns the bend.
    GuitarBend *dive_back = nullptr;

    for (Spanner *sp : spanner_back_) {
        if (!sp->is_guitar_bend()) {
            continue;
        }

        GuitarBend *bend = to_guitar_bend(sp);
        if (bend->bend_type() == GuitarBendType::SLIGHT_BEND || bend->bend_type() == GuitarBendType::DIP) {
            continue;
        }

        if (!bend->is_dive()) {
            return bend;
        } else {
            dive_back = bend;
        }
    }

    return dive_back;
}

GuitarBend *Note::dive_for() const {
    for (Spanner *sp : spanner_for_) {
        if (sp->is_guitar_bend() && to_guitar_bend(sp)->is_dive() && to_guitar_bend(sp)->bend_type() != GuitarBendType::SCOOP) {
            return to_guitar_bend(sp);
        }
    }

    return nullptr;
}

GuitarBend *Note::dive_back() const {
    for (Spanner *sp : spanner_back_) {
        if (sp->is_guitar_bend() && to_guitar_bend(sp)->is_dive() && to_guitar_bend(sp)->bend_type() != GuitarBendType::DIP) {
            return to_guitar_bend(sp);
        }
    }

    return nullptr;
}

Tie *Note::tie_for_non_partial() const {
    if (!tie_for_ || tie_for_->type() != ElementType::TIE) {
        return nullptr;
    }

    return tie_for_;
}

Tie *Note::tie_back_non_partial() const {
    if (!tie_back_ || tie_back_->type() != ElementType::TIE) {
        return nullptr;
    }

    return tie_back_;
}

LaissezVib *Note::laissez_vib() const {
    if (!tie_for_ || !tie_for_->is_laissez_vib()) {
        return nullptr;
    }

    return to_laissez_vib(tie_for_);
}

PartialTie *Note::incoming_partial_tie() const {
    if (!tie_back_ || !tie_back_->is_partial_tie()) {
        return nullptr;
    }

    return to_partial_tie(tie_back_);
}

PartialTie *Note::outgoing_partial_tie() const {
    if (!tie_for_ || !tie_for_->is_partial_tie()) {
        return nullptr;
    }

    return to_partial_tie(tie_for_);
}

void Note::set_tie_for(Tie *t) {
    if (!t) {
        jump_points_.clear();
    }
    tie_for_ = t;
    if (tie_for_ && !tie_for_->is_laissez_vib()) {
        tie_for_->update_possible_jump_points();
    }
}

void Note::set_tie_back(Tie *t) {
    if (tie_back_ && t && tie_back_->jump_point()) {
        t->set_jump_point(tie_back_->jump_point());
        tie_back_->set_jump_point(nullptr);
    }
    tie_back_ = t;
}

//---------------------------------------------------------
//   line
//---------------------------------------------------------

int Note::line() const { return fixed() ? fixed_line_ : line_; }

//---------------------------------------------------------
//   setHeadScheme
//---------------------------------------------------------

void Note::set_head_scheme(NoteHeadScheme val) {
    IF_ASSERT_FAILED(int(val) >= -1 && int(val) < int(NoteHeadScheme::HEAD_SCHEMES)) { val = NoteHeadScheme::HEAD_AUTO; }
    head_scheme_ = val;
}

//---------------------------------------------------------
//   setHeadGroup
//---------------------------------------------------------

void Note::set_head_group(NoteHeadGroup val) {
    assert(int(val) >= 0 && int(val) < int(NoteHeadGroup::HEAD_GROUPS));
    head_group_ = val;
}

//---------------------------------------------------------
//   ottaveCapoFret
//    offset added by Ottava's and Capo Fret.
//---------------------------------------------------------

int Note::ottave_capo_fret() const {
    const Chord *ch = chord();
    Fraction segment_tick = ch->segment()->tick();
    const Staff *staff = this->staff();

    const CapoParams &capo = staff->capo(segment_tick);
    int capo_fret = 0;

    using MODE = CapoParams::TransposeMode;
    if (capo.active && MODE::PLAYBACK_ONLY == capo.transpose_mode) {
        if (capo.ignored_strings.empty() || !muse::contains(capo.ignored_strings, static_cast<string_idx_t>(string_))) {
            capo_fret = capo.fret_position;
        }
    }

    return staff->pitch_offset(segment_tick) + capo_fret;
}

//---------------------------------------------------------
//   ppitch
//    playback pitch
//---------------------------------------------------------

int Note::ppitch() const {
    const Chord *ch = chord();

    // if staff is drum
    // match tremolo and articulation between variants and chord
    if (play_ && ch) {
        const Staff *staff = ch->staff();
        Fraction tick = ch->tick();

        if (staff && staff->is_drum_staff(tick)) {
            const Drumset *ds = origin_part()->instrument(tick)->drumset();
            if (ds) {
                DrumInstrumentVariant div = ds->find_variant(pitch_, ch->articulations(), ch->tremolo_type());
                if (div.pitch != ::mu::engraving::invalid_pitch) {
                    return div.pitch;
                }
            }
        }
    }

    return pitch_ + ottave_capo_fret() + linked_ottava_pitch_offset();
}

//---------------------------------------------------------
//   epitch
//    effective pitch, i.e. a pitch which is visible in the
//    currently used written notation.
//    honours transposing instruments
//---------------------------------------------------------

int Note::epitch() const { return pitch_ - (concert_pitch() ? 0 : transposition()) + linked_ottava_pitch_offset(); }

//---------------------------------------------------------
//   linkedOttavaPitchOffset
//    if an ottava exists in another part but has been excluded
//    from this one, this returns the corresponding pitch offset
//---------------------------------------------------------

int Note::linked_ottava_pitch_offset() const {
    int linked_ottava_pitch_offset = 0;
    if (links() && staff()->pitch_offset(tick()) == 0) {
        for (EngravingObject *link : *links()) {
            int offset = to_note(link)->staff()->pitch_offset(tick());
            if (offset != 0) {
                linked_ottava_pitch_offset = offset;
                break;
            }
        }
    }
    return linked_ottava_pitch_offset;
}

//---------------------------------------------------------
//   octave
//    compute octave number from note information
//---------------------------------------------------------

int Note::octave() const { return ((epitch() + ottave_capo_fret() - static_cast<int>(tpc2alter(tpc()))) / 12) - 1; }

int Note::playing_octave() const { return mu::engraving::playing_octave(ppitch(), tpc1()); }

double Note::playing_tuning() const { return tuning_ + cent_offset_; }

//---------------------------------------------------------
//   customizeVelocity
//    Input is the global velocity determined by dynamic
//    signs and crescendo/diminuendo etc.
//    Returns the actual play velocity for this note
//    modified by veloOffset
//---------------------------------------------------------

int Note::customize_velocity(int velo) const {
    if (velo_type_ == VeloType::OFFSET_VAL) {
        velo = velo + (velo * user_velocity()) / 100;
    } else if (velo_type_ == VeloType::USER_VAL) {
        velo = user_velocity();
    }
    return std::clamp(velo, 1, 127);
}

//---------------------------------------------------------
//   updateRelLine
//    calculate the real note line depending on clef,
//    absLine is the absolute line
//---------------------------------------------------------

void Note::update_rel_line(int abs_line, bool undoable) {
    if (!staff()) {
        return;
    }
    // int idx      = staffIdx() + chord()->staffMove();
    assert(staff_idx() == chord()->staff_idx());
    staff_idx_t idx = chord()->v_staff_idx();

    const Staff *staff = score()->staff(idx);
    const StaffType *st = staff->staff_type_for_element(this);
    if (st->is_tab_staff()) {
        // tab staff is already correct, and the following relStep method doesn't apply whatsoever to tab staves
        return;
    }
    ClefType clef = staff->clef(chord()->tick());
    int line = rel_step(abs_line, clef);

    if (undoable && (line_ != ::mu::engraving::invalid_line) && (line != line_)) {
        undo_change_property(Pid::LINE, line);
    } else {
        set_line(line);
    }

    int off = st->step_offset();
    double ld = st->line_distance().val();
    mutldata()->set_pos_y((line_ + off * 2.0) * spatium() * .5 * ld);
}

//---------------------------------------------------------
//   updateLine
//---------------------------------------------------------

void Note::update_line() {
    int abs_line = abs_step(tpc(), epitch());
    update_rel_line(abs_line, false);
}

//---------------------------------------------------------
//   setNval
//    set note properties from NoteVal
//---------------------------------------------------------

void Note::set_nval(const NoteVal &nval, Fraction tick) {
    set_pitch(nval.pitch);
    user_velocity_ = nval.velocity_override;
    fret_ = nval.fret;
    string_ = nval.string;

    tpc_[0] = nval.tpc1;
    tpc_[1] = nval.tpc2;

    if (tick == Fraction(-1, 1) && chord()) {
        tick = chord()->tick();
    }
    Interval v = staff()->transpose(tick);
    if (nval.tpc1 == Tpc::TPC_INVALID) {
        if (nval.tpc2 == Tpc::TPC_INVALID) {
            Key key = staff()->concert_key(tick);
            tpc_[0] = pitch2tpc(nval.pitch, key, Prefer::NEAREST);
        } else {
            if (v.is_zero()) {
                tpc_[0] = tpc_[1];
            } else {
                tpc_[0] = Transpose::transpose_tpc(tpc_[1], v, true);
            }
        }
    }
    if (nval.tpc2 == Tpc::TPC_INVALID) {
        if (v.is_zero()) {
            tpc_[1] = tpc_[0];
        } else {
            v.flip();
            tpc_[1] = Transpose::transpose_tpc(tpc_[0], v, true);
        }
    }

    head_group_ = NoteHeadGroup(nval.head_group);
}

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Note::local_spatium_changed(double old_value, double new_value) {
    EngravingItem::local_spatium_changed(old_value, new_value);
    for (EngravingItem *e : dots()) {
        e->local_spatium_changed(old_value, new_value);
    }
    for (EngravingItem *e : el()) {
        e->local_spatium_changed(old_value, new_value);
    }
    for (Spanner *spanner : spanner_back()) {
        for (auto k : spanner->spanner_segments()) {
            k->local_spatium_changed(old_value, new_value);
        }
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Note::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PITCH: return pitch();
        case Pid::CENT_OFFSET: return cent_offset();
        case Pid::TPC1: return tpc_[0];
        case Pid::TPC2: return tpc_[1];
        case Pid::SMALL: return is_small();
        case Pid::MIRROR_HEAD: return user_mirror();
        case Pid::DOT_POSITION: return PropertyValue::from_value<DirectionV>(user_dot_position());
        case Pid::HEAD_SCHEME: return head_scheme();
        case Pid::HEAD_GROUP: return head_group();
        case Pid::USER_VELOCITY: return user_velocity();
        case Pid::TUNING: return tuning();
        case Pid::FRET: return fret();
        case Pid::STRING: return string();
        case Pid::GHOST: return ghost();
        case Pid::DEAD: return dead_note();
        case Pid::HEAD_TYPE: return head_type();
        case Pid::VELO_TYPE: return velo_type_;
        case Pid::PLAY: return play();
        case Pid::LINE: return line_;
        case Pid::FIXED: return fixed();
        case Pid::FIXED_LINE: return fixed_line();
        case Pid::HAS_PARENTHESES: return has_parens_ ? ParenthesesMode::BOTH : ParenthesesMode::NONE;
        case Pid::HIDE_GENERATED_PARENTHESES: return hide_generated_parens_;
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (chord()) {
                return EngravingItem::get_property(property_id).to_bool() && chord()->get_property(property_id).to_bool();
            }
        default: break;
    }
    return EngravingItem::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Note::set_property(Pid property_id, const PropertyValue &v) {
    Measure *m = chord() ? chord()->measure() : nullptr;
    switch (property_id) {
        case Pid::PITCH: set_pitch(v.to_int()); break;
        case Pid::CENT_OFFSET: set_cent_offset(v.to_double()); break;
        case Pid::TPC1: tpc_[0] = v.to_int(); break;
        case Pid::TPC2: tpc_[1] = v.to_int(); break;
        case Pid::LINE: set_line(v.to_int()); break;
        case Pid::SMALL: set_small(v.to_bool()); break;
        case Pid::MIRROR_HEAD: set_user_mirror(v.value<DirectionH>()); break;
        case Pid::DOT_POSITION: set_user_dot_position(v.value<DirectionV>()); break;
        case Pid::HEAD_SCHEME: set_head_scheme(v.value<NoteHeadScheme>()); break;
        case Pid::HEAD_GROUP:
            set_head_group(v.value<NoteHeadGroup>());
            if (links()) {
                for (EngravingObject *score_element : *links()) {
                    Note *note = to_note(score_element);
                    note->set_dead_note(staff() && !staff()->is_drum_staff(tick()) && head_group_ == NoteHeadGroup::HEAD_CROSS);
                    note->set_head_group(head_group_);
                }
            } else {
                set_dead_note(staff() && !staff()->is_drum_staff(tick()) && head_group_ == NoteHeadGroup::HEAD_CROSS);
                set_head_group(head_group_);
            }
            break;
        case Pid::USER_VELOCITY: set_user_velocity(v.to_int()); break;
        case Pid::TUNING: set_tuning(v.to_double()); break;
        case Pid::FRET: set_fret(v.to_int()); break;
        case Pid::STRING: set_string(v.to_int()); break;
        case Pid::GHOST: set_ghost(v.to_bool()); break;
        case Pid::DEAD:
            set_dead_note(v.to_bool());
            if (!staff()->is_drum_staff(tick())) {
                NoteHeadGroup head =
                    (dead_note_ && head_group_ != NoteHeadGroup::HEAD_CROSS) ? NoteHeadGroup::HEAD_CROSS : NoteHeadGroup::HEAD_NORMAL;
                set_head_group(head);
            }
            break;
        case Pid::HEAD_TYPE: set_head_type(v.value<NoteHeadType>()); break;
        case Pid::VELO_TYPE: velo_type_ = v.value<VeloType>(); break;
        case Pid::VISIBLE: {
            set_visible(v.to_bool());
            if (m) {
                m->check_multi_voices(chord()->staff_idx());
            }
            break;
        }
        case Pid::PLAY: set_play(v.to_bool()); break;
        case Pid::FIXED: set_fixed(v.to_bool()); break;
        case Pid::FIXED_LINE: set_fixed_line(v.to_int()); break;
        case Pid::HAS_PARENTHESES:
            if (v.value<ParenthesesMode>() != ParenthesesMode::BOTH && v.value<ParenthesesMode>() != ParenthesesMode::NONE) {
                ASSERT_X("Notes cannot set left & right parens individually");
            }
            has_parens_ = v.value<ParenthesesMode>() == ParenthesesMode::BOTH;
            break;
        case Pid::HIDE_GENERATED_PARENTHESES: set_hide_generated_parens(v.to_bool()); break;
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (v.to_bool() == true && chord()) {
                // when re-linking, also re-link the parent chord
                chord()->set_property(property_id, v);
            }
        // fall through
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Note::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::CENT_OFFSET: return 0.0;
        case Pid::GHOST:
        case Pid::DEAD:
        case Pid::SMALL: return false;
        case Pid::MIRROR_HEAD: return DirectionH::AUTO;
        case Pid::DOT_POSITION: return DirectionV::AUTO;
        case Pid::HEAD_SCHEME: return NoteHeadScheme::HEAD_AUTO;
        case Pid::HEAD_GROUP: return NoteHeadGroup::HEAD_NORMAL;
        case Pid::USER_VELOCITY: return 0;
        case Pid::TUNING: return 0.0;
        case Pid::FRET:
        case Pid::STRING: return -1;
        case Pid::HEAD_TYPE: return NoteHeadType::HEAD_AUTO;
        case Pid::VELO_TYPE: return VeloType::USER_VAL;
        case Pid::PLAY: return true;
        case Pid::FIXED: return false;
        case Pid::FIXED_LINE: return 0;
        case Pid::TPC2: return get_property(Pid::TPC1);
        case Pid::PITCH:
        case Pid::TPC1: return PropertyValue();
        case Pid::VISIBLE:
            if (staff_type() && staff_type()->is_tab_staff()) {
                GuitarBend *bend = bend_back();
                if (bend && !bend->is_full_release()) {
                    return false;
                }
            }
            return EngravingItem::property_default(property_id);
        case Pid::COLOR: {
            const StaffType *st = staff_type();
            if (st && st->is_tab_staff() && st->fret_use_text_style()) {
                return style().style_v(Sid::TAB_FRET_NUMBER_COLOR);
            }
            return EngravingItem::property_default(property_id);
        }
        case Pid::HIDE_GENERATED_PARENTHESES: return false;
        default: break;
    }
    return EngravingItem::property_default(property_id);
}

void Note::style_changed() {
    const StaffType *st = staff_type();
    if (st && st->is_tab_staff() && st->fret_use_text_style()) {
        set_property(Pid::COLOR, style().style_v(Sid::TAB_FRET_NUMBER_COLOR));
    }
    EngravingItem::style_changed();
}

//---------------------------------------------------------
//   setHeadType
//---------------------------------------------------------

void Note::set_head_type(NoteHeadType t) { head_type_ = t; }

//---------------------------------------------------------
//   setOnTimeOffset
//---------------------------------------------------------

void Note::set_on_time_offset(int val) {
    play_events_[0].set_ontime(val);
    chord()->set_play_event_type(PlayEventType::USER);
}

//---------------------------------------------------------
//   setOffTimeOffset
//---------------------------------------------------------

void Note::set_off_time_offset(int val) {
    play_events_[0].set_len(val - play_events_[0].ontime());
    chord()->set_play_event_type(PlayEventType::USER);
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void Note::set_score(Score *s) {
    EngravingItem::set_score(s);
    if (tie_for_) {
        tie_for_->set_score(s);
    }
    if (accidental_) {
        accidental_->set_score(s);
    }
    for (NoteDot *dot : dots_) {
        dot->set_score(s);
    }
    for (EngravingItem *el : el_) {
        el->set_score(s);
    }
}

//---------------------------------------------------------
//   accessibleExtraInfo
//---------------------------------------------------------

NoteVal Note::note_val() const {
    NoteVal nval;
    nval.pitch = pitch();
    nval.velocity_override = user_velocity();
    nval.tpc1 = tpc1();
    nval.tpc2 = tpc2();
    nval.fret = fret();
    nval.string = string();
    nval.head_group = head_group();

    return nval;
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

TranslatableString Note::subtype_user_name() const { return TConv::user_name(head_group_); }

//---------------------------------------------------------
//   nextInEl
//   returns next element in _el
//---------------------------------------------------------

EngravingItem *Note::next_in_el(EngravingItem *e) {
    if (e == el_.back()) {
        return nullptr;
    }
    auto i = std::find(el_.begin(), el_.end(), e);
    if (i == el_.end()) {
        return nullptr;
    }
    return *(i + 1);
}

//---------------------------------------------------------
//   prevInEl
//   returns prev element in _el
//---------------------------------------------------------

EngravingItem *Note::prev_in_el(EngravingItem *e) {
    if (e == el_.front()) {
        return nullptr;
    }
    auto i = std::find(el_.begin(), el_.end(), e);
    if (i == el_.end()) {
        return nullptr;
    }
    return *(i - 1);
}

static bool tie_valid(Tie *tie) { return tie && !tie->segments_empty(); }

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *Note::next_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }
    if (!e) {
        return nullptr;
    }
    switch (e->type()) {
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
        case ElementType::FINGERING:
        case ElementType::TEXT:
        case ElementType::BEND: {
            EngravingItem *next = next_in_el(e); // return next element in _el
            if (next) {
                return next;
            } else if (tie_valid(tie_for_)) {
                return tie_for_->front_segment();
            } else if (!spanner_for_.empty()) {
                for (auto i : spanner_for_) {
                    if (i->is_glissando()) {
                        return i->spanner_segments().front();
                    }
                }
            }
            return nullptr;
        }

        case ElementType::TIE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
            if (!spanner_for_.empty()) {
                for (auto i : spanner_for_) {
                    if (i->is_glissando()) {
                        return i->spanner_segments().front();
                    }
                }
            }
            return chord()->next_element();

        case ElementType::NOTELINE_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::GUITAR_BEND_SEGMENT: return chord()->next_element();

        case ElementType::ACCIDENTAL:
            if (!el_.empty()) {
                return el_[0];
            }
            if (tie_valid(tie_for_)) {
                return tie_for_->front_segment();
            }
            if (!spanner_for_.empty()) {
                for (auto i : spanner_for_) {
                    if (i->is_glissando()) {
                        return i->spanner_segments().front();
                    }
                }
            }
            return nullptr;

        case ElementType::NOTE: {
            if (is_pre_bend_or_dive_start() || is_grace_bend_start()) {
                return bend_for()->front_segment();
            }

            GraceNotesGroup &grace_notes_after = chord()->grace_notes_after();
            if (!grace_notes_after.empty()) {
                Chord *grace_notes_after_first_chord = grace_notes_after.front();
                if (grace_notes_after_first_chord) {
                    return grace_notes_after_first_chord->notes().front();
                }
            }

            if (!el_.empty()) {
                return el_[0];
            }
            if (tie_valid(tie_for_)) {
                return tie_for_->front_segment();
            }
            if (!spanner_for_.empty()) {
                for (auto i : spanner_for_) {
                    if (i->is_glissando() || i->is_guitar_bend()) {
                        return i->spanner_segments().front();
                    }
                }
            }
            return nullptr;
        }
        default: return nullptr;
    }
}

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *Note::prev_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().back();
    }
    if (!e) {
        return nullptr;
    }
    switch (e->type()) {
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
        case ElementType::FINGERING:
        case ElementType::TEXT:
        case ElementType::BEND: {
            EngravingItem *prev = prev_in_el(e); // return prev element in _el
            if (prev) {
                return prev;
            }
        }
            return this;
        case ElementType::TIE_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
            if (!el_.empty()) {
                return el_.back();
            }
            return this;
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::GUITAR_BEND_SEGMENT:
            if (tie_valid(tie_for_)) {
                return tie_for_->front_segment();
            } else if (!el_.empty()) {
                return el_.back();
            }
            return this;
        case ElementType::ACCIDENTAL: return this;
        default: return nullptr;
    }
}

//---------------------------------------------------------
//   lastElementBeforeSegment
//---------------------------------------------------------

EngravingItem *Note::last_element_before_segment() {
    if (!spanner_for_.empty()) {
        for (auto i : spanner_for_) {
            if (i->is_glissando() || i->is_guitar_bend() || i->is_note_line()) {
                return i->spanner_segments().front();
            }
        }
    }
    if (tie_valid(tie_for_)) {
        return tie_for_->front_segment();
    }
    if (!el_.empty()) {
        return el_.back();
    }
    return this;
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Note::next_segment_element() {
    if (chord()->is_grace()) {
        return EngravingItem::next_segment_element();
    }

    const std::vector<Note *> &notes = chord()->notes();
    if (this == notes.front()) {
        return chord()->next_segment_element();
    }
    auto i = std::find(notes.begin(), notes.end(), this);
    return *(i - 1);
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Note::prev_segment_element() {
    if (chord()->is_grace()) {
        return EngravingItem::prev_segment_element();
    }

    const std::vector<Note *> &notes = chord()->notes();
    if (this == notes.back()) {
        return chord()->prev_segment_element();
    }
    auto i = std::find(notes.begin(), notes.end(), this);
    return *++i;
}

//---------------------------------------------------------
//   lastTiedNote
//---------------------------------------------------------

Note *Note::last_tied_note(bool ignore_playback) const {
    std::vector<const Note *> notes;
    const Note *note = this;
    notes.push_back(note);
    while (note->tie_for() && (ignore_playback || note->tie_for()->play_spanner())) {
        if (std::find(notes.begin(), notes.end(), note->tie_for()->end_note()) != notes.end()) {
            break;
        }
        if (!note->tie_for()->end_note()) {
            break;
        }
        note = note->tie_for()->end_note();
        notes.push_back(note);
    }
    return const_cast<Note *>(note);
}

//---------------------------------------------------------
//   firstTiedNote
//    if note has ties, return last note in chain
//    - handle recursion in connected notes
//---------------------------------------------------------

Note *Note::first_tied_note(bool ignore_playback) const {
    std::vector<const Note *> notes;
    const Note *note = this;
    notes.push_back(note);
    while (note->tie_back() && (ignore_playback || note->tie_back()->play_spanner())) {
        if (std::find(notes.begin(), notes.end(), note->tie_back()->start_note()) != notes.end()) {
            break;
        }
        if (!note->tie_back()->start_note()) {
            break;
        }
        note = note->tie_back()->start_note();
        notes.push_back(note);
    }
    return const_cast<Note *>(note);
}

//---------------------------------------------------------
//   tiedNotes
//---------------------------------------------------------

std::vector<Note *> Note::find_tied_notes(Note *start_note, bool follow_partial_ties) {
    // Returns all notes ahead of startNote in a chain of ties
    // Follows partial tie paths recursively
    Note *note = start_note;
    std::vector<Note *> notes;
    notes.push_back(note);

    while (note->tie_for()) {
        if (follow_partial_ties) {
            for (TieJumpPoint *jump_point : *note->tie_jump_points()) {
                if (!jump_point->active() || jump_point->following_note()) {
                    continue;
                }
                if (!jump_point->note() || std::find(notes.begin(), notes.end(), jump_point->note()) != notes.end()) {
                    continue;
                }
                // ONLY backtrack when end point is a full tie eg. around a segno
                const bool end_tie_is_full_tie = jump_point->end_tie() && !jump_point->end_tie()->is_partial_tie();
                Note *jump_point_note = end_tie_is_full_tie ? jump_point->end_tie()->start_note()->first_tied_note() : jump_point->note();
                std::vector<Note *> partial_tie_notes = find_tied_notes(jump_point_note, !end_tie_is_full_tie);
                notes.insert(notes.end(), partial_tie_notes.begin(), partial_tie_notes.end());
            }
        }

        Note *end_note = note->tie_for()->end_note();
        if (!end_note || std::find(notes.begin(), notes.end(), end_note) != notes.end()) {
            break;
        }
        note = end_note;
        notes.push_back(note);
    }

    return notes;
}

std::vector<Note *> Note::tied_notes() const {
    // Backtrack to the first tied note in a chain, then return all notes in the chain ahead of it
    Note *note = first_tied_note();

    std::vector<Note *> notes = find_tied_notes(note);

    return notes;
}

//---------------------------------------------------------
//   unisonIndex
//---------------------------------------------------------

int Note::unison_index() const {
    int index = 0;
    for (Note *n : chord()->notes()) {
        if (n->pitch() == pitch()) {
            if (n == this) {
                return index;
            } else {
                ++index;
            }
        }
    }
    return 0;
}

//---------------------------------------------------------
//   disconnectTiedNotes
//---------------------------------------------------------

void Note::disconnect_tied_notes() {
    if (tie_back() && tie_back()->start_note()) {
        tie_back()->start_note()->remove(tie_back());
    }
    if (tie_for() && tie_for()->end_note()) {
        tie_for()->end_note()->set_tie_back(0);
    }
}

//---------------------------------------------------------
//   connectTiedNotes
//---------------------------------------------------------

void Note::connect_tied_notes() {
    if (tie_back() && !tie_back()->is_partial_tie()) {
        tie_back()->set_end_note(this);
        if (tie_back()->start_note()) {
            tie_back()->start_note()->add(tie_back());
        }
    }
    if (tie_for() && tie_for()->end_note() && !tie_for()->is_partial_tie()) {
        tie_for()->end_note()->set_tie_back(tie_for());
    }
}

//---------------------------------------------------------
//   accidentalType
//---------------------------------------------------------

AccidentalType Note::accidental_type() const { return accidental_ ? accidental_->accidental_type() : AccidentalType::NONE; }

//---------------------------------------------------------
//   setAccidentalType
//---------------------------------------------------------

void Note::set_accidental_type(AccidentalType type) {
    if (score()) {
        EditNote::change_accidental(score(), this, type);
    }
}

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void Note::undo_unlink() {
    EngravingItem::undo_unlink();
    for (EngravingItem *e : el_) {
        e->undo_unlink();
    }
    for (Spanner *s : spanner_for_) {
        s->undo_unlink();
    }
    if (tie_for_) {
        tie_for_->undo_unlink();
    }
}

//---------------------------------------------------------
//   slides
//---------------------------------------------------------

static bool is_slide_to(Note::SlideType slide_type) {
    return slide_type == Note::SlideType::UP_TO_NOTE || slide_type == Note::SlideType::DOWN_TO_NOTE;
}

static bool is_slide_from(Note::SlideType slide_type) {
    return slide_type == Note::SlideType::UP_FROM_NOTE || slide_type == Note::SlideType::DOWN_FROM_NOTE;
}

void Note::attach_slide(SlideType slide_type) {
    if (is_slide_to(slide_type)) {
        slide_to_type_ = slide_type;
    } else if (is_slide_from(slide_type)) {
        slide_from_type_ = slide_type;
    }
}

bool Note::has_slide_to_note() const { return slide_to_type_ != SlideType::UNDEFINED; }

bool Note::has_slide_from_note() const { return slide_from_type_ != SlideType::UNDEFINED; }

void Note::set_parentheses_mode(const ParenthesesMode &v, bool add_to_linked, bool generated) {
    IF_ASSERT_FAILED(v == ParenthesesMode::BOTH || v == ParenthesesMode::NONE) {
        LOGE() << "Notes cannot set left & right parens individually";
        return;
    }

    if ((has_parens_ && v == ParenthesesMode::BOTH) || (!has_parens_ && v == ParenthesesMode::NONE)) {
        return;
    }

    const NoteParenthesisInfo *note_paren_info = parenthesis_info();

    Parenthesis *left_paren = note_paren_info ? note_paren_info->left_paren() : nullptr;

    const bool has_generated_paren = left_paren && left_paren->generated();
    const bool has_user_paren = left_paren && !left_paren->generated();

    bool has_paren = v == ParenthesesMode::BOTH;

    if (generated && has_paren == has_generated_paren) {
        return;
    }

    if (!generated && has_paren == has_user_paren) {
        return;
    }

    has_parens_ = has_paren;

    if (has_paren) {
        EditChord::add_chord_parentheses(chord(), {this}, add_to_linked, generated);
    } else {
        EditChord::remove_chord_parentheses(chord(), {this}, add_to_linked, generated);
    }
}

const NoteParenthesisInfo *Note::parenthesis_info() const { return chord() ? chord()->find_note_parenthesis_info(this) : nullptr; }

bool Note::is_grace() const { return note_type() != NoteType::NORMAL; }

bool Note::is_pre_bend_or_dive_start() const {
    if (!is_grace()) {
        return false;
    }

    const GuitarBend *bend = bend_for();
    if (!bend) {
        return false;
    }

    const GuitarBendType bend_type = bend->bend_type();
    return bend_type == GuitarBendType::PRE_BEND || bend_type == GuitarBendType::PRE_DIVE;
}

bool Note::is_grace_bend_start() const {
    if (!is_grace()) {
        return false;
    }

    const GuitarBend *bend = bend_for();

    return bend && bend->bend_type() == GuitarBendType::GRACE_NOTE_BEND;
}

bool Note::is_continuation_of_bend() const {
    if (bend_back()) {
        return true;
    }

    Tie *tie = tie_back();
    Note *note = nullptr;
    while (tie && tie->start_note()) {
        note = tie->start_note();
        if (note->bend_back()) {
            return true;
        }

        if (tie == note->tie_back()) {
            return false;
        }

        tie = note->tie_back();
    }

    return false;
}

bool Note::has_another_straight_above_or_below(bool above) const {
    if (!chord()) {
        return false;
    }

    const std::vector<Note *> &notes = chord()->notes();

    if ((above && this == notes.back()) || (!above && this == notes.front())) {
        return false;
    }

    const double limit_diff = 0.5 * spatium();
    for (Note *note : notes) {
        if (note == this) {
            continue;
        }
        if (std::fabs(note->pos().x() - pos().x()) > limit_diff) {
            return false;
        }
        if ((above && note->line() < line_) || (!above && note->line() > line_)) {
            return true;
        }
    }

    return false;
}

PointF Note::pos_in_staff_coordinates() {
    double X = x() + chord()->x() + chord()->segment()->x() + chord()->measure()->x();
    return PointF(X, y());
}

void Note::set_is_trill_cue_note(bool v) {
    is_trill_cue_note_ = v;
    if (chord()) {
        chord()->set_is_trill_cue_note(v);
    }
}

void Note::add_line_attach_point(PointF point, EngravingItem *line, bool start) {
    // IMPORTANT: the point is expected in *staff* coordinates
    // We transform into note coordinates by subtracting the note position in staff coordinates
    point -= pos_in_staff_coordinates();
    line_attach_points_.push_back(LineAttachPoint(line, point.x(), point.y(), start));
}

bool Note::negative_fret_used() const {
    return configuration()->negative_frets_allowed() && fret_ < 0 && string_ != ::mu::engraving::invalid_string_index;
}

int Note::string_or_line() const {
    // The number string() returns doesn't count spaces.  This should be used where it is expected even numbers are spaces and odd are lines
    return staff()->staff_type(tick())->is_tab_staff() ? string() * 2 : line();
}

//---------------------------------------------------------
//   Note::transposeDiatonic
//---------------------------------------------------------

bool Note::transpose_diatonic(int interval, bool keep_alterations, bool use_double_accidentals) {
    // compute note current absolute step
    int alter;
    Fraction tick = chord()->segment()->tick();
    Key key = staff() ? staff()->key(tick) : Key::C;
    int abs_step = pitch2abs_step_by_key(epitch(), tpc(), key, alter);

    // get pitch and tcp corresponding to unaltered degree for this key
    int new_pitch = abs_step2pitch_by_key(abs_step + interval, key);
    int new_tpc = step2tpc_by_key((abs_step + interval) % ::mu::engraving::step_delta_octave, key);

    if (!pitch_is_valid(new_pitch)) {
        return false;
    }

    // if required, transfer original degree alteration to new pitch and tpc
    if (keep_alterations) {
        new_pitch += alter;
        new_tpc += alter * ::mu::engraving::tpc_delta_semitone;
    }

    // transpose appropriately
    int new_tpc1 = TPC_INVALID;
    int new_tpc2 = TPC_INVALID;
    Interval v = staff() ? staff()->transpose(tick) : Interval(0);
    if (concert_pitch()) {
        v.flip();
        new_tpc1 = clamp_enharmonic(new_tpc, use_double_accidentals);
        new_tpc2 = Transpose::transpose_tpc(new_tpc, v, use_double_accidentals);
    } else {
        new_pitch += v.chromatic;
        new_tpc1 = Transpose::transpose_tpc(new_tpc, v, use_double_accidentals);
        new_tpc2 = clamp_enharmonic(new_tpc, use_double_accidentals);
    }

    // check pitch is in range
    new_pitch = clamp_pitch_octaved(new_pitch);

    // store new data
    EditNote::undo_change_pitch(score(), this, new_pitch, new_tpc1, new_tpc2);
    return true;
}

bool Note::transpose(Interval interval, bool use_double_sharps_flats) {
    int npitch = pitch() + interval.chromatic;
    if (!pitch_is_valid(npitch)) {
        return false;
    }
    int ntpc1 = Transpose::transpose_tpc(tpc1(), interval, use_double_sharps_flats);
    int ntpc2 = ntpc1;
    if (transposition()) {
        if (staff()) {
            Interval v = staff()->transpose(tick());
            v.flip();
            ntpc2 = Transpose::transpose_tpc(ntpc1, v, use_double_sharps_flats);
        } else {
            ntpc2 = Transpose::transpose_tpc(tpc2(), interval, use_double_sharps_flats);
        }
    }
    EditNote::undo_change_pitch(score(), this, npitch, ntpc1, ntpc2);
    return true;
}

staff_idx_t Note::v_staff_idx() const {
    const Chord *c = chord();
    return c ? c->v_staff_idx() : EngravingItem::v_staff_idx();
}
} // namespace mu::engraving
