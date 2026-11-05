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
#include "masterscore.h"
#include "stringdata.h"
#include "textbase.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//: Channel name for otherwise unnamed channels
const char *InstrChannel::DEFAULT_NAME = "normal";
const char *InstrChannel::HARMONY_NAME = "harmony";
const char *InstrChannel::PALM_MUTE_NAME = "palmmute";

Instrument InstrumentList::defaultInstrument;

//---------------------------------------------------------
//   operator
//---------------------------------------------------------

bool MidiArticulation::operator==(const MidiArticulation &i) const {
    return (i.name == name) && (i.velocity == velocity) && (i.gateTime == gateTime);
}

//---------------------------------------------------------
//   Instrument
//---------------------------------------------------------

Instrument::Instrument(String id) {
    id_ = id;
    InstrChannel *a = new InstrChannel;
    a->setName(String::fromUtf8(InstrChannel::DEFAULT_NAME));
    channel_.push_back(a);

    minPitchA_ = MIN_PITCH;
    maxPitchA_ = MAX_PITCH;
    minPitchP_ = MIN_PITCH;
    maxPitchP_ = MAX_PITCH;
    useDrumset_ = false;
    drumset_ = 0;
    singleNoteDynamics_ = true;
}

Instrument::Instrument(const Instrument &i) {
    id_ = i.id_;
    soundId_ = i.soundId_;
    instrumentLabel_ = i.instrumentLabel_;
    trackName_ = i.trackName_;
    minPitchA_ = i.minPitchA_;
    maxPitchA_ = i.maxPitchA_;
    minPitchP_ = i.minPitchP_;
    maxPitchP_ = i.maxPitchP_;
    transpose_ = i.transpose_;
    musicXmlId_ = i.musicXmlId_;
    stringData_ = i.stringData_;
    drumset_ = 0;
    setDrumset(i.drumset_);
    useDrumset_ = i.useDrumset_;
    stringData_ = i.stringData_;
    midiActions_ = i.midiActions_;
    articulation_ = i.articulation_;
    singleNoteDynamics_ = i.singleNoteDynamics_;
    channel_.reserve(i.channel_.size());
    for (InstrChannel *c : i.channel_) {
        channel_.push_back(new InstrChannel(*c));
    }
    clefType_ = i.clefType_;
    trait_ = i.trait_;
    glissandoStyle_ = i.glissandoStyle_;
}

void Instrument::operator=(const Instrument &i) {
    muse::DeleteAll(channel_);
    channel_.clear();
    delete drumset_;

    id_ = i.id_;
    soundId_ = i.soundId_;
    instrumentLabel_ = i.instrumentLabel_;
    trackName_ = i.trackName_;
    minPitchA_ = i.minPitchA_;
    maxPitchA_ = i.maxPitchA_;
    minPitchP_ = i.minPitchP_;
    maxPitchP_ = i.maxPitchP_;
    transpose_ = i.transpose_;
    musicXmlId_ = i.musicXmlId_;
    stringData_ = i.stringData_;
    drumset_ = 0;
    setDrumset(i.drumset_);
    useDrumset_ = i.useDrumset_;
    stringData_ = i.stringData_;
    midiActions_ = i.midiActions_;
    articulation_ = i.articulation_;
    singleNoteDynamics_ = i.singleNoteDynamics_;
    channel_.reserve(i.channel_.size());
    for (InstrChannel *c : i.channel_) {
        channel_.push_back(new InstrChannel(*c));
    }
    clefType_ = i.clefType_;
    trait_ = i.trait_;
    glissandoStyle_ = i.glissandoStyle_;
}

//---------------------------------------------------------
//   ~Instrument
//---------------------------------------------------------

Instrument::~Instrument() {
    muse::DeleteAll(channel_);
    delete drumset_;
    drumset_ = nullptr;
}

String Instrument::recognizeMusicXmlId() const {
    // Return a MusicXML "Sound ID", which is essentially an instrument ID.
    // See https://github.com/w3c/musicxml/blob/gh-pages/schema/sounds.xml
    static const String defaultMusicXmlId(u"keyboard.piano");
    static const String defaultMusicXmlPercussionId(u"drum.group"); // our General MIDI Percussion

    std::vector<String> nameList;
    nameList.reserve(3);

    nameList.push_back(trackName_);
    nameList.push_back(instrumentLabel_.longName());
    nameList.push_back(instrumentLabel_.shortName());

    const InstrumentTemplate *tmplByName = mu::engraving::searchTemplateForInstrNameList(nameList, useDrumset_);

    if (tmplByName && !tmplByName->musicXmlId.isEmpty()) {
        return tmplByName->musicXmlId;
    }

    const InstrChannel *channel = this->channel(0);

    if (!channel) {
        return defaultMusicXmlId;
    }

    const InstrumentTemplate *tmplMidiProgram =
        mu::engraving::searchTemplateForMidiProgram(channel->bank(), channel->program(), useDrumset_);

    if (tmplMidiProgram && !tmplMidiProgram->musicXmlId.isEmpty()) {
        return tmplMidiProgram->musicXmlId;
    }

    return useDrumset_ ? defaultMusicXmlPercussionId : defaultMusicXmlId;
}

String Instrument::recognizeId() const {
    // When reading a score created with pre-3.6, MuseScore's instrument ID
    // isn't saved in the score file, so we must try to guess the ID based on
    // the MusicXML ID, which is saved. However, MusicXML IDs are not unique,
    // so we must also consider other data to find the best match, preferring
    // more generic instruments when there's a tie.

    // In these cases, the best and/or most generic match is already known,
    // and we don't want to risk returning something else.
    if (musicXmlId_ == u"drum.group") {
        return u"percussion-synthesizer"; // General MIDI Percussion (most generic kit)
    } else if (musicXmlId_ == u"drum.group.set") {
        return u"drumset"; // Large Drum Kit (most generic drum kit)
    } else if (musicXmlId_.startsWith(u"mdl.")) {
        // Use fixed mapping for MDL1 instruments to ensure we get the
        // marching versions (e.g. "marching-snare" and not "snare-drum").
        // See https://github.com/musescore/mdl/blob/master/resources/instruments/mdl_1_3_0.xml
        if (musicXmlId_ == u"mdl.drum.snare-drum") {
            return u"marching-snare";
        } else if (musicXmlId_ == u"mdl.drum.tenor-drum") {
            return u"marching-tenor-drums";
        } else if (musicXmlId_ == u"mdl.drum.bass-drum") {
            return u"marching-bass-drums";
        } else if (musicXmlId_ == u"mdl.metal.cymbal.crash") {
            return u"marching-cymbals";
        } else if (musicXmlId_ == u"mdl.drum.group.set") {
            return u"drumset";
        }
    }

    // Several instruments have MusicXML ID "strings.group". Let's use the
    // value of controller 32 of the "arco" channel to distinguish them.
    const String arco = String(u"arco");
    const bool groupHack = musicXmlId_ == String(u"strings.group");
    const int idxref = channelIdx(arco);
    const int val32ref = (idxref < 0) ? -1 : channel(idxref)->bank();

    // For other instruments, consider how closely the instrument data
    // matches each of our templates. Use the ID that gives the best match.
    String fallback;           // ID that gave the best match so far
    int bestMatchStrength = 0; // higher when ID is a better match

    for (const InstrumentGroup *g : instrumentGroups) {
        for (const InstrumentTemplate *it : g->instrumentTemplates) {
            if (it->musicXmlId != musicXmlId_) {
                continue;
            }
            if (groupHack) {
                if (fallback.isEmpty()) {
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
                int matchStrength = 0 + ((minPitchP() == it->minPitchP) ? 1 : 0) + ((minPitchA() == it->minPitchA) ? 1 : 0) +
                                    ((maxPitchA() == it->maxPitchA) ? 1 : 0) + ((maxPitchP() == it->maxPitchP) ? 1 : 0);
                const int perfectMatchStrength = 4;
                assert(matchStrength <= perfectMatchStrength);
                if (fallback.isEmpty() || matchStrength > bestMatchStrength) {
                    // Set a fallback ID or update it because we've found a better one.
                    fallback = it->id;
                    bestMatchStrength = matchStrength;
                    if (bestMatchStrength == perfectMatchStrength) {
                        break; // stop looking for matches
                    }
                } else if ((matchStrength == bestMatchStrength) && (it->id.size() < fallback.size())) {
                    // Update fallback ID because we've found a shorter one that is equally good.
                    // Shorter IDs tend to correspond to more generic instruments (e.g. "piano"
                    // vs. "grand-piano") so it's better to use a shorter one if unsure.
                    fallback = it->id;
                }
            }
        }
    }

    if (!fallback.isEmpty()) {
        return fallback;
    }

    return useDrumset_ ? u"percussion-synthesizer" // General MIDI Percussion (most generic kit)
                         :
                         u"piano";
}

int Instrument::recognizeMidiProgram() const {
    const InstrumentTemplate *tp = searchTemplate(id_);

    if (tp && !tp->channel.empty() && tp->channel[0].program() >= 0) {
        return tp->channel[0].program();
    }

    return 0;
}

//---------------------------------------------------------
//   action
//---------------------------------------------------------

NamedEventList *Instrument::midiAction(const String &s, int channelIdx) const {
    // first look in channel list

    for (const NamedEventList &a : channel_[channelIdx]->midiActions) {
        if (s == a.name) {
            return const_cast<NamedEventList *>(&a);
        }
    }

    for (const NamedEventList &a : midiActions_) {
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
    volume_ = defaultVolume;
    pan_ = 64; // actually 63.5 for center
    chorus_ = 0;
    reverb_ = 0;
    color_ = DEFAULT_COLOR;
}

//---------------------------------------------------------
//   initList
//---------------------------------------------------------

std::vector<MidiCoreEvent> &InstrChannel::initList() const {
    if (mustUpdateInit_) {
        updateInitList();
        mustUpdateInit_ = false;
    }
    return init_;
}

//---------------------------------------------------------
//   setVolume
//---------------------------------------------------------

void InstrChannel::setVolume(char value) {
    if (volume_ != value) {
        volume_ = value;
        firePropertyChanged(Prop::VOLUME);
    }
    mustUpdateInit_ = true;
}

//---------------------------------------------------------
//   setPan
//---------------------------------------------------------

void InstrChannel::setPan(char value) {
    if (pan_ != value) {
        pan_ = value;
        firePropertyChanged(Prop::PAN);
    }
    mustUpdateInit_ = true;
}

//---------------------------------------------------------
//   setChorus
//---------------------------------------------------------

void InstrChannel::setChorus(char value) {
    if (chorus_ != value) {
        chorus_ = value;
        firePropertyChanged(Prop::CHORUS);
    }
    mustUpdateInit_ = true;
}

//---------------------------------------------------------
//   setReverb
//---------------------------------------------------------

void InstrChannel::setReverb(char value) {
    if (reverb_ != value) {
        reverb_ = value;
        firePropertyChanged(Prop::REVERB);
    }
    mustUpdateInit_ = true;
}

//---------------------------------------------------------
//   setName
//---------------------------------------------------------

void InstrChannel::setName(const String &value) {
    if (name_ != value) {
        name_ = value;
        firePropertyChanged(Prop::NAME);
    }
}

//---------------------------------------------------------
//   setSynti
//---------------------------------------------------------

void InstrChannel::setSynti(const String &value) {
    if (synti_ != value) {
        synti_ = value;
        firePropertyChanged(Prop::SYNTI);
    }
}

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void InstrChannel::setColor(int value) {
    if (color_ != value) {
        color_ = value;
        firePropertyChanged(Prop::COLOR);
    }
}

//---------------------------------------------------------
//   setProgram
//---------------------------------------------------------

void InstrChannel::setProgram(int value) {
    if (program_ != value) {
        program_ = value;
        firePropertyChanged(Prop::PROGRAM);
    }
    mustUpdateInit_ = true;
}

//---------------------------------------------------------
//   setBank
//---------------------------------------------------------

void InstrChannel::setBank(int value) {
    if (bank_ != value) {
        bank_ = value;
        firePropertyChanged(Prop::BANK);
    }
    mustUpdateInit_ = true;
}

//---------------------------------------------------------
//   setChannel
//---------------------------------------------------------

void InstrChannel::setChannel(int value) {
    if (channel_ != value) {
        channel_ = value;
        firePropertyChanged(Prop::CHANNEL);
    }
}

//---------------------------------------------------------
//   setUserBankController
//---------------------------------------------------------

void InstrChannel::setUserBankController(bool val) {
    if (userBankController_ != val) {
        userBankController_ = val;
        firePropertyChanged(Prop::USER_BANK_CONTROL);
    }
}

//---------------------------------------------------------
//   switchExpressive
//    Switches channel from non-expressive to expressive patch or vice versa
//    This works only with MuseScore General soundfont
//---------------------------------------------------------

void InstrChannel::switchExpressive(Synthesizer *synth, bool expressive, bool force /* = false */) {
    UNUSED(synth);
    UNUSED(expressive);
    UNUSED(force);
    //! TODO Needs poting to MU4
    NOT_IMPLEMENTED;

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

void InstrChannel::updateInitList() const {
    MidiCoreEvent e;
    if (program_ != -1) {
        e.setType(ME_CONTROLLER);
        e.setDataA(CTRL_PROGRAM);
        e.setDataB(program_);
        init_[int(A::PROGRAM)] = e;
    }

    e.setData(ME_CONTROLLER, CTRL_HBANK, (bank_ >> 7) & 0x7f);
    init_[int(A::HBANK)] = e;

    e.setData(ME_CONTROLLER, CTRL_LBANK, bank_ & 0x7f);
    init_[int(A::LBANK)] = e;

    e.setData(ME_CONTROLLER, CTRL_VOLUME, volume());
    init_[int(A::VOLUME)] = e;

    e.setData(ME_CONTROLLER, CTRL_PANPOT, pan());
    init_[int(A::PAN)] = e;

    e.setData(ME_CONTROLLER, CTRL_CHORUS_SEND, chorus());
    init_[int(A::CHORUS)] = e;

    e.setData(ME_CONTROLLER, CTRL_REVERB_SEND, reverb());
    init_[int(A::REVERB)] = e;
}

//---------------------------------------------------------
//   addListener
//---------------------------------------------------------

void InstrChannel::addListener(ChannelListener *l) { notifier_.addListener(l); }

//---------------------------------------------------------
//   removeListener
//---------------------------------------------------------

void InstrChannel::removeListener(ChannelListener *l) { notifier_.removeListener(l); }

//---------------------------------------------------------
//   PartChannelSettingsLink
//---------------------------------------------------------

PartChannelSettingsLink::PartChannelSettingsLink(InstrChannel *main, InstrChannel *bound, bool excerpt)
    : main_(main), bound_(bound), excerpt_(excerpt) {
    // Maybe it would be good to assign common properties if the link
    // is constructed in non-excerpt mode. But it is not currently
    // necessary as playback channels are currently recreated on each
    // MIDI remapping.

    main->addListener(this);
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

void PartChannelSettingsLink::applyProperty(InstrChannel::Prop p, const InstrChannel *from, InstrChannel *to) {
    switch (p) {
        case InstrChannel::Prop::VOLUME: to->setVolume(from->volume()); break;
        case InstrChannel::Prop::PAN: to->setPan(from->pan()); break;
        case InstrChannel::Prop::CHORUS: to->setChorus(from->chorus()); break;
        case InstrChannel::Prop::REVERB: to->setReverb(from->reverb()); break;
        case InstrChannel::Prop::NAME: to->setName(from->name()); break;
        case InstrChannel::Prop::PROGRAM: to->setProgram(from->program()); break;
        case InstrChannel::Prop::BANK: to->setBank(from->bank()); break;
        case InstrChannel::Prop::COLOR: to->setColor(from->color()); break;
        case InstrChannel::Prop::SYNTI: to->setSynti(from->synti()); break;
        case InstrChannel::Prop::CHANNEL: to->setChannel(from->channel()); break;
        case InstrChannel::Prop::USER_BANK_CONTROL: to->setUserBankController(from->userBankController()); break;
    }
}

//---------------------------------------------------------
//   PartChannelSettingsLink::propertyChanged
//---------------------------------------------------------

void PartChannelSettingsLink::propertyChanged(InstrChannel::Prop p) { applyProperty(p, main_, bound_); }

//---------------------------------------------------------
//   channelIdx
//---------------------------------------------------------

int Instrument::channelIdx(const String &s) const {
    int idx = 0;
    for (const InstrChannel *a : channel_) {
        if (a->name().isEmpty() && s == InstrChannel::DEFAULT_NAME) {
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

double Instrument::getVelocityMultiplier(const String &name) const {
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

void Instrument::updateGateTime(int *gateTime, const String &name) const {
    for (const MidiArticulation &a : articulation_) {
        if (a.name == name) {
            // Imagine ["staccato", "accent"] articulations
            // accent will override the gate time,
            // so we have to take the minimum value from all articulations
            *gateTime = std::min(*gateTime, a.gateTime);
            break;
        }
    }
}

//---------------------------------------------------------
//   switchExpressive
//---------------------------------------------------------

void Instrument::switchExpressive(MasterScore *score, Synthesizer *synth, bool expressive, bool force /* = false */) {
    // Only switch to expressive where necessary
    if (!synth || (expressive && !singleNoteDynamics())) {
        return;
    }

    for (InstrChannel *c : channel()) {
        c->switchExpressive(synth, expressive, force);
        if (score->playbackChannel(c)) {
            score->playbackChannel(c)->switchExpressive(synth, expressive, force);
        }
    }
}

bool Instrument::isVocalInstrument() const {
    String instrumentFamily = family();
    return instrumentFamily == u"voices" || instrumentFamily == u"voice-groups";
}

bool Instrument::isNormallyMultiStaveInstrument() const {
    String instrumentFamily = family();
    return instrumentFamily == u"keyboards" || instrumentFamily == u"organs" || instrumentFamily == u"keyboard-percussion" ||
           instrumentFamily == u"harps" || instrumentFamily == u"accordions";
}

GlissandoStyle Instrument::glissandoStyle() const { return glissandoStyle_; }

void Instrument::setGlissandoStyle(GlissandoStyle style) { glissandoStyle_ = style; }

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool Instrument::operator==(const Instrument &i) const {
    bool equal = i.instrumentLabel_ == instrumentLabel_;
    equal &= i.trackName_ == trackName_;
    equal &= i.id_ == id_;
    equal &= i.soundId_ == soundId_;
    equal &= i.musicXmlId_ == musicXmlId_;

    equal &= i.minPitchA_ == minPitchA_;
    equal &= i.maxPitchA_ == maxPitchA_;
    equal &= i.minPitchP_ == minPitchP_;
    equal &= i.maxPitchP_ == maxPitchP_;
    equal &= i.transpose_.diatonic == transpose_.diatonic;
    equal &= i.transpose_.chromatic == transpose_.chromatic;

    equal &= i.useDrumset_ == useDrumset_;
    if (i.drumset_ && drumset_) {
        equal &= *i.drumset_ == *drumset_;
    } else {
        equal &= i.drumset_ == drumset_;
    }
    equal &= *i.stringData() == *stringData();

    equal &= i.midiActions_ == midiActions_;
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

    equal &= i.clefType_ == clefType_;

    equal &= i.singleNoteDynamics_ == singleNoteDynamics_;

    return equal;
}

bool Instrument::operator!=(const Instrument &i) const { return !(*this == i); }

//---------------------------------------------------------
//   isDifferentInstrument
///   Checks if the passed instrument is a different instrument.
///   Does not compare channels.
//---------------------------------------------------------

bool Instrument::isDifferentInstrument(const Instrument &i) const { return this->operator!=(i); }

String Instrument::family() const {
    static const String NO_FAMILY = u"-";

    if (familyCache_.empty()) {
        auto search = searchTemplateIndexForId(id_);

        if (search.instrTemplate) {
            familyCache_ = search.instrTemplate->familyId();
        } else {
            familyCache_ = NO_FAMILY;
        }
    }

    return familyCache_ == NO_FAMILY ? String() : familyCache_;
}

String Instrument::group() const {
    static const String NO_GROUP = u"-";

    if (groupCache_.empty()) {
        auto search = searchTemplateIndexForId(id_);

        if (search.instrTemplate) {
            groupCache_ = search.instrTemplate->groupId;
        } else {
            groupCache_ = NO_GROUP;
        }
    }

    return groupCache_ == NO_GROUP ? String() : groupCache_;
}

//---------------------------------------------------------
//   setUseDrumset
//---------------------------------------------------------

void Instrument::setUseDrumset(bool val) {
    useDrumset_ = val;
    if (val && !drumset_) {
        drumset_ = new Drumset(*smDrumset);
    }
}

//---------------------------------------------------------
//   setDrumset
//---------------------------------------------------------

void Instrument::setDrumset(const Drumset *ds) {
    delete drumset_;
    if (ds) {
        useDrumset_ = true;
        drumset_ = new Drumset(*ds);
    } else {
        useDrumset_ = false;
        drumset_ = 0;
    }
}

size_t Instrument::cleffTypeCount() const { return clefType_.size(); }

//---------------------------------------------------------
//   clefType
//---------------------------------------------------------

ClefTypeList Instrument::clefType(size_t staffIdx) const {
    if (staffIdx >= clefType_.size()) {
        if (clefType_.empty()) {
            return ClefTypeList();
        }
        return clefType_[0];
    }
    return clefType_[staffIdx];
}

//---------------------------------------------------------
//   setClefType
//---------------------------------------------------------

void Instrument::setClefType(size_t staffIdx, const ClefTypeList &c) {
    while (clefType_.size() <= staffIdx) {
        clefType_.push_back(ClefTypeList());
    }
    clefType_[staffIdx] = c;
}

//---------------------------------------------------------
//   minPitchP
//---------------------------------------------------------

int Instrument::minPitchP() const { return minPitchP_; }

//---------------------------------------------------------
//   maxPitchP
//---------------------------------------------------------

int Instrument::maxPitchP() const { return maxPitchP_; }

//---------------------------------------------------------
//   minPitchA
//---------------------------------------------------------

int Instrument::minPitchA() const { return minPitchA_; }

//---------------------------------------------------------
//   maxPitchA
//---------------------------------------------------------

int Instrument::maxPitchA() const { return maxPitchA_; }

//---------------------------------------------------------
//   musicXmlId
//---------------------------------------------------------

String Instrument::musicXmlId() const { return musicXmlId_; }

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

const Instrument *InstrumentList::instrument(int tick) const {
    if (empty()) {
        return &defaultInstrument;
    }
    auto i = upper_bound(tick);
    if (i == begin()) {
        return &defaultInstrument;
    }
    --i;
    return i->second;
}

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

Instrument *InstrumentList::instrument(int tick) {
    if (empty()) {
        return &defaultInstrument;
    }
    auto i = upper_bound(tick);
    if (i == begin()) {
        return &defaultInstrument;
    }
    --i;
    return i->second;
}

//---------------------------------------------------------
//   setInstrument
//---------------------------------------------------------

void InstrumentList::setInstrument(Instrument *instr, int tick) {
    if (!insert({tick, instr}).second) {
        (*this)[tick] = instr;
    }
}

bool InstrumentList::contains(const String &instrumentId) const {
    for (const auto &pair : *this) {
        if (pair.second->id() == instrumentId) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   trackName
//---------------------------------------------------------

String Instrument::trackName() const { return trackName_; }

void Instrument::setTrackName(const String &s) { trackName_ = s; }

String Instrument::nameAsXmlText() const { return instrumentLabel_.longName(); }

String Instrument::nameAsPlainText() const { return TextBase::unEscape(instrumentLabel_.longName()); }

String Instrument::abbreviatureAsXmlText() const { return instrumentLabel_.shortName(); }

String Instrument::abbreviatureAsPlainText() const { return TextBase::unEscape(instrumentLabel_.shortName()); }

Instrument Instrument::fromTemplate(const InstrumentTemplate *templ) {
    Instrument instrument(templ->id);
    instrument.setSoundId(templ->soundId);
    instrument.setAmateurPitchRange(templ->minPitchA, templ->maxPitchA);
    instrument.setProfessionalPitchRange(templ->minPitchP, templ->maxPitchP);
    instrument.setInstrumentName(templ->instrumentName);

    instrument.setTrackName(templ->trackName);
    instrument.setTranspose(templ->transpose);
    instrument.setMusicXmlId(templ->musicXmlId);
    instrument.useDrumset_ = templ->useDrumset;

    if (templ->useDrumset) {
        instrument.setDrumset(templ->drumset ? templ->drumset : smDrumset);
    }

    for (staff_idx_t i = 0; i < templ->staffCount; ++i) {
        instrument.setClefType(i, templ->clefTypes[i]);
    }

    instrument.setMidiActions(templ->midiActions);
    instrument.setArticulation(templ->midiArticulations);
    instrument.channel_.clear();

    for (const InstrChannel &c : templ->channel) {
        instrument.channel_.push_back(new InstrChannel(c));
    }

    if (instrument.channel_.empty()) {
        InstrChannel *fallbackChannel = new InstrChannel;
        fallbackChannel->setName(String::fromUtf8(InstrChannel::DEFAULT_NAME));
        fallbackChannel->setProgram(instrument.recognizeMidiProgram());
        instrument.channel_.push_back(fallbackChannel);
    }

    instrument.setStringData(templ->stringData);
    instrument.setSingleNoteDynamics(templ->singleNoteDynamics);
    instrument.setTrait(templ->trait);
    instrument.glissandoStyle_ = templ->glissandoStyle;

    return instrument;
}

Trait Instrument::trait() const { return trait_; }

void Instrument::setTrait(const Trait &trait) { trait_ = trait; }

bool Instrument::isPrimary() const { return isPrimary_; }

void Instrument::setIsPrimary(bool isPrimary) { isPrimary_ = isPrimary; }

void Instrument::updateInstrumentId() {
    if (musicXmlId_.isEmpty()) {
        musicXmlId_ = recognizeMusicXmlId();
    }

    if (id_.isEmpty()) {
        id_ = recognizeId();
    }
}

//---------------------------------------------------------
//   Instrument::playbackChannel
//---------------------------------------------------------

const InstrChannel *Instrument::playbackChannel(int idx, const MasterScore *score) const { return score->playbackChannel(channel(idx)); }

//---------------------------------------------------------
//   Instrument::playbackChannel
//---------------------------------------------------------

InstrChannel *Instrument::playbackChannel(int idx, MasterScore *score) { return score->playbackChannel(channel(idx)); }

//---------------------------------------------------------
//   getSingleNoteDynamicsFromTemplate
//---------------------------------------------------------

bool Instrument::getSingleNoteDynamicsFromTemplate() const {
    String templateName = trackName().toLower().replace(u" ", u"-").replace(u"♭", u"b");
    const InstrumentTemplate *tp = searchTemplate(templateName);
    if (tp) {
        return tp->singleNoteDynamics;
    }
    return true;
}

//---------------------------------------------------------
//   setSingleNoteDynamicsFromTemplate
//---------------------------------------------------------

void Instrument::setSingleNoteDynamicsFromTemplate() { setSingleNoteDynamics(getSingleNoteDynamicsFromTemplate()); }
} // namespace mu::engraving
