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

#ifndef MU_ENGRAVING_COMPAT_EVENT_H
#define MU_ENGRAVING_COMPAT_EVENT_H

#include <map>
#include <vector>

#include "midiinstrumenteffects.h"
#include "midicoreevent.h"

namespace mu::engraving {
class Note;
class Harmony;

enum class BeatType : unsigned char;

//---------------------------------------------------------
//   PlayEvent
//    interface to Synthesizer
//---------------------------------------------------------

class PlayEvent : public MidiCoreEvent {
    OBJECT_ALLOCATOR(engraving, PlayEvent)
protected:
    float tuning_value = .0f;

public:
    PlayEvent() : MidiCoreEvent() {}

    PlayEvent(const MidiCoreEvent &e) : MidiCoreEvent(e) {}

    PlayEvent(uint8_t t, uint8_t c, uint8_t a, uint8_t b) : MidiCoreEvent(t, c, a, b) {}

    float tuning() const { return tuning_value; }

    void set_tuning(float v) { tuning_value = v; }
};

//---------------------------------------------------------
//   NPlayEvent
//    used for Sequencer interface
//---------------------------------------------------------

class NPlayEvent : public PlayEvent {
    OBJECT_ALLOCATOR(engraving, NPlayEvent)

public:
    NPlayEvent() : PlayEvent() {}

    NPlayEvent(uint8_t t, uint8_t c, uint8_t a, uint8_t b) : PlayEvent(t, c, a, b) {}

    NPlayEvent(const MidiCoreEvent &e) : PlayEvent(e) {}

    NPlayEvent(BeatType beat_type);

    const Note *note() const { return note_value; }

    void set_note(const Note *v) { note_value = v; }

    const Harmony *harmony() const { return harmony_value; }

    void set_harmony(const Harmony *v) { harmony_value = v; }

    void set_effect(MidiInstrumentEffect effect) { effect_value = effect; }

    MidiInstrumentEffect effect() const { return effect_value; }

    size_t get_originating_staff() const { return origin; }

    void set_originating_staff(size_t i) { origin = i; }

    void set_discard(size_t d) { discard_value = d; }

    size_t discard() const { return discard_value; }

    bool is_muted() const;

    void set_portamento(bool p) { portamento_value = p; }

    bool portamento() const {
        return portamento_value == true ||
               (this->type() == ME_CONTROLLER &&
                (this->controller() == CTRL_PORTAMENTO || this->controller() == CTRL_PORTAMENTO_CONTROL ||
                 this->controller() == CTRL_PORTAMENTO_TIME_MSB || this->controller() == CTRL_PORTAMENTO_TIME_LSB));
    }

private:

    const Note *note_value = nullptr;
    const Harmony *harmony_value = nullptr;
    size_t origin = size_t(-1);
    size_t discard_value = 0;
    bool portamento_value = false;
    MidiInstrumentEffect effect_value = MidiInstrumentEffect::NONE;
};

//---------------------------------------------------------
//   Event
//---------------------------------------------------------

class Event : public PlayEvent {
    OBJECT_ALLOCATOR(engraving, Event)

    int ontime_value;
    int noquant_ontime_value;
    int noquant_duration_value;
    int duration_value;
    int tpc_value; // tonal pitch class
    int voice_value;
    std::vector<Event> notes_value;
    uint8_t *edata_value; // always zero terminated (_data[_len] == 0; )
    int len_value;
    int meta_type_value;
    const Note *note_value;

public:
    Event();
    Event(const Event &);
    Event(int t);
    ~Event();
    bool operator==(const Event &) const;

    void dump() const;

    int noquant_ontime() const { return noquant_ontime_value; }

    void set_noquant_ontime(int v) { noquant_ontime_value = v; }

    int noquant_duration() const { return noquant_duration_value; }

    void set_noquant_duration(int v) { noquant_duration_value = v; }

    int ontime() const { return ontime_value; }

    void set_ontime(int v) { ontime_value = v; }

    int duration() const { return duration_value; }

    void set_duration(int v) { duration_value = v; }

    int voice() const { return voice_value; }

    void set_voice(int val) { voice_value = val; }

    int offtime() const { return ontime_value + duration_value; }

    std::vector<Event> &notes() { return notes_value; }

    const uint8_t *edata() const { return edata_value; }

    void set_e_data(uint8_t *d) { edata_value = d; }

    int len() const { return len_value; }

    void set_len(int l) { len_value = l; }

    int meta_type() const { return meta_type_value; }

    void set_meta_type(int v) { meta_type_value = v; }

    int tpc() const { return tpc_value; }

    void set_tpc(int v) { tpc_value = v; }

    const Note *note() const { return note_value; }

    void set_note(const Note *v) { note_value = v; }
};

//---------------------------------------------------------
//   EventList
//   EventsHolder
//---------------------------------------------------------

class EventList : public std::vector<Event> {
    OBJECT_ALLOCATOR(engraving, EventList)
public:
    void insert(const Event &);
    void insert_note(int channel, Note *);
};

class EventsHolder {
    OBJECT_ALLOCATOR(engraving, EventsHolder)

    using events_multimap_t = std::multimap<int, NPlayEvent>;
    std::vector<events_multimap_t> channels;
public:
    [[nodiscard]] size_t size() const { return channels.size(); }

    events_multimap_t &operator[](std::size_t idx);
    const events_multimap_t &operator[](std::size_t idx) const;
    void merge_pitch_wheel_events(EventsHolder &pitch_wheel_events);
    void fixup_midi();
};

typedef EventList::iterator iEvent;
typedef EventList::const_iterator ciEvent;
} // namespace mu::engraving
#endif
