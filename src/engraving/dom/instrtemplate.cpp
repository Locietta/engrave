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

#include "instrtemplate.h"

#include <limits>

#include "bracket.h"
#include "containers.h"
#include "drumset.h"
#include "io/file.h"
#include "log.h"
#include "rw/read460/tread.h"
#include "rw/write/twrite.h"
#include "rw/xmlreader.h"
#include "rw/xmlwriter.h"
#include "scoreorder.h"
#include "stafftype.h"
#include "stringdata.h"
#include "stringutils.h"
#include "translation.h"
#include "types/typesconv.h"
#include "utils.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::engraving {
std::vector<const InstrumentGroup *> instrument_groups;
std::vector<const InstrumentGenre *> instrument_genres;
std::vector<const InstrumentFamily *> instrument_families;
std::vector<MidiArticulation> midi_articulations; // global articulations
std::vector<ScoreOrder> instrument_orders;

InstrumentIndex::InstrumentIndex(int g, int i, const InstrumentTemplate *it) : group_index{g}, instr_index{i}, instr_template{it} {
    template_count = 0;
    for (const InstrumentGroup *ig : instrument_groups) {
        template_count += ig->instrument_templates.size();
    }
}

const InstrumentGenre *search_instrument_genre(const String &id) {
    for (const InstrumentGenre *ig : instrument_genres) {
        if (ig->id == id) {
            return ig;
        }
    }
    return nullptr;
}

static const InstrumentFamily *search_instrument_family(const String &name) {
    for (const InstrumentFamily *fam : instrument_families) {
        if (fam->id == name) {
            return fam;
        }
    }
    return nullptr;
}

static InstrumentGroup *search_instrument_group(const String &id) {
    for (const InstrumentGroup *g : instrument_groups) {
        if (g->id == id) {
            return const_cast<InstrumentGroup *>(g);
        }
    }
    return nullptr;
}

static MidiArticulation search_articulation(const String &name) {
    for (MidiArticulation a : midi_articulations) {
        if (a.name == name) {
            return a;
        }
    }
    return MidiArticulation();
}

static int read_staff_idx(XmlReader &e) {
    int idx = e.int_attribute("staff", 1) - 1;
    if (idx >= ::mu::engraving::max_staves) {
        idx = ::mu::engraving::max_staves - 1;
    }
    if (idx < 0) {
        idx = 0;
    }
    return idx;
}

static TraitType trait_type_from_string(const String &str) {
    static const std::unordered_map<String, TraitType> types{
        {u"transposition", TraitType::TRANSPOSITION}, {u"tuning", TraitType::TUNING}, {u"course", TraitType::COURSE}};

    return muse::value(types, str.to_lower(), TraitType::UNKNOWN);
}

//---------------------------------------------------------
//   read InstrumentGroup
//---------------------------------------------------------

void InstrumentGroup::read(XmlReader &e) {
    id = e.attribute("id");
    name = muse::mtrc("engraving/instruments/group", e.attribute("name"));
    extended = e.int_attribute("extended", 0);

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "instrument" || tag == "Instrument") {
            String sid = e.attribute("id");
            InstrumentTemplate *t = const_cast<InstrumentTemplate *>(search_template(sid));
            if (t == 0) {
                t = new InstrumentTemplate;
                // init with global articulation
                t->midi_articulations.insert(t->midi_articulations.end(), midi_articulations.begin(), midi_articulations.end());
                t->sequence_order = static_cast<int>(instrument_templates.size());
                instrument_templates.push_back(t);
            }
            t->read(e);
        } else if (tag == "ref") {
            const InstrumentTemplate *ttt = search_template(e.read_text());
            if (ttt) {
                InstrumentTemplate *t = new InstrumentTemplate(*ttt);
                instrument_templates.push_back(t);
            } else {
                LOGD("instrument reference not found <%s>", e.text().to_utf8().data());
            }
        } else if (tag == "name") {
            name = muse::mtrc("engraving/instruments/group", e.read_ascii_text().ascii());
        } else if (tag == "extended") {
            extended = e.read_int();
        } else {
            e.unknown();
        }
    }
    if (id.is_empty()) {
        id = name.to_lower().replace(u" ", u"-");
    }

    for (const InstrumentTemplate *templ : instrument_templates) {
        const_cast<InstrumentTemplate *>(templ)->group_id = id;
    }
}

//---------------------------------------------------------
//   clear InstrumentGroup
//---------------------------------------------------------

void InstrumentGroup::clear() {
    muse::delete_all(instrument_templates);
    instrument_templates.clear();
}

//---------------------------------------------------------
//   InstrumentTemplate
//---------------------------------------------------------

InstrumentTemplate::InstrumentTemplate() {
    staff_count = 1;
    min_pitch_a = ::mu::engraving::min_pitch;
    max_pitch_a = ::mu::engraving::max_pitch;
    min_pitch_p = ::mu::engraving::min_pitch;
    max_pitch_p = ::mu::engraving::max_pitch;
    staff_group = StaffGroup::STANDARD;
    staff_type_preset = 0;
    use_drumset = false;
    drumset = 0;
    extended = false;
    single_note_dynamics = true;
    family = nullptr;

    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        clef_types[i].concert_clef = ClefType::G;
        clef_types[i].transposing_clef = ClefType::G;
        staff_lines[i] = 5;
        small_staff[i] = false;
        bracket[i] = BracketType::NO_BRACKET;
        bracket_span[i] = 0;
    }
    transpose.diatonic = 0;
    transpose.chromatic = 0;
}

InstrumentTemplate::InstrumentTemplate(const InstrumentTemplate &t) { init(t); }

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void InstrumentTemplate::init(const InstrumentTemplate &t) {
    id = t.id;
    sound_id = t.sound_id;
    track_name = t.track_name;
    instrument_name = t.instrument_name;
    description = t.description;
    music_xml_id = t.music_xml_id;
    staff_count = t.staff_count;
    extended = t.extended;
    min_pitch_a = t.min_pitch_a;
    max_pitch_a = t.max_pitch_a;
    min_pitch_p = t.min_pitch_p;
    max_pitch_p = t.max_pitch_p;
    transpose = t.transpose;
    staff_group = t.staff_group;
    staff_type_preset = t.staff_type_preset;
    use_drumset = t.use_drumset;
    drumset = t.drumset ? new Drumset(*t.drumset) : nullptr;
    string_data = t.string_data;
    midi_actions = t.midi_actions;
    genres = t.genres;
    channel = t.channel;
    family = t.family;
    single_note_dynamics = t.single_note_dynamics;
    sequence_order = t.sequence_order;
    trait = t.trait;
    group_id = t.group_id;
    glissando_style = t.glissando_style;
    barline_span = t.barline_span;

    for (int i = 0; i < ::mu::engraving::max_staves; ++i) {
        clef_types[i] = t.clef_types[i];
        staff_lines[i] = t.staff_lines[i];
        small_staff[i] = t.small_staff[i];
        bracket[i] = t.bracket[i];
        bracket_span[i] = t.bracket_span[i];
    }
}

InstrumentTemplate::~InstrumentTemplate() {
    delete drumset;
    drumset = nullptr;
}

InstrumentTemplate &InstrumentTemplate::operator=(const InstrumentTemplate &templ) {
    init(templ);
    return *this;
}

bool InstrumentTemplate::is_valid() const { return !id.empty(); }

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void InstrumentTemplate::write(XmlWriter &xml) const {
    xml.start_element("Instrument", {{"id", id}});

    if (!sound_id.empty()) {
        xml.tag("soundId", sound_id);
    }

    write::TWrite::write(instrument_name, xml);

    if (!instrument_name.long_name().empty()) {
        xml.tag("trackName", instrument_name.long_name());
    }
    xml.tag("description", description);
    xml.tag("musicXMLid", music_xml_id);
    if (extended) {
        xml.tag("extended", extended);
    }
    write::TWrite::write(&string_data, xml);
    if (staff_count > 1) {
        xml.tag("staves", static_cast<int>(staff_count));
    }
    for (staff_idx_t i = 0; i < staff_count; ++i) {
        if (clef_types[i].concert_clef == clef_types[i].transposing_clef) {
            if (i) {
                xml.tag("clef", {{"staff", i + 1}}, TConv::to_xml(clef_types[i].concert_clef));
            } else {
                xml.tag("clef", TConv::to_xml(clef_types[i].concert_clef));
            }
        } else {
            if (i) {
                xml.tag("concertClef", {{"staff", i + 1}}, TConv::to_xml(clef_types[i].concert_clef));
                xml.tag("transposingClef", {{"staff", i + 1}}, TConv::to_xml(clef_types[i].transposing_clef));
            } else {
                xml.tag("concertClef", TConv::to_xml(clef_types[i].concert_clef));
                xml.tag("transposingClef", TConv::to_xml(clef_types[i].transposing_clef));
            }
        }
        if (staff_lines[i] != 5) {
            if (i) {
                xml.tag("stafflines", {{"staff", i + 1}}, staff_lines[i]);
            } else {
                xml.tag("stafflines", staff_lines[i]);
            }
        }
        if (small_staff[i]) {
            if (i) {
                xml.tag("smallStaff", {{"staff", i + 1}}, small_staff[i]);
            } else {
                xml.tag("smallStaff", small_staff[i]);
            }
        }

        if (bracket[i] != BracketType::NO_BRACKET) {
            if (i) {
                xml.tag("bracket", {{"staff", i + 1}}, int(bracket[i]));
            } else {
                xml.tag("bracket", int(bracket[i]));
            }
        }
        if (bracket_span[i] != 0) {
            if (i) {
                xml.tag("bracketSpan", {{"staff", i + 1}}, bracket_span[i]);
            } else {
                xml.tag("bracketSpan", bracket_span[i]);
            }
        }
        if (barline_span[i]) {
            if (i > 0) {
                xml.tag("barlineSpan", {{"staff", i + 1}}, int{barline_span[i]});
            } else {
                xml.tag("barlineSpan", int{barline_span[i]});
            }
        }
    }
    if (min_pitch_a != ::mu::engraving::min_pitch || max_pitch_a != ::mu::engraving::max_pitch) {
        xml.tag("aPitchRange", String(u"%1-%2").arg(min_pitch_a).arg(max_pitch_a));
    }
    if (min_pitch_p != ::mu::engraving::min_pitch || max_pitch_p != ::mu::engraving::max_pitch) {
        xml.tag("pPitchRange", String(u"%1-%2").arg(min_pitch_p).arg(max_pitch_p));
    }
    if (transpose.diatonic) {
        xml.tag("transposeDiatonic", transpose.diatonic);
    }
    if (transpose.chromatic) {
        xml.tag("transposeChromatic", transpose.chromatic);
    }
    if (use_drumset) {
        xml.tag("drumset", int(use_drumset));
    }
    if (drumset) {
        drumset->save(xml);
    }

    if (!single_note_dynamics) { // default is true
        xml.tag("singleNoteDynamics", single_note_dynamics);
    }

    xml.tag("glissandoStyle", TConv::to_xml(glissando_style));

    for (const NamedEventList &a : midi_actions) {
        write::TWrite::write(&a, xml, "MidiAction");
    }
    for (const InstrChannel &a : channel) {
        write::TWrite::write(&a, xml, nullptr);
    }
    for (const MidiArticulation &ma : midi_articulations) {
        bool is_global = false;
        for (const MidiArticulation &ga : mu::engraving::midi_articulations) {
            if (ma == ga) {
                is_global = true;
                break;
            }
        }
        if (!is_global) {
            write::TWrite::write(&ma, xml);
        }
    }
    if (family) {
        xml.tag("family", family->id);
    }

    xml.end_element();
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

String translate_instrument_name(const String &instrument_id, const String &name_type, const String &text) {
    String disambiguation = instrument_id + u' ' + name_type;
    return muse::mtrc("engraving/instruments", text, disambiguation);
}

void InstrumentTemplate::read(XmlReader &e) {
    id = e.attribute("id");

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "soundId") {
            sound_id = e.read_text();
        } else if (tag == "longName" || tag == "name") { // "name" is obsolete
            [[maybe_unused]] int pos = e.int_attribute("pos", 0);
            instrument_name.set_long_name(translate_instrument_name(id, u"longName", e.read_text()));
        } else if (tag == "shortName" || tag == "short-name") { // "short-name" is obsolete
            [[maybe_unused]] int pos = e.int_attribute("pos", 0);
            instrument_name.set_short_name(translate_instrument_name(id, u"shortName", e.read_text()));
        } else if (tag == "trackName") {
            track_name = translate_instrument_name(id, u"trackName", e.read_text());
        } else if (tag == "description") {
            description = translate_instrument_name(id, u"description", e.read_text());
        } else if (tag == "extended") {
            extended = e.read_int();
        } else if (tag == "staves") {
            staff_count = e.read_int();
            bracket_span[0] = static_cast<int>(staff_count);
            //                  for (int i = 0; i < staves-1; ++i)
            //                        barlineSpan[i] = true;
        } else if (tag == "clef") { // sets both transposing and concert clef
            int idx = read_staff_idx(e);
            ClefType ct = TConv::from_xml(e.read_ascii_text(), ClefType::G);
            clef_types[idx].concert_clef = ct;
            clef_types[idx].transposing_clef = ct;
        } else if (tag == "concertClef") {
            int idx = read_staff_idx(e);
            clef_types[idx].concert_clef = TConv::from_xml(e.read_ascii_text(), ClefType::G);
        } else if (tag == "transposingClef") {
            int idx = read_staff_idx(e);
            clef_types[idx].transposing_clef = TConv::from_xml(e.read_ascii_text(), ClefType::G);
        } else if (tag == "stafflines") {
            int idx = read_staff_idx(e);
            staff_lines[idx] = e.read_int();
        } else if (tag == "smallStaff") {
            int idx = read_staff_idx(e);
            small_staff[idx] = e.read_int();
        } else if (tag == "bracket") {
            int idx = read_staff_idx(e);
            bracket[idx] = BracketType(e.read_int());
        } else if (tag == "bracketSpan") {
            int idx = read_staff_idx(e);
            bracket_span[idx] = e.read_int();
        } else if (tag == "barlineSpan") {
            int idx = read_staff_idx(e);
            int span = e.read_int();
            for (int i = 0; i < span - 1; ++i) {
                barline_span[idx + i] = true;
            }
        } else if (tag == "aPitchRange") {
            set_pitch_range(e.read_text(), min_pitch_a, max_pitch_a);
        } else if (tag == "pPitchRange") {
            set_pitch_range(e.read_text(), min_pitch_p, max_pitch_p);
        } else if (tag == "transposition") { // obsolete
            int i = e.read_int();
            transpose.chromatic = i;
            transpose.diatonic = Interval::chromatic2diatonic(i);
        } else if (tag == "transposeChromatic") {
            transpose.chromatic = e.read_int();
        } else if (tag == "transposeDiatonic") {
            transpose.diatonic = e.read_int();
        } else if (tag == "traitName") {
            trait.type = trait_type_from_string(e.attribute("type"));
            String trait_name = translate_instrument_name(id, u"traitName", e.read_text());
            trait.is_default = trait_name.contains(u'*');
            trait.is_hidden_on_score = trait_name.contains(u'(') && trait_name.contains(u')');
            trait.name = trait_name.remove(u'*').remove(u'(').remove(u')');
        } else if (tag == "StringData") {
            read460::TRead::read(&string_data, e);
        } else if (tag == "drumset") {
            use_drumset = e.read_int();
        } else if (tag == "Drum") {
            // if we see one of these tags, a custom drumset will be created
            if (!drumset) {
                drumset = new Drumset(*sm_drumset);
                drumset->clear();
            }
            drumset->load_drum(e);
        } else if (tag == "percussionPanelColumns") {
            // if we see one of these tags, a custom drumset will be created
            if (!drumset) {
                drumset = new Drumset(*sm_drumset);
                drumset->clear();
            }
            drumset->set_percussion_panel_columns(e.read_int());
        } else if (tag == "MidiAction") {
            NamedEventList a;
            read460::TRead::read(&a, e);
            midi_actions.push_back(a);
        } else if (tag == "Channel" || tag == "channel") {
            InstrChannel a;
            InstrumentTrackId t_id;
            read460::ReadContext rctx(nullptr);
            read460::TRead::read(&a, e, rctx, nullptr, t_id);
            channel.push_back(a);
        } else if (tag == "Articulation") {
            MidiArticulation a;
            read460::TRead::read(&a, e);
            size_t n = midi_articulations.size();
            size_t i;
            for (i = 0; i < n; ++i) {
                if (midi_articulations[i].name == a.name) {
                    midi_articulations[i] = a;
                    break;
                }
            }
            if (i == n) {
                midi_articulations.push_back(a);
            }
        } else if (tag == "stafftype") {
            int staff_idx = read_staff_idx(e);
            String xml_preset_name = e.attribute("staffTypePreset");
            String stf_group = e.read_text();
            if (stf_group == "percussion") {
                staff_group = StaffGroup::PERCUSSION;
            } else if (stf_group == "tablature") {
                staff_group = StaffGroup::TAB;
            } else {
                staff_group = StaffGroup::STANDARD;
            }
            staff_type_preset = 0;
            if (!xml_preset_name.is_empty()) {
                staff_type_preset = StaffType::preset_from_xml_name(xml_preset_name);
            }
            if (!staff_type_preset || staff_type_preset->group() != staff_group) {
                staff_type_preset = StaffType::get_default_preset(staff_group);
            }
            if (staff_type_preset) {
                staff_lines[staff_idx] = staff_type_preset->lines();
            }
        } else if (tag == "init") {
            String val(e.read_text());
            const InstrumentTemplate *ttt = search_template(val);
            if (ttt) {
                String id_value = id;
                init(*ttt);
                id = id_value;
            } else {
                LOGD("InstrumentTemplate:: init instrument <%s> not found", muPrintable(val));
            }
        } else if (tag == "musicXMLid") {
            music_xml_id = e.read_text();
        } else if (tag == "family") {
            family = search_instrument_family(e.read_text());
        } else if (tag == "genre") {
            String val(e.read_text());
            link_genre(val);
        } else if (tag == "singleNoteDynamics") {
            single_note_dynamics = e.read_bool();
        } else if (tag == "glissandoStyle") {
            glissando_style = TConv::from_xml(e.read_ascii_text(), GlissandoStyle::CHROMATIC);
        } else {
            e.unknown();
        }
    }
    if (channel.empty()) {
        InstrChannel a;
        a.set_chorus(0);
        a.set_reverb(0);
        a.set_name(String::from_utf8(InstrChannel::default_name));
        a.set_program(0);
        a.set_bank(0);
        a.set_volume(90);
        a.set_pan(0);
        channel.push_back(a);
    }

    if (track_name.is_empty() && !instrument_name.long_name().is_empty()) {
        track_name = instrument_name.long_name();
    }
    if (description.is_empty() && !instrument_name.short_name().is_empty()) {
        description = instrument_name.short_name();
    }
    if (id.empty()) {
        id = track_name.to_lower().replace(u' ', u'-');
    }

    if (staff_count == 0) {
        LOGD(" 2Instrument: staves == 0 <%s>", muPrintable(id));
    }
}

//---------------------------------------------------------
//   setPitchRange
//---------------------------------------------------------

void InstrumentTemplate::set_pitch_range(const String &s, int &a, int &b) const {
    StringList sl = s.split(u'-');
    if (sl.size() != 2) {
        a = ::mu::engraving::min_pitch;
        b = ::mu::engraving::max_pitch;
        return;
    }
    a = sl.at(0).to_int();
    b = sl.at(1).to_int();
}

//---------------------------------------------------------
//   clearInstrumentTemplates
//---------------------------------------------------------

void clear_instrument_templates() {
    for (const InstrumentGroup *g : instrument_groups) {
        const_cast<InstrumentGroup *>(g)->clear();
    }
    muse::delete_all(instrument_groups);
    instrument_groups.clear();
    muse::delete_all(instrument_genres);
    instrument_genres.clear();
    muse::delete_all(instrument_families);
    instrument_families.clear();
    midi_articulations.clear();
    instrument_orders.clear();
}

//---------------------------------------------------------
//   loadInstrumentTemplates
//---------------------------------------------------------

bool load_instrument_templates(const muse::io::PathT &instr_templates_path) {
    File qf(instr_templates_path);
    if (!qf.open(IODevice::READ_ONLY)) {
        LOGE() << "Could not load instrument templates from " << instr_templates_path;
        return false;
    }

    XmlReader e(&qf);
    while (e.read_next_start_element()) {
        if (e.name() == "museScore") {
            while (e.read_next_start_element()) {
                const AsciiStringView tag(e.name());
                if (tag == "instrument-group" || tag == "InstrumentGroup") {
                    String id_group(e.attribute("id"));
                    InstrumentGroup *group = search_instrument_group(id_group);
                    if (group == 0) {
                        group = new InstrumentGroup;
                        instrument_groups.push_back(group);
                    }
                    group->read(e);
                } else if (tag == "Articulation") {
                    // read global articulation
                    String name(e.attribute("name"));
                    MidiArticulation a = search_articulation(name);
                    read460::TRead::read(&a, e);
                    midi_articulations.push_back(a);
                } else if (tag == "Genre") {
                    String id_genre(e.attribute("id"));
                    InstrumentGenre *genre = const_cast<InstrumentGenre *>(search_instrument_genre(id_genre));
                    if (!genre) {
                        genre = new InstrumentGenre;
                        instrument_genres.push_back(genre);
                    }
                    genre->read(e);
                } else if (tag == "Family") {
                    String id_family(e.attribute("id"));
                    InstrumentFamily *fam = const_cast<InstrumentFamily *>(search_instrument_family(id_family));
                    if (!fam) {
                        fam = new InstrumentFamily;
                        instrument_families.push_back(fam);
                    }
                    fam->read(e);
                } else if (tag == "Order") {
                    ScoreOrder order;
                    order.read(e);
                    instrument_orders.push_back(order);
                } else {
                    e.unknown();
                }
            }
        }
    }

    return true;
}

const InstrumentTemplate *search_template(const String &name) {
    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->id == name) {
                return it;
            }
        }
    }
    return 0;
}

// Given an unknown instrument (i.e. one that lacks a valid MuseScore ID),
// compare it to each of our built-in instruments and return the strongest
// available match, or `nullptr` if no match is found.
//
// Match strength is determined according to multiple factors (see code). Feel
// free to add more factors to this function. Please don't create new functions
// that only consider a subset of factors. You can disable matching against a
// specific factor by setting it to its default value in the input instrument.
//
// Use this to recognize instruments in pre-3.6 score files, and in non-native
// formats like MIDI and MusicXML (e.g. see importmusicxmlpass2.cpp). This
// ensures the correct sound and engraving properties are loaded.

const InstrumentTemplate *combined_template_search(const Instrument &instrument) {
    const InstrChannel *const channel = instrument.channel(0);
    const int bank = channel->bank();
    const int program = channel->program();
    const int transpose_chromatic = instrument.transpose().chromatic;
    const int transpose_key = (transpose_chromatic % 12) + (transpose_chromatic < 0 ? 12 : 0); // 0=C, 1=C#/Db, 2=D...
    const String &track_name = instrument.track_name();
    const String &trait_name = instrument.trait().name;
    String music_xml_id = instrument.music_xml_id();

    if (music_xml_id.is_empty() && track_name.is_empty() && bank == 0 && program == -1) {
        return nullptr; // Not enough information to find a matching template.
    }

    if (music_xml_id.is_empty()) {
        if (track_name.contains(u"drum", muse::CASE_INSENSITIVE)) {
            music_xml_id = u"drum.group.set";
        } else if (track_name == u"MusicXML Part" || track_name.contains(u"piano", muse::CASE_INSENSITIVE)) {
            music_xml_id = u"keyboard.piano";
        }
    } else {
        // Workaround for old generic instrument templates.
        if ((music_xml_id == u"wind.reed.clarinet" || music_xml_id == u"brass.trumpet") && (trait_name == u"B♭" || transpose_key == 10)) {
            music_xml_id.append(u".bflat");
        }
    }

    // Perform a weighted search over instrument names. Current limitations:
    // * We only check in the Preferences language. (Ideally we'd check English, German & Italian.)
    // * We don't consider aliases (e.g. we look for "Violoncello" but not "Double Bass").
    static constexpr int track_name_weight = 128;
    static constexpr int long_name_weight = 64;
    static constexpr int short_name_weight = 16; // Not standardized outside MuseScore.
    static constexpr int trait_name_weight = 4;  // More reliable than transposition interval.

    // Also search over these parameters, which don't depend on the language.
    static constexpr int musicxml_id_weight = 32;      // Not unique. Newer, more specialist IDs aren't in old files.
    static constexpr int midi_weight = 8;              // Standardization limited to General MIDI Level 2.
    static constexpr int transposition_key_weight = 2; // Unreliable (e.g. in concert-pitch MusicXML).
    static constexpr int transposition_octave_weight = 1;

    // Optimize performance: Ignore low-strength matches.
    static constexpr int min_match_strength = midi_weight;

    // Also consider name almost-matches (i.e. fuzzy text search).
    static constexpr size_t initial_levenshtein_distance = std::numeric_limits<size_t>::max();

    const InstrumentTemplate *best_match = nullptr;
    int best_match_strength = min_match_strength;
    auto best_match_lev_dist = initial_levenshtein_distance;

    for (const InstrumentGroup *group : instrument_groups) {
        for (const InstrumentTemplate *templ : group->instrument_templates) {
            if (templ->trait.name == u"[hide]") {
                continue;
            }

            int match_strength = 0;

            if (!track_name.is_empty() && track_name == templ->track_name) {
                match_strength += track_name_weight;
            }

            if (!instrument.long_name().empty() && instrument.long_name() == templ->instrument_name.long_name()) {
                match_strength += long_name_weight;
            }

            if (!instrument.short_name().empty() && instrument.short_name() == templ->instrument_name.short_name()) {
                match_strength += short_name_weight;
            }

            if (!trait_name.is_empty() && trait_name == templ->trait.name) {
                match_strength += trait_name_weight;
            }

            if (!music_xml_id.empty() && music_xml_id == templ->music_xml_id) {
                match_strength += musicxml_id_weight;
            }

            for (const InstrChannel &templ_channel : templ->channel) {
                if (bank == templ_channel.bank() && program == templ_channel.program()) {
                    match_strength += midi_weight;
                    break;
                }
            }

            const int chromatic_diff = std::abs(transpose_chromatic - templ->transpose.chromatic);

            if (chromatic_diff % 12 == 0) { // Same key.
                match_strength += transposition_key_weight;
            }

            if (chromatic_diff < 12) { // Same octave.
                match_strength += transposition_octave_weight;
            }

            if (match_strength < best_match_strength) {
                continue; // Keep looking.
            }

            if (match_strength > best_match_strength) {
                best_match = templ;
                best_match_strength = match_strength;
                best_match_lev_dist = initial_levenshtein_distance; // Reset.
            }

            // Find smallest lev. dist. among templates with match strength equal to best.
            auto lev_dist = initial_levenshtein_distance;

            if (!track_name.is_empty()) {
                lev_dist = muse::strings::levenshtein_distance(track_name.to_std_string(), templ->track_name.to_std_string());
            }

            lev_dist = std::min(lev_dist, muse::strings::levenshtein_distance(instrument.long_name().to_std_string(),
                                                                              templ->instrument_name.long_name().to_std_string()));

            if (lev_dist < best_match_lev_dist) {
                best_match = templ;
                best_match_lev_dist = lev_dist;
            }
        }
    }

    return best_match;
}

const InstrumentTemplate *search_template_for_music_xml_id(const String &mxml_id) {
    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->music_xml_id == mxml_id) {
                return it;
            }
        }
    }
    return 0;
}

const InstrumentTemplate *search_template_for_instr_name_list(const std::vector<String> &name_list, bool use_drumset, bool case_sensitive) {
    const InstrumentTemplate *best_match = nullptr; // default if no matches
    int best_match_strength = 0;                    // higher for better matches

    for (const String &name : name_list) {
        if (name.is_empty()) {
            continue;
        }

        auto string_equals_name = case_sensitive ? std::function([&name](const String &s) { return s == name; }) :
                                                   std::function([&name](const String &s) { return s.is_equal_ignore_case(name); });

        auto staff_name_equals_name = case_sensitive ? std::function([&name](const String &sn) { return sn == name; }) :
                                                       std::function([&name](const String &sn) { return sn.is_equal_ignore_case(name); });

        for (const InstrumentGroup *g : instrument_groups) {
            for (const InstrumentTemplate *it : g->instrument_templates) {
                if (it->use_drumset != use_drumset) {
                    continue;
                }

                int match_strength =
                    0 + (4 * (string_equals_name(it->track_name) ? 1 : 0)) // most weight to track name since there are fewer duplicates
                    + (2 * (staff_name_equals_name(it->instrument_name.long_name()) ? 1 : 0)) +
                    (1 * (staff_name_equals_name(it->instrument_name.short_name()) ? 1 : 0)); // least weight to short name
                const int perfect_match_strength = 7;
                assert(match_strength <= perfect_match_strength);
                if (match_strength > best_match_strength) {
                    best_match = it;
                    best_match_strength = match_strength;
                    if (best_match_strength == perfect_match_strength) {
                        break; // stop looking for matches
                    }
                }
            }
        }
    }

    if (!best_match) {
        static const std::wregex drumset_regex(L"drum ?(set|kit)", std::regex_constants::icase);

        for (const String &name : name_list) {
            if (name.contains(drumset_regex)) {
                return search_template(u"drumset"); // Large Drum Kit
            }

            if (name.contains(u"drum", muse::CASE_INSENSITIVE) || name.contains(u"percussion", muse::CASE_INSENSITIVE)) {
                return search_template(u"percussion-synthesizer"); // General MIDI percussion
            }

            if (name.contains(u"piano", muse::CASE_INSENSITIVE)) {
                return search_template(u"piano");
            }
        }
    }

    return best_match; // nullptr if no matches found
}

const InstrumentTemplate *search_template_for_midi_program(int bank, int program, bool use_drumset) {
    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->use_drumset != use_drumset) {
                continue;
            }

            for (const InstrChannel &channel : it->channel) {
                if (channel.bank() == bank && channel.program() == program) {
                    return it;
                }
            }
        }
    }

    return nullptr;
}

void add_template_to_group(const InstrumentTemplate *templ, const String &group_id) {
    IF_ASSERT_FAILED(templ) { return; }

    InstrumentGroup *group = search_instrument_group(group_id);
    if (group) {
        group->instrument_templates.push_back(templ);
    }
}

//---------------------------------------------------------
//   searchTemplateIndexForTrackName
//---------------------------------------------------------

InstrumentIndex search_template_index_for_track_name(const String &track_name) {
    int inst_index = 0;
    int grp_index = 0;
    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->track_name == track_name) {
                return InstrumentIndex(grp_index, inst_index, it);
            }
            ++inst_index;
        }
        ++grp_index;
    }
    return InstrumentIndex(-1, -1, nullptr);
}

//---------------------------------------------------------
//   searchTemplateIndexForId
//---------------------------------------------------------

InstrumentIndex search_template_index_for_id(const String &id) {
    int inst_index = 0;
    int grp_index = 0;
    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->id == id) {
                return InstrumentIndex(grp_index, inst_index, it);
            }
            ++inst_index;
        }
        ++grp_index;
    }
    return InstrumentIndex(-1, inst_index, nullptr);
}

//---------------------------------------------------------
//   linkGenre
//      link the current instrument template to the genre list specified by "genre"
//      Each genre is a list of pointers to instrument templates
//      The list of genres is at application level
//---------------------------------------------------------

void InstrumentTemplate::link_genre(const String &genre) {
    const InstrumentGenre *ig = search_instrument_genre(genre);
    if (ig) {
        genres.push_back(ig);
    }
}

void InstrumentGenre::write(XmlWriter &xml) const {
    xml.start_element("Genre", {{"id", id}});
    xml.tag("name", name);
    xml.end_element();
}

void InstrumentGenre::read(XmlReader &e) {
    id = e.attribute("id");
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            name = muse::mtrc("engraving/instruments/genre", e.read_text());
        } else {
            e.unknown();
        }
    }
}

void InstrumentFamily::write(XmlWriter &xml) const {
    xml.start_element("Family", {{"id", id}});
    xml.tag("name", name);
    xml.end_element();
}

void InstrumentFamily::read(XmlReader &e) {
    id = e.attribute("id");
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            name = muse::mtrc("engraving/instruments/family", e.read_text());
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   clefType
//---------------------------------------------------------

ClefTypeList InstrumentTemplate::clef_type(staff_idx_t staff_idx) const {
    if (staff_idx < staff_count) {
        return clef_types[staff_idx];
    }
    return clef_types[0];
}

String InstrumentTemplate::family_id() const { return family ? family->id : String(); }

bool InstrumentTemplate::contains_genre(const String &genre_id) const {
    for (const InstrumentGenre *genre : genres) {
        if (genre->id == genre_id) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   defaultClef
//    traverse the instrument list for first instrument
//    with midi patch 'program'. Return the default clef
//    for this instrument.
//---------------------------------------------------------

ClefType default_clef(int program) {
    if (program >= 24 && program < 32) { // this are guitars
        return ClefType::G8_VB;
    } else if (program >= 32 && program < 40) { // this is bass
        return ClefType::F8_VB;
    }

    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->channel[0].bank() == 0 && it->channel[0].program() == program) {
                return it->clef_types[0].concert_clef;
            }
        }
    }
    return ClefType::G;
}
} // namespace mu::engraving
