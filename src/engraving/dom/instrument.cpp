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

#include "instrument.h"

#include "drumset.h"
#include "instrtemplate.h"
#include "log.h"
#include "masterscore.h"
#include "stringdata.h"
#include "textbase.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//: Channel name for otherwise unnamed channels
const char *InstrChannel::default_name = "normal";
const char *InstrChannel::harmony_name = "harmony";
const char *InstrChannel::palm_mute_name = "palmmute";

Instrument InstrumentList::default_instrument;

//---------------------------------------------------------
//   operator
//---------------------------------------------------------

bool MidiArticulation::operator==(const MidiArticulation &i) const {
    return (i.name == name) && (i.velocity == velocity) && (i.gate_time == gate_time);
}

//---------------------------------------------------------
//   Instrument
//---------------------------------------------------------

Instrument::Instrument(String id) {
    id_ = id;
    InstrChannel *a = new InstrChannel;
    a->set_name(String::from_utf8(InstrChannel::default_name));
    channel_.push_back(a);

    min_pitch_a_ = ::mu::engraving::min_pitch;
    max_pitch_a_ = ::mu::engraving::max_pitch;
    min_pitch_p_ = ::mu::engraving::min_pitch;
    max_pitch_p_ = ::mu::engraving::max_pitch;
    use_drumset_ = false;
    drumset_ = 0;
    single_note_dynamics_ = true;
}

Instrument::Instrument(const Instrument &i) {
    id_ = i.id_;
    sound_id_ = i.sound_id_;
    instrument_label_ = i.instrument_label_;
    track_name_ = i.track_name_;
    min_pitch_a_ = i.min_pitch_a_;
    max_pitch_a_ = i.max_pitch_a_;
    min_pitch_p_ = i.min_pitch_p_;
    max_pitch_p_ = i.max_pitch_p_;
    transpose_ = i.transpose_;
    music_xml_id_ = i.music_xml_id_;
    string_data_ = i.string_data_;
    drumset_ = 0;
    set_drumset(i.drumset_);
    use_drumset_ = i.use_drumset_;
    string_data_ = i.string_data_;
    midi_actions_ = i.midi_actions_;
    articulation_ = i.articulation_;
    single_note_dynamics_ = i.single_note_dynamics_;
    channel_.reserve(i.channel_.size());
    for (InstrChannel *c : i.channel_) {
        channel_.push_back(new InstrChannel(*c));
    }
    clef_type_ = i.clef_type_;
    trait_ = i.trait_;
    glissando_style_ = i.glissando_style_;
}

void Instrument::operator=(const Instrument &i) {
    muse::delete_all(channel_);
    channel_.clear();
    delete drumset_;

    id_ = i.id_;
    sound_id_ = i.sound_id_;
    instrument_label_ = i.instrument_label_;
    track_name_ = i.track_name_;
    min_pitch_a_ = i.min_pitch_a_;
    max_pitch_a_ = i.max_pitch_a_;
    min_pitch_p_ = i.min_pitch_p_;
    max_pitch_p_ = i.max_pitch_p_;
    transpose_ = i.transpose_;
    music_xml_id_ = i.music_xml_id_;
    string_data_ = i.string_data_;
    drumset_ = 0;
    set_drumset(i.drumset_);
    use_drumset_ = i.use_drumset_;
    string_data_ = i.string_data_;
    midi_actions_ = i.midi_actions_;
    articulation_ = i.articulation_;
    single_note_dynamics_ = i.single_note_dynamics_;
    channel_.reserve(i.channel_.size());
    for (InstrChannel *c : i.channel_) {
        channel_.push_back(new InstrChannel(*c));
    }
    clef_type_ = i.clef_type_;
    trait_ = i.trait_;
    glissando_style_ = i.glissando_style_;
}

//---------------------------------------------------------
//   ~Instrument
//---------------------------------------------------------

Instrument::~Instrument() {
    muse::delete_all(channel_);
    delete drumset_;
    drumset_ = nullptr;
}

String Instrument::recognize_music_xml_id() const {
    // Return a MusicXML "Sound ID", which is essentially an instrument ID.
    // See https://github.com/w3c/musicxml/blob/gh-pages/schema/sounds.xml
    static const String default_music_xml_id(u"keyboard.piano");
    static const String default_music_xml_percussion_id(u"drum.group"); // our General MIDI Percussion

    std::vector<String> name_list;
    name_list.reserve(3);

    name_list.push_back(track_name_);
    name_list.push_back(instrument_label_.long_name());
    name_list.push_back(instrument_label_.short_name());

    const InstrumentTemplate *tmpl_by_name = mu::engraving::search_template_for_instr_name_list(name_list, use_drumset_);

    if (tmpl_by_name && !tmpl_by_name->music_xml_id.is_empty()) {
        return tmpl_by_name->music_xml_id;
    }

    const InstrChannel *channel = this->channel(0);

    if (!channel) {
        return default_music_xml_id;
    }

    const InstrumentTemplate *tmpl_midi_program =
        mu::engraving::search_template_for_midi_program(channel->bank(), channel->program(), use_drumset_);

    if (tmpl_midi_program && !tmpl_midi_program->music_xml_id.is_empty()) {
        return tmpl_midi_program->music_xml_id;
    }

    return use_drumset_ ? default_music_xml_percussion_id : default_music_xml_id;
}

String Instrument::recognize_id() const {
    // When reading a score created with pre-3.6, MuseScore's instrument ID
    // isn't saved in the score file, so we must try to guess the ID based on
    // the MusicXML ID, which is saved. However, MusicXML IDs are not unique,
    // so we must also consider other data to find the best match, preferring
    // more generic instruments when there's a tie.

    // In these cases, the best and/or most generic match is already known,
    // and we don't want to risk returning something else.
    if (music_xml_id_ == u"drum.group") {
        return u"percussion-synthesizer"; // General MIDI Percussion (most generic kit)
    } else if (music_xml_id_ == u"drum.group.set") {
        return u"drumset"; // Large Drum Kit (most generic drum kit)
    } else if (music_xml_id_.starts_with(u"mdl.")) {
        // Use fixed mapping for MDL1 instruments to ensure we get the
        // marching versions (e.g. "marching-snare" and not "snare-drum").
        // See https://github.com/musescore/mdl/blob/master/resources/instruments/mdl_1_3_0.xml
        if (music_xml_id_ == u"mdl.drum.snare-drum") {
            return u"marching-snare";
        } else if (music_xml_id_ == u"mdl.drum.tenor-drum") {
            return u"marching-tenor-drums";
        } else if (music_xml_id_ == u"mdl.drum.bass-drum") {
            return u"marching-bass-drums";
        } else if (music_xml_id_ == u"mdl.metal.cymbal.crash") {
            return u"marching-cymbals";
        } else if (music_xml_id_ == u"mdl.drum.group.set") {
            return u"drumset";
        }
    }

    // Several instruments have MusicXML ID "strings.group". Let's use the
    // value of controller 32 of the "arco" channel to distinguish them.
    const String arco = String(u"arco");
    const bool group_hack = music_xml_id_ == String(u"strings.group");
    const int idxref = channel_idx(arco);
    const int val32ref = (idxref < 0) ? -1 : channel(idxref)->bank();

    // For other instruments, consider how closely the instrument data
    // matches each of our templates. Use the ID that gives the best match.
    String fallback;             // ID that gave the best match so far
    int best_match_strength = 0; // higher when ID is a better match

    for (const InstrumentGroup *g : instrument_groups) {
        for (const InstrumentTemplate *it : g->instrument_templates) {
            if (it->music_xml_id != music_xml_id_) {
                continue;
            }
            if (group_hack) {
                if (fallback.is_empty()) {
                    // Instrument "Strings" doesn't have bank defined so
                    // if no "strings.group" instrument with requested bank
                    // is found, assume "Strings".
                    fallback = it->id;
                }
                for (const InstrChannel &chan : it->channel) {
                    if ((chan.name() == arco) && (chan.bank() == val32ref)) {
                        return it->id;
                    }
                }
            } else {
                int match_strength = 0 + ((min_pitch_p() == it->min_pitch_p) ? 1 : 0) + ((min_pitch_a() == it->min_pitch_a) ? 1 : 0) +
                                     ((max_pitch_a() == it->max_pitch_a) ? 1 : 0) + ((max_pitch_p() == it->max_pitch_p) ? 1 : 0);
                const int perfect_match_strength = 4;
                assert(match_strength <= perfect_match_strength);
                if (fallback.is_empty() || match_strength > best_match_strength) {
                    // Set a fallback ID or update it because we've found a better one.
                    fallback = it->id;
                    best_match_strength = match_strength;
                    if (best_match_strength == perfect_match_strength) {
                        break; // stop looking for matches
                    }
                } else if ((match_strength == best_match_strength) && (it->id.size() < fallback.size())) {
                    // Update fallback ID because we've found a shorter one that is equally good.
                    // Shorter IDs tend to correspond to more generic instruments (e.g. "piano"
                    // vs. "grand-piano") so it's better to use a shorter one if unsure.
                    fallback = it->id;
                }
            }
        }
    }

    if (!fallback.is_empty()) {
        return fallback;
    }

    return use_drumset_ ? u"percussion-synthesizer" // General MIDI Percussion (most generic kit)
                          :
                          u"piano";
}

int Instrument::recognize_midi_program() const {
    const InstrumentTemplate *tp = search_template(id_);

    if (tp && !tp->channel.empty() && tp->channel[0].program() >= 0) {
        return tp->channel[0].program();
    }

    return 0;
}

//---------------------------------------------------------
//   action
//---------------------------------------------------------

NamedEventList *Instrument::midi_action(const String &s, int channel_idx) const {
    // first look in channel list

    for (const NamedEventList &a : channel_[channel_idx]->midi_actions) {
        if (s == a.name) {
            return const_cast<NamedEventList *>(&a);
        }
    }

    for (const NamedEventList &a : midi_actions_) {
        if (s == a.name) {
            return const_cast<NamedEventList *>(&a);
        }
    }
    return 0;
}

//---------------------------------------------------------
//   Channel
//---------------------------------------------------------

InstrChannel::InstrChannel() {
    for (int i = 0; i < int(A::INIT_COUNT); ++i) {
        init_.push_back(MidiCoreEvent());
    }
    synti_ = u"Fluid"; // default synthesizer
    channel_ = -1;
    program_ = -1;
    bank_ = 0;
    volume_ = ::mu::engraving::InstrChannel::default_volume;
    pan_ = 64; // actually 63.5 for center
    chorus_ = 0;
    reverb_ = 0;
    color_ = ::mu::engraving::InstrChannel::default_color;
}

//---------------------------------------------------------
//   initList
//---------------------------------------------------------

std::vector<MidiCoreEvent> &InstrChannel::init_list() const {
    if (must_update_init_) {
        update_init_list();
        must_update_init_ = false;
    }
    return init_;
}

//---------------------------------------------------------
//   setVolume
//---------------------------------------------------------

void InstrChannel::set_volume(char value) {
    if (volume_ != value) {
        volume_ = value;
        fire_property_changed(Prop::VOLUME);
    }
    must_update_init_ = true;
}

//---------------------------------------------------------
//   setPan
//---------------------------------------------------------

void InstrChannel::set_pan(char value) {
    if (pan_ != value) {
        pan_ = value;
        fire_property_changed(Prop::PAN);
    }
    must_update_init_ = true;
}

//---------------------------------------------------------
//   setChorus
//---------------------------------------------------------

void InstrChannel::set_chorus(char value) {
    if (chorus_ != value) {
        chorus_ = value;
        fire_property_changed(Prop::CHORUS);
    }
    must_update_init_ = true;
}

//---------------------------------------------------------
//   setReverb
//---------------------------------------------------------

void InstrChannel::set_reverb(char value) {
    if (reverb_ != value) {
        reverb_ = value;
        fire_property_changed(Prop::REVERB);
    }
    must_update_init_ = true;
}

//---------------------------------------------------------
//   setName
//---------------------------------------------------------

void InstrChannel::set_name(const String &value) {
    if (name_ != value) {
        name_ = value;
        fire_property_changed(Prop::NAME);
    }
}

//---------------------------------------------------------
//   setSynti
//---------------------------------------------------------

void InstrChannel::set_synti(const String &value) {
    if (synti_ != value) {
        synti_ = value;
        fire_property_changed(Prop::SYNTI);
    }
}

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void InstrChannel::set_color(int value) {
    if (color_ != value) {
        color_ = value;
        fire_property_changed(Prop::COLOR);
    }
}

//---------------------------------------------------------
//   setProgram
//---------------------------------------------------------

void InstrChannel::set_program(int value) {
    if (program_ != value) {
        program_ = value;
        fire_property_changed(Prop::PROGRAM);
    }
    must_update_init_ = true;
}

//---------------------------------------------------------
//   setBank
//---------------------------------------------------------

void InstrChannel::set_bank(int value) {
    if (bank_ != value) {
        bank_ = value;
        fire_property_changed(Prop::BANK);
    }
    must_update_init_ = true;
}

//---------------------------------------------------------
//   setChannel
//---------------------------------------------------------

void InstrChannel::set_channel(int value) {
    if (channel_ != value) {
        channel_ = value;
        fire_property_changed(Prop::CHANNEL);
    }
}

//---------------------------------------------------------
//   setUserBankController
//---------------------------------------------------------

void InstrChannel::set_user_bank_controller(bool val) {
    if (user_bank_controller_ != val) {
        user_bank_controller_ = val;
        fire_property_changed(Prop::USER_BANK_CONTROL);
    }
}

//---------------------------------------------------------
//   switchExpressive
//    Switches channel from non-expressive to expressive patch or vice versa
//    This works only with MuseScore General soundfont
//---------------------------------------------------------

void InstrChannel::switch_expressive([[maybe_unused]] Synthesizer *synth, [[maybe_unused]] bool expressive,
                                     [[maybe_unused]] bool force /* = false */) {
    //! TODO Needs poting to MU4
    MUSE_NOT_IMPLEMENTED;

    //    if ((_userBankController && !force) || !synth) {
    //        return;
    //    }

    //    // Don't try to switch if we already have done so
    //    if (expressive == _switchedToExpressive) {
    //        return;
    //    }

    //    // Check that we're actually changing the MuseScore General soundfont
    //    const auto fontsInfo = synth->soundFontsInfo();
    //    if (fontsInfo.empty()) {
    //        return;
    //    }
    //    const auto& info = fontsInfo.front();
    //    if (!info.fontName.contains("MS Basic", Qt::CaseInsensitive)) {
    //        LOGD().nospace() << "Soundfont '" << info.fontName << "' is not MuseScore General, cannot update expressive";
    //        return;
    //    }

    //    // Work out where the new expressive patch will be
    //    // All expressive instruments are +1 bank higher than the
    //    // normal counterparts, except on bank 0, where they are placed on bank 17
    //    // and on bank 8, which uses bank 18 instead.
    //    int searchBankNum;
    //    int newBankNum;
    //    if (expressive) {
    //        int relativeBank = bank() % 129;
    //        if (relativeBank == 0) {
    //            newBankNum = 17;
    //        } else if (relativeBank == 8) {
    //            newBankNum = 18;
    //        } else {
    //            newBankNum = relativeBank + 1;
    //        }
    //        _switchedToExpressive = true;
    //    } else {
    //        int relativeBank = bank() % 129;
    //        if (relativeBank == 17) {
    //            newBankNum = 0;
    //        } else if (relativeBank == 18) {
    //            newBankNum = 8;
    //        } else {
    //            newBankNum = relativeBank - 1;
    //        }
    //        _switchedToExpressive = false;
    //    }

    //    // Floor bank num to multiple of 129 and add new num to get bank num of new patch
    //    searchBankNum = (bank() / 129) * 129 + newBankNum;
    //    const auto& pl = synth->getPatchInfo();
    //    for (const MidiPatch* p : pl) {
    //        if (p->synti == "Fluid") {
    //            if (searchBankNum == p->bank && program() == p->prog) {
    //                setBank(p->bank);
    //                return;
    //            }
    //        }
    //    }
}

//---------------------------------------------------------
//   updateInitList
//---------------------------------------------------------

void InstrChannel::update_init_list() const {
    MidiCoreEvent e;
    if (program_ != -1) {
        e.set_type(ME_CONTROLLER);
        e.set_data_a(CTRL_PROGRAM);
        e.set_data_b(program_);
        init_[int(A::PROGRAM)] = e;
    }

    e.set_data(ME_CONTROLLER, CTRL_HBANK, (bank_ >> 7) & 0x7f);
    init_[int(A::HBANK)] = e;

    e.set_data(ME_CONTROLLER, CTRL_LBANK, bank_ & 0x7f);
    init_[int(A::LBANK)] = e;

    e.set_data(ME_CONTROLLER, CTRL_VOLUME, volume());
    init_[int(A::VOLUME)] = e;

    e.set_data(ME_CONTROLLER, CTRL_PANPOT, pan());
    init_[int(A::PAN)] = e;

    e.set_data(ME_CONTROLLER, CTRL_CHORUS_SEND, chorus());
    init_[int(A::CHORUS)] = e;

    e.set_data(ME_CONTROLLER, CTRL_REVERB_SEND, reverb());
    init_[int(A::REVERB)] = e;
}

//---------------------------------------------------------
//   addListener
//---------------------------------------------------------

void InstrChannel::add_listener(ChannelListener *l) { notifier_.add_listener(l); }

//---------------------------------------------------------
//   removeListener
//---------------------------------------------------------

void InstrChannel::remove_listener(ChannelListener *l) { notifier_.remove_listener(l); }

//---------------------------------------------------------
//   PartChannelSettingsLink
//---------------------------------------------------------

PartChannelSettingsLink::PartChannelSettingsLink(InstrChannel *main, InstrChannel *bound, bool excerpt)
    : main_(main), bound_(bound), excerpt_(excerpt) {
    // Maybe it would be good to assign common properties if the link
    // is constructed in non-excerpt mode. But it is not currently
    // necessary as playback channels are currently recreated on each
    // MIDI remapping.

    main->add_listener(this);
}

//---------------------------------------------------------
//   PartChannelSettingsLink
//---------------------------------------------------------

PartChannelSettingsLink::PartChannelSettingsLink(PartChannelSettingsLink &&other)
    : ChannelListener(), // swap() will set the notifier instead
      main_(nullptr), bound_(nullptr), excerpt_(false) {
    swap(*this, other);
}

//---------------------------------------------------------
//   PartChannelSettingsLink::operator=
//---------------------------------------------------------

PartChannelSettingsLink &PartChannelSettingsLink::operator=(PartChannelSettingsLink &&other) {
    if (this != &other) {
        swap(*this, other);
    }
    return *this;
}

//---------------------------------------------------------
//   swap
//---------------------------------------------------------

void swap(PartChannelSettingsLink &l1, PartChannelSettingsLink &l2) {
    mu::engraving::swap(static_cast<ChannelListener &>(l1), static_cast<ChannelListener &>(l2));
    using std::swap;
    swap(l1.main_, l2.main_);
    swap(l1.bound_, l2.bound_);
    swap(l1.excerpt_, l2.excerpt_);
}

//---------------------------------------------------------
//   PartChannelSettingsLink::applyProperty
//---------------------------------------------------------

void PartChannelSettingsLink::apply_property(InstrChannel::Prop p, const InstrChannel *from, InstrChannel *to) {
    switch (p) {
        case InstrChannel::Prop::VOLUME: to->set_volume(from->volume()); break;
        case InstrChannel::Prop::PAN: to->set_pan(from->pan()); break;
        case InstrChannel::Prop::CHORUS: to->set_chorus(from->chorus()); break;
        case InstrChannel::Prop::REVERB: to->set_reverb(from->reverb()); break;
        case InstrChannel::Prop::NAME: to->set_name(from->name()); break;
        case InstrChannel::Prop::PROGRAM: to->set_program(from->program()); break;
        case InstrChannel::Prop::BANK: to->set_bank(from->bank()); break;
        case InstrChannel::Prop::COLOR: to->set_color(from->color()); break;
        case InstrChannel::Prop::SYNTI: to->set_synti(from->synti()); break;
        case InstrChannel::Prop::CHANNEL: to->set_channel(from->channel()); break;
        case InstrChannel::Prop::USER_BANK_CONTROL: to->set_user_bank_controller(from->user_bank_controller()); break;
    }
}

//---------------------------------------------------------
//   PartChannelSettingsLink::propertyChanged
//---------------------------------------------------------

void PartChannelSettingsLink::property_changed(InstrChannel::Prop p) { apply_property(p, main_, bound_); }

//---------------------------------------------------------
//   channelIdx
//---------------------------------------------------------

int Instrument::channel_idx(const String &s) const {
    int idx = 0;
    for (const InstrChannel *a : channel_) {
        if (a->name().is_empty() && s == InstrChannel::default_name) {
            return idx;
        }
        if (s == a->name()) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

//---------------------------------------------------------
//   getVelocityMultiplier
//---------------------------------------------------------

double Instrument::get_velocity_multiplier(const String &name) const {
    for (const MidiArticulation &a : articulation_) {
        if (a.name == name) {
            return double(a.velocity) / 100;
        }
    }
    return 1;
}

//---------------------------------------------------------
//   updateGateTime
//---------------------------------------------------------

void Instrument::update_gate_time(int *gate_time, const String &name) const {
    for (const MidiArticulation &a : articulation_) {
        if (a.name == name) {
            // Imagine ["staccato", "accent"] articulations
            // accent will override the gate time,
            // so we have to take the minimum value from all articulations
            *gate_time = std::min(*gate_time, a.gate_time);
            break;
        }
    }
}

//---------------------------------------------------------
//   switchExpressive
//---------------------------------------------------------

void Instrument::switch_expressive(MasterScore *score, Synthesizer *synth, bool expressive, bool force /* = false */) {
    // Only switch to expressive where necessary
    if (!synth || (expressive && !single_note_dynamics())) {
        return;
    }

    for (InstrChannel *c : channel()) {
        c->switch_expressive(synth, expressive, force);
        if (score->playback_channel(c)) {
            score->playback_channel(c)->switch_expressive(synth, expressive, force);
        }
    }
}

bool Instrument::is_vocal_instrument() const {
    String instrument_family = family();
    return instrument_family == u"voices" || instrument_family == u"voice-groups";
}

bool Instrument::is_normally_multi_stave_instrument() const {
    String instrument_family = family();
    return instrument_family == u"keyboards" || instrument_family == u"organs" || instrument_family == u"keyboard-percussion" ||
           instrument_family == u"harps" || instrument_family == u"accordions";
}

GlissandoStyle Instrument::glissando_style() const { return glissando_style_; }

void Instrument::set_glissando_style(GlissandoStyle style) { glissando_style_ = style; }

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool Instrument::operator==(const Instrument &i) const {
    bool equal = i.instrument_label_ == instrument_label_;
    equal &= i.track_name_ == track_name_;
    equal &= i.id_ == id_;
    equal &= i.sound_id_ == sound_id_;
    equal &= i.music_xml_id_ == music_xml_id_;

    equal &= i.min_pitch_a_ == min_pitch_a_;
    equal &= i.max_pitch_a_ == max_pitch_a_;
    equal &= i.min_pitch_p_ == min_pitch_p_;
    equal &= i.max_pitch_p_ == max_pitch_p_;
    equal &= i.transpose_.diatonic == transpose_.diatonic;
    equal &= i.transpose_.chromatic == transpose_.chromatic;

    equal &= i.use_drumset_ == use_drumset_;
    if (i.drumset_ && drumset_) {
        equal &= *i.drumset_ == *drumset_;
    } else {
        equal &= i.drumset_ == drumset_;
    }
    equal &= *i.string_data() == *string_data();

    equal &= i.midi_actions_ == midi_actions_;
    equal &= i.articulation_ == articulation_;

    if (i.channel_.size() == channel_.size()) {
        for (size_t cur = 0; cur < channel_.size(); cur++) {
            if (*i.channel_[cur] != *channel_[cur]) {
                return false;
            }
        }
    } else {
        return false;
    }

    equal &= i.clef_type_ == clef_type_;

    equal &= i.single_note_dynamics_ == single_note_dynamics_;

    return equal;
}

bool Instrument::operator!=(const Instrument &i) const { return !(*this == i); }

//---------------------------------------------------------
//   isDifferentInstrument
///   Checks if the passed instrument is a different instrument.
///   Does not compare channels.
//---------------------------------------------------------

bool Instrument::is_different_instrument(const Instrument &i) const { return this->operator!=(i); }

String Instrument::family() const {
    static const String no_family = u"-";

    if (family_cache_.empty()) {
        auto search = search_template_index_for_id(id_);

        if (search.instr_template) {
            family_cache_ = search.instr_template->family_id();
        } else {
            family_cache_ = no_family;
        }
    }

    return family_cache_ == no_family ? String() : family_cache_;
}

String Instrument::group() const {
    static const String no_group = u"-";

    if (group_cache_.empty()) {
        auto search = search_template_index_for_id(id_);

        if (search.instr_template) {
            group_cache_ = search.instr_template->group_id;
        } else {
            group_cache_ = no_group;
        }
    }

    return group_cache_ == no_group ? String() : group_cache_;
}

//---------------------------------------------------------
//   setUseDrumset
//---------------------------------------------------------

void Instrument::set_use_drumset(bool val) {
    use_drumset_ = val;
    if (val && !drumset_) {
        drumset_ = new Drumset(*sm_drumset);
    }
}

//---------------------------------------------------------
//   setDrumset
//---------------------------------------------------------

void Instrument::set_drumset(const Drumset *ds) {
    delete drumset_;
    if (ds) {
        use_drumset_ = true;
        drumset_ = new Drumset(*ds);
    } else {
        use_drumset_ = false;
        drumset_ = 0;
    }
}

size_t Instrument::cleff_type_count() const { return clef_type_.size(); }

//---------------------------------------------------------
//   clefType
//---------------------------------------------------------

ClefTypeList Instrument::clef_type(size_t staff_idx) const {
    if (staff_idx >= clef_type_.size()) {
        if (clef_type_.empty()) {
            return ClefTypeList();
        }
        return clef_type_[0];
    }
    return clef_type_[staff_idx];
}

//---------------------------------------------------------
//   setClefType
//---------------------------------------------------------

void Instrument::set_clef_type(size_t staff_idx, const ClefTypeList &c) {
    while (clef_type_.size() <= staff_idx) {
        clef_type_.push_back(ClefTypeList());
    }
    clef_type_[staff_idx] = c;
}

//---------------------------------------------------------
//   minPitchP
//---------------------------------------------------------

int Instrument::min_pitch_p() const { return min_pitch_p_; }

//---------------------------------------------------------
//   maxPitchP
//---------------------------------------------------------

int Instrument::max_pitch_p() const { return max_pitch_p_; }

//---------------------------------------------------------
//   minPitchA
//---------------------------------------------------------

int Instrument::min_pitch_a() const { return min_pitch_a_; }

//---------------------------------------------------------
//   maxPitchA
//---------------------------------------------------------

int Instrument::max_pitch_a() const { return max_pitch_a_; }

//---------------------------------------------------------
//   musicXmlId
//---------------------------------------------------------

String Instrument::music_xml_id() const { return music_xml_id_; }

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

const Instrument *InstrumentList::instrument(int tick) const {
    if (empty()) {
        return &default_instrument;
    }
    auto i = upper_bound(tick);
    if (i == begin()) {
        return &default_instrument;
    }
    --i;
    return i->second;
}

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

Instrument *InstrumentList::instrument(int tick) {
    if (empty()) {
        return &default_instrument;
    }
    auto i = upper_bound(tick);
    if (i == begin()) {
        return &default_instrument;
    }
    --i;
    return i->second;
}

//---------------------------------------------------------
//   setInstrument
//---------------------------------------------------------

void InstrumentList::set_instrument(Instrument *instr, int tick) {
    if (!insert({tick, instr}).second) {
        (*this)[tick] = instr;
    }
}

bool InstrumentList::contains(const String &instrument_id) const {
    for (const auto &pair : *this) {
        if (pair.second->id() == instrument_id) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   trackName
//---------------------------------------------------------

String Instrument::track_name() const { return track_name_; }

void Instrument::set_track_name(const String &s) { track_name_ = s; }

String Instrument::name_as_xml_text() const { return instrument_label_.long_name(); }

String Instrument::name_as_plain_text() const { return TextBase::un_escape(instrument_label_.long_name()); }

String Instrument::abbreviature_as_xml_text() const { return instrument_label_.short_name(); }

String Instrument::abbreviature_as_plain_text() const { return TextBase::un_escape(instrument_label_.short_name()); }

Instrument Instrument::from_template(const InstrumentTemplate *templ) {
    Instrument instrument(templ->id);
    instrument.set_sound_id(templ->sound_id);
    instrument.set_amateur_pitch_range(templ->min_pitch_a, templ->max_pitch_a);
    instrument.set_professional_pitch_range(templ->min_pitch_p, templ->max_pitch_p);
    instrument.set_instrument_name(templ->instrument_name);

    instrument.set_track_name(templ->track_name);
    instrument.set_transpose(templ->transpose);
    instrument.set_music_xml_id(templ->music_xml_id);
    instrument.use_drumset_ = templ->use_drumset;

    if (templ->use_drumset) {
        instrument.set_drumset(templ->drumset ? templ->drumset : sm_drumset);
    }

    for (staff_idx_t i = 0; i < templ->staff_count; ++i) {
        instrument.set_clef_type(i, templ->clef_types[i]);
    }

    instrument.set_midi_actions(templ->midi_actions);
    instrument.set_articulation(templ->midi_articulations);
    instrument.channel_.clear();

    for (const InstrChannel &c : templ->channel) {
        instrument.channel_.push_back(new InstrChannel(c));
    }

    if (instrument.channel_.empty()) {
        InstrChannel *fallback_channel = new InstrChannel;
        fallback_channel->set_name(String::from_utf8(InstrChannel::default_name));
        fallback_channel->set_program(instrument.recognize_midi_program());
        instrument.channel_.push_back(fallback_channel);
    }

    instrument.set_string_data(templ->string_data);
    instrument.set_single_note_dynamics(templ->single_note_dynamics);
    instrument.set_trait(templ->trait);
    instrument.glissando_style_ = templ->glissando_style;

    return instrument;
}

Trait Instrument::trait() const { return trait_; }

void Instrument::set_trait(const Trait &trait) { trait_ = trait; }

bool Instrument::is_primary() const { return is_primary_; }

void Instrument::set_is_primary(bool is_primary) { is_primary_ = is_primary; }

void Instrument::update_instrument_id() {
    if (music_xml_id_.is_empty()) {
        music_xml_id_ = recognize_music_xml_id();
    }

    if (id_.is_empty()) {
        id_ = recognize_id();
    }
}

//---------------------------------------------------------
//   Instrument::playbackChannel
//---------------------------------------------------------

const InstrChannel *Instrument::playback_channel(int idx, const MasterScore *score) const { return score->playback_channel(channel(idx)); }

//---------------------------------------------------------
//   Instrument::playbackChannel
//---------------------------------------------------------

InstrChannel *Instrument::playback_channel(int idx, MasterScore *score) { return score->playback_channel(channel(idx)); }

//---------------------------------------------------------
//   getSingleNoteDynamicsFromTemplate
//---------------------------------------------------------

bool Instrument::get_single_note_dynamics_from_template() const {
    String template_name = track_name().to_lower().replace(u" ", u"-").replace(u"♭", u"b");
    const InstrumentTemplate *tp = search_template(template_name);
    if (tp) {
        return tp->single_note_dynamics;
    }
    return true;
}

//---------------------------------------------------------
//   setSingleNoteDynamicsFromTemplate
//---------------------------------------------------------

void Instrument::set_single_note_dynamics_from_template() { set_single_note_dynamics(get_single_note_dynamics_from_template()); }
} // namespace mu::engraving
