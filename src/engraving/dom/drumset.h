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

#include "pitchspelling.h"
#include "../types/types.h"

#include <array>

namespace mu::engraving {
class XmlWriter;
class XmlReader;
class Articulation;

struct DrumInstrumentVariant {
    int pitch = ::mu::engraving::invalid_pitch;
    TremoloType tremolo = TremoloType::INVALID_TREMOLO;
    String articulation_name;

    bool operator==(const DrumInstrumentVariant &other) const {
        return pitch == other.pitch && tremolo == other.tremolo && articulation_name == other.articulation_name;
    }
};

//---------------------------------------------------------
//   DrumInstrument
//---------------------------------------------------------

struct DrumInstrument {
    String name;

    // if notehead = HEAD_CUSTOM, custom, use noteheads
    NoteHeadGroup notehead = NoteHeadGroup::HEAD_INVALID; ///< notehead symbol set
    std::array<SymId, int(NoteHeadType::HEAD_TYPES)> noteheads = {SymId::NOTEHEAD_WHOLE, SymId::NOTEHEAD_HALF, SymId::NOTEHEAD_BLACK,
                                                                  SymId::NOTEHEAD_DOUBLE_WHOLE};

    int line = 0; ///< place notehead onto this line
    DirectionV stem_direction = DirectionV::AUTO;

    int panel_row = -1;
    int panel_column = -1;

    int voice = 0;
    String shortcut;
    std::vector<DrumInstrumentVariant> variants;

    DrumInstrument() {}

    DrumInstrument(const String &n, NoteHeadGroup nh, int l, DirectionV d, int pr = -1, int pc = -1, int v = 0, String sc = String())
        : name(n), notehead(nh), line(l), stem_direction(d), panel_row(pr), panel_column(pc), voice(v), shortcut(sc) {}

    void add_variant(DrumInstrumentVariant v) { variants.push_back(v); }

    bool operator==(const DrumInstrument &other) const {
        return name == other.name && notehead == other.notehead && noteheads == other.noteheads && line == other.line &&
               stem_direction == other.stem_direction && voice == other.voice && shortcut == other.shortcut && variants == other.variants &&
               panel_row == other.panel_row && panel_column == other.panel_column;
    }
};

static const int drum_instruments = 128;

//---------------------------------------------------------
//   Drumset
//    defines noteheads and line position for all
//    possible midi notes in a drumset
//---------------------------------------------------------

class Drumset {
public:
    bool is_valid(int pitch) const;

    NoteHeadGroup note_head(int pitch) const { return drums_[pitch].notehead; }

    SymId note_heads(int pitch, NoteHeadType t) const { return drums_[pitch].noteheads[int(t)]; }

    int line(int pitch) const { return drums_[pitch].line; }

    int voice(int pitch) const { return drums_[pitch].voice; }

    DirectionV stem_direction(int pitch) const { return drums_[pitch].stem_direction; }

    const String &name(int pitch) const { return drums_[pitch].name; }

    String translated_name(int pitch) const;

    String shortcut(int pitch) const { return drums_[pitch].shortcut; }

    const std::vector<DrumInstrumentVariant> &variants(int pitch) const { return drums_[pitch].variants; }

    int panel_row(int pitch) const { return drums_[pitch].panel_row; }

    int panel_column(int pitch) const { return drums_[pitch].panel_column; }

    size_t percussion_panel_columns() const { return percussion_panel_columns_; }

    void set_percussion_panel_columns(size_t columns) { percussion_panel_columns_ = columns; }

    int pitch_for_shortcut(const String &shortcut) const;

    // defaultPitchForLine tries to find the pitch of a normal notehead at "line". If a normal notehead can't be found it will
    // instead return the "first valid pitch" (i.e. the lowest used midi note)
    int default_pitch_for_line(int line) const;

    void save(XmlWriter &) const;
    void load_drum(XmlReader &);
    bool read_drum_properties(XmlReader &, int);
    void clear();
    int next_pitch(int) const;
    int prev_pitch(int) const;

    DrumInstrument &drum(int i) { return drums_[i]; }

    const DrumInstrument &drum(int i) const { return drums_[i]; }

    void set_drum(int pitch, const DrumInstrument &di) { drums_[pitch] = di; }

    DrumInstrumentVariant find_variant(int pitch, const std::vector<Articulation *> &articulations, TremoloType trem_type) const;

    static void init_drumset();

    bool operator==(const Drumset &other) const {
        for (int pitch = 0; pitch < ::mu::engraving::drum_instruments; ++pitch) {
            if (drums_[pitch] == other.drums_[pitch]) {
                continue;
            }
            return false;
        }
        return percussion_panel_columns_ == other.percussion_panel_columns_;
    }

    bool operator!=(const Drumset &other) const { return !(*this == other); }

private:
    DrumInstrument drums_[::mu::engraving::drum_instruments];
    size_t percussion_panel_columns_ = 8;
};

extern Drumset *sm_drumset;
} // namespace mu::engraving
