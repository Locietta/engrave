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
#ifndef MUSE_MIDI_IMIDIOUTPORT_H
#define MUSE_MIDI_IMIDIOUTPORT_H

#include "miditypes.h"
#include "types/ret.h"

namespace muse::midi {
struct IMidiOutPort {
    virtual ~IMidiOutPort() = default;

    virtual MidiDeviceList available_devices() const = 0;

    virtual Ret connect(const MidiDeviceID &device_id) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual MidiDeviceID device_id() const = 0;

    // Whether the output port supports it, rather than whether the receiver supports it
    // (If the receiver does not support MIDI 2.0, then it's the output port's resposibility to convert to MIDI 1.0)
    virtual bool supports_mid_i20_output() const = 0;

    virtual Ret send_event(const Event &e) = 0;
};
} // namespace muse::midi

#endif // MUSE_MIDI_IMIDIPORT_H
