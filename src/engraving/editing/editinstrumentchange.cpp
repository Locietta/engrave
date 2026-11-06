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

#include "editinstrumentchange.h"

#include "../dom/instrchange.h"
#include "../dom/instrument.h"
#include "../dom/masterscore.h"
#include "../dom/part.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/staff.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangeInstrument
//---------------------------------------------------------

void ChangeInstrument::flip() {
    Part *part = is->staff()->part();
    Fraction tick_start = is->segment()->tick();
    Instrument *oi = is->instrument(); //new Instrument(*is->instrument());

    // set instrument in both part and instrument change element
    is->set_instrument(instrument); //*instrument
    part->set_instrument(instrument, tick_start);

    // update score
    is->master_score()->rebuild_midi_mapping();
    is->master_score()->update_channel();
    is->score()->set_instruments_changed(true);
    is->trigger_layout_all();

    // remember original instrument
    instrument = oi;
}
