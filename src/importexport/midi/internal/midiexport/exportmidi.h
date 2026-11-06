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

#ifndef EXPORTMIDI_H
#define EXPORTMIDI_H

#include "../midishared/midifile.h"
#include "engraving/compat/midi/compatmidirenderinternal.h"
#include "engraving/compat/midi/pausemap.h"
#include "io/path.h"

namespace mu::engraving {
struct Score;
struct SynthesizerState;
} // namespace mu::engraving

namespace mu::iex::midi {
using MidiPath = muse::io::PathT;

//---------------------------------------------------------
//   ExportMidi
//---------------------------------------------------------

struct ExportMidi {
    ExportMidi(engraving::Score *s) { score_ = s; }

    bool write(const MidiPath &name, bool midi_expand_repeats, bool export_rp_ns);
    bool write(MidiIODevice *device, bool midi_expand_repeats, bool export_rp_ns);
    bool write(const MidiPath &name, bool midi_expand_repeats, bool export_rp_ns, const engraving::SynthesizerState &synth_state);
    bool write(MidiIODevice *device, bool midi_expand_repeats, bool export_rp_ns, const engraving::SynthesizerState &synth_state);

private:
    void write_header(const engraving::CompatMidiRendererInternal::Context &context);

    MidiFile midi_file_;
    engraving::Score *score_ = nullptr;
};
} // namespace mu::iex::midi
#endif // EXPORTMIDI_H
