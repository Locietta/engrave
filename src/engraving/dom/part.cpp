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

#include "containers.h"

#include "iengravingconfiguration.h"

#include "style/style.h"
#include "rw/xmlwriter.h"

#include "chordrest.h"
#include "factory.h"
#include "fret.h"
#include "harppedaldiagram.h"
#include "instrtemplate.h"
#include "instrchange.h"
#include "linkedobjects.h"
#include "masterscore.h"
#include "measure.h"
#include "score.h"
#include "sharedpart.h"
#include "staff.h"
#include "system.h"
#include "stringtunings.h"
#include "textbase.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
const Fraction Part::MAIN_INSTRUMENT_TICK = Fraction(-1, 1);
//---------------------------------------------------------
//   Part
//---------------------------------------------------------

Part::Part(Score *s, ElementType type) : EngravingObject(type, s) {
    show_ = true;
    soloist_ = false;
    instruments_.setInstrument(new Instrument, -1); // default instrument
    preferSharpFlat_ = PreferSharpFlat::AUTO;
}

Part::~Part() {
    // A part is not an item, so nothing else unregisters its id. The register lives
    // on the master score, which outlives the part unless the whole document is
    // going away; a leftover entry would hand the id to whatever is allocated at the
    // same address next.
    MasterScore *master = score() ? score()->masterScore() : nullptr;
    if (master && Score::isAlive(master)) {
        master->eidRegister()->onItemDestroyed(this);
    }
}

//---------------------------------------------------------
//   initFromInstrTemplate
//---------------------------------------------------------

void Part::initFromInstrTemplate(const InstrumentTemplate *t) { setInstrument(Instrument::fromTemplate(t)); }

const ID &Part::id() const { return id_; }

void Part::setId(const ID &id) { id_ = id; }

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

String Part::familyId() const {
    if (instruments_.empty()) {
        return String();
    }

    InstrumentIndex ii = searchTemplateIndexForId(instrumentId());
    return ii.instrTemplate && ii.instrTemplate->family ? ii.instrTemplate->family->id : String();
}

//---------------------------------------------------------
//   Part::masterPart
//---------------------------------------------------------

const Part *Part::masterPart() const {
    if (score()->isMaster()) {
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
        if (le->isStaff() && toStaff(le)->score()->isMaster()) {
            if (Part *p = toStaff(le)->part()) {
                return p;
            }
        }
    }
    return this;
}

//---------------------------------------------------------
//   Part::masterPart
//---------------------------------------------------------

Part *Part::masterPart() { return const_cast<Part *>(const_cast<const Part *>(this)->masterPart()); }

const Part *Part::originPart(track_idx_t, const Fraction &) const { return this; }

Part *Part::originPart(track_idx_t track, const Fraction &tick) {
    return const_cast<Part *>(const_cast<const Part *>(this)->originPart(track, tick));
}

size_t Part::nstaves() const { return staves_.size(); }

size_t Part::visibleStavesCount() const {
    size_t result = 0;
    for (const Staff *staff : staves_) {
        if (staff->show()) {
            ++result;
        }
    }

    return result;
}

const std::vector<Staff *> &Part::staves() const { return staves_; }

std::set<staff_idx_t> Part::staveIdxList() const {
    std::set<staff_idx_t> result;

    for (const Staff *stave : staves_) {
        if (!stave) {
            continue;
        }

        result.insert(stave->idx());
    }

    return result;
}

void Part::appendStaff(Staff *staff) { staves_.push_back(staff); }

void Part::clearStaves() { staves_.clear(); }

//---------------------------------------------------------
//   setStaves
//---------------------------------------------------------

void Part::setStaves(int n) {
    int ns = static_cast<int>(staves_.size());
    if (n < ns) {
        LOGD("Part::setStaves(): remove staves not implemented!");
        return;
    }

    int staffIdx = static_cast<int>(score()->staffIdx(this)) + ns;
    for (int i = ns; i < n; ++i) {
        Staff *staff = Factory::createStaff(this);
        score()->insertStaff(staff, i);

        for (Measure *m = score()->firstMeasure(); m; m = m->nextMeasure()) {
            m->insertStaff(staff, staffIdx);
            if (m->hasMMRest()) {
                m->mmRest()->insertStaff(staff, staffIdx);
            }
        }
        ++staffIdx;
    }
}

//---------------------------------------------------------
//   insertStaff
//---------------------------------------------------------

void Part::insertStaff(Staff *staff, staff_idx_t idx) {
    if (idx >= staves_.size()) {
        idx = staves_.size();
    }
    staves_.insert(staves_.begin() + idx, staff);
    staff->setPart(this);
}

//---------------------------------------------------------
//   removeStaff
//---------------------------------------------------------

void Part::removeStaff(Staff *staff) {
    if (!muse::remove(staves_, staff)) {
        LOGD("Part::removeStaff: not found %p", staff);
        return;
    }
}

bool Part::show() const {
    if (score()->configuration()->debuggingOptions().showOriginAndCombinedStaves) {
        return show_;
    }

    bool sharedPartEnabled = sharedPart() && sharedPart()->enabled();
    return show_ && !sharedPartEnabled;
}

//---------------------------------------------------------
//   setMidiProgram
//---------------------------------------------------------

void Part::setMidiProgram(int program, int bank) {
    InstrChannel *c = instrument()->channel(0);
    c->setProgram(program);
    c->setBank(bank);
}

//---------------------------------------------------------
//   midiProgram
//---------------------------------------------------------

int Part::midiProgram() const { return instrument()->playbackChannel(0, masterScore())->program(); }

//---------------------------------------------------------
//   midiChannel
//---------------------------------------------------------

int Part::midiChannel() const { return masterScore()->midiChannel(instrument()->channel(0)->channel()); }

//---------------------------------------------------------
//   midiPort
//---------------------------------------------------------

int Part::midiPort() const { return masterScore()->midiPort(instrument()->channel(0)->channel()); }

//---------------------------------------------------------
//   setMidiChannel
//   Called from importmusicxml, importMidi and importGtp*.
//   Specify tick to set MIDI channel to an InstrumentChange element.
//   Usage:
//   setMidiChannel(channel)       to set channel
//   setMidiChannel(-1, port)      to set port
//   setMidiChannel(channel, port) to set both
//---------------------------------------------------------

void Part::setMidiChannel(int ch, int port, const Fraction &tick) {
    InstrChannel *channel = instrument(tick)->channel(0);
    if (channel->channel() == -1) {
        masterScore()->addMidiMapping(channel, this, port, ch);
    } else {
        masterScore()->updateMidiMapping(channel, this, port, ch);
    }
}

//---------------------------------------------------------
//   setInstrument
//---------------------------------------------------------

void Part::setInstrument(Instrument *i, Fraction tick) { instruments_.setInstrument(i, tick.ticks()); }

void Part::setInstrument(Instrument *i, int tick) { instruments_.setInstrument(i, tick); }

void Part::setInstrument(const Instrument &&i, Fraction tick) { instruments_.setInstrument(new Instrument(i), tick.ticks()); }

void Part::setInstrument(const Instrument &i, Fraction tick) { instruments_.setInstrument(new Instrument(i), tick.ticks()); }

void Part::setInstruments(const InstrumentList &instruments) {
    instruments_.clear();

    for (auto it = instruments.begin(); it != instruments.end(); ++it) {
        instruments_.setInstrument(it->second, it->first);
    }
}

//---------------------------------------------------------
//   removeInstrument
//---------------------------------------------------------

void Part::removeInstrument(Instrument *instr, const Fraction &tick) {
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

void Part::removeNonPrimaryInstruments() {
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

const Instrument *Part::instrumentById(const String &id) const {
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

const StringData *Part::stringData(const Fraction &tick, staff_idx_t staffIdx) const {
    if (!score()) {
        return nullptr;
    }

    const Instrument *instrument = this->instrument(tick);
    if (!instrument) {
        return nullptr;
    }

    bool reflectTranspositionInLinkedTab = true;

    const Staff *staff = staffIdx != muse::nidx ? score()->staff(staffIdx) : nullptr;
    if (staff && staff->isTabStaff(tick)) {
        if (const Staff *primaryStaff = staff->primaryStaff()) {
            reflectTranspositionInLinkedTab = primaryStaff->reflectTranspositionInLinkedTab();
        }
    }

    StringTunings *stringTunings = nullptr;

    if (reflectTranspositionInLinkedTab) {
        auto it = muse::findLessOrEqual(stringTunings_, tick.ticks());
        if (it != stringTunings_.end()) {
            stringTunings = it->second;
        }
    }

    if (stringTunings) {
        //!NOTE: if there is string tunings element between current instrument and current tick,
        //! then return string data from string tunings element
        const Instrument *stringTuningsInstrument = this->instrument(stringTunings->tick());
        if (instrument == stringTuningsInstrument) {
            return stringTunings->stringData();
        }
    }

    return instrument->stringData();
}

void Part::addStringTunings(StringTunings *stringTunings) {
    DO_ASSERT(!isSharedPart());
    stringTunings_[stringTunings->segment()->tick().ticks()] = stringTunings;
}

void Part::removeStringTunings(StringTunings *stringTunings) {
    int tick = stringTunings->segment()->tick().ticks();
    auto it = stringTunings_.find(tick);

    if (it != stringTunings_.end() && it->second == stringTunings) {
        stringTunings_.erase(it);
    }
}

StringTunings *Part::nextStringTuning(const Fraction &tick) const {
    auto i = stringTunings_.upper_bound(tick.ticks());
    return (i == stringTunings_.end()) ? nullptr : i->second;
}

//---------------------------------------------------------
//   instrumentId
//---------------------------------------------------------

String Part::instrumentId(const Fraction &tick) const { return instrument(tick)->id(); }

//---------------------------------------------------------
//   longName
//---------------------------------------------------------

String Part::longName(const Fraction &tick) const { return instrument(tick)->longName(); }

//---------------------------------------------------------
//   instrumentName
//---------------------------------------------------------

String Part::instrumentName(const Fraction &tick) const { return instrument(tick)->trackName(); }

//---------------------------------------------------------
//   shortName
//---------------------------------------------------------

String Part::shortName(const Fraction &tick) const { return instrument(tick)->shortName(); }

//---------------------------------------------------------
//   setLongName
//---------------------------------------------------------

void Part::setLongName(const String &s, const Fraction &tick) { instrument(tick)->setLongName(s); }

//---------------------------------------------------------
//   setShortName
//---------------------------------------------------------

void Part::setShortName(const String &s, const Fraction &tick) { instrument(tick)->setShortName(s); }

//---------------------------------------------------------
//   setLongNameAll
//---------------------------------------------------------

void Part::setLongNameAll(const String &s) {
    for (auto instrument : instruments_) {
        instrument.second->setLongName(s);
    }
}

//---------------------------------------------------------
//   setShortNameAll
//---------------------------------------------------------

void Part::setShortNameAll(const String &s) {
    for (auto instrument : instruments_) {
        instrument.second->setShortName(s);
    }
}

int Part::number(const Fraction &tick) const { return instrument(tick)->number(); }

void Part::setNumber(int v, const Fraction &tick) { instrument(tick)->setNumber(v); }

String Part::transposition(const Fraction &tick) const { return instrument(tick)->transposition(); }

void Part::setTransposition(const String &s, const Fraction &tick) { instrument(tick)->setTransposition(s); }

//---------------------------------------------------------
//   setPlainLongName
//---------------------------------------------------------

void Part::setPlainLongName(const String &s) { setLongName(XmlWriter::xmlString(s)); }

//---------------------------------------------------------
//   setPlainShortName
//---------------------------------------------------------

void Part::setPlainShortName(const String &s) { setShortName(XmlWriter::xmlString(s)); }

//---------------------------------------------------------
//   setPlainLongNameAll
//---------------------------------------------------------

void Part::setPlainLongNameAll(const String &s) { setLongNameAll(XmlWriter::xmlString(s)); }

//---------------------------------------------------------
//   setPlainShortNameAll
//---------------------------------------------------------

void Part::setPlainShortNameAll(const String &s) { setShortNameAll(XmlWriter::xmlString(s)); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Part::getProperty(Pid id) const {
    switch (id) {
        case Pid::VISIBLE: return PropertyValue(show_);
        case Pid::HIDE_WHEN_EMPTY: return PropertyValue(hideWhenEmpty_);
        case Pid::HIDE_STAVES_WHEN_INDIVIDUALLY_EMPTY: return PropertyValue(hideStavesWhenIndividuallyEmpty_);
        case Pid::USE_DRUMSET: return instrument()->useDrumset();
        case Pid::PREFER_SHARP_FLAT: return int(preferSharpFlat());
        default: return PropertyValue();
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Part::setProperty(Pid id, const PropertyValue &property) {
    switch (id) {
        case Pid::VISIBLE: setShow(property.toBool()); break;
        case Pid::HIDE_WHEN_EMPTY: setHideWhenEmpty(property.value<AutoOnOff>()); break;
        case Pid::HIDE_STAVES_WHEN_INDIVIDUALLY_EMPTY: setHideStavesWhenIndividuallyEmpty(property.toBool()); break;
        case Pid::USE_DRUMSET: instrument()->setUseDrumset(property.toBool()); break;
        case Pid::PREFER_SHARP_FLAT: setPreferSharpFlat(PreferSharpFlat(property.toInt())); break;
        default: LOGD("Part::setProperty: unknown id %d", int(id)); break;
    }
    score()->setLayoutAll();
    return true;
}

TrackRange Part::trackRange() const {
    IF_ASSERT_FAILED(!staves_.empty()) { return {}; }

    const track_idx_t startTrack = staves_.front()->idx() * VOICES;
    return {startTrack, startTrack + staves_.size() * VOICES};
}

InstrumentTrackIdList Part::instrumentTrackIdList() const {
    InstrumentTrackIdList result;
    std::set<String> seen;

    for (const auto &pair : instruments_) {
        String instrId = pair.second->id();
        if (seen.insert(instrId).second) {
            result.push_back({id_, instrId});
        }
    }

    return result;
}

InstrumentTrackIdSet Part::instrumentTrackIdSet() const {
    InstrumentTrackIdSet result;

    for (const auto &pair : instruments_) {
        result.insert({id_, pair.second->id()});
    }

    return result;
}

//---------------------------------------------------------
//   insertTime
//---------------------------------------------------------

void Part::insertTime(const Fraction &tick, const Fraction &len) {
    if (len.isZero()) {
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
        harpDiagrams_.erase(harpDiagrams_.lower_bound(tick.ticks()), harpDiagrams_.lower_bound((tick - len).ticks()));
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
    for (auto h = harpDiagrams_.lower_bound(tick.ticks()); h != harpDiagrams_.end();) {
        HarpPedalDiagram *diagram = h->second;
        int t = h->first;
        harpDiagrams_.erase(h++);
        hd2[t + len.ticks()] = diagram;
    }
    harpDiagrams_.insert(hd2.begin(), hd2.end());
}

//---------------------------------------------------------
//   addHarpDiagram
//---------------------------------------------------------

void Part::addHarpDiagram(HarpPedalDiagram *harpDiagram) {
    DO_ASSERT(!isSharedPart());
    harpDiagrams_[harpDiagram->segment()->tick().ticks()] = harpDiagram;
}

//---------------------------------------------------------
//   removeHarpDiagram
//---------------------------------------------------------

void Part::removeHarpDiagram(HarpPedalDiagram *harpDiagram) {
    int tick = harpDiagram->segment()->tick().ticks();
    auto it = harpDiagrams_.find(tick);

    if (it != harpDiagrams_.end() && it->second == harpDiagram) {
        harpDiagrams_.erase(it);
    }
}

//---------------------------------------------------------
//   clearHarpDiagrams
//---------------------------------------------------------

void Part::clearHarpDiagrams() { harpDiagrams_.clear(); }

//---------------------------------------------------------
//   currentHarpDiagram
//---------------------------------------------------------

HarpPedalDiagram *Part::currentHarpDiagram(const Fraction &tick) const {
    auto i = harpDiagrams_.upper_bound(tick.ticks());
    if (i != harpDiagrams_.begin()) {
        --i;
    }
    if (i == harpDiagrams_.end()) {
        return nullptr;
    } else if (tick < Fraction::fromTicks(i->first)) {
        return nullptr;
    }
    return i->second;
}

//---------------------------------------------------------
//   nextHarpDiagram
//---------------------------------------------------------

HarpPedalDiagram *Part::nextHarpDiagram(const Fraction &tick) const {
    auto i = harpDiagrams_.upper_bound(tick.ticks());
    return (i == harpDiagrams_.end()) ? nullptr : i->second;
}

//---------------------------------------------------------
//   prevHarpDiagram
//---------------------------------------------------------

HarpPedalDiagram *Part::prevHarpDiagram(const Fraction &tick) const {
    auto i = harpDiagrams_.lower_bound(tick.ticks());
    if (i == harpDiagrams_.begin()) {
        return nullptr;
    }
    i--;
    return i->second;
}

//---------------------------------------------------------
//   currentHarpDiagramTick
//---------------------------------------------------------

Fraction Part::currentHarpDiagramTick(const Fraction &tick) const {
    if (harpDiagrams_.empty()) {
        return Fraction(0, 1);
    }
    auto i = harpDiagrams_.upper_bound(tick.ticks());
    if (i == harpDiagrams_.begin()) {
        return Fraction(0, 1);
    }
    --i;
    return Fraction::fromTicks(i->first);
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

String Part::plainPartName(const Instrument *i) {
    const String richName = i->longName();
    String name;
    bool inTag = false;
    for (size_t idx = 0; idx < richName.size(); ++idx) {
        const Char c = richName.at(idx);
        if (c == u'<') {
            inTag = true;
        } else if (c == u'>') {
            inTag = false;
        } else if (!inTag) {
            name.append(c);
        }
    }
    name = TextBase::unEscape(name);

    if (name.empty()) {
        // Legacy scores store the part name in the instrument track name.
        name = i->trackName();
    }

    return name;
}

String Part::partName() const {
    const Instrument *i = instrument();
    String fullName = plainPartName(i);

    const String &transp = i->transposition();
    if (!transp.empty()) {
        //: For instrument transposition, e.g. Horn in F
        fullName += u" " + muse::mtrc("notation", "in") + u" " + transp;
    }

    int n = number();
    if (n != 0) {
        fullName += u" " + String::number(n);
    }

    return fullName;
}

bool Part::isVisible() const { return show_; }

//---------------------------------------------------------
//   lyricCount
//---------------------------------------------------------

int Part::lyricCount() const {
    if (!score()) {
        return 0;
    }

    if (!score()->firstMeasure()) {
        return 0;
    }

    size_t count = 0;
    SegmentType st = SegmentType::ChordRest;
    const TrackRange range = trackRange();
    for (Segment *seg = score()->firstMeasure()->first(st); seg; seg = seg->next1(st)) {
        for (track_idx_t i = range.startTrack; i < range.endTrack; ++i) {
            ChordRest *cr = toChordRest(seg->element(i));
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

int Part::harmonyCount() const {
    if (!score()) {
        return 0;
    }

    Measure *firstM = score()->firstMeasure();
    if (!firstM) {
        return 0;
    }

    SegmentType st = SegmentType::ChordRest;
    int count = 0;
    const TrackRange range = trackRange();
    for (const Segment *seg = firstM->first(st); seg; seg = seg->next1(st)) {
        for (const EngravingItem *e : seg->annotations()) {
            if ((e->isHarmony() || (e->isFretDiagram() && toFretDiagram(e)->harmony())) && e->track() >= range.startTrack &&
                e->track() < range.endTrack) {
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
void Part::updateHarmonyChannels(bool isDoOnInstrumentChanged, bool checkRemoval) {
    auto onInstrumentChanged = [this]() {
        masterScore()->rebuildMidiMapping();
        masterScore()->updateChannel();
        score()->setInstrumentsChanged(true);
        score()->setLayoutAll(); //do we need this?
    };

    // usage of harmony count is okay even if expensive since checking harmony channel will shortcircuit if existent
    // harmonyCount will only be called on loading of a score (where it will need to be scanned for harmony anyway)
    // or when the first harmony of a score is just added
    if (checkRemoval) {
        //may be a bit expensive since it gets called after every single delete or undo, but it should be okay for now
        //~OPTIM~
        if (harmonyCount() == 0) {
            Instrument *instr = instrument();
            int hChIdx = instr->channelIdx(String::fromUtf8(InstrChannel::HARMONY_NAME));
            if (hChIdx != -1) {
                InstrChannel *hChan = instr->channel(hChIdx);
                instr->removeChannel(hChan);
                delete hChan;
                if (isDoOnInstrumentChanged) {
                    onInstrumentChanged();
                }
                return;
            }
        }
    }

    if (!harmonyChannel() && harmonyCount() > 0) {
        Instrument *instr = instrument();
        InstrChannel *c = new InstrChannel(*instr->channel(0));
        // default to program 0, which is piano in General MIDI
        c->setProgram(0);
        if (c->bank() == 128) { // drumset?
            c->setBank(0);
        }
        c->setName(String::fromUtf8(InstrChannel::HARMONY_NAME));
        instr->appendChannel(c);
        onInstrumentChanged();
    }
}

//---------------------------------------------------------
//   harmonyChannel
//---------------------------------------------------------

const InstrChannel *Part::harmonyChannel() const {
    const Instrument *instr = instrument();
    if (!instr) {
        return nullptr;
    }

    int chanIdx = instr->channelIdx(String::fromUtf8(InstrChannel::HARMONY_NAME));
    if (chanIdx == -1) {
        return nullptr;
    }

    const InstrChannel *chan = instr->channel(chanIdx);
    assert(chan);
    return chan;
}

bool Part::hasChordSymbol() const { return harmonyChannel() != nullptr; }

//---------------------------------------------------------
//   hasPitchedStaff
//---------------------------------------------------------

bool Part::hasPitchedStaff() const {
    for (Staff *s : staves()) {
        if (s && s->isPitchedStaff(Fraction(0, 1))) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasTabStaff
//---------------------------------------------------------

bool Part::hasTabStaff() const {
    for (Staff *s : staves()) {
        if (s && s->isTabStaff(Fraction(0, 1))) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasDrumStaff
//---------------------------------------------------------

bool Part::hasDrumStaff() const {
    for (Staff *s : staves()) {
        if (s && s->isDrumStaff(Fraction(0, 1))) {
            return true;
        }
    }
    return false;
}
} // namespace mu::engraving
