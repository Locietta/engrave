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

void MasterScore::rebuild_midi_mapping() {
    Score *playback_score = playback_score_ ? playback_score_ : this;
    set_playback_score(nullptr);

    remove_deleted_midi_mapping();
    int maxport = update_midi_mapping();
    reorder_midi_mapping();
    rebuild_excerpts_midi_mapping();
    master_score()->set_midi_port_count(maxport);

    set_playback_score(playback_score);
}

//---------------------------------------------------------
//   checkMidiMapping
//   midi mapping is simple if all ports and channels
//   don't decrease and don't have 'holes' except drum tracks
//---------------------------------------------------------

void MasterScore::check_midi_mapping() {
    is_simple_midi_mapping_ = true;
    rebuild_midi_mapping();

    std::vector<bool> drum;
    drum.reserve(midi_mapping_.size());
    for (Part *part : parts()) {
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            for (size_t j = 0; j < instr->channel().size(); ++j) {
                drum.push_back(instr->use_drumset());
            }
        }
    }
    int last_channel = -1; // port*16+channel
    int last_drum_port = -1;
    size_t index = 0;
    for (const MidiMapping &m : midi_mapping_) {
        if (index >= drum.size()) {
            break;
        }
        if (drum[index]) {
            last_drum_port++;
            if (m.port() != last_drum_port) {
                is_simple_midi_mapping_ = false;
                return;
            }
        } else {
            last_channel++;
            if (last_channel % 16 == 9) {
                last_channel++;
            }
            int p = last_channel / 16;
            int c = last_channel % 16;
            if (m.port() != p || m.channel() != c) {
                is_simple_midi_mapping_ = false;
                return;
            }
        }
        index++;
    }
}

//---------------------------------------------------------
//   getNextFreeMidiMapping
//---------------------------------------------------------

int MasterScore::get_next_free_midi_mapping(std::set<int> &occupied_midi_channels, unsigned int &search_midi_mapping_from, int p, int ch) {
    if (ch != -1 && p != -1) {
        return p * 16 + ch;
    } else if (ch != -1 && p == -1) {
        for (int port = 0;; port++) {
            if (!muse::contains(occupied_midi_channels, port * 16 + ch)) {
                occupied_midi_channels.insert(port * 16 + ch);
                return port * 16 + ch;
            }
        }
    } else if (ch == -1 && p != -1) {
        for (int channel = 0; channel < 16; channel++) {
            if (channel != 9 && !muse::contains(occupied_midi_channels, p * 16 + channel)) {
                occupied_midi_channels.insert(p * 16 + channel);
                return p * 16 + channel;
            }
        }
    }

    for (;; search_midi_mapping_from++) {
        if (search_midi_mapping_from % 16 != 9 && !muse::contains(occupied_midi_channels, int(search_midi_mapping_from))) {
            occupied_midi_channels.insert(search_midi_mapping_from);
            return search_midi_mapping_from;
        }
    }
}

//---------------------------------------------------------
//   getNextFreeDrumMidiMapping
//---------------------------------------------------------

int MasterScore::get_next_free_drum_midi_mapping(std::set<int> &occupied_midi_channels) {
    for (int i = 0;; i++) {
        if (!muse::contains(occupied_midi_channels, i * 16 + 9)) {
            occupied_midi_channels.insert(i * 16 + 9);
            return i * 16 + 9;
        }
    }
}

//---------------------------------------------------------
//   rebuildExcerptsMidiMapping
//---------------------------------------------------------

void MasterScore::rebuild_excerpts_midi_mapping() {
    for (Excerpt *ex : excerpts()) {
        for (Part *p : ex->excerpt_score()->parts()) {
            const Part *master_part = p->master_part();
            if (!master_part->score()->is_master()) {
                LOGW() << "rebuildExcerptsMidiMapping: no part in master score is linked with " << p->part_name();
                continue;
            }
            assert(p->instruments().size() == master_part->instruments().size());
            for (const auto &[tick, i_master] : master_part->instruments()) {
                Instrument *i_local = p->instrument(Fraction::from_ticks(tick));
                const size_t nchannels = i_master->channel().size();
                if (i_local->channel().size() != nchannels) {
                    // may happen, e.g., if user changes an instrument
                    (*i_local) = (*i_master);
                    continue;
                }
                for (size_t c = 0; c < nchannels; ++c) {
                    InstrChannel *c_local = i_local->channel(static_cast<int>(c));
                    const InstrChannel *c_master = i_master->channel(static_cast<int>(c));
                    c_local->set_channel(c_master->channel());
                }
            }
        }
    }
}

//---------------------------------------------------------
//   reorderMidiMapping
//   Set mappings in order you see in Add->Instruments
//---------------------------------------------------------

void MasterScore::reorder_midi_mapping() {
    using std::swap;
    int sequence_number = 0;
    for (Part *part : parts()) {
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            for (InstrChannel *channel : instr->channel()) {
                if (midi_mapping_[sequence_number].part() == part && midi_mapping_[sequence_number].master_channel_ == channel) {
                    sequence_number++;
                    continue;
                }

                const int should_be = channel->channel();
                swap(midi_mapping_[sequence_number], midi_mapping_[should_be]);
                midi_mapping_[sequence_number].articulation()->set_channel(sequence_number);
                channel->set_channel(sequence_number);
                midi_mapping_[should_be].articulation()->set_channel(should_be);

                sequence_number++;
            }
        }
    }
}

//---------------------------------------------------------
//   removeDeletedMidiMapping
//   Remove mappings to deleted instruments
//---------------------------------------------------------

void MasterScore::remove_deleted_midi_mapping() {
    int remove_offset = 0;
    int mapping_size = int(midi_mapping_.size());
    for (int index = 0; index < mapping_size; index++) {
        Part *part = midi_mapping(index)->part();
        if (!muse::contains(parts(), part)) {
            remove_offset++;
            continue;
        }
        // Not all channels could exist
        bool channel_exists = false;
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            channel_exists = (midi_mapping_[index].articulation()->channel() != -1 &&
                              muse::contains(instr->channel(), midi_mapping_[index].master_channel_) &&
                              !(midi_mapping_[index].port() == -1 && midi_mapping_[index].channel() == -1));
            if (channel_exists) {
                break;
            }
        }
        if (!channel_exists) {
            remove_offset++;
            continue;
        }
        // Let's do a left shift by 'removeOffset' items if necessary
        if (index != 0 && remove_offset != 0) {
            midi_mapping_[index - remove_offset] = std::move(midi_mapping_[index]);

            const int chan_val = midi_mapping_[index - remove_offset].articulation()->channel();
            midi_mapping_[index - remove_offset].articulation()->set_channel(chan_val - remove_offset);
        }
    }
    // We have 'removeOffset' deleted instruments, let's remove their mappings
    for (int index = 0; index < remove_offset; index++) {
        midi_mapping_.pop_back();
    }
}

//---------------------------------------------------------
//   updateMidiMapping
//   Add mappings to new instruments and repair existing ones
//---------------------------------------------------------

int MasterScore::update_midi_mapping() {
    int maxport = 0;
    std::set<int> occupied_midi_channels;      // each entry is port*16+channel, port range: 0-inf, channel: 0-15
    unsigned int search_midi_mapping_from = 0; // makes getting next free MIDI mapping faster

    for (const MidiMapping &mm : midi_mapping_) {
        if (mm.port() == -1 || mm.channel() == -1) {
            continue;
        }
        occupied_midi_channels.insert(static_cast<int>(mm.port()) * 16 + (int) mm.channel());
        if (maxport < mm.port()) {
            maxport = mm.port();
        }
    }

    for (Part *part : parts()) {
        for (const auto &pair : part->instruments()) {
            const Instrument *instr = pair.second;
            const bool use_drumset = instr->use_drumset();
            for (InstrChannel *channel : instr->channel()) {
                do_update_midi_mapping(maxport, occupied_midi_channels, search_midi_mapping_from, part, channel, use_drumset);
            }
        }
    }

    return maxport;
}

void MasterScore::do_update_midi_mapping(int &maxport, std::set<int> &occupied_midi_channels, unsigned int &search_midi_mapping_from,
                                         Part *part, InstrChannel *channel, bool use_drumset) {
    bool channel_exists = false;
    for (const MidiMapping &mapping : midi_mapping_) {
        const bool valid_channel_index = channel->channel() >= 0 && channel->channel() < static_cast<int>(midi_mapping_.size());
        if (channel == mapping.master_channel_ && valid_channel_index) {
            channel_exists = true;
            break;
        }
    }

    if (!channel_exists) {
        int midi_port;
        int midi_channel;

        if (use_drumset) {
            midi_port = get_next_free_drum_midi_mapping(occupied_midi_channels) / 16;
            midi_channel = 9;
        } else {
            const int nm = get_next_free_midi_mapping(occupied_midi_channels, search_midi_mapping_from);
            midi_port = nm / 16;
            midi_channel = nm % 16;
        }

        if (midi_port > maxport) {
            maxport = midi_port;
        }

        add_midi_mapping(channel, part, midi_port, midi_channel);
        return;
    }

    // Channel could already exist, but have unassigned port or channel...
    MidiMapping &mapping = midi_mapping_.at(channel->channel());

    if (mapping.port() == -1) {
        const int nm = get_next_free_midi_mapping(occupied_midi_channels, search_midi_mapping_from, -1, mapping.channel());
        mapping.port_ = nm / 16;
        return;
    }

    if (midi_mapping_[channel->channel()].channel() == -1) {
        if (use_drumset) {
            mapping.port_ = get_next_free_drum_midi_mapping(occupied_midi_channels) / 16;
            mapping.channel_ = 9;
            return;
        }
        const int nm = get_next_free_midi_mapping(occupied_midi_channels, search_midi_mapping_from, mapping.port());
        mapping.port_ = nm / 16;
        mapping.channel_ = nm % 16;
    }
}

//---------------------------------------------------------
//   addMidiMapping
//---------------------------------------------------------

void MasterScore::add_midi_mapping(InstrChannel *channel, Part *part, int midi_port, int midi_channel) {
    if (!part->score()->is_master()) {
        return;
    }

    MidiMapping mm;
    mm.part_ = part;
    mm.master_channel_ = channel;
    mm.articulation_.reset(new InstrChannel(*channel));
    mm.link_ = PartChannelSettingsLink(mm.articulation(), mm.master_channel_, /* excerpt */ false);

    mm.port_ = midi_port;
    mm.channel_ = midi_channel;

    const int mscore_channel = int(midi_mapping_.size());
    mm.articulation_->set_channel(mscore_channel);
    mm.master_channel_->set_channel(mscore_channel);

    midi_mapping_.push_back(std::move(mm));
}

//---------------------------------------------------------
//   updateMidiMapping
//---------------------------------------------------------

void MasterScore::update_midi_mapping(InstrChannel *channel, Part *part, int midi_port, int midi_channel) {
    const int c = channel->channel();
    if (c < 0) {
        return;
    }
    if (c >= int(master_score()->midi_mapping().size())) {
        LOGD("Can't set midi channel: midiMapping is empty!");
        return;
    }
    MidiMapping &mm = midi_mapping_[c];

    if (midi_channel != -1) {
        mm.channel_ = midi_channel;
    }
    if (midi_port != -1) {
        mm.port_ = midi_port;
    }
    if (part) {
        mm.part_ = part->master_part();
    }
}
} // namespace mu::engraving
