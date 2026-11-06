/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#include "tremolofilter.h"

#include "dom/chord.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"

using namespace mu::engraving;

bool TremoloFilter::is_playable(const EngravingItem *item, const RenderingContext & /*ctx*/) {
    if (!item->is_chord() && !item->is_note()) {
        return false;
    }

    if (item->is_chord()) {
        const Chord *chord = item_cast<const Chord *>(item);
        TremoloSingleChord *tremolo_single = chord->tremolo_single_chord();
        TremoloTwoChord *tremolo_two = chord->tremolo_two_chord();

        if (!tremolo_single && !tremolo_two) {
            return true;
        }

        if (!tremolo_two) {
            return true;
        }

        return tremolo_two->chord1() == chord;
    }

    return true;
}
