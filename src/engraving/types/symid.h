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

namespace mu::engraving {
enum class SmuflAnchorId : unsigned char {
    STEM_DOWN_NW,
    STEM_UP_SE,
    STEM_DOWN_SW,
    STEM_UP_NW,
    CUT_OUT_NE,
    CUT_OUT_NW,
    CUT_OUT_SE,
    CUT_OUT_SW,
    OPTICAL_CENTER,
};

#include "symid_p.h"
} // namespace mu::engraving
