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

#pragma once

#include "../dom/measure.h"
#include "engraving/editing/cmd.h"

namespace mu::engraving {
// Source measure-chain mutations preserve linked-score timing.
void insert_measure_chain(MeasureBase *first, MeasureBase *last, bool move_staff_type_changes);
void remove_measure_chain(MeasureBase *first, MeasureBase *last, bool move_staff_type_changes);
void apply_measure_length(Measure *measure, Fraction length);
void apply_measure_repeat_count(Measure *measure, int count, staff_idx_t staff);

void apply_change_mmrest(Measure *input_m, Measure *mmr);

// ChangeMMRestNext & ChangeMMRestPrev SHOULD ONLY BE USED FOR MMRests
// Setting next_ & prev_ involves the score's MeasureList for regular measures
// Source measure links use the chain operations above.
void apply_change_mmrest_next(Measure *mmrest, MeasureBase *next);

void apply_change_mmrest_prev(Measure *mmrest, MeasureBase *prev);

} // namespace mu::engraving
