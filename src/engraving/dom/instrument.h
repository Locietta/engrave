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

#include <vector>

#include "../compat/midi/midicoreevent.h"
#include "clef.h"
#include "global/containers.h"
#include "interval.h"
#include "notifier.h"
#include "stafflabel.h"
#include "stringdata.h"

namespace mu::engraving {
struct ChannelListener;
struct Drumset;
struct InstrumentTemplate;
struct MasterScore;
struct Part;
struct StringData;
struct Synthesizer;

//---------------------------------------------------------
//   NamedEventList
//---------------------------------------------------------

struct NamedEventList {
    String name;
    String descr;
    std::vector<MidiCoreEvent> events;

    bool operator==(const NamedEventList &i) const { return i.name == name && i.events == events; }
};

//---------------------------------------------------------
//   MidiArticulation
//---------------------------------------------------------

struct MidiArticulation {
    String name;
    String descr;
    int velocity = 0;  // velocity change: -100% - +100%
    int gate_time = 0; // gate time change: -100% - +100%

    MidiArticulation() {}

    MidiArticulation(const String &n, const String &d, int v, int g) : name(n), descr(d), velocity(v), gate_time(g) {}

    bool operator==(const MidiArticulation &i) const;
};

//---------------------------------------------------------
//   Channel
//---------------------------------------------------------

struct InstrChannel {
    static const char *default_name;
    static const char *harmony_name;
    static const char *palm_mute_name;
    static const int default_color = 0x3399ff;
    static constexpr char default_volume = 100;

    enum class A : char { HBANK, LBANK, PROGRAM, VOLUME, PAN, CHORUS, REVERB, INIT_COUNT };

    enum class Prop : char { VOLUME, PAN, CHORUS, REVERB, NAME, PROGRAM, BANK, COLOR, SYNTI, CHANNEL, USER_BANK_CONTROL };

    std::vector<MidiCoreEvent> &init_list() const;

    String name() const { return name_; }

    void set_name(const String &value);

    String synti() const { return synti_; }

    void set_synti(const String &value);

    int color() const { return color_; }

    void set_color(int value);

    char volume() const { return volume_; }

    void set_volume(char value);

    char pan() const { return pan_; }

    void set_pan(char value);

    char chorus() const { return chorus_; }

    void set_chorus(char value);

    char reverb() const { return reverb_; }

    void set_reverb(char value);

    void add_to_init(const MidiCoreEvent &e) { init_.push_back(e); }

    void set_must_update_init(bool arg) { must_update_init_ = arg; }

    int program() const { return program_; }

    void set_program(int value);

    int bank() const { return bank_; }

    void set_bank(int value);

    int channel() const { return channel_; }

    void set_channel(int value);

    // If the bank controller is set by the user or not
    bool user_bank_controller() const { return user_bank_controller_; }

    void set_user_bank_controller(bool val);

    bool is_harmony_channel() const { return name_ == String::from_utf8(InstrChannel::harmony_name); }

    std::vector<NamedEventList> midi_actions;
    std::vector<MidiArticulation> articulation;

    InstrChannel();

    void update_init_list() const;

    bool operator==(const InstrChannel &c) const { return (name_ == c.name_) && (channel_ == c.channel_); }

    bool operator!=(const InstrChannel &c) const { return !(*this == c); }

    void add_listener(ChannelListener *l);
    void remove_listener(ChannelListener *l);

    void switch_expressive(Synthesizer *synth, bool expressive, bool force = false);

    void set_notify_about_changed_enabled(bool arg) { notify_about_changed_enabled_ = arg; }

private:
    void fire_property_changed(InstrChannel::Prop prop) {
        if (notify_about_changed_enabled_) {
            notifier_.notify(prop);
        }
    }

    Notifier<InstrChannel::Prop> notifier_;
    bool notify_about_changed_enabled_ = true;

    // this are the indexes of controllers which are always present in
    // Channel init EventList (maybe zero)
    String name_;

    int color_ = 0; //rgb

    String synti_;

    char volume_ = 0;
    char pan_ = 0;

    char chorus_ = 0;
    char reverb_ = 0;

    int program_ = 0; // current values as shown in mixer
    int bank_ = 0;    // initialized from "init"
    int channel_ = 0; // mscore channel number, mapped to midi port/channel

    // MuseScore General-specific SND flags:
    //! TODO Needs porting to MU4
    bool user_bank_controller_ = false; // if the user has changed the bank controller as opposed to switchExpressive
    //bool _switchedToExpressive = false;   // if the patch has been automatically switched to an expr variant

    mutable std::vector<MidiCoreEvent> init_;
    mutable bool must_update_init_ = true;
};

//---------------------------------------------------------
//   ChannelListener
//---------------------------------------------------------

struct ChannelListener : Listener<InstrChannel::Prop> {
private:
    OBJECT_ALLOCATOR(engraving, ChannelListener)
public:
    virtual void property_changed(InstrChannel::Prop property) = 0;

    void set_notifier(InstrChannel *ch) {
        Listener::set_notifier(nullptr);
        if (ch) {
            ch->add_listener(this);
        }
    }

private:
    void receive(InstrChannel::Prop prop) override { property_changed(prop); }
};

//---------------------------------------------------------
//   PartChannelSettingsLink
//---------------------------------------------------------

struct PartChannelSettingsLink final : private ChannelListener {
    PartChannelSettingsLink() = default;
    PartChannelSettingsLink(InstrChannel *main, InstrChannel *bound, bool excerpt);
    PartChannelSettingsLink(const PartChannelSettingsLink &) = delete;
    PartChannelSettingsLink(PartChannelSettingsLink &&);
    PartChannelSettingsLink &operator=(const PartChannelSettingsLink &) = delete;
    PartChannelSettingsLink &operator=(PartChannelSettingsLink &&);

    ~PartChannelSettingsLink() {}

    friend void swap(PartChannelSettingsLink &, PartChannelSettingsLink &);

private:

    static void apply_property(InstrChannel::Prop p, const InstrChannel *from, InstrChannel *to);
    void property_changed(InstrChannel::Prop p) override;

    InstrChannel *main_ = nullptr;
    InstrChannel *bound_ = nullptr;
    bool excerpt_ = false;
};

//---------------------------------------------------------
//   Trait
//---------------------------------------------------------

enum class TraitType : unsigned char { UNKNOWN, TUNING, TRANSPOSITION, COURSE };

struct Trait {
    String name;

    TraitType type = TraitType::UNKNOWN;

    bool is_default = false;
    bool is_hidden_on_score = false;

    bool is_valid() const { return !name.is_empty(); }
};

//---------------------------------------------------------
//   Instrument
//---------------------------------------------------------

struct Instrument {
    Instrument(String id = String());
    Instrument(const Instrument &);
    ~Instrument();

    NamedEventList *midi_action(const String &s, int channel) const;
    int channel_idx(const String &s) const;
    double get_velocity_multiplier(const String &name) const;
    void update_gate_time(int *gate_time, const String &name) const;

    String recognize_music_xml_id() const;
    String recognize_id() const;
    int recognize_midi_program() const;

    void operator=(const Instrument &);
    bool operator==(const Instrument &) const;
    bool operator!=(const Instrument &) const;

    bool is_different_instrument(const Instrument &i) const;

    const String &id() const { return id_; }

    const String &sound_id() const { return sound_id_; }

    void set_sound_id(const String &id) { sound_id_ = id; }

    String family() const;
    String group() const;

    void set_id(const String &id) { id_ = id; }

    void set_min_pitch_p(int v) { min_pitch_p_ = v; }

    void set_max_pitch_p(int v) { max_pitch_p_ = v; }

    void set_min_pitch_a(int v) { min_pitch_a_ = v; }

    void set_max_pitch_a(int v) { max_pitch_a_ = v; }

    Interval transpose() const { return transpose_; }

    void set_transpose(const Interval &v) { transpose_ = v; }

    void set_music_xml_id(const String &music_xml_id) { music_xml_id_ = music_xml_id; }

    void set_drumset(const Drumset *ds);

    const Drumset *drumset() const { return drumset_; }

    Drumset *drumset() { return drumset_; }

    bool use_drumset() const { return use_drumset_; }

    void set_use_drumset(bool val);

    void set_amateur_pitch_range(int a, int b) {
        min_pitch_a_ = a;
        max_pitch_a_ = b;
    }

    void set_professional_pitch_range(int a, int b) {
        min_pitch_p_ = a;
        max_pitch_p_ = b;
    }

    InstrChannel *channel(int idx) { return muse::value(channel_, idx); }

    const InstrChannel *channel(int idx) const { return muse::value(channel_, idx); }

    InstrChannel *playback_channel(int idx, MasterScore *);
    const InstrChannel *playback_channel(int idx, const MasterScore *) const;
    size_t cleff_type_count() const;
    ClefTypeList clef_type(size_t staff_idx) const;
    void set_clef_type(size_t staff_idx, const ClefTypeList &c);

    const std::vector<NamedEventList> &midi_actions() const { return midi_actions_; }

    void add_midi_action(const NamedEventList &l) { midi_actions_.push_back(l); }

    const std::vector<MidiArticulation> &articulation() const { return articulation_; }

    void add_midi_articulation(const MidiArticulation &a) { articulation_.push_back(a); }

    const std::vector<InstrChannel *> &channel() const { return channel_; }

    void append_channel(InstrChannel *c) { channel_.push_back(c); }

    void remove_channel(InstrChannel *c) { muse::remove(channel_, c); }

    void clear_channels() { channel_.clear(); }

    void set_midi_actions(const std::vector<NamedEventList> &l) { midi_actions_ = l; }

    void set_articulation(const std::vector<MidiArticulation> &l) { articulation_ = l; }

    const StringData *string_data() const { return &string_data_; }

    void set_string_data(const StringData &d) { string_data_.set(d); }

    bool has_strings() const { return string_data_.strings() > 0; }

    void set_long_name(const String &f) { instrument_label_.set_long_name(f); }

    void set_short_name(const String &f) { instrument_label_.set_short_name(f); }

    void set_instrument_name(const InstrumentLabel &n) { instrument_label_ = n; }

    const String &long_name() const { return instrument_label_.long_name(); }

    const String &short_name() const { return instrument_label_.short_name(); }

    const InstrumentLabel &instrument_label() const { return instrument_label_; }

    InstrumentLabel &instrument_label() { return instrument_label_; }

    int number() const { return instrument_label_.number(); }

    void set_number(int v) { return instrument_label_.set_number(v); }

    const String &transposition() const { return instrument_label_.transposition(); }

    void set_transposition(const String &s) { instrument_label_.set_transposition(s); }

    int min_pitch_p() const;
    int max_pitch_p() const;
    int min_pitch_a() const;
    int max_pitch_a() const;
    String music_xml_id() const;

    String track_name() const;
    void set_track_name(const String &s);
    String name_as_xml_text() const;
    String name_as_plain_text() const;
    String abbreviature_as_xml_text() const;
    String abbreviature_as_plain_text() const;
    static Instrument from_template(const InstrumentTemplate *t);

    Trait trait() const;
    void set_trait(const Trait &trait);

    bool is_primary() const;
    void set_is_primary(bool is_primary);

    void update_instrument_id();

    bool single_note_dynamics() const { return single_note_dynamics_; }

    void set_single_note_dynamics(bool val) { single_note_dynamics_ = val; }

    void set_single_note_dynamics_from_template();
    bool get_single_note_dynamics_from_template() const;
    void switch_expressive(MasterScore *score, Synthesizer *synth, bool expressive, bool force = false);

    bool is_vocal_instrument() const;
    bool is_normally_multi_stave_instrument() const;

    GlissandoStyle glissando_style() const;
    void set_glissando_style(GlissandoStyle style);

private:

    InstrumentLabel instrument_label_;

    String track_name_;
    String id_;
    String sound_id_;

    int min_pitch_a_ = 0;
    int max_pitch_a_ = 0;
    int min_pitch_p_ = 0;
    int max_pitch_p_ = 0;
    Interval transpose_;
    String music_xml_id_;

    bool use_drumset_ = false;
    Drumset *drumset_ = nullptr;
    StringData string_data_;

    std::vector<NamedEventList> midi_actions_;
    std::vector<MidiArticulation> articulation_;
    std::vector<InstrChannel *> channel_; // at least one entry
    std::vector<ClefTypeList> clef_type_;

    bool single_note_dynamics_ = false;

    Trait trait_;
    bool is_primary_ = false;

    mutable String family_cache_;
    mutable String group_cache_;

    GlissandoStyle glissando_style_ = GlissandoStyle::CHROMATIC;
};

//---------------------------------------------------------
//   InstrumentList
//---------------------------------------------------------

struct InstrumentList : std::map<const int, Instrument *> {
private:
    OBJECT_ALLOCATOR(engraving, InstrumentList)

public:
    InstrumentList() {}

    const Instrument *instrument(int tick) const;
    Instrument *instrument(int tick);
    void set_instrument(Instrument *, int tick);
    bool contains(const String &instrument_id) const;

private:

    static Instrument default_instrument;
};
} // namespace mu::engraving
