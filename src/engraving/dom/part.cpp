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

#include "part.h"

#include "chordrest.h"
#include "containers.h"
#include "engraving/internal/engravingconfiguration.h"
#include "factory.h"
#include "fret.h"
#include "harppedaldiagram.h"
#include "instrchange.h"
#include "instrtemplate.h"
#include "linkedobjects.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "rw/xmlwriter.h"
#include "score.h"
#include "sharedpart.h"
#include "sourceidentity.h"
#include "staff.h"
#include "stringtunings.h"
#include "style/style.h"
#include "system.h"
#include "textbase.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
const Fraction Part::main_instrument_tick = Fraction(-1, 1);
//---------------------------------------------------------
//   Part
//---------------------------------------------------------

Part::Part(Score *s, ElementType type) : EngravingObject(type, s) {
    show_ = true;
    soloist_ = false;
    instruments_.set_instrument(new Instrument, -1); // default instrument
    prefer_sharp_flat_ = PreferSharpFlat::AUTO;
}

Part::~Part() {
    // A part is not an item, so nothing else unregisters its id. The register lives
    // on the master score, which outlives the part unless the whole document is
    // going away; a leftover entry would hand the id to whatever is allocated at the
    // same address next.
    MasterScore *master = score() ? score()->master_score() : nullptr;
    if (master && Score::is_alive(master)) {
        master->eid_register()->on_item_destroyed(this);
    }
}

//---------------------------------------------------------
//   initFromInstrTemplate
//---------------------------------------------------------

void Part::init_from_instr_template(const InstrumentTemplate *t) { set_instrument(Instrument::from_template(t)); }

const ID &Part::id() const { return id_; }

void Part::set_id(const ID &id) { id_ = id; }

Part *Part::clone() const { return new Part(*this); }

//---------------------------------------------------------
//   staff
//---------------------------------------------------------

Staff *Part::staff(staff_idx_t idx) const {
    if (idx >= staves_.size()) {
        return nullptr;
    }

    return staves_[idx];
}

//---------------------------------------------------------
//   family
//---------------------------------------------------------

String Part::family_id() const {
    if (instruments_.empty()) {
        return String();
    }

    InstrumentIndex ii = search_template_index_for_id(instrument_id());
    return ii.instr_template && ii.instr_template->family ? ii.instr_template->family->id : String();
}

//---------------------------------------------------------
//   Part::masterPart
//---------------------------------------------------------

const Part *Part::master_part() const {
    if (score()->is_master()) {
        return this;
    }
    if (staves_.empty()) {
        return this;
    }

    Staff *st = staves_[0];
    LinkedObjects *links = st->links();
    if (!links) {
        return this;
    }

    for (EngravingObject *le : *links) {
        if (le->is_staff() && to_staff(le)->score()->is_master()) {
            if (Part *p = to_staff(le)->part()) {
                return p;
            }
        }
    }
    return this;
}

//---------------------------------------------------------
//   Part::masterPart
//---------------------------------------------------------

Part *Part::master_part() { return const_cast<Part *>(const_cast<const Part *>(this)->master_part()); }

const Part *Part::origin_part(track_idx_t, const Fraction &) const { return this; }

Part *Part::origin_part(track_idx_t track, const Fraction &tick) {
    return const_cast<Part *>(const_cast<const Part *>(this)->origin_part(track, tick));
}

size_t Part::nstaves() const { return staves_.size(); }

size_t Part::visible_staves_count() const {
    size_t result = 0;
    for (const Staff *staff : staves_) {
        if (staff->show()) {
            ++result;
        }
    }

    return result;
}

const std::vector<Staff *> &Part::staves() const { return staves_; }

std::set<staff_idx_t> Part::stave_idx_list() const {
    std::set<staff_idx_t> result;

    for (const Staff *stave : staves_) {
        if (!stave) {
            continue;
        }

        result.insert(stave->idx());
    }

    return result;
}

void Part::append_staff(Staff *staff) { staves_.push_back(staff); }

void Part::clear_staves() { staves_.clear(); }

//---------------------------------------------------------
//   setStaves
//---------------------------------------------------------

void Part::set_staves(int n) {
    int ns = static_cast<int>(staves_.size());
    if (n < ns) {
        LOGD("Part::setStaves(): remove staves not implemented!");
        return;
    }

    int staff_idx = static_cast<int>(score()->staff_idx(this)) + ns;
    for (int i = ns; i < n; ++i) {
        Staff *staff = Factory::create_staff(this);
        score()->insert_staff(staff, i);

        for (Measure *m = score()->first_measure(); m; m = m->next_measure()) {
            m->insert_staff(staff, staff_idx);
            if (m->has_mm_rest()) {
                m->mm_rest()->insert_staff(staff, staff_idx);
            }
        }
        ++staff_idx;
    }
}

//---------------------------------------------------------
//   insertStaff
//---------------------------------------------------------

void Part::insert_staff(Staff *staff, staff_idx_t idx) {
    if (idx >= staves_.size()) {
        idx = staves_.size();
    }
    staves_.insert(staves_.begin() + idx, staff);
    staff->set_part(this);
}

//---------------------------------------------------------
//   removeStaff
//---------------------------------------------------------

void Part::remove_staff(Staff *staff) {
    if (!muse::remove(staves_, staff)) {
        LOGD("Part::removeStaff: not found %p", staff);
        return;
    }
}

bool Part::show() const {
    if (score()->configuration()->debugging_options().show_origin_and_combined_staves) {
        return show_;
    }

    bool shared_part_enabled = shared_part() && shared_part()->enabled();
    return show_ && !shared_part_enabled;
}

//---------------------------------------------------------
//   setMidiProgram
//---------------------------------------------------------

void Part::set_midi_program(int program, int bank) {
    InstrChannel *c = instrument()->channel(0);
    c->set_program(program);
    c->set_bank(bank);
}

//---------------------------------------------------------
//   midiProgram
//---------------------------------------------------------

int Part::midi_program() const { return instrument()->playback_channel(0, master_score())->program(); }

//---------------------------------------------------------
//   midiChannel
//---------------------------------------------------------

int Part::midi_channel() const { return master_score()->midi_channel(instrument()->channel(0)->channel()); }

//---------------------------------------------------------
//   midiPort
//---------------------------------------------------------

int Part::midi_port() const { return master_score()->midi_port(instrument()->channel(0)->channel()); }

//---------------------------------------------------------
//   setMidiChannel
//   Called from importmusicxml, importMidi and importGtp*.
//   Specify tick to set MIDI channel to an InstrumentChange element.
//   Usage:
//   setMidiChannel(channel)       to set channel
//   setMidiChannel(-1, port)      to set port
//   setMidiChannel(channel, port) to set both
//---------------------------------------------------------

void Part::set_midi_channel(int ch, int port, const Fraction &tick) {
    InstrChannel *channel = instrument(tick)->channel(0);
    if (channel->channel() == -1) {
        master_score()->add_midi_mapping(channel, this, port, ch);
    } else {
        master_score()->update_midi_mapping(channel, this, port, ch);
    }
}

//---------------------------------------------------------
//   setInstrument
//---------------------------------------------------------

void Part::set_instrument(Instrument *i, Fraction tick) { instruments_.set_instrument(i, tick.ticks()); }

void Part::set_instrument(Instrument *i, int tick) { instruments_.set_instrument(i, tick); }

void Part::set_instrument(const Instrument &&i, Fraction tick) { instruments_.set_instrument(new Instrument(i), tick.ticks()); }

void Part::set_instrument(const Instrument &i, Fraction tick) { instruments_.set_instrument(new Instrument(i), tick.ticks()); }

void Part::set_instruments(const InstrumentList &instruments) {
    instruments_.clear();

    for (auto it = instruments.begin(); it != instruments.end(); ++it) {
        instruments_.set_instrument(it->second, it->first);
    }
}

//---------------------------------------------------------
//   removeInstrument
//---------------------------------------------------------

void Part::remove_instrument(Instrument *instr, const Fraction &tick) {
    auto i = instruments_.find(tick.ticks());
    if (i == instruments_.end()) {
        LOGD("Part::removeInstrument: not found at tick %d", tick.ticks());
        return;
    }
    if (i->second != instr) {
        LOGD("Part::removeInstrument: item at tick %d (%p) != instr (%p)", tick.ticks(), i->second, instr);
        return;
    }
    instruments_.erase(i);
}

//---------------------------------------------------------
//   removeNonPrimaryInstruments
//---------------------------------------------------------

void Part::remove_non_primary_instruments() {
    auto it = instruments_.begin();
    while (it != instruments_.end()) {
        if (it->first != -1) {
            it = instruments_.erase(it);
            continue;
        }
        ++it;
    }
}

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

Instrument *Part::instrument(Fraction tick) { return instruments_.instrument(tick.ticks()); }

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

const Instrument *Part::instrument(Fraction tick) const { return instruments_.instrument(tick.ticks()); }

const Instrument *Part::instrument_by_id(const String &id) const {
    for (const auto &pair : instruments_) {
        if (pair.second->id() == id) {
            return pair.second;
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   instruments
//---------------------------------------------------------

const InstrumentList &Part::instruments() const { return instruments_; }

const StringData *Part::string_data(const Fraction &tick, staff_idx_t staff_idx) const {
    if (!score()) {
        return nullptr;
    }

    const Instrument *instrument = this->instrument(tick);
    if (!instrument) {
        return nullptr;
    }

    bool reflect_transposition_in_linked_tab = true;

    const Staff *staff = staff_idx != muse::nidx ? score()->staff(staff_idx) : nullptr;
    if (staff && staff->is_tab_staff(tick)) {
        if (const Staff *primary_staff = staff->primary_staff()) {
            reflect_transposition_in_linked_tab = primary_staff->reflect_transposition_in_linked_tab();
        }
    }

    StringTunings *string_tunings = nullptr;

    if (reflect_transposition_in_linked_tab) {
        auto it = muse::find_less_or_equal(string_tunings_, tick.ticks());
        if (it != string_tunings_.end()) {
            string_tunings = it->second;
        }
    }

    if (string_tunings) {
        //!NOTE: if there is string tunings element between current instrument and current tick,
        //! then return string data from string tunings element
        const Instrument *string_tunings_instrument = this->instrument(string_tunings->tick());
        if (instrument == string_tunings_instrument) {
            return string_tunings->string_data();
        }
    }

    return instrument->string_data();
}

void Part::add_string_tunings(StringTunings *string_tunings) {
    DO_ASSERT(!is_shared_part());
    string_tunings_[string_tunings->segment()->tick().ticks()] = string_tunings;
}

void Part::remove_string_tunings(StringTunings *string_tunings) {
    int tick = string_tunings->segment()->tick().ticks();
    auto it = string_tunings_.find(tick);

    if (it != string_tunings_.end() && it->second == string_tunings) {
        string_tunings_.erase(it);
    }
}

StringTunings *Part::next_string_tuning(const Fraction &tick) const {
    auto i = string_tunings_.upper_bound(tick.ticks());
    return (i == string_tunings_.end()) ? nullptr : i->second;
}

//---------------------------------------------------------
//   instrumentId
//---------------------------------------------------------

String Part::instrument_id(const Fraction &tick) const { return instrument(tick)->id(); }

//---------------------------------------------------------
//   longName
//---------------------------------------------------------

String Part::long_name(const Fraction &tick) const { return instrument(tick)->long_name(); }

//---------------------------------------------------------
//   instrumentName
//---------------------------------------------------------

String Part::instrument_name(const Fraction &tick) const { return instrument(tick)->track_name(); }

//---------------------------------------------------------
//   shortName
//---------------------------------------------------------

String Part::short_name(const Fraction &tick) const { return instrument(tick)->short_name(); }

//---------------------------------------------------------
//   setLongName
//---------------------------------------------------------

void Part::set_long_name(const String &s, const Fraction &tick) { instrument(tick)->set_long_name(s); }

//---------------------------------------------------------
//   setShortName
//---------------------------------------------------------

void Part::set_short_name(const String &s, const Fraction &tick) { instrument(tick)->set_short_name(s); }

//---------------------------------------------------------
//   setLongNameAll
//---------------------------------------------------------

void Part::set_long_name_all(const String &s) {
    for (auto instrument : instruments_) {
        instrument.second->set_long_name(s);
    }
}

//---------------------------------------------------------
//   setShortNameAll
//---------------------------------------------------------

void Part::set_short_name_all(const String &s) {
    for (auto instrument : instruments_) {
        instrument.second->set_short_name(s);
    }
}

int Part::number(const Fraction &tick) const { return instrument(tick)->number(); }

void Part::set_number(int v, const Fraction &tick) { instrument(tick)->set_number(v); }

String Part::transposition(const Fraction &tick) const { return instrument(tick)->transposition(); }

void Part::set_transposition(const String &s, const Fraction &tick) { instrument(tick)->set_transposition(s); }

//---------------------------------------------------------
//   setPlainLongName
//---------------------------------------------------------

void Part::set_plain_long_name(const String &s) { set_long_name(XmlWriter::xml_string(s)); }

//---------------------------------------------------------
//   setPlainShortName
//---------------------------------------------------------

void Part::set_plain_short_name(const String &s) { set_short_name(XmlWriter::xml_string(s)); }

//---------------------------------------------------------
//   setPlainLongNameAll
//---------------------------------------------------------

void Part::set_plain_long_name_all(const String &s) { set_long_name_all(XmlWriter::xml_string(s)); }

//---------------------------------------------------------
//   setPlainShortNameAll
//---------------------------------------------------------

void Part::set_plain_short_name_all(const String &s) { set_short_name_all(XmlWriter::xml_string(s)); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Part::get_property(Pid id) const {
    switch (id) {
        case Pid::VISIBLE: return PropertyValue(show_);
        case Pid::HIDE_WHEN_EMPTY: return PropertyValue(hide_when_empty_);
        case Pid::HIDE_STAVES_WHEN_INDIVIDUALLY_EMPTY: return PropertyValue(hide_staves_when_individually_empty_);
        case Pid::USE_DRUMSET: return instrument()->use_drumset();
        case Pid::PREFER_SHARP_FLAT: return int(prefer_sharp_flat());
        default: return PropertyValue();
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Part::set_property(Pid id, const PropertyValue &property) {
    switch (id) {
        case Pid::VISIBLE: set_show(property.to_bool()); break;
        case Pid::HIDE_WHEN_EMPTY: set_hide_when_empty(property.value<AutoOnOff>()); break;
        case Pid::HIDE_STAVES_WHEN_INDIVIDUALLY_EMPTY: set_hide_staves_when_individually_empty(property.to_bool()); break;
        case Pid::USE_DRUMSET: instrument()->set_use_drumset(property.to_bool()); break;
        case Pid::PREFER_SHARP_FLAT: set_prefer_sharp_flat(PreferSharpFlat(property.to_int())); break;
        default: LOGD("Part::setProperty: unknown id %d", int(id)); break;
    }
    score()->set_layout_all();
    return true;
}

TrackRange Part::track_range() const {
    IF_ASSERT_FAILED(!staves_.empty()) { return {}; }

    const track_idx_t start_track = staves_.front()->idx() * ::mu::engraving::voices;
    return {start_track, start_track + staves_.size() * ::mu::engraving::voices};
}

InstrumentTrackIdList Part::instrument_track_id_list() const {
    InstrumentTrackIdList result;
    std::set<String> seen;

    for (const auto &pair : instruments_) {
        String instr_id = pair.second->id();
        if (seen.insert(instr_id).second) {
            result.push_back({id_, instr_id});
        }
    }

    return result;
}

InstrumentTrackIdSet Part::instrument_track_id_set() const {
    InstrumentTrackIdSet result;

    for (const auto &pair : instruments_) {
        result.insert({id_, pair.second->id()});
    }

    return result;
}

//---------------------------------------------------------
//   insertTime
//---------------------------------------------------------

void Part::insert_time(const Fraction &tick, const Fraction &len) {
    if (len.is_zero()) {
        return;
    }

    // move all instruments

    if (len < Fraction(0, 1)) {
        // remove instruments between tickpos >= tick and tickpos < (tick+len)
        // ownership goes back to class InstrumentChange()

        auto si = instruments_.lower_bound(tick.ticks());
        auto ei = instruments_.lower_bound((tick - len).ticks());
        instruments_.erase(si, ei);

        // remove harp pedal diagrams between tickpo >= tick
        harp_diagrams_.erase(harp_diagrams_.lower_bound(tick.ticks()), harp_diagrams_.lower_bound((tick - len).ticks()));
    }

    InstrumentList il;

    for (auto i = instruments_.lower_bound(tick.ticks()); i != instruments_.end();) {
        Instrument *instrument = i->second;
        int t = i->first;
        instruments_.erase(i++);
        il[t + len.ticks()] = instrument;
    }
    instruments_.insert(il.begin(), il.end());

    std::map<int, HarpPedalDiagram *> hd2;
    for (auto h = harp_diagrams_.lower_bound(tick.ticks()); h != harp_diagrams_.end();) {
        HarpPedalDiagram *diagram = h->second;
        int t = h->first;
        harp_diagrams_.erase(h++);
        hd2[t + len.ticks()] = diagram;
    }
    harp_diagrams_.insert(hd2.begin(), hd2.end());
}

//---------------------------------------------------------
//   addHarpDiagram
//---------------------------------------------------------

void Part::add_harp_diagram(HarpPedalDiagram *harp_diagram) {
    DO_ASSERT(!is_shared_part());
    harp_diagrams_[harp_diagram->segment()->tick().ticks()] = harp_diagram;
}

//---------------------------------------------------------
//   removeHarpDiagram
//---------------------------------------------------------

void Part::remove_harp_diagram(HarpPedalDiagram *harp_diagram) {
    int tick = harp_diagram->segment()->tick().ticks();
    auto it = harp_diagrams_.find(tick);

    if (it != harp_diagrams_.end() && it->second == harp_diagram) {
        harp_diagrams_.erase(it);
    }
}

//---------------------------------------------------------
//   clearHarpDiagrams
//---------------------------------------------------------

void Part::clear_harp_diagrams() { harp_diagrams_.clear(); }

//---------------------------------------------------------
//   currentHarpDiagram
//---------------------------------------------------------

HarpPedalDiagram *Part::current_harp_diagram(const Fraction &tick) const {
    auto i = harp_diagrams_.upper_bound(tick.ticks());
    if (i != harp_diagrams_.begin()) {
        --i;
    }
    if (i == harp_diagrams_.end()) {
        return nullptr;
    } else if (tick < Fraction::from_ticks(i->first)) {
        return nullptr;
    }
    return i->second;
}

//---------------------------------------------------------
//   nextHarpDiagram
//---------------------------------------------------------

HarpPedalDiagram *Part::next_harp_diagram(const Fraction &tick) const {
    auto i = harp_diagrams_.upper_bound(tick.ticks());
    return (i == harp_diagrams_.end()) ? nullptr : i->second;
}

//---------------------------------------------------------
//   prevHarpDiagram
//---------------------------------------------------------

HarpPedalDiagram *Part::prev_harp_diagram(const Fraction &tick) const {
    auto i = harp_diagrams_.lower_bound(tick.ticks());
    if (i == harp_diagrams_.begin()) {
        return nullptr;
    }
    i--;
    return i->second;
}

//---------------------------------------------------------
//   currentHarpDiagramTick
//---------------------------------------------------------

Fraction Part::current_harp_diagram_tick(const Fraction &tick) const {
    if (harp_diagrams_.empty()) {
        return Fraction(0, 1);
    }
    auto i = harp_diagrams_.upper_bound(tick.ticks());
    if (i == harp_diagrams_.begin()) {
        return Fraction(0, 1);
    }
    --i;
    return Fraction::from_ticks(i->first);
}

//---------------------------------------------------------
//   plainPartName
//    Upstream 8bba2d2823 ("Group naming") replaced the plain-text
//    Part::partName_ with a name derived from the instrument label, whose
//    long name is rich text (<font>/<b>/<sym> markup). Every partName()
//    caller - mixer track titles, MIDI export, excerpt names,
//    accessibility - expects plain display text, and the MusicXML exporter
//    has always stripped such markup from part names
//    (MScoreTextToMusicXml::toPlainText, documented for MuseScore 1.3 part
//    names). Convert to plain text here, once, for all of them.
//    The same commit dropped the fallback to Instrument::trackName() that
//    read114/read206/read460 applied when no part name was stored
//    ("if (partName.isEmpty()) partName = instrument()->trackName()"),
//    which loses the part name of scores without a <longName> element;
//    keep 4.7's fallback semantics.
//---------------------------------------------------------

String Part::plain_part_name(const Instrument *i) {
    const String rich_name = i->long_name();
    String name;
    bool in_tag = false;
    for (size_t idx = 0; idx < rich_name.size(); ++idx) {
        const Char c = rich_name.at(idx);
        if (c == u'<') {
            in_tag = true;
        } else if (c == u'>') {
            in_tag = false;
        } else if (!in_tag) {
            name.append(c);
        }
    }
    name = TextBase::un_escape(name);

    if (name.empty()) {
        // Legacy scores store the part name in the instrument track name.
        name = i->track_name();
    }

    return name;
}

String Part::part_name() const {
    const Instrument *i = instrument();
    String full_name = plain_part_name(i);

    const String &transp = i->transposition();
    if (!transp.empty()) {
        //: For instrument transposition, e.g. Horn in F
        full_name += u" " + muse::mtrc("notation", "in") + u" " + transp;
    }

    int n = number();
    if (n != 0) {
        full_name += u" " + String::number(n);
    }

    return full_name;
}

bool Part::is_visible() const { return show_; }

//---------------------------------------------------------
//   lyricCount
//---------------------------------------------------------

int Part::lyric_count() const {
    if (!score()) {
        return 0;
    }

    if (!score()->first_measure()) {
        return 0;
    }

    size_t count = 0;
    SegmentType st = SegmentType::CHORD_REST;
    const TrackRange range = track_range();
    for (Segment *seg = score()->first_measure()->first(st); seg; seg = seg->next1(st)) {
        for (track_idx_t i = range.start_track; i < range.end_track; ++i) {
            ChordRest *cr = to_chord_rest(seg->element(i));
            if (cr) {
                count += cr->lyrics().size();
            }
        }
    }
    return int(count);
}

//---------------------------------------------------------
//   harmonyCount
//---------------------------------------------------------

int Part::harmony_count() const {
    if (!score()) {
        return 0;
    }

    Measure *first_m = score()->first_measure();
    if (!first_m) {
        return 0;
    }

    SegmentType st = SegmentType::CHORD_REST;
    int count = 0;
    const TrackRange range = track_range();
    for (const Segment *seg = first_m->first(st); seg; seg = seg->next1(st)) {
        for (const EngravingItem *e : source_annotations(*seg)) {
            if ((e->is_harmony() || (e->is_fret_diagram() && to_fret_diagram(e)->harmony())) && e->track() >= range.start_track &&
                e->track() < range.end_track) {
                count++;
            }
        }
    }
    return count;
}

//---------------------------------------------------------
//   updateHarmonyChannels
///   update the harmony channel by creating a new channel
///   when appropriate or using the existing one
///
///   checkRemoval can be set to true to check to see if we
///   can remove the harmony channel
//---------------------------------------------------------
void Part::update_harmony_channels(bool is_do_on_instrument_changed, bool check_removal) {
    auto on_instrument_changed = [this]() {
        master_score()->rebuild_midi_mapping();
        master_score()->update_channel();
        score()->set_instruments_changed(true);
        score()->set_layout_all(); //do we need this?
    };

    // usage of harmony count is okay even if expensive since checking harmony channel will shortcircuit if existent
    // harmonyCount will only be called on loading of a score (where it will need to be scanned for harmony anyway)
    // or when the first harmony of a score is just added
    if (check_removal) {
        //may be a bit expensive since it gets called after every single delete or undo, but it should be okay for now
        //~OPTIM~
        if (harmony_count() == 0) {
            Instrument *instr = instrument();
            int h_ch_idx = instr->channel_idx(String::from_utf8(InstrChannel::harmony_name));
            if (h_ch_idx != -1) {
                InstrChannel *h_chan = instr->channel(h_ch_idx);
                instr->remove_channel(h_chan);
                delete h_chan;
                if (is_do_on_instrument_changed) {
                    on_instrument_changed();
                }
                return;
            }
        }
    }

    if (!harmony_channel() && harmony_count() > 0) {
        Instrument *instr = instrument();
        InstrChannel *c = new InstrChannel(*instr->channel(0));
        // default to program 0, which is piano in General MIDI
        c->set_program(0);
        if (c->bank() == 128) { // drumset?
            c->set_bank(0);
        }
        c->set_name(String::from_utf8(InstrChannel::harmony_name));
        instr->append_channel(c);
        on_instrument_changed();
    }
}

//---------------------------------------------------------
//   harmonyChannel
//---------------------------------------------------------

const InstrChannel *Part::harmony_channel() const {
    const Instrument *instr = instrument();
    if (!instr) {
        return nullptr;
    }

    int chan_idx = instr->channel_idx(String::from_utf8(InstrChannel::harmony_name));
    if (chan_idx == -1) {
        return nullptr;
    }

    const InstrChannel *chan = instr->channel(chan_idx);
    assert(chan);
    return chan;
}

bool Part::has_chord_symbol() const { return harmony_channel() != nullptr; }

//---------------------------------------------------------
//   hasPitchedStaff
//---------------------------------------------------------

bool Part::has_pitched_staff() const {
    for (Staff *s : staves()) {
        if (s && s->is_pitched_staff(Fraction(0, 1))) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasTabStaff
//---------------------------------------------------------

bool Part::has_tab_staff() const {
    for (Staff *s : staves()) {
        if (s && s->is_tab_staff(Fraction(0, 1))) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasDrumStaff
//---------------------------------------------------------

bool Part::has_drum_staff() const {
    for (Staff *s : staves()) {
        if (s && s->is_drum_staff(Fraction(0, 1))) {
            return true;
        }
    }
    return false;
}
} // namespace mu::engraving
