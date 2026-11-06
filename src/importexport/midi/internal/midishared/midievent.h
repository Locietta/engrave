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

#ifndef MIDISHARED_MIDIEVENT_H
#define MIDISHARED_MIDIEVENT_H

#include "engraving/compat/midi/midicoreevent.h"

namespace mu::iex::midi {
struct MidiEvent : engraving::MidiCoreEvent {
protected:
    std::vector<uchar> edata_value;
    int len_value{0};
    int meta_type_value{0};

public:
    MidiEvent() {}

    MidiEvent(uchar t, uchar c, uchar a, uchar b) : MidiCoreEvent(t, c, a, b), edata_value(0), len_value(0) {}

    const uchar *edata() const { return edata_value.data(); }

    void set_e_data(std::vector<uchar> &&d) { edata_value = d; }

    int len() const { return len_value; }

    void set_len(int l) { len_value = l; }

    int meta_type() const { return meta_type_value; }

    void set_meta_type(int v) { meta_type_value = v; }
};
} // namespace mu::iex::midi

#endif
