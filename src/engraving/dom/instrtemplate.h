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

#include <array>
#include <vector>

#include "clef.h"
#include "instrument.h"
#include "io/path.h"
#include "mscore.h"
#include "stringdata.h"

namespace mu::engraving {
struct XmlWriter;
struct XmlReader;
struct Part;
struct Staff;
struct StaffType;
struct ScoreOrder;

//---------------------------------------------------------
//   InstrumentGenre
//---------------------------------------------------------

struct InstrumentGenre {
    String id;
    String name;

    InstrumentGenre() {}

    void write(XmlWriter &xml) const;
    void read(XmlReader &);
};

//---------------------------------------------------------
//   InstrumentFamily
//---------------------------------------------------------

struct InstrumentFamily {
    String id;
    String name;

    InstrumentFamily() {}

    void write(XmlWriter &xml) const;
    void read(XmlReader &);
};

//---------------------------------------------------------
//   InstrumentTemplate
//---------------------------------------------------------

struct InstrumentTemplate {
    InstrumentTemplate();
    InstrumentTemplate(const InstrumentTemplate &);
    ~InstrumentTemplate();
    InstrumentTemplate &operator=(const InstrumentTemplate &);

    String id;
    String sound_id;
    String track_name;
    InstrumentLabel instrument_name;
    String music_xml_id; ///< used in MusicXML 3.0
    String description;  ///< a longer description of the instrument

    size_t staff_count = 0;
    int sequence_order = 0;

    Trait trait;

    int min_pitch_a = 0; // pitch range playable by an amateur
    int max_pitch_a = 0;
    int min_pitch_p = 0; // pitch range playable by professional
    int max_pitch_p = 0;

    Interval transpose; // for transposing instruments

    StaffGroup staff_group;
    const StaffType *staff_type_preset = nullptr;
    bool use_drumset = false;
    Drumset *drumset = nullptr;

    StringData string_data;

    std::vector<NamedEventList> midi_actions;
    std::vector<MidiArticulation> midi_articulations;
    std::vector<InstrChannel> channel;
    std::vector<const InstrumentGenre *> genres; //; list of genres this instrument belongs to
    const InstrumentFamily *family = nullptr;    //; family the instrument belongs to

    ClefTypeList clef_types[::mu::engraving::max_staves];
    int staff_lines[::mu::engraving::max_staves];
    BracketType bracket[::mu::engraving::max_staves]; // bracket type (NO_BRACKET)
    int bracket_span[::mu::engraving::max_staves];
    std::array<bool, ::mu::engraving::max_staves> barline_span{};
    bool small_staff[::mu::engraving::max_staves];

    bool extended = false; // belongs to extended instrument set if true
    bool single_note_dynamics = false;

    String group_id;

    GlissandoStyle glissando_style = GlissandoStyle::CHROMATIC;

    bool is_valid() const;

    void write(XmlWriter &xml) const;
    void read(XmlReader &);
    ClefTypeList clef_type(staff_idx_t staff_idx) const;
    String family_id() const;
    bool contains_genre(const String &genre_id) const;

private:
    void init(const InstrumentTemplate &);
    void set_pitch_range(const String &s, int &a, int &b) const;
    void link_genre(const String &);
};

//---------------------------------------------------------
//   InstrumentGroup
//---------------------------------------------------------

struct InstrumentGroup {
    String id;
    String name;
    bool extended; // belongs to extended instruments set if true
    std::vector<const InstrumentTemplate *> instrument_templates;
    void read(XmlReader &);
    void clear();

    InstrumentGroup() { extended = false; }
};

//---------------------------------------------------------
//   InstrumentIndex
//---------------------------------------------------------

struct InstrumentIndex {
    int group_index = 0;
    int instr_index = 0;
    size_t template_count = 0;
    const InstrumentTemplate *instr_template = nullptr;

    InstrumentIndex(int g, int i, const InstrumentTemplate *it);
};

extern std::vector<const InstrumentGenre *> instrument_genres;
extern std::vector<const InstrumentFamily *> instrument_families;
extern std::vector<const InstrumentGroup *> instrument_groups;
extern std::vector<MidiArticulation> midi_articulations;
extern std::vector<ScoreOrder> instrument_orders;
extern void clear_instrument_templates();
extern bool load_instrument_templates(const muse::io::PathT &instr_templates_path);
extern const InstrumentTemplate *combined_template_search(const Instrument &instrument);
extern InstrumentIndex search_template_index_for_track_name(const String &track_name);
extern InstrumentIndex search_template_index_for_id(const String &id);
extern const InstrumentTemplate *search_template(const String &name);
extern const InstrumentTemplate *search_template_for_music_xml_id(const String &mxml_id);
extern const InstrumentTemplate *search_template_for_instr_name_list(const std::vector<String> &name_list, bool use_drumset = false,
                                                                     bool case_sensitive = true);
extern const InstrumentTemplate *search_template_for_midi_program(int bank, int program, bool use_drumset = false);
extern const InstrumentGenre *search_instrument_genre(const String &id);

extern void add_template_to_group(const InstrumentTemplate *templ, const String &group_id);

extern ClefType default_clef(int patch);
} // namespace mu::engraving
