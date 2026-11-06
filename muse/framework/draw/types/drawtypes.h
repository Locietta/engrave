/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#include "global/types/flags.h"

namespace muse::draw {
enum class CompositionMode { SOURCE_OVER, HARD_LIGHT, SOURCE_IN };

enum class PolygonMode { ODD_EVEN, WINDING, CONVEX, POLYLINE };

enum class PenStyle { // pen style
    NO_PEN,
    SOLID_LINE,
    DASH_LINE,
    DOT_LINE,
    DASH_DOT_LINE,
    DASH_DOT_DOT_LINE,
    CUSTOM_DASH_LINE
};

enum class PenCapStyle { // line endcap style
    FLAT_CAP = 0x00,
    SQUARE_CAP = 0x10,
    ROUND_CAP = 0x20
};

enum class PenJoinStyle { // line join style
    MITER_JOIN = 0x00,
    BEVEL_JOIN = 0x40,
    ROUND_JOIN = 0x80
};

enum class BrushStyle { // brush style
    NO_BRUSH,
    SOLID_PATTERN,
    DENSE1_PATTERN,
    DENSE2_PATTERN,
    DENSE3_PATTERN,
    DENSE4_PATTERN,
    DENSE5_PATTERN,
    DENSE6_PATTERN,
    DENSE7_PATTERN,
    HOR_PATTERN,
    VER_PATTERN,
    CROSS_PATTERN,
    B_DIAG_PATTERN,
    F_DIAG_PATTERN,
    DIAG_CROSS_PATTERN,
    LINEAR_GRADIENT_PATTERN,
    RADIAL_GRADIENT_PATTERN,
    CONICAL_GRADIENT_PATTERN,
    TEXTURE_PATTERN = 24
};

enum class FillRule { ODD_EVEN_FILL, WINDING_FILL };

// Keep in sync with Qt::AlignmentFlag (https://doc.qt.io/qt-6/qt.html#AlignmentFlag-enum)
enum AlignmentFlag {
    ALIGN_LEFT = 0x0001,
    ALIGN_LEADING = ALIGN_LEFT,
    ALIGN_RIGHT = 0x0002,
    ALIGN_TRAILING = ALIGN_RIGHT,
    ALIGN_H_CENTER = 0x0004,
    ALIGN_JUSTIFY = 0x0008,
    ALIGN_ABSOLUTE = 0x0010,
    ALIGN_HORIZONTAL_MASK = ALIGN_LEFT | ALIGN_RIGHT | ALIGN_H_CENTER | ALIGN_JUSTIFY | ALIGN_ABSOLUTE,

    ALIGN_TOP = 0x0020,
    ALIGN_BOTTOM = 0x0040,
    ALIGN_V_CENTER = 0x0080,
    ALIGN_BASELINE = 0x0100,
    // Note that 0x100 will clash with Qt::TextSingleLine = 0x100 due to what the comment above
    // this enum declaration states. However, since Qt::AlignBaseline is only used by layouts,
    // it doesn't make sense to pass Qt::AlignBaseline to QPainter::drawText(), so there
    // shouldn't really be any ambiguity between the two overlapping enum values.
    ALIGN_VERTICAL_MASK = ALIGN_TOP | ALIGN_BOTTOM | ALIGN_V_CENTER | ALIGN_BASELINE,

    ALIGN_CENTER = ALIGN_V_CENTER | ALIGN_H_CENTER
};

DECLARE_FLAGS(Alignment, AlignmentFlag)
DECLARE_OPERATORS_FOR_FLAGS(Alignment)

// Keep in sync with Qt::TextFlag (https://doc.qt.io/qt-6/qt.html#TextFlag-enum)
enum TextFlag {
    TEXT_SINGLE_LINE = 0x0100,
    TEXT_DONT_CLIP = 0x0200,
    TEXT_EXPAND_TABS = 0x0400,
    TEXT_SHOW_MNEMONIC = 0x0800,
    TEXT_WORD_WRAP = 0x1000,
    TEXT_WRAP_ANYWHERE = 0x2000,
    TEXT_DONT_PRINT = 0x4000,
    TEXT_INCLUDE_TRAILING_SPACES = 0x08000000,
    TEXT_HIDE_MNEMONIC = 0x8000,
    TEXT_JUSTIFICATION_FORCED = 0x10000,
    TEXT_FORCE_LEFT_TO_RIGHT = 0x20000,
    TEXT_FORCE_RIGHT_TO_LEFT = 0x40000,
    // Ensures that the longest variant is always used when computing the
    // size of a multi-variant string.
    TEXT_LONGEST_VARIANT = 0x80000
};

DECLARE_FLAGS(TextFlags, TextFlag)
DECLARE_OPERATORS_FOR_FLAGS(TextFlags)
} // namespace muse::draw
