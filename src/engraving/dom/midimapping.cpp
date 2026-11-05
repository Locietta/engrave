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

#include "excerpt.h"
#include "instrument.h"
#include "masterscore.h"
#include "part.h"
#include "score.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   rebuildMidiMapping
//---------------------------------------------------------

void MasterScore::rebuildMidiMapping() {
    Score *playbackScore = playbackScore_ ? playbackScore_ : this;
    setPlaybackScore(nullptr);

    removeDeletedMidiMapping();
    int maxport = updateMidiMapping();
    reorderMidiMapping();
    rebuildExcerptsMidiMapping();
    masterScore()->setMidiPortCount(maxport);

    setPlaybackScore(playbackScore);
}

//---------------------------------------------------------
//   checkMidiMapping
//   midi mapping is simple if all ports and channels
//   don't decrease and don't have 'holes' except drum tracks
//---------------------------------------------------------

void MasterScore::checkMidiMapping() {
    isSimpleMidiMapping_ = true;
    rebuildMidiMapping();

    std::vector<bool> drum;
    drum.reserve(midiMapping_.size());
    for (Part *part : parts()) {
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            for (size_t j = 0; j < instr->channel().size(); ++j) {
                drum.push_back(instr->useDrumset());
            }
        }
    }
    int lastChannel = -1; // port*16+channel
    int lastDrumPort = -1;
    size_t index = 0;
    for (const MidiMapping &m : midiMapping_) {
        if (index >= drum.size()) {
            break;
        }
        if (drum[index]) {
            lastDrumPort++;
            if (m.port() != lastDrumPort) {
                isSimpleMidiMapping_ = false;
                return;
            }
        } else {
            lastChannel++;
            if (lastChannel % 16 == 9) {
                lastChannel++;
            }
            int p = lastChannel / 16;
            int c = lastChannel % 16;
            if (m.port() != p || m.channel() != c) {
                isSimpleMidiMapping_ = false;
                return;
            }
        }
        index++;
    }
}

//---------------------------------------------------------
//   getNextFreeMidiMapping
//---------------------------------------------------------

int MasterScore::getNextFreeMidiMapping(std::set<int> &occupiedMidiChannels, unsigned int &searchMidiMappingFrom, int p, int ch) {
    if (ch != -1 && p != -1) {
        return p * 16 + ch;
    } else if (ch != -1 && p == -1) {
        for (int port = 0;; port++) {
            if (!muse::contains(occupiedMidiChannels, port * 16 + ch)) {
                occupiedMidiChannels.insert(port * 16 + ch);
                return port * 16 + ch;
            }
        }
    } else if (ch == -1 && p != -1) {
        for (int channel = 0; channel < 16; channel++) {
            if (channel != 9 && !muse::contains(occupiedMidiChannels, p * 16 + channel)) {
                occupiedMidiChannels.insert(p * 16 + channel);
                return p * 16 + channel;
            }
        }
    }

    for (;; searchMidiMappingFrom++) {
        if (searchMidiMappingFrom % 16 != 9 && !muse::contains(occupiedMidiChannels, int(searchMidiMappingFrom))) {
            occupiedMidiChannels.insert(searchMidiMappingFrom);
            return searchMidiMappingFrom;
        }
    }
}

//---------------------------------------------------------
//   getNextFreeDrumMidiMapping
//---------------------------------------------------------

int MasterScore::getNextFreeDrumMidiMapping(std::set<int> &occupiedMidiChannels) {
    for (int i = 0;; i++) {
        if (!muse::contains(occupiedMidiChannels, i * 16 + 9)) {
            occupiedMidiChannels.insert(i * 16 + 9);
            return i * 16 + 9;
        }
    }
}

//---------------------------------------------------------
//   rebuildExcerptsMidiMapping
//---------------------------------------------------------

void MasterScore::rebuildExcerptsMidiMapping() {
    for (Excerpt *ex : excerpts()) {
        for (Part *p : ex->excerptScore()->parts()) {
            const Part *masterPart = p->masterPart();
            if (!masterPart->score()->isMaster()) {
                LOGW() << "rebuildExcerptsMidiMapping: no part in master score is linked with " << p->partName();
                continue;
            }
            assert(p->instruments().size() == masterPart->instruments().size());
            for (const auto &[tick, iMaster] : masterPart->instruments()) {
                Instrument *iLocal = p->instrument(Fraction::fromTicks(tick));
                const size_t nchannels = iMaster->channel().size();
                if (iLocal->channel().size() != nchannels) {
                    // may happen, e.g., if user changes an instrument
                    (*iLocal) = (*iMaster);
                    continue;
                }
                for (size_t c = 0; c < nchannels; ++c) {
                    InstrChannel *cLocal = iLocal->channel(static_cast<int>(c));
                    const InstrChannel *cMaster = iMaster->channel(static_cast<int>(c));
                    cLocal->setChannel(cMaster->channel());
                }
            }
        }
    }
}

//---------------------------------------------------------
//   reorderMidiMapping
//   Set mappings in order you see in Add->Instruments
//---------------------------------------------------------

void MasterScore::reorderMidiMapping() {
    using std::swap;
    int sequenceNumber = 0;
    for (Part *part : parts()) {
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            for (InstrChannel *channel : instr->channel()) {
                if (midiMapping_[sequenceNumber].part() == part && midiMapping_[sequenceNumber].masterChannel_ == channel) {
                    sequenceNumber++;
                    continue;
                }

                const int shouldBe = channel->channel();
                swap(midiMapping_[sequenceNumber], midiMapping_[shouldBe]);
                midiMapping_[sequenceNumber].articulation()->setChannel(sequenceNumber);
                channel->setChannel(sequenceNumber);
                midiMapping_[shouldBe].articulation()->setChannel(shouldBe);

                sequenceNumber++;
            }
        }
    }
}

//---------------------------------------------------------
//   removeDeletedMidiMapping
//   Remove mappings to deleted instruments
//---------------------------------------------------------

void MasterScore::removeDeletedMidiMapping() {
    int removeOffset = 0;
    int mappingSize = int(midiMapping_.size());
    for (int index = 0; index < mappingSize; index++) {
        Part *part = midiMapping(index)->part();
        if (!muse::contains(parts(), part)) {
            removeOffset++;
            continue;
        }
        // Not all channels could exist
        bool channelExists = false;
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            channelExists = (midiMapping_[index].articulation()->channel() != -1 &&
                             muse::contains(instr->channel(), midiMapping_[index].masterChannel_) &&
                             !(midiMapping_[index].port() == -1 && midiMapping_[index].channel() == -1));
            if (channelExists) {
                break;
            }
        }
        if (!channelExists) {
            removeOffset++;
            continue;
        }
        // Let's do a left shift by 'removeOffset' items if necessary
        if (index != 0 && removeOffset != 0) {
            midiMapping_[index - removeOffset] = std::move(midiMapping_[index]);

            const int chanVal = midiMapping_[index - removeOffset].articulation()->channel();
            midiMapping_[index - removeOffset].articulation()->setChannel(chanVal - removeOffset);
        }
    }
    // We have 'removeOffset' deleted instruments, let's remove their mappings
    for (int index = 0; index < removeOffset; index++) {
        midiMapping_.pop_back();
    }
}

//---------------------------------------------------------
//   updateMidiMapping
//   Add mappings to new instruments and repair existing ones
//---------------------------------------------------------

int MasterScore::updateMidiMapping() {
    int maxport = 0;
    std::set<int> occupiedMidiChannels;     // each entry is port*16+channel, port range: 0-inf, channel: 0-15
    unsigned int searchMidiMappingFrom = 0; // makes getting next free MIDI mapping faster

    for (const MidiMapping &mm : midiMapping_) {
        if (mm.port() == -1 || mm.channel() == -1) {
            continue;
        }
        occupiedMidiChannels.insert(static_cast<int>(mm.port()) * 16 + (int) mm.channel());
        if (maxport < mm.port()) {
            maxport = mm.port();
        }
    }

    for (Part *part : parts()) {
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            const bool useDrumset = instr->useDrumset();
            for (InstrChannel *channel : instr->channel()) {
                doUpdateMidiMapping(maxport, occupiedMidiChannels, searchMidiMappingFrom, part, channel, useDrumset);
            }
        }
    }

    return maxport;
}

void MasterScore::doUpdateMidiMapping(int &maxport, std::set<int> &occupiedMidiChannels, unsigned int &searchMidiMappingFrom, Part *part,
                                      InstrChannel *channel, bool useDrumset) {
    bool channelExists = false;
    for (const MidiMapping &mapping : midiMapping_) {
        const bool validChannelIndex = channel->channel() >= 0 && channel->channel() < static_cast<int>(midiMapping_.size());
        if (channel == mapping.masterChannel_ && validChannelIndex) {
            channelExists = true;
            break;
        }
    }

    if (!channelExists) {
        int midiPort;
        int midiChannel;

        if (useDrumset) {
            midiPort = getNextFreeDrumMidiMapping(occupiedMidiChannels) / 16;
            midiChannel = 9;
        } else {
            const int nm = getNextFreeMidiMapping(occupiedMidiChannels, searchMidiMappingFrom);
            midiPort = nm / 16;
            midiChannel = nm % 16;
        }

        if (midiPort > maxport) {
            maxport = midiPort;
        }

        addMidiMapping(channel, part, midiPort, midiChannel);
        return;
    }

    // Channel could already exist, but have unassigned port or channel...
    MidiMapping &mapping = midiMapping_.at(channel->channel());

    if (mapping.port() == -1) {
        const int nm = getNextFreeMidiMapping(occupiedMidiChannels, searchMidiMappingFrom, -1, mapping.channel());
        mapping.port_ = nm / 16;
        return;
    }

    if (midiMapping_[channel->channel()].channel() == -1) {
        if (useDrumset) {
            mapping.port_ = getNextFreeDrumMidiMapping(occupiedMidiChannels) / 16;
            mapping.channel_ = 9;
            return;
        }
        const int nm = getNextFreeMidiMapping(occupiedMidiChannels, searchMidiMappingFrom, mapping.port());
        mapping.port_ = nm / 16;
        mapping.channel_ = nm % 16;
    }
}

//---------------------------------------------------------
//   addMidiMapping
//---------------------------------------------------------

void MasterScore::addMidiMapping(InstrChannel *channel, Part *part, int midiPort, int midiChannel) {
    if (!part->score()->isMaster()) {
        return;
    }

    MidiMapping mm;
    mm.part_ = part;
    mm.masterChannel_ = channel;
    mm.articulation_.reset(new InstrChannel(*channel));
    mm.link_ = PartChannelSettingsLink(mm.articulation(), mm.masterChannel_, /* excerpt */ false);

    mm.port_ = midiPort;
    mm.channel_ = midiChannel;

    const int mscoreChannel = int(midiMapping_.size());
    mm.articulation_->setChannel(mscoreChannel);
    mm.masterChannel_->setChannel(mscoreChannel);

    midiMapping_.push_back(std::move(mm));
}

//---------------------------------------------------------
//   updateMidiMapping
//---------------------------------------------------------

void MasterScore::updateMidiMapping(InstrChannel *channel, Part *part, int midiPort, int midiChannel) {
    const int c = channel->channel();
    if (c < 0) {
        return;
    }
    if (c >= int(masterScore()->midiMapping().size())) {
        LOGD("Can't set midi channel: midiMapping is empty!");
        return;
    }
    MidiMapping &mm = midiMapping_[c];

    if (midiChannel != -1) {
        mm.channel_ = midiChannel;
    }
    if (midiPort != -1) {
        mm.port_ = midiPort;
    }
    if (part) {
        mm.part_ = part->masterPart();
    }
}
} // namespace mu::engraving
