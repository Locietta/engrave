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

#include "event.h"

#include "dom/note.h"
#include "dom/sig.h"

namespace mu::engraving {
//---------------------------------------------------------
//   Event::Event
//---------------------------------------------------------

Event::Event() {
    type_value = 0;
    ontime_value = -1;
    noquant_ontime_value = 0;
    noquant_duration_value = 0;
    channel_value = 0;
    a_value = 0;
    b_value = 0;
    duration_value = 0;
    tpc_value = 0;
    voice_value = 0;
    edata_value = 0;
    len_value = 0;
    meta_type_value = 0;
    note_value = 0;
    tuning_value = 0.0;
}

Event::Event(int t) {
    type_value = t;
    ontime_value = -1;
    noquant_ontime_value = 0;
    noquant_duration_value = 0;
    channel_value = 0;
    a_value = 0;
    b_value = 0;
    duration_value = 0;
    tpc_value = 0;
    voice_value = 0;
    edata_value = 0;
    len_value = 0;
    meta_type_value = 0;
    note_value = 0;
    tuning_value = 0.0;
}

Event::Event(const Event &e) : PlayEvent(e) {
    type_value = e.type_value;
    ontime_value = e.ontime_value;
    noquant_ontime_value = e.noquant_ontime_value;
    noquant_duration_value = e.noquant_duration_value;
    channel_value = e.channel_value;
    a_value = e.a_value;
    b_value = e.b_value;
    duration_value = e.duration_value;
    tpc_value = e.tpc_value;
    voice_value = e.voice_value;
    notes_value = e.notes_value;
    if (e.edata_value) {
        edata_value = new unsigned char[e.len_value + 1]; // don’t forget trailing zero
        memcpy(edata_value, e.edata_value, e.len_value + 1);
    } else {
        edata_value = 0;
    }
    len_value = e.len_value;
    meta_type_value = e.meta_type_value;
    note_value = e.note_value;
    tuning_value = e.tuning_value;
}

Event::~Event() { delete[] edata_value; }

//---------------------------------------------------------
//   NPlayEvent::NPlayEvent (beatType2metronomeEvent)
//---------------------------------------------------------

NPlayEvent::NPlayEvent(BeatType beat_type) {
    set_type(ME_TICK2);
    set_velo(127);
    switch (beat_type) {
        case BeatType::DOWNBEAT: set_type(ME_TICK1); break;
        case BeatType::SIMPLE_STRESSED:
        case BeatType::COMPOUND_STRESSED:
            // use defaults
            break;
        case BeatType::SIMPLE_UNSTRESSED:
        case BeatType::COMPOUND_UNSTRESSED: set_velo(80); break;
        case BeatType::COMPOUND_SUBBEAT: set_velo(25); break;
        case BeatType::SUBBEAT: set_velo(15); break;
    }
}

//---------------------------------------------------------
//   isMuted
//---------------------------------------------------------

bool NPlayEvent::is_muted() const {
    //! TODO

    //    const Note* n = note();
    //    if (n) {
    //        MasterScore* cs = n->masterScore();
    //        Staff* staff = n->staff();
    //        Instrument* instr = staff->part()->instrument(n->tick());
    //        const Channel* a = instr->playbackChannel(n->subchannel(), cs);
    //        return a->mute() || a->soloMute() || !staff->playbackVoice(n->voice());
    //    }

    //    const Harmony* h = harmony();
    //    if (h) {
    //        const Channel* hCh = h->part()->harmonyChannel();
    //        if (hCh) { //if there is a harmony channel
    //            const Channel* pCh = h->masterScore()->playbackChannel(hCh);
    //            return pCh->mute() || pCh->soloMute();
    //        }
    //    }

    return false;
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Event::dump() const {
    printf("event ");
    switch (type_value) {
        case ME_NOTEON: printf("noteon    "); break;
        case ME_CONTROLLER: printf("controller"); break;
        case ME_PROGRAM: printf("program   "); break;
        default: printf("0x%02x    ", type_value); break;
    }
    printf(" 0x%02x 0x%02x\n", a_value, b_value);
}

bool Event::operator==(const Event &) const {
    return false; // TODO
}

//---------------------------------------------------------
// insert
//---------------------------------------------------------

void EventList::insert(const Event &e) {
    int ontime = e.ontime();
    if (!empty() && back().ontime() > ontime) {
        for (auto i = begin(); i != end(); ++i) {
            if (i->ontime() > ontime) {
                std::vector<Event>::insert(i, e);
                return;
            }
        }
    }
    push_back(e);
}

EventsHolder::events_multimap_t &EventsHolder::operator[](std::size_t idx) {
    if (size() == 0) {
        channels.emplace_back();
    }
    if (idx >= size()) {
        auto diff = idx - size();
        do {
            channels.emplace_back();
        } while (diff-- != 0);
    }
    return channels[idx];
}

const EventsHolder::events_multimap_t &EventsHolder::operator[](std::size_t idx) const {
    // Since EventsHolder acts more like a vector
    // Using const subscript operator for a nonexistent element is UB
    /*
     * cppreference.com
     * Unlike std::map::operator[], this operator never inserts a new element into the container.
     * Accessing a nonexistent element through this operator is undefined behavior.
     */
    assert(idx < size());
    return channels[idx];
}

void EventsHolder::merge_pitch_wheel_events(EventsHolder &pitch_wheel_events) {
    for (size_t i = 0; i < size(); ++i) {
        for (const auto &event_pair : channels[i]) {
            const auto &event = event_pair.second;
            const auto &tick = event_pair.first;
            if (event.type() == ME_NOTEON && event.velo() != 0) {
                const auto &pw_event = muse::find_less(pitch_wheel_events[i], tick);
                if (pw_event != pitch_wheel_events[i].end() && pw_event->second.type() == ME_PITCHBEND) {
                    PitchWheelSpecs specs;
                    NPlayEvent pw_reset(ME_PITCHBEND, (uint8_t) i, specs.m_limit % 128, specs.m_limit / 128);
                    pw_reset.set_originating_staff(pw_event->second.get_originating_staff());

                    int tick_for_pw_reset = 0;
                    if (tick - specs.m_step > pw_event->first) {
                        tick_for_pw_reset = tick - specs.m_step;
                    } else {
                        tick_for_pw_reset = (tick + pw_event->first) / 2;
                    }

                    channels[i].insert(std::pair<int, NPlayEvent>(tick_for_pw_reset, pw_reset));
                }
            }
        }
        channels[i].merge(pitch_wheel_events[i]);
    }
}

//---------------------------------------------------------
//   class EventsHolder::fixupMIDI
//---------------------------------------------------------

void EventsHolder::fixup_midi() {
    /* track info for each of the 128 possible MIDI notes */
    struct ChannelInfo {
        /* which event the first ME_NOTEON came from */
        NPlayEvent *event[128];
        /* how often is the note on right now? */
        unsigned short now_playing[128];
    };

    /* track info for each channel (on the heap, 0-initialised) */
    auto *info = (struct ChannelInfo *) calloc(size() + 1, sizeof(struct ChannelInfo));

    for (auto &mm : channels) {
        auto it = mm.begin();
        while (it != mm.end()) {
            NPlayEvent &event = it->second;
            /* ME_NOTEOFF is never emitted, no need to check for it */
            if (event.type() == ME_NOTEON) {
                uint8_t np = info[event.channel()].now_playing[event.pitch()];
                if (event.velo() == 0) {
                    /* already off (should not happen) or still playing? */
                    if (np == 0 || --np > 0) {
                        event.set_discard(1);
                    } else {
                        /* hoist NOTEOFF to same track as NOTEON */
                        event.set_originating_staff(info[event.channel()].event[event.pitch()]->get_originating_staff());
                    }
                } else {
                    if (++np > 1) {
                        /* restrike, possibly on different track */
                        event.set_discard(info[event.channel()].event[event.pitch()]->get_originating_staff() + 1);
                    }
                    info[event.channel()].event[event.pitch()] = &event;
                }
                info[event.channel()].now_playing[event.pitch()] = np;
            }

            ++it;
        }
    }

    free((void *) info);
}
} // namespace mu::engraving
