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

#pragma once

namespace mu::engraving {
class Score;
class Transaction;

enum class BeamMode : signed char;

class EditBeam {
public:
    static void set_beam_mode(Transaction &tx, Score *score, BeamMode mode);
    static void beam_selected_range(Transaction &tx, Score *score);
    static void reset_beam_mode(Transaction &tx, Score *score);
};
} // namespace mu::engraving
