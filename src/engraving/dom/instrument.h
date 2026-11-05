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

#include "global/containers.h"

#include "clef.h"
#include "interval.h"
#include "notifier.h"
#include "stafflabel.h"
#include "stringdata.h"

#include "../compat/midi/midicoreevent.h"

namespace mu::engraving {
class ChannelListener;
class Drumset;
class InstrumentTemplate;
class MasterScore;
class Part;
class StringData;
class Synthesizer;

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
    int velocity = 0; // velocity change: -100% - +100%
    int gateTime = 0; // gate time change: -100% - +100%

    MidiArticulation() {}

    MidiArticulation(const String &n, const String &d, int v, int g) : name(n), descr(d), velocity(v), gateTime(g) {}

    bool operator==(const MidiArticulation &i) const;
};

//---------------------------------------------------------
//   Channel
//---------------------------------------------------------

class InstrChannel {
public:
    static const char *DEFAULT_NAME;
    static const char *HARMONY_NAME;
    static const char *PALM_MUTE_NAME;
    static const int DEFAULT_COLOR = 0x3399ff;
    static constexpr char defaultVolume = 100;

    enum class A : char { HBANK, LBANK, PROGRAM, VOLUME, PAN, CHORUS, REVERB, INIT_COUNT };

    enum class Prop : char { VOLUME, PAN, CHORUS, REVERB, NAME, PROGRAM, BANK, COLOR, SYNTI, CHANNEL, USER_BANK_CONTROL };

    std::vector<MidiCoreEvent> &initList() const;

    String name() const { return name_; }

    void setName(const String &value);

    String synti() const { return synti_; }

    void setSynti(const String &value);

    int color() const { return color_; }

    void setColor(int value);

    char volume() const { return volume_; }

    void setVolume(char value);

    char pan() const { return pan_; }

    void setPan(char value);

    char chorus() const { return chorus_; }

    void setChorus(char value);

    char reverb() const { return reverb_; }

    void setReverb(char value);

    void addToInit(const MidiCoreEvent &e) { init_.push_back(e); }

    void setMustUpdateInit(bool arg) { mustUpdateInit_ = arg; }

    int program() const { return program_; }

    void setProgram(int value);

    int bank() const { return bank_; }

    void setBank(int value);

    int channel() const { return channel_; }

    void setChannel(int value);

    // If the bank controller is set by the user or not
    bool userBankController() const { return userBankController_; }

    void setUserBankController(bool val);

    bool isHarmonyChannel() const { return name_ == String::fromUtf8(InstrChannel::HARMONY_NAME); }

    std::vector<NamedEventList> midiActions;
    std::vector<MidiArticulation> articulation;

    InstrChannel();

    void updateInitList() const;

    bool operator==(const InstrChannel &c) const { return (name_ == c.name_) && (channel_ == c.channel_); }

    bool operator!=(const InstrChannel &c) const { return !(*this == c); }

    void addListener(ChannelListener *l);
    void removeListener(ChannelListener *l);

    void switchExpressive(Synthesizer *synth, bool expressive, bool force = false);

    void setNotifyAboutChangedEnabled(bool arg) { notifyAboutChangedEnabled_ = arg; }

private:
    void firePropertyChanged(InstrChannel::Prop prop) {
        if (notifyAboutChangedEnabled_) {
            notifier_.notify(prop);
        }
    }

    Notifier<InstrChannel::Prop> notifier_;
    bool notifyAboutChangedEnabled_ = true;

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
    bool userBankController_ = false; // if the user has changed the bank controller as opposed to switchExpressive
    //bool _switchedToExpressive = false;   // if the patch has been automatically switched to an expr variant

    mutable std::vector<MidiCoreEvent> init_;
    mutable bool mustUpdateInit_ = true;
};

//---------------------------------------------------------
//   ChannelListener
//---------------------------------------------------------

class ChannelListener : public Listener<InstrChannel::Prop> {
    OBJECT_ALLOCATOR(engraving, ChannelListener)
public:
    virtual void propertyChanged(InstrChannel::Prop property) = 0;

    void setNotifier(InstrChannel *ch) {
        Listener::setNotifier(nullptr);
        if (ch) {
            ch->addListener(this);
        }
    }

private:
    void receive(InstrChannel::Prop prop) override { propertyChanged(prop); }
};

//---------------------------------------------------------
//   PartChannelSettingsLink
//---------------------------------------------------------

class PartChannelSettingsLink final : private ChannelListener {
public:
    PartChannelSettingsLink() = default;
    PartChannelSettingsLink(InstrChannel *main, InstrChannel *bound, bool excerpt);
    PartChannelSettingsLink(const PartChannelSettingsLink &) = delete;
    PartChannelSettingsLink(PartChannelSettingsLink &&);
    PartChannelSettingsLink &operator=(const PartChannelSettingsLink &) = delete;
    PartChannelSettingsLink &operator=(PartChannelSettingsLink &&);

    ~PartChannelSettingsLink() {}

    friend void swap(PartChannelSettingsLink &, PartChannelSettingsLink &);

private:

    static void applyProperty(InstrChannel::Prop p, const InstrChannel *from, InstrChannel *to);
    void propertyChanged(InstrChannel::Prop p) override;

    InstrChannel *main_ = nullptr;
    InstrChannel *bound_ = nullptr;
    bool excerpt_ = false;
};

//---------------------------------------------------------
//   Trait
//---------------------------------------------------------

enum class TraitType : unsigned char { Unknown, Tuning, Transposition, Course };

struct Trait {
    String name;

    TraitType type = TraitType::Unknown;

    bool isDefault = false;
    bool isHiddenOnScore = false;

    bool isValid() const { return !name.isEmpty(); }
};

//---------------------------------------------------------
//   Instrument
//---------------------------------------------------------

class Instrument {
public:
    Instrument(String id = String());
    Instrument(const Instrument &);
    ~Instrument();

    NamedEventList *midiAction(const String &s, int channel) const;
    int channelIdx(const String &s) const;
    double getVelocityMultiplier(const String &name) const;
    void updateGateTime(int *gateTime, const String &name) const;

    String recognizeMusicXmlId() const;
    String recognizeId() const;
    int recognizeMidiProgram() const;

    void operator=(const Instrument &);
    bool operator==(const Instrument &) const;
    bool operator!=(const Instrument &) const;

    bool isDifferentInstrument(const Instrument &i) const;

    const String &id() const { return id_; }

    const String &soundId() const { return soundId_; }

    void setSoundId(const String &id) { soundId_ = id; }

    String family() const;
    String group() const;

    void setId(const String &id) { id_ = id; }

    void setMinPitchP(int v) { minPitchP_ = v; }

    void setMaxPitchP(int v) { maxPitchP_ = v; }

    void setMinPitchA(int v) { minPitchA_ = v; }

    void setMaxPitchA(int v) { maxPitchA_ = v; }

    Interval transpose() const { return transpose_; }

    void setTranspose(const Interval &v) { transpose_ = v; }

    void setMusicXmlId(const String &musicXmlId) { musicXmlId_ = musicXmlId; }

    void setDrumset(const Drumset *ds);

    const Drumset *drumset() const { return drumset_; }

    Drumset *drumset() { return drumset_; }

    bool useDrumset() const { return useDrumset_; }

    void setUseDrumset(bool val);

    void setAmateurPitchRange(int a, int b) {
        minPitchA_ = a;
        maxPitchA_ = b;
    }

    void setProfessionalPitchRange(int a, int b) {
        minPitchP_ = a;
        maxPitchP_ = b;
    }

    InstrChannel *channel(int idx) { return muse::value(channel_, idx); }

    const InstrChannel *channel(int idx) const { return muse::value(channel_, idx); }

    InstrChannel *playbackChannel(int idx, MasterScore *);
    const InstrChannel *playbackChannel(int idx, const MasterScore *) const;
    size_t cleffTypeCount() const;
    ClefTypeList clefType(size_t staffIdx) const;
    void setClefType(size_t staffIdx, const ClefTypeList &c);

    const std::vector<NamedEventList> &midiActions() const { return midiActions_; }

    void addMidiAction(const NamedEventList &l) { midiActions_.push_back(l); }

    const std::vector<MidiArticulation> &articulation() const { return articulation_; }

    void addMidiArticulation(const MidiArticulation &a) { articulation_.push_back(a); }

    const std::vector<InstrChannel *> &channel() const { return channel_; }

    void appendChannel(InstrChannel *c) { channel_.push_back(c); }

    void removeChannel(InstrChannel *c) { muse::remove(channel_, c); }

    void clearChannels() { channel_.clear(); }

    void setMidiActions(const std::vector<NamedEventList> &l) { midiActions_ = l; }

    void setArticulation(const std::vector<MidiArticulation> &l) { articulation_ = l; }

    const StringData *stringData() const { return &stringData_; }

    void setStringData(const StringData &d) { stringData_.set(d); }

    bool hasStrings() const { return stringData_.strings() > 0; }

    void setLongName(const String &f) { instrumentLabel_.setLongName(f); }

    void setShortName(const String &f) { instrumentLabel_.setShortName(f); }

    void setInstrumentName(const InstrumentLabel &n) { instrumentLabel_ = n; }

    const String &longName() const { return instrumentLabel_.longName(); }

    const String &shortName() const { return instrumentLabel_.shortName(); }

    const InstrumentLabel &instrumentLabel() const { return instrumentLabel_; }

    InstrumentLabel &instrumentLabel() { return instrumentLabel_; }

    int number() const { return instrumentLabel_.number(); }

    void setNumber(int v) { return instrumentLabel_.setNumber(v); }

    const String &transposition() const { return instrumentLabel_.transposition(); }

    void setTransposition(const String &s) { instrumentLabel_.setTransposition(s); }

    int minPitchP() const;
    int maxPitchP() const;
    int minPitchA() const;
    int maxPitchA() const;
    String musicXmlId() const;

    String trackName() const;
    void setTrackName(const String &s);
    String nameAsXmlText() const;
    String nameAsPlainText() const;
    String abbreviatureAsXmlText() const;
    String abbreviatureAsPlainText() const;
    static Instrument fromTemplate(const InstrumentTemplate *t);

    Trait trait() const;
    void setTrait(const Trait &trait);

    bool isPrimary() const;
    void setIsPrimary(bool isPrimary);

    void updateInstrumentId();

    bool singleNoteDynamics() const { return singleNoteDynamics_; }

    void setSingleNoteDynamics(bool val) { singleNoteDynamics_ = val; }

    void setSingleNoteDynamicsFromTemplate();
    bool getSingleNoteDynamicsFromTemplate() const;
    void switchExpressive(MasterScore *score, Synthesizer *synth, bool expressive, bool force = false);

    bool isVocalInstrument() const;
    bool isNormallyMultiStaveInstrument() const;

    GlissandoStyle glissandoStyle() const;
    void setGlissandoStyle(GlissandoStyle style);

private:

    InstrumentLabel instrumentLabel_;

    String trackName_;
    String id_;
    String soundId_;

    int minPitchA_ = 0;
    int maxPitchA_ = 0;
    int minPitchP_ = 0;
    int maxPitchP_ = 0;
    Interval transpose_;
    String musicXmlId_;

    bool useDrumset_ = false;
    Drumset *drumset_ = nullptr;
    StringData stringData_;

    std::vector<NamedEventList> midiActions_;
    std::vector<MidiArticulation> articulation_;
    std::vector<InstrChannel *> channel_; // at least one entry
    std::vector<ClefTypeList> clefType_;

    bool singleNoteDynamics_ = false;

    Trait trait_;
    bool isPrimary_ = false;

    mutable String familyCache_;
    mutable String groupCache_;

    GlissandoStyle glissandoStyle_ = GlissandoStyle::CHROMATIC;
};

//---------------------------------------------------------
//   InstrumentList
//---------------------------------------------------------

class InstrumentList : public std::map<const int, Instrument *> {
    OBJECT_ALLOCATOR(engraving, InstrumentList)

public:
    InstrumentList() {}

    const Instrument *instrument(int tick) const;
    Instrument *instrument(int tick);
    void setInstrument(Instrument *, int tick);
    bool contains(const String &instrumentId) const;

private:

    static Instrument defaultInstrument;
};
} // namespace mu::engraving
