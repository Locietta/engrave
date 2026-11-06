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

#include "drumset.h"

#include "translation.h"

#include "types/typesconv.h"
#include "types/symnames.h"

#include "rw/xmlwriter.h"
#include "rw/xmlreader.h"

#include "articulation.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
Drumset *sm_drumset = nullptr; // standard midi drumset

bool Drumset::is_valid(int pitch) const { return pitch >= 0 && pitch < ::mu::engraving::drum_instruments && !drums_[pitch].name.empty(); }

String Drumset::translated_name(int pitch) const { return muse::mtrc("engraving/drumset", name(pitch)); }

int Drumset::pitch_for_shortcut(const String &shortcut) const {
    if (shortcut.is_empty()) {
        return -1;
    }

    for (int pitch = 0; pitch < ::mu::engraving::drum_instruments; ++pitch) {
        if (!is_valid(pitch)) {
            continue;
        }

        if (drum(pitch).shortcut != shortcut) {
            continue;
        }

        return pitch;
    }

    return -1;
}

int Drumset::default_pitch_for_line(int val) const {
    int first_valid_pitch = -1;
    for (int pitch = 0; pitch < ::mu::engraving::drum_instruments; ++pitch) {
        if (!is_valid(pitch) || line(pitch) != val) {
            continue;
        }

        if (first_valid_pitch < 0) {
            first_valid_pitch = pitch;
        }

        const NoteHeadGroup head_group = note_head(pitch);
        if (head_group == NoteHeadGroup::HEAD_NORMAL) {
            return pitch;
        }
    }
    return first_valid_pitch;
}

//---------------------------------------------------------
//   save
//---------------------------------------------------------

void Drumset::save(XmlWriter &xml) const {
    xml.tag("percussionPanelColumns", percussion_panel_columns_);
    for (int i = 0; i < ::mu::engraving::drum_instruments; ++i) {
        if (!is_valid(i)) {
            continue;
        }
        xml.start_element("Drum", {{"pitch", i}});
        const NoteHeadGroup nh = note_head(i);
        //write custom as Normal notehead group + noteheads tag to keep compatibility with 2.X versions
        const NoteHeadGroup save_nh_value = (nh == NoteHeadGroup::HEAD_CUSTOM) ? NoteHeadGroup::HEAD_NORMAL : nh;
        xml.tag("head", TConv::to_xml(save_nh_value));
        if (nh == NoteHeadGroup::HEAD_CUSTOM) {
            xml.start_element("noteheads");
            for (int j = 0; j < int(NoteHeadType::HEAD_TYPES); j++) {
                xml.tag(TConv::to_xml(NoteHeadType(j)), SymNames::name_for_sym_id(note_heads(i, NoteHeadType(j))));
            }
            xml.end_element();
        }
        xml.tag("line", line(i));
        xml.tag("voice", voice(i));
        xml.tag("name", name(i));
        xml.tag("stem", int(stem_direction(i)));
        if (!shortcut(i).empty()) {
            xml.tag("shortcut", shortcut(i));
        }
        const std::vector<DrumInstrumentVariant> &vs = variants(i);
        if (!vs.empty()) {
            xml.start_element("variants");
            for (const auto &v : vs) {
                xml.start_element("variant", {{"pitch", v.pitch}});
                if (!v.articulation_name.empty()) {
                    xml.tag("articulation", v.articulation_name);
                }
                if (v.tremolo != TremoloType::INVALID_TREMOLO) {
                    xml.tag("tremolo", TConv::to_xml(v.tremolo));
                }
                xml.end_element();
            }
            xml.end_element();
        }
        if (panel_row(i) > -1 && panel_column(i) > -1) {
            xml.tag("panelRow", panel_row(i));
            xml.tag("panelColumn", panel_column(i));
        }
        xml.end_element();
    }
}

bool Drumset::read_drum_properties(XmlReader &e, int pitch) {
    if (pitch < 0 || pitch > ::mu::engraving::drum_instruments - 1) {
        return false;
    }

    const AsciiStringView tag(e.name());
    if (tag == "head") {
        drums_[pitch].notehead = TConv::from_xml(e.read_ascii_text(), NoteHeadGroup::HEAD_NORMAL);
    } else if (tag == "noteheads") {
        drums_[pitch].notehead = NoteHeadGroup::HEAD_CUSTOM;
        while (e.read_next_start_element()) {
            const AsciiStringView nh_tag(e.name());
            int note_type = int(TConv::from_xml(nh_tag, NoteHeadType::HEAD_AUTO));
            if (note_type > int(NoteHeadType::HEAD_TYPES) - 1 || note_type < 0) {
                return false;
            }

            drums_[pitch].noteheads[note_type] = SymNames::sym_id_by_name(e.read_ascii_text());
        }
    } else if (tag == "line") {
        drums_[pitch].line = e.read_int();
    } else if (tag == "voice") {
        drums_[pitch].voice = e.read_int();
    } else if (tag == "name") {
        drums_[pitch].name = e.read_text();
    } else if (tag == "stem") {
        drums_[pitch].stem_direction = DirectionV(e.read_int());
    } else if (tag == "shortcut") {
        drums_[pitch].shortcut = e.read_text();
    } else if (tag == "variants") {
        while (e.read_next_start_element()) {
            const AsciiStringView tagv(e.name());
            if (tagv == "variant") {
                DrumInstrumentVariant div;
                div.pitch = e.attribute("pitch").to_int();
                while (e.read_next_start_element()) {
                    const AsciiStringView taga(e.name());
                    if (taga == "articulation") {
                        div.articulation_name = e.read_text();
                    } else if (taga == "tremolo") {
                        div.tremolo = TConv::from_xml(e.read_ascii_text(), TremoloType::INVALID_TREMOLO);
                    }
                }
                drums_[pitch].add_variant(div);
            }
        }
    } else if (tag == "panelRow") {
        drums_[pitch].panel_row = e.read_int();
    } else if (tag == "panelColumn") {
        drums_[pitch].panel_column = e.read_int();
    } else {
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   load
//---------------------------------------------------------

void Drumset::load_drum(XmlReader &e) {
    int pitch = e.int_attribute("pitch", -1);
    if (pitch < 0 || pitch > ::mu::engraving::drum_instruments - 1) {
        LOGD("load drumset: invalid pitch %d", pitch);
        return;
    }
    while (e.read_next_start_element()) {
        if (read_drum_properties(e, pitch)) {
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Drumset::clear() {
    for (int i = 0; i < ::mu::engraving::drum_instruments; ++i) {
        drums_[i].name.clear();
        drums_[i].notehead = NoteHeadGroup::HEAD_INVALID;
        drums_[i].shortcut.clear();
        drums_[i].variants.clear();
        drums_[i].panel_row = -1;
        drums_[i].panel_column = -1;
    }
}

//---------------------------------------------------------
//   nextPitch
//---------------------------------------------------------

int Drumset::next_pitch(int ii) const {
    for (int i = ii + 1; i < ::mu::engraving::drum_instruments - 1; ++i) {
        if (is_valid(i)) {
            return i;
        }
    }
    for (int i = 0; i <= ii; ++i) {
        if (is_valid(i)) {
            return i;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   prevPitch
//---------------------------------------------------------

int Drumset::prev_pitch(int ii) const {
    for (int i = ii - 1; i >= 0; --i) {
        if (is_valid(i)) {
            return i;
        }
    }
    for (int i = ::mu::engraving::drum_instruments - 1; i >= ii; --i) {
        if (is_valid(i)) {
            return i;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   findVariant
/// find a variant for the given pitch with matching chord articulation and tremolo
//---------------------------------------------------------

DrumInstrumentVariant Drumset::find_variant(int p, const std::vector<Articulation *> &articulations, TremoloType trem_type) const {
    DrumInstrumentVariant div;
    const std::vector<DrumInstrumentVariant> &vs = variants(p);
    for (const DrumInstrumentVariant &v : vs) {
        bool match_tremolo =
            ((trem_type == TremoloType::INVALID_TREMOLO) && (v.tremolo == TremoloType::INVALID_TREMOLO)) || (v.tremolo == trem_type);
        bool match_articulation = v.articulation_name.empty() && articulations.empty();
        for (const Articulation *a : articulations) {
            match_articulation = a->articulation_name() == v.articulation_name;
            if (!match_articulation) {
                break;
            }
        }
        if (match_articulation && match_tremolo) {
            div = v;
            break;
        }
    }
    return div;
}

//---------------------------------------------------------
//   initDrumset
//    initialize standard midi drumset
//---------------------------------------------------------

void Drumset::init_drumset() {
    sm_drumset = new Drumset;
    for (int i = 0; i < ::mu::engraving::drum_instruments; ++i) {
        sm_drumset->drum(i).name.clear();
        sm_drumset->drum(i).notehead = NoteHeadGroup::HEAD_INVALID;
        sm_drumset->drum(i).line = 0;
        sm_drumset->drum(i).shortcut.clear();
        sm_drumset->drum(i).voice = 0;
        sm_drumset->drum(i).stem_direction = DirectionV::UP;
        sm_drumset->drum(i).panel_row = -1;
        sm_drumset->drum(i).panel_column = -1;
    }

    // Code generated by share/instruments/update_instruments_xml.py. To edit this code:
    // 1. Make your changes in the proper place:
    //     * To change instrument data, edit the online spreadsheet.
    //     * For other code changes, edit the Python script.
    // 2. Run the Python script to generate the changes here.
    // 3. Run Uncrustify and update the Python script if necessary to match Uncrustify's formatting.

    // BEGIN GENERATED CODE

    // Laser (High Q)
    sm_drumset->drum(27) = DrumInstrument(TConv::user_name(DrumNum(27)), NoteHeadGroup::HEAD_SLASH,
                                          /*line*/ 8, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Slap
    sm_drumset->drum(28) = DrumInstrument(TConv::user_name(DrumNum(28)), NoteHeadGroup::HEAD_CUSTOM,
                                          /*line*/ 4, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    sm_drumset->drum(28).noteheads[static_cast<int>(NoteHeadType::HEAD_WHOLE)] = SymNames::sym_id_by_name("noteheadSlashX");
    sm_drumset->drum(28).noteheads[static_cast<int>(NoteHeadType::HEAD_HALF)] = SymNames::sym_id_by_name("noteheadSlashX");
    sm_drumset->drum(28).noteheads[static_cast<int>(NoteHeadType::HEAD_QUARTER)] = SymNames::sym_id_by_name("noteheadSlashX");
    sm_drumset->drum(28).noteheads[static_cast<int>(NoteHeadType::HEAD_BREVIS)] = SymNames::sym_id_by_name("noteheadSlashX");

    // Scratch Push
    sm_drumset->drum(29) = DrumInstrument(TConv::user_name(DrumNum(29)), NoteHeadGroup::HEAD_SLASH,
                                          /*line*/ 6, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Scratch Pull
    sm_drumset->drum(30) = DrumInstrument(TConv::user_name(DrumNum(30)), NoteHeadGroup::HEAD_SLASH,
                                          /*line*/ 6, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Sticks
    sm_drumset->drum(31) = DrumInstrument(TConv::user_name(DrumNum(31)), NoteHeadGroup::HEAD_PLUS,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Square Click
    sm_drumset->drum(32) = DrumInstrument(TConv::user_name(DrumNum(32)), NoteHeadGroup::HEAD_PLUS,
                                          /*line*/ 10, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Metronome Click
    sm_drumset->drum(33) = DrumInstrument(TConv::user_name(DrumNum(33)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ 10, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Metronome Bell
    sm_drumset->drum(34) = DrumInstrument(TConv::user_name(DrumNum(34)), NoteHeadGroup::HEAD_TRIANGLE_UP,
                                          /*line*/ 10, DirectionV::UP,
                                          /*panelRow*/ 0,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Acoustic Bass Drum
    sm_drumset->drum(35) = DrumInstrument(TConv::user_name(DrumNum(35)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 8, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 1,
                                          /*shortcut*/ String());

    // Electric Bass Drum
    sm_drumset->drum(36) = DrumInstrument(TConv::user_name(DrumNum(36)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 7, DirectionV::DOWN,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 1,
                                          /*shortcut*/ String());

    // Side Stick
    sm_drumset->drum(37) = DrumInstrument(TConv::user_name(DrumNum(37)), NoteHeadGroup::HEAD_SLASHED1,
                                          /*line*/ 3, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Acoustic Snare
    sm_drumset->drum(38) = DrumInstrument(TConv::user_name(DrumNum(38)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 3, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Hand Clap
    sm_drumset->drum(39) = DrumInstrument(TConv::user_name(DrumNum(39)), NoteHeadGroup::HEAD_PLUS,
                                          /*line*/ -2, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Electric Snare
    sm_drumset->drum(40) = DrumInstrument(TConv::user_name(DrumNum(40)), NoteHeadGroup::HEAD_SLASH,
                                          /*line*/ 3, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low Floor Tom
    sm_drumset->drum(41) = DrumInstrument(TConv::user_name(DrumNum(41)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 6, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Closed Hi-hat
    sm_drumset->drum(42) = DrumInstrument(TConv::user_name(DrumNum(42)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 1,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // High Floor Tom
    sm_drumset->drum(43) = DrumInstrument(TConv::user_name(DrumNum(43)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 5, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Pedal Hi-hat
    sm_drumset->drum(44) = DrumInstrument(TConv::user_name(DrumNum(44)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ 9, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 1,
                                          /*shortcut*/ String());

    // Low Tom
    sm_drumset->drum(45) = DrumInstrument(TConv::user_name(DrumNum(45)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 4, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Open Hi-hat
    sm_drumset->drum(46) = DrumInstrument(TConv::user_name(DrumNum(46)), NoteHeadGroup::HEAD_XCIRCLE,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low-Mid Tom
    sm_drumset->drum(47) = DrumInstrument(TConv::user_name(DrumNum(47)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 2, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Hi-Mid Tom
    sm_drumset->drum(48) = DrumInstrument(TConv::user_name(DrumNum(48)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 1, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Crash Cymbal 1
    sm_drumset->drum(49) = DrumInstrument(TConv::user_name(DrumNum(49)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ -2, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // High Tom
    sm_drumset->drum(50) = DrumInstrument(TConv::user_name(DrumNum(50)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 2,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Ride Cymbal 1
    sm_drumset->drum(51) = DrumInstrument(TConv::user_name(DrumNum(51)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // China Cymbal
    sm_drumset->drum(52) = DrumInstrument(TConv::user_name(DrumNum(52)), NoteHeadGroup::HEAD_CUSTOM,
                                          /*line*/ -3, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    sm_drumset->drum(52).noteheads[static_cast<int>(NoteHeadType::HEAD_WHOLE)] = SymNames::sym_id_by_name("noteheadHeavyXHat");
    sm_drumset->drum(52).noteheads[static_cast<int>(NoteHeadType::HEAD_HALF)] = SymNames::sym_id_by_name("noteheadHeavyXHat");
    sm_drumset->drum(52).noteheads[static_cast<int>(NoteHeadType::HEAD_QUARTER)] = SymNames::sym_id_by_name("noteheadHeavyXHat");
    sm_drumset->drum(52).noteheads[static_cast<int>(NoteHeadType::HEAD_BREVIS)] = SymNames::sym_id_by_name("noteheadHeavyXHat");

    // Ride Bell
    sm_drumset->drum(53) = DrumInstrument(TConv::user_name(DrumNum(53)), NoteHeadGroup::HEAD_DIAMOND,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Tambourine
    sm_drumset->drum(54) = DrumInstrument(TConv::user_name(DrumNum(54)), NoteHeadGroup::HEAD_DIAMOND,
                                          /*line*/ 6, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Splash Cymbal
    sm_drumset->drum(55) = DrumInstrument(TConv::user_name(DrumNum(55)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ -4, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Cowbell
    sm_drumset->drum(56) = DrumInstrument(TConv::user_name(DrumNum(56)), NoteHeadGroup::HEAD_TRIANGLE_DOWN,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Crash Cymbal 2
    sm_drumset->drum(57) = DrumInstrument(TConv::user_name(DrumNum(57)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ -3, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Vibraslap
    sm_drumset->drum(58) = DrumInstrument(TConv::user_name(DrumNum(58)), NoteHeadGroup::HEAD_TI,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 3,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Ride Cymbal 2
    sm_drumset->drum(59) = DrumInstrument(TConv::user_name(DrumNum(59)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ 2, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // High Bongo
    sm_drumset->drum(60) = DrumInstrument(TConv::user_name(DrumNum(60)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low Bongo
    sm_drumset->drum(61) = DrumInstrument(TConv::user_name(DrumNum(61)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Mute High Conga
    sm_drumset->drum(62) = DrumInstrument(TConv::user_name(DrumNum(62)), NoteHeadGroup::HEAD_CUSTOM,
                                          /*line*/ 1, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    sm_drumset->drum(62).noteheads[static_cast<int>(NoteHeadType::HEAD_WHOLE)] = SymNames::sym_id_by_name("noteheadXOrnate");
    sm_drumset->drum(62).noteheads[static_cast<int>(NoteHeadType::HEAD_HALF)] = SymNames::sym_id_by_name("noteheadXOrnate");
    sm_drumset->drum(62).noteheads[static_cast<int>(NoteHeadType::HEAD_QUARTER)] = SymNames::sym_id_by_name("noteheadXOrnate");
    sm_drumset->drum(62).noteheads[static_cast<int>(NoteHeadType::HEAD_BREVIS)] = SymNames::sym_id_by_name("noteheadXOrnate");

    // Open High Conga
    sm_drumset->drum(63) = DrumInstrument(TConv::user_name(DrumNum(63)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 1, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low Conga
    sm_drumset->drum(64) = DrumInstrument(TConv::user_name(DrumNum(64)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 2, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // High Timbale
    sm_drumset->drum(65) = DrumInstrument(TConv::user_name(DrumNum(65)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 5, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low Timbale
    sm_drumset->drum(66) = DrumInstrument(TConv::user_name(DrumNum(66)), NoteHeadGroup::HEAD_NORMAL,
                                          /*line*/ 7, DirectionV::UP,
                                          /*panelRow*/ 4,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // High Agogô
    sm_drumset->drum(67) = DrumInstrument(TConv::user_name(DrumNum(67)), NoteHeadGroup::HEAD_TRIANGLE_DOWN,
                                          /*line*/ -2, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low Agogô
    sm_drumset->drum(68) = DrumInstrument(TConv::user_name(DrumNum(68)), NoteHeadGroup::HEAD_TRIANGLE_DOWN,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Cabasa
    sm_drumset->drum(69) = DrumInstrument(TConv::user_name(DrumNum(69)), NoteHeadGroup::HEAD_DIAMOND,
                                          /*line*/ 2, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Maracas
    sm_drumset->drum(70) = DrumInstrument(TConv::user_name(DrumNum(70)), NoteHeadGroup::HEAD_DIAMOND,
                                          /*line*/ 4, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Short Whistle
    sm_drumset->drum(71) = DrumInstrument(TConv::user_name(DrumNum(71)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ -3, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Long Whistle
    sm_drumset->drum(72) = DrumInstrument(TConv::user_name(DrumNum(72)), NoteHeadGroup::HEAD_TI,
                                          /*line*/ -3, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Short Güiro
    sm_drumset->drum(73) = DrumInstrument(TConv::user_name(DrumNum(73)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Long Güiro
    sm_drumset->drum(74) = DrumInstrument(TConv::user_name(DrumNum(74)), NoteHeadGroup::HEAD_SLASHED1,
                                          /*line*/ -1, DirectionV::UP,
                                          /*panelRow*/ 5,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Claves
    sm_drumset->drum(75) = DrumInstrument(TConv::user_name(DrumNum(75)), NoteHeadGroup::HEAD_LA,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // High Woodblock
    sm_drumset->drum(76) = DrumInstrument(TConv::user_name(DrumNum(76)), NoteHeadGroup::HEAD_LA,
                                          /*line*/ 5, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Low Woodblock
    sm_drumset->drum(77) = DrumInstrument(TConv::user_name(DrumNum(77)), NoteHeadGroup::HEAD_LA,
                                          /*line*/ 7, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Mute Cuica
    sm_drumset->drum(78) = DrumInstrument(TConv::user_name(DrumNum(78)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ 8, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Open Cuica
    sm_drumset->drum(79) = DrumInstrument(TConv::user_name(DrumNum(79)), NoteHeadGroup::HEAD_SLASHED2,
                                          /*line*/ 8, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Mute Triangle
    sm_drumset->drum(80) = DrumInstrument(TConv::user_name(DrumNum(80)), NoteHeadGroup::HEAD_CROSS,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 5,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Open Triangle
    sm_drumset->drum(81) = DrumInstrument(TConv::user_name(DrumNum(81)), NoteHeadGroup::HEAD_TRIANGLE_UP,
                                          /*line*/ 0, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 6,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Shaker
    sm_drumset->drum(82) = DrumInstrument(TConv::user_name(DrumNum(82)), NoteHeadGroup::HEAD_DIAMOND,
                                          /*line*/ 5, DirectionV::UP,
                                          /*panelRow*/ 6,
                                          /*panelColumn*/ 7,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Jingle Bell
    sm_drumset->drum(83) = DrumInstrument(TConv::user_name(DrumNum(83)), NoteHeadGroup::HEAD_TRIANGLE_DOWN,
                                          /*line*/ 3, DirectionV::UP,
                                          /*panelRow*/ 7,
                                          /*panelColumn*/ 0,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Belltree
    sm_drumset->drum(84) = DrumInstrument(TConv::user_name(DrumNum(84)), NoteHeadGroup::HEAD_TI,
                                          /*line*/ 2, DirectionV::UP,
                                          /*panelRow*/ 7,
                                          /*panelColumn*/ 1,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Castanets
    sm_drumset->drum(85) = DrumInstrument(TConv::user_name(DrumNum(85)), NoteHeadGroup::HEAD_LA,
                                          /*line*/ 2, DirectionV::UP,
                                          /*panelRow*/ 7,
                                          /*panelColumn*/ 2,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // Mute Surdo
    sm_drumset->drum(86) = DrumInstrument(TConv::user_name(DrumNum(86)), NoteHeadGroup::HEAD_CUSTOM,
                                          /*line*/ 4, DirectionV::UP,
                                          /*panelRow*/ 7,
                                          /*panelColumn*/ 3,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    sm_drumset->drum(86).noteheads[static_cast<int>(NoteHeadType::HEAD_WHOLE)] = SymNames::sym_id_by_name("noteheadSlashX");
    sm_drumset->drum(86).noteheads[static_cast<int>(NoteHeadType::HEAD_HALF)] = SymNames::sym_id_by_name("noteheadSlashX");
    sm_drumset->drum(86).noteheads[static_cast<int>(NoteHeadType::HEAD_QUARTER)] = SymNames::sym_id_by_name("noteheadSlashX");
    sm_drumset->drum(86).noteheads[static_cast<int>(NoteHeadType::HEAD_BREVIS)] = SymNames::sym_id_by_name("noteheadSlashX");

    // Open Surdo
    sm_drumset->drum(87) = DrumInstrument(TConv::user_name(DrumNum(87)), NoteHeadGroup::HEAD_SLASH,
                                          /*line*/ 4, DirectionV::UP,
                                          /*panelRow*/ 7,
                                          /*panelColumn*/ 4,
                                          /*voice*/ 0,
                                          /*shortcut*/ String());

    // END GENERATED CODE
}
} // namespace mu::engraving
