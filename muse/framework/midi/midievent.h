/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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
#ifndef MUSE_MIDI_MIDIEVENT_H
#define MUSE_MIDI_MIDIEVENT_H

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include "containers.h"

#ifndef MU_FALLTHROUGH
#if __has_cpp_attribute(fallthrough)
#define MU_FALLTHROUGH() [[fallthrough]]
#else
#define MU_FALLTHROUGH() (void) 0
#endif
#endif

namespace muse::midi {
using channel_t = uint8_t;
using tuning_t = float;

/*!
 * MIDI Event stored in Universal MIDI Packet Format Message
 * @see M2-104-UM
 * @see midi.org
 */
struct Event {
    enum class MessageType {
        UTILITY = 0x0,
        SYSTEM_REAL_TIME = 0x1,
        //! MIDI1.0 voice message
        CHANNEL_VOICE10 = 0x2,
        SYSTEM_EXCLUSIVE_DATA = 0x3,
        //! MIDI2.0 voice message
        CHANNEL_VOICE20 = 0x4,
        DATA = 0x5
    };

    enum class Opcode {
        REGISTERED_PER_NOTE_CONTROLLER = 0b0000,
        ASSIGNABLE_PER_NOTE_CONTROLLER = 0b0001,
        REGISTERED_CONTROLLER = 0b0010,
        ASSIGNABLE_CONTROLLER = 0b0011,
        RELATIVE_REGISTERED_CONTROLLER = 0b0100,
        RELATIVE_ASSIGNABLE_CONTROLLER = 0b0101,
        PER_NOTE_PITCH_BEND = 0b0110,
        NOTE_OFF = 0b1000,
        NOTE_ON = 0b1001,
        POLY_PRESSURE = 0b1010,
        CONTROL_CHANGE = 0b1011,
        PROGRAM_CHANGE = 0b1100,
        CHANNEL_PRESSURE = 0b1101,
        PITCH_BEND = 0b1110,
        PER_NOTE_MANAGEMENT = 0b1111
    };

    enum class AttributeType { NO_DATA = 0x00, MANUFACTURER_SPECIFIC = 0x01, PROFILE_SPECIFIC = 0x02, PITCH = 0x03 };

    enum class UtilityStatus { NO_OPERATION = 0x00, JR_CLOCK = 0x01, JR_TIMESTAMP = 0x02 };

    Event() : data_({0, 0, 0, 0}) {}

    Event(const std::array<uint32_t, 4> &d) : data_(d) {}

    Event(Opcode opcode, MessageType type = MessageType::CHANNEL_VOICE20) {
        set_message_type(type);
        set_opcode(opcode);
    }

    //! 4.8.1 NOOP Utility message
    static Event noop() { return Event(); }

    //! from standard MIDI1.0 (specification 1996)
    static Event from_midi10_package(uint32_t data) {
        Event e;

        union {
            unsigned char byte[4];
            uint32_t full;
        } u;

        u.full = data;

        if ((u.byte[0] >= 0x80) && (u.byte[0] < 0xF0)) {
            e.data_[0] = (u.byte[0] << 16) | (u.byte[1] << 8) | u.byte[2];
            e.set_message_type(MessageType::CHANNEL_VOICE10);
        }
        return e;
    }

    uint32_t to_midi10_package() const {
        if (message_type() == MessageType::CHANNEL_VOICE10) {
            union {
                unsigned char byte[4];
                uint32_t uint32;
            } p;

            p.uint32 = data_[0];
            std::swap(p.byte[0], p.byte[2]);
            p.byte[3] = 0;
            return p.uint32;
        }
        return 0;
    }

    static Event from_midi10_bytes(const unsigned char *pointer, size_t length) {
        Event e;
        uint32_t val = 0;
        assert(length <= 3);
        for (size_t i = 0; i < length; ++i) {
            uint32_t byte_val = static_cast<uint32_t>(pointer[i]);
            val |= byte_val << ((2 - i) * 8);
        }
        e.data_[0] = val;
        e.set_message_type(MessageType::CHANNEL_VOICE10);
        return e;
    }

    size_t to_midi10_bytes(unsigned char *pointer) const {
        if (message_type() == MessageType::CHANNEL_VOICE10) {
            auto val = data_[0];
            size_t c = Event::midi10_byte_count_for_opcode(opcode());
            assert(c <= 3);
            for (size_t i = 0; i < c; ++i) {
                uint32_t byte_val = (val >> ((2 - i) * 8)) & 0xff;
                pointer[i] = static_cast<unsigned char>(byte_val);
            }
            return c;
        }
        return 0;
    }

    static std::vector<Event> from_midi10_sys_ex_bytes(const uint8_t *data, size_t size) {
        if (!data || size == 0) {
            return {};
        }

        // Remove F0/F7 if present
        if (size >= 2 && data[0] == 0xF0 && data[size - 1] == 0xF7) {
            data += 1;
            size -= 2;
        }

        std::vector<Event> result;
        size_t pos = 0;

        while (pos < size) {
            const size_t chunk = std::min<size_t>(6, size - pos);
            std::array<uint32_t, 4> words = {};

            uint32_t status;
            if (pos == 0 && chunk == size) {
                status = 0; // COMPLETE
            } else if (pos == 0) {
                status = 1; // START
            } else if (pos + chunk >= size) {
                status = 3; // END
            } else {
                status = 2; // CONTINUE
            }

            uint32_t w0 = 0;

            // MessageType = SysEx
            w0 |= (static_cast<uint32_t>(MessageType::SYSTEM_EXCLUSIVE_DATA) << 28);

            // Group = 0
            w0 |= (0 << 24);

            // Status
            w0 |= (status << 20);

            // Byte count
            w0 |= (static_cast<uint32_t>(chunk) << 16);

            // First 2 bytes
            if (chunk >= 1) {
                w0 |= static_cast<uint32_t>(data[pos + 0]) << 8;
            }
            if (chunk >= 2) {
                w0 |= static_cast<uint32_t>(data[pos + 1]);
            }

            // Remaining bytes (up to 4)
            uint32_t w1 = 0;
            for (size_t i = 2; i < chunk; ++i) {
                w1 |= static_cast<uint32_t>(data[pos + i]) << (24 - (i - 2) * 8);
            }

            words[0] = w0;
            words[1] = w1;

            result.emplace_back(words);
            pos += chunk;
        }

        return result;
    }

    static Event from_midi20_words(const uint32_t *data, size_t count) {
        Event e;
        size_t num_bytes = std::min(count, e.data_.size()) * sizeof(uint32_t);
        std::memcpy(e.data_.data(), data, num_bytes);
        return e;
    }

    const uint32_t *midi20_words() const { return data_.data(); }

    size_t midi20_word_count() const { return Event::word_count_for_message_type(message_type()); }

    bool operator==(const Event &other) const { return data_ == other.data_; }

    bool operator!=(const Event &other) const { return !operator==(other); }

    operator bool() const { return operator!=(noop()) && is_valid(); }

    bool operator<(const Event &other) const { return data_ < other.data_; }

    bool is_channel_voice() const {
        return message_type() == MessageType::CHANNEL_VOICE10 || message_type() == MessageType::CHANNEL_VOICE20;
    }

    bool is_channel_voice20() const { return message_type() == MessageType::CHANNEL_VOICE20; }

    bool is_message_type_in(const std::set<MessageType> &types) const { return types.find(message_type()) != types.end(); }

    bool is_opcode_in(const std::set<Opcode> &opcodes) const { return opcodes.find(opcode()) != opcodes.end(); }

    //! check UMP for correct structure
    bool is_valid() const {
        switch (message_type()) {
            case MessageType::UTILITY: {
                std::set<UtilityStatus> statuses = {UtilityStatus::NO_OPERATION, UtilityStatus::JR_CLOCK, UtilityStatus::JR_TIMESTAMP};
                return statuses.find(static_cast<UtilityStatus>(status())) != statuses.end();
            }

            case MessageType::SYSTEM_REAL_TIME:
            case MessageType::SYSTEM_EXCLUSIVE_DATA:
            case MessageType::DATA: return true;

            case MessageType::CHANNEL_VOICE10:
                return is_opcode_in({Opcode::NOTE_OFF, Opcode::NOTE_ON, Opcode::POLY_PRESSURE, Opcode::CONTROL_CHANGE,
                                     Opcode::PROGRAM_CHANGE, Opcode::CHANNEL_PRESSURE, Opcode::PITCH_BEND});

            case MessageType::CHANNEL_VOICE20:
                return is_opcode_in({Opcode::REGISTERED_PER_NOTE_CONTROLLER, Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER,
                                     Opcode::REGISTERED_CONTROLLER, Opcode::ASSIGNABLE_CONTROLLER, Opcode::RELATIVE_REGISTERED_CONTROLLER,
                                     Opcode::RELATIVE_ASSIGNABLE_CONTROLLER, Opcode::PER_NOTE_PITCH_BEND, Opcode::NOTE_OFF, Opcode::NOTE_ON,
                                     Opcode::POLY_PRESSURE, Opcode::CONTROL_CHANGE, Opcode::PROGRAM_CHANGE, Opcode::CHANNEL_PRESSURE,
                                     Opcode::PITCH_BEND, Opcode::PER_NOTE_MANAGEMENT});
        }
        return false;
    }

    MessageType message_type() const { return static_cast<MessageType>(data_[0] >> 28); }

    void set_message_type(MessageType type) {
        uint32_t mask = static_cast<uint32_t>(type) << 28;
        data_[0] &= 0x0FFFFFFF;
        data_[0] |= mask;
    }

    channel_t group() const { return (data_[0] >> 24) & 0b00001111; }

    void set_group(channel_t group) {
        assert(group < 16);
        uint32_t mask = group << 24;
        data_[0] &= 0xF0FFFFFF;
        data_[0] |= mask;
    }

    Opcode opcode() const {
        assert_channel_voice();
        return static_cast<Opcode>((data_[0] >> 20) & 0b00001111);
    }

    void set_opcode(Opcode code) {
        assert_channel_voice();
        uint32_t mask = static_cast<uint32_t>(code) << 20;
        data_[0] &= 0xFF0FFFFF;
        data_[0] |= mask;
    }

    uint8_t status() const {
        assert_message_type({MessageType::SYSTEM_REAL_TIME, MessageType::UTILITY});

        switch (message_type()) {
            case MessageType::SYSTEM_REAL_TIME: return (data_[0] >> 16) & 0b11111111;
            case MessageType::UTILITY: return (data_[0] >> 16) & 0b00001111;
            default: break;
        }
        return 0;
    }

    channel_t channel() const {
        assert_channel_voice();
        return (data_[0] >> 16) & 0b00001111;
    }

    void set_channel(channel_t channel) {
        assert_channel_voice();
        assert(channel < 16);
        uint32_t mask = channel << 16;
        data_[0] &= 0xFFF0FFFF;
        data_[0] |= mask;
    }

    uint8_t note() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF, Opcode::POLY_PRESSURE, Opcode::PER_NOTE_PITCH_BEND, Opcode::PER_NOTE_MANAGEMENT,
                       Opcode::REGISTERED_PER_NOTE_CONTROLLER, Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER});
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10:
            case MessageType::CHANNEL_VOICE20: return (data_[0] >> 8) & 0x7F; break;
            default: assert(false);
        }
        return 0;
    }

    void set_note(uint8_t value) {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF, Opcode::POLY_PRESSURE, Opcode::PER_NOTE_PITCH_BEND, Opcode::PER_NOTE_MANAGEMENT,
                       Opcode::REGISTERED_PER_NOTE_CONTROLLER, Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER});
        assert(value < 128);
        uint32_t mask = value << 8;
        data_[0] &= 0xFFFF00FF;
        data_[0] |= mask;
    }

    //! return note from Pitch attribute (for NoteOn & NoteOff) events) if exists else return note()
    uint8_t pitch_note() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        if (attribute_type() == AttributeType::PITCH) {
            return static_cast<uint8_t>(attribute() >> 9);
        }
        return note();
    }

    //! return tuning in semitones from Pitch attribute (for NoteOn & NoteOff) events) if exists else return 0.f
    tuning_t pitch_tuning() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        if (attribute_type() == AttributeType::PITCH) {
            return static_cast<tuning_t>((attribute() & 0x1FF) / static_cast<tuning_t>(0x200));
        }
        return 0.f;
    }

    //! return tuning in cents
    tuning_t pitch_tuning_cents() const { return pitch_tuning() * 100; }

    //! 4.2.14.3 @see pitchNote(), pitchTuning()
    void set_pitch_note(uint8_t note, tuning_t tuning) {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        assert_message_type({MessageType::CHANNEL_VOICE20});

        set_attribute_type(AttributeType::PITCH);
        uint16_t attribute = static_cast<uint16_t>(tuning * 0x200);
        attribute &= 0x1FF; //for safe clear first 7 bits
        attribute |= static_cast<uint16_t>(note) << 9;
        set_attribute(attribute);
    }

    uint8_t velocity7() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});

        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: return data_[0] & 0x7F;
            case MessageType::CHANNEL_VOICE20: return static_cast<uint8_t>(scale_down(data_[1] >> 16, 16, 7));
            default: assert(false);
        }
        return 0;
    }

    void set_velocity7(uint8_t value) {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        assert(value < 128);

        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: {
                uint32_t mask = value & 0x7F;
                data_[0] &= 0xFFFFFF00;
                data_[0] |= mask;
            } break;
            case MessageType::CHANNEL_VOICE20: {
                uint32_t mask = scale_up(value, 7, 16) << 16;
                data_[1] &= 0x0000FFFF;
                data_[1] |= mask;
            } break;
            default: assert(false);
        }
    }

    uint16_t velocity16() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});

        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: return static_cast<uint16_t>(scale_up(data_[0] & 0x7F, 7, 16));
            case MessageType::CHANNEL_VOICE20: return static_cast<uint16_t>(data_[1] >> 16);
            default: assert(false);
        }
        return 0;
    }

    void set_velocity16(uint16_t value) {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});

        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: {
                uint32_t mask = scale_down(value, 16, 7);
                data_[0] &= 0xFFFFFF00;
                data_[0] |= mask;
            } break;
            case MessageType::CHANNEL_VOICE20: {
                uint32_t mask = value << 16;
                data_[1] &= 0x0000FFFF;
                data_[1] |= mask;
            } break;
            default: assert(false);
        }
    }

    uint32_t data() const {
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10:
                switch (opcode()) {
                    case Opcode::POLY_PRESSURE:
                    case Opcode::CONTROL_CHANGE: return data_[0] & 0x7F;
                    case Opcode::CHANNEL_PRESSURE: return (data_[0] >> 8) & 0x7F;
                    case Opcode::PITCH_BEND: return ((data_[0] & 0x7F) << 7) | ((data_[0] & 0x7F00) >> 8);
                    default: assert(false);
                }
                MU_FALLTHROUGH();
            case MessageType::CHANNEL_VOICE20:
                switch (opcode()) {
                    case Opcode::POLY_PRESSURE:
                    case Opcode::REGISTERED_PER_NOTE_CONTROLLER:
                    case Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER:
                    case Opcode::REGISTERED_CONTROLLER:
                    case Opcode::ASSIGNABLE_CONTROLLER:
                    case Opcode::RELATIVE_REGISTERED_CONTROLLER:
                    case Opcode::RELATIVE_ASSIGNABLE_CONTROLLER:
                    case Opcode::CONTROL_CHANGE:
                    case Opcode::CHANNEL_PRESSURE:
                    case Opcode::PITCH_BEND:
                    case Opcode::PER_NOTE_PITCH_BEND: return data_[1];
                    default: assert(false);
                }
                MU_FALLTHROUGH();
            default:; //TODO
        }

        return 0;
    }

    void set_data(uint32_t data) {
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10:
                switch (opcode()) {
                    case Opcode::POLY_PRESSURE:
                    case Opcode::CONTROL_CHANGE: {
                        assert(data < 128);
                        uint32_t mask = data & 0x7F;
                        data_[0] &= 0xFFFFFF00;
                        data_[0] |= mask;
                        break;
                    }
                    case Opcode::CHANNEL_PRESSURE: {
                        assert(data < 128);
                        uint32_t mask = (data & 0x7F) << 8;
                        data_[0] &= 0xFFFF00FF;
                        data_[0] |= mask;
                        break;
                    }
                    case Opcode::PITCH_BEND: {
                        data &= 0x3FFF;
                        data_[0] &= 0xFFFF0000;
                        //3d byte: r,lsb 4th: r,msb
                        data_[0] |= ((data >> 7) & 0x7F) | ((data & 0x7F) << 8);
                        break;
                    }
                    default: assert(false);
                }
                break;

            case MessageType::CHANNEL_VOICE20:
                switch (opcode()) {
                    case Opcode::POLY_PRESSURE:
                    case Opcode::REGISTERED_PER_NOTE_CONTROLLER:
                    case Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER:
                    case Opcode::REGISTERED_CONTROLLER:
                    case Opcode::ASSIGNABLE_CONTROLLER:
                    case Opcode::RELATIVE_REGISTERED_CONTROLLER:
                    case Opcode::RELATIVE_ASSIGNABLE_CONTROLLER:
                    case Opcode::CONTROL_CHANGE:
                    case Opcode::CHANNEL_PRESSURE:
                    case Opcode::PITCH_BEND:
                    case Opcode::PER_NOTE_PITCH_BEND: data_[1] = data; break;
                    default: assert(false);
                }
                break;
            default: assert(false);
        }
    }

    uint8_t data7() const {
        uint32_t val = data();
        if (message_type() == MessageType::CHANNEL_VOICE20) {
            val = scale_down(val, 32, 7);
        } else if (message_type() == MessageType::CHANNEL_VOICE10 && opcode() == Opcode::PITCH_BEND) {
            val = scale_down(val, 14, 7);
        }
        return val & 0x7F;
    }

    uint32_t data14() const {
        uint32_t val = data();
        if (message_type() == MessageType::CHANNEL_VOICE20) {
            return scale_down(val, 32, 14);
        }
        return val;
    }

    uint32_t pitch_bend14() const {
        assert(is_channel_voice() && opcode() == Opcode::PITCH_BEND);
        return data14();
    }

    /*! return signed value:
     *  float value for Pitch attribute of NoteOn
     *  @see: data() can return pitch centered unsigned raw value depenned on MIDI version
     */
    float pitch() const {
        assert_opcode({Opcode::PITCH_BEND});
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE20: return (data() - 0x80000000) / static_cast<float>(0xFFFFFFFF);
            default: /* silence */ break;
        }
        return (data() - 8192) / static_cast<float>(0x3FFF); //MIDI1.0
    }

    uint8_t index() const {
        assert_opcode({Opcode::CONTROL_CHANGE, Opcode::REGISTERED_PER_NOTE_CONTROLLER, Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER,
                       Opcode::REGISTERED_CONTROLLER, Opcode::ASSIGNABLE_CONTROLLER, Opcode::RELATIVE_REGISTERED_CONTROLLER,
                       Opcode::RELATIVE_ASSIGNABLE_CONTROLLER});
        switch (opcode()) {
            case Opcode::CONTROL_CHANGE:
            case Opcode::REGISTERED_CONTROLLER:
            case Opcode::ASSIGNABLE_CONTROLLER:
            case Opcode::RELATIVE_REGISTERED_CONTROLLER:
            case Opcode::RELATIVE_ASSIGNABLE_CONTROLLER: return (data_[0] >> 8) & 0x7F;

            case Opcode::REGISTERED_PER_NOTE_CONTROLLER:
            case Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER: return data_[0] & 0xFF;

            default: assert(false);
        }
        return 0;
    }

    void set_index(uint8_t value) {
        assert_opcode({Opcode::CONTROL_CHANGE, Opcode::REGISTERED_PER_NOTE_CONTROLLER, Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER,
                       Opcode::REGISTERED_CONTROLLER, Opcode::ASSIGNABLE_CONTROLLER, Opcode::RELATIVE_REGISTERED_CONTROLLER,
                       Opcode::RELATIVE_ASSIGNABLE_CONTROLLER});

        switch (opcode()) {
            case Opcode::CONTROL_CHANGE:
            case Opcode::REGISTERED_CONTROLLER:
            case Opcode::ASSIGNABLE_CONTROLLER:
            case Opcode::RELATIVE_REGISTERED_CONTROLLER:
            case Opcode::RELATIVE_ASSIGNABLE_CONTROLLER: {
                assert(value < 128);
                uint32_t mask = value << 8;
                data_[0] &= 0xFFFF00FF;
                data_[0] |= mask;
                break;
            }
            case Opcode::REGISTERED_PER_NOTE_CONTROLLER:
            case Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER: {
                data_[0] &= 0xFFFFFF00;
                data_[0] |= value;
                break;
            }
            default: assert(false);
        }
    }

    uint8_t program() const {
        assert_opcode({Opcode::PROGRAM_CHANGE});
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: return (data_[0] >> 8) & 0x7F; break;
            case MessageType::CHANNEL_VOICE20: return (data_[1] >> 24) & 0x7F; break;
            default: assert(false);
        }
        return 0;
    }

    void set_program(uint8_t value) {
        assert_opcode({Opcode::PROGRAM_CHANGE});
        assert(value < 128);
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: {
                uint32_t mask = value << 8;
                data_[0] &= 0xFFFF00FF;
                data_[0] |= mask;
                break;
            }
            case MessageType::CHANNEL_VOICE20: {
                uint32_t mask = value << 24;
                data_[1] &= 0x00FFFFFF;
                data_[1] |= mask;
                break;
            }
            default: assert(false);
        }
    }

    uint16_t bank() const {
        assert_opcode({Opcode::PROGRAM_CHANGE, Opcode::REGISTERED_CONTROLLER, Opcode::ASSIGNABLE_CONTROLLER,
                       Opcode::RELATIVE_REGISTERED_CONTROLLER, Opcode::RELATIVE_ASSIGNABLE_CONTROLLER});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        if (opcode() == Opcode::PROGRAM_CHANGE) {
            return ((data_[1] & 0x7F00) >> 1) | (data_[1] & 0x7F);
        }
        return (data_[0] >> 8) & 0x7F;
    }

    void set_bank(uint16_t bank) {
        assert_opcode({Opcode::PROGRAM_CHANGE, Opcode::REGISTERED_CONTROLLER, Opcode::ASSIGNABLE_CONTROLLER,
                       Opcode::RELATIVE_REGISTERED_CONTROLLER, Opcode::RELATIVE_ASSIGNABLE_CONTROLLER});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        if (opcode() == Opcode::PROGRAM_CHANGE) {
            data_[0] |= 0x01; //set BankValid bit
            uint32_t mask = (bank & 0x3F80) << 8 | (bank & 0x7F);
            data_[1] &= 0xFFFF0000;
            data_[1] |= mask;
            return;
        }
        assert(bank < 128);
        uint32_t mask = bank << 8;
        data_[0] &= 0xFFFF00FF;
        data_[0] |= mask;
    }

    bool is_bank_valid() const {
        assert_opcode({Opcode::PROGRAM_CHANGE});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        return data_[0] & 0x01;
    }

    AttributeType attribute_type() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        return static_cast<AttributeType>(data_[0] & 0xFF);
    }

    void set_attribute_type(AttributeType type) {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        data_[0] &= 0xFFFFFF00;
        data_[0] |= static_cast<uint32_t>(type);
    }

    void set_attribute_type(uint8_t type) { set_attribute_type(static_cast<AttributeType>(type)); }

    uint16_t attribute() const {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        if (message_type() == MessageType::CHANNEL_VOICE20) {
            return data_[1] & 0xFFFF;
        }
        return 0x00;
    }

    void set_attribute(uint16_t value) {
        assert_opcode({Opcode::NOTE_ON, Opcode::NOTE_OFF});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        data_[1] &= 0xFFFF0000;
        data_[1] |= value;
    }

    void set_per_note_detach(bool value) {
        assert_opcode({Opcode::PER_NOTE_MANAGEMENT});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        data_[0] &= 0xFFFFFFFD;
        data_[0] |= static_cast<uint32_t>(value);
    }

    void set_per_note_reset(bool value) {
        assert_opcode({Opcode::PER_NOTE_MANAGEMENT});
        assert_message_type({MessageType::CHANNEL_VOICE20});
        data_[0] &= 0xFFFFFFFE;
        data_[0] |= static_cast<uint32_t>(value);
    }

    //!convert ChannelVoice from MIDI2.0 to MIDI1.0
    std::vector<Event> to_mid_i10() const {
        std::vector<Event> events;
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: events.push_back(*this); break;
            case MessageType::CHANNEL_VOICE20: {
                auto basic10_event = Event(opcode(), MessageType::CHANNEL_VOICE10);
                basic10_event.set_channel(channel());
                basic10_event.set_group(group());
                switch (opcode()) {
                    //D2.1
                    case Opcode::POLY_PRESSURE:
                    case Opcode::CONTROL_CHANGE:
                    case Opcode::NOTE_ON:
                    case Opcode::NOTE_OFF: {
                        auto e = basic10_event;
                        auto v1 = (data_[0] & 0x7F00);
                        auto v2 = data_[1] >> 25;
                        if (opcode() == Opcode::NOTE_ON && v2 == 0) {
                            //4.2.2 velocity comment
                            v2 = 1;
                        }
                        e.data_[0] |= v1 | v2;
                        events.push_back(e);
                        break;
                    }

                    //D2.2
                    case Opcode::CHANNEL_PRESSURE: {
                        auto e = basic10_event;
                        e.set_data(scale_down(data(), 32, 7));
                        events.push_back(e);
                        break;
                    }

                    //D2.3
                    case Opcode::ASSIGNABLE_CONTROLLER:
                    case Opcode::REGISTERED_CONTROLLER: {
                        std::vector<std::pair<uint8_t, uint32_t>> control_changes = {
                            {(opcode() == Opcode::REGISTERED_CONTROLLER ? 101 : 99), bank()},
                            {(opcode() == Opcode::REGISTERED_CONTROLLER ? 100 : 98), index()},
                            {6, data() >> 25},               // first 7 bits
                            {38, (data() & 0x1FC0000) >> 18} // second 7 bits
                        };
                        for (auto &c : control_changes) {
                            auto e = basic10_event;
                            e.set_opcode(Opcode::CONTROL_CHANGE);
                            e.set_index(c.first);
                            e.set_data(c.second);
                            events.push_back(e);
                        }
                        break;
                    }

                    //D2.4
                    case Opcode::PROGRAM_CHANGE: {
                        if (is_bank_valid()) {
                            auto e = basic10_event;
                            e.set_opcode(Opcode::CONTROL_CHANGE);
                            e.set_index(0);
                            e.set_data((data_[1] & 0x7F00) >> 8);
                            events.push_back(e);
                            e.set_index(0);
                            e.set_data(data_[1] & 0x7F);
                            events.push_back(e);
                        }
                        auto e = basic10_event;
                        e.set_program(program());
                        events.push_back(e);
                        break;
                    }
                    //D2.5
                    case Opcode::PITCH_BEND: {
                        auto e = basic10_event;
                        e.set_data(pitch_bend14());
                        events.push_back(e);
                        break;
                    }
                    default: break;
                }
                break;
            }
            default: break;
        }
        return events;
    }

    //!convert ChannelVoice from MIDI1.0 to MIDI2.0
    Event to_mid_i20(Event chain = noop()) const {
        Event event;
        switch (message_type()) {
            case MessageType::CHANNEL_VOICE20: return *this; break;
            case MessageType::CHANNEL_VOICE10: {
                if (chain) {
                    event = chain;
                } else {
                    event = Event(opcode(), MessageType::CHANNEL_VOICE20);
                    event.set_channel(channel());
                    event.set_group(group());
                }
                switch (opcode()) {
                    //D3.1
                    case Opcode::NOTE_ON:
                    case Opcode::NOTE_OFF:
                        event.set_note(note());
                        event.set_velocity7(velocity7());
                        if (velocity7() == 0) {
                            event.set_opcode(Opcode::NOTE_OFF);
                        }
                        break;
                    //D3.2
                    case Opcode::POLY_PRESSURE:
                        event.set_note(note());
                        event.set_data(scale_up(data(), 7, 32));
                        break;
                    //D3.3
                    case Opcode::CONTROL_CHANGE: {
                        switch (index()) {
                            default:
                                event.set_index(index());
                                event.set_data(scale_up(data(), 7, 32));
                                break;
                            // RPN and NPRN controller messages are no ordenary conrollers
                            // and need special handling in MIDI 2.0
                            case 99:
                                event.set_opcode(Opcode::ASSIGNABLE_CONTROLLER);
                                event.set_bank(static_cast<uint16_t>(data()));
                                break;
                            case 101:
                                event.set_opcode(Opcode::REGISTERED_CONTROLLER);
                                event.set_bank(static_cast<uint16_t>(data()));
                                break;
                            case 98:
                            case 100: event.set_index(static_cast<uint8_t>(data())); break;
                            case 6:
                                event.data_[0] &= 0x1FFFFFF;
                                event.data_[0] |= (data() & 0x7F) << 25;
                                break;
                            case 38:
                                event.data_[0] &= 0xFE03FFFF;
                                event.data_[0] |= (data() & 0x7F) << 18;
                                break;
                        }
                        break;
                    }

                    //D3.4
                    case Opcode::PROGRAM_CHANGE: event.set_program(program()); break;

                    //D3.5
                    case Opcode::CHANNEL_PRESSURE: event.set_data(scale_up(data(), 7, 32)); break;

                    //D3.6
                    case Opcode::PITCH_BEND: event.set_data(scale_up(data(), 14, 32)); break;

                    default: break;
                }
                break;
            }
            default: break;
        }
        return event;
    }

    std::string opcode_string() const {
        static const std::map<Opcode, std::string> m = {
            {Opcode::REGISTERED_PER_NOTE_CONTROLLER, std::string("Opcode::RegisteredPerNoteController")},
            {Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER, std::string("Opcode::AssignablePerNoteController")},
            {Opcode::REGISTERED_CONTROLLER, std::string("Opcode::RegisteredController")},
            {Opcode::ASSIGNABLE_CONTROLLER, std::string("Opcode::AssignableController")},
            {Opcode::RELATIVE_REGISTERED_CONTROLLER, std::string("Opcode::RelativeRegisteredController")},
            {Opcode::RELATIVE_ASSIGNABLE_CONTROLLER, std::string("Opcode::RelativeAssignableController")},
            {Opcode::PER_NOTE_PITCH_BEND, std::string("Opcode::PerNotePitchBend")},
            {Opcode::NOTE_OFF, std::string("Opcode::NoteOff")},
            {Opcode::NOTE_ON, std::string("Opcode::NoteOn")},
            {Opcode::POLY_PRESSURE, std::string("Opcode::PolyPressure")},
            {Opcode::CONTROL_CHANGE, std::string("Opcode::ControlChange")},
            {Opcode::PROGRAM_CHANGE, std::string("Opcode::ProgramChange")},
            {Opcode::CHANNEL_PRESSURE, std::string("Opcode::ChannelPressure")},
            {Opcode::PITCH_BEND, std::string("Opcode::PitchBend")},
            {Opcode::PER_NOTE_MANAGEMENT, std::string("Opcode::PerNoteManagement")},
        };
        return muse::value(m, opcode());
    }

    std::string to_string() const {
        std::string str;
        auto data_to_str = [&str, this]() {
            str += " {";
            for (auto &d : data_) {
                str += " " + std::to_string(d);
            }
            str += "}";
        };

        switch (message_type()) {
            case MessageType::CHANNEL_VOICE10: {
                str += "MIDI1.0 " + opcode_string();
                str += " group: " + std::to_string(group());
                str += " channel: " + std::to_string(channel());
                switch (opcode()) {
                    case Opcode::NOTE_ON:
                    case Opcode::NOTE_OFF: str += " note: " + std::to_string(note()) + " velocity: " + std::to_string(velocity7()); break;
                    case Opcode::POLY_PRESSURE: str += " note: " + std::to_string(note()) + " data: " + std::to_string(data()); break;
                    case Opcode::CONTROL_CHANGE: str += " index: " + std::to_string(index()) + " data: " + std::to_string(data()); break;
                    case Opcode::PROGRAM_CHANGE: str += " program: " + std::to_string(program()); break;
                    case Opcode::CHANNEL_PRESSURE: str += " data: " + std::to_string(data()); break;
                    case Opcode::PITCH_BEND: str += " value: " + std::to_string(pitch()); break;
                    default: /* silence warning */ break;
                }
                break;
            }
            case MessageType::CHANNEL_VOICE20: {
                str += "MIDI2.0 " + opcode_string();
                str += " group: " + std::to_string(group());
                str += " channel: " + std::to_string(channel());
                switch (opcode()) {
                    case Opcode::NOTE_OFF:
                    case Opcode::NOTE_ON:
                        str += " note: " + std::to_string(note()) + " velocity: " + std::to_string(velocity16()) +
                               " attr type: " + std::to_string(static_cast<uint32_t>(attribute_type())) +
                               " attr value: " + std::to_string(attribute());
                        if (attribute_type() == AttributeType::PITCH) {
                            str += "pitch: note:" + std::to_string(pitch_note()) + " " + std::to_string(pitch_tuning()) + " semitone";
                        }
                        break;
                    case Opcode::POLY_PRESSURE:
                    case Opcode::PER_NOTE_PITCH_BEND: str += " note: " + std::to_string(note()) + " data: " + std::to_string(data()); break;
                    case Opcode::REGISTERED_PER_NOTE_CONTROLLER:
                    case Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER:
                        str +=
                            " note: " + std::to_string(note()) + " index: " + std::to_string(index()) + " data: " + std::to_string(data());
                        break;
                    case Opcode::PER_NOTE_MANAGEMENT:
                        str += " note: " + std::to_string(note()) + (data_[0] & 0b10 ? " detach controller" : "") +
                               (data_[0] & 0b01 ? " reset controller" : "");
                        break;
                    case Opcode::CONTROL_CHANGE: str += " index: " + std::to_string(index()) + " data: " + std::to_string(data()); break;
                    case Opcode::PROGRAM_CHANGE:
                        str += " program: " + std::to_string(program()) + (is_bank_valid() ? " set bank: " + std::to_string(bank()) : "");
                        break;
                    case Opcode::REGISTERED_CONTROLLER:
                    case Opcode::ASSIGNABLE_CONTROLLER:
                    case Opcode::RELATIVE_REGISTERED_CONTROLLER:
                    case Opcode::RELATIVE_ASSIGNABLE_CONTROLLER:
                        str +=
                            " bank: " + std::to_string(bank()) + " index: " + std::to_string(index()) + " data: " + std::to_string(data());
                        break;
                    case Opcode::CHANNEL_PRESSURE: str += " data: " + std::to_string(data()); break;
                    case Opcode::PITCH_BEND: str += " value: " + std::to_string(pitch()); break;
                }

                break;
            }
            case MessageType::UTILITY:
                str += "MIDI2.0 Utility ";
                if (data_[0] == 0) {
                    str += " NOOP";
                    break;
                }
                data_to_str();
                break;
            case MessageType::SYSTEM_REAL_TIME:
                str += "MIDI System";
                data_to_str();
                break;
            case MessageType::SYSTEM_EXCLUSIVE_DATA:
                str += "MIDI System Exclusive";
                data_to_str();
                break;
            case MessageType::DATA:
                str += "MIDI2.0 Data";
                data_to_str();
                break;
        }
        return str;
    }

    static size_t word_count_for_message_type(MessageType message_type) {
        // MIDI Association Document: M2-104-UM
        // Document Version 1.1.2
        // Draft Date 2023-10-27
        // Published 2023-11-10
        // Section 2.1.4
        switch (message_type) {
            case MessageType::UTILITY: return 1;
            case MessageType::SYSTEM_REAL_TIME: return 1;
            case MessageType::CHANNEL_VOICE10: return 1;
            case MessageType::SYSTEM_EXCLUSIVE_DATA: return 2;
            case MessageType::CHANNEL_VOICE20: return 2;
            case MessageType::DATA: return 4;
        }

        // reserved
        switch (uint32_t(message_type)) {
            case 0x6: return 1;
            case 0x7: return 1;
            case 0x8: return 2;
            case 0x9: return 2;
            case 0xa: return 2;
            case 0xb: return 3;
            case 0xc: return 3;
            case 0xd: return 4;
            case 0xe: return 4;
            case 0xf: return 4;
        }

        return 0;
    }

    static size_t midi10_byte_count_for_opcode(Opcode opcode) {
        switch (opcode) {
            case Opcode::REGISTERED_PER_NOTE_CONTROLLER:
            case Opcode::ASSIGNABLE_PER_NOTE_CONTROLLER:
            case Opcode::RELATIVE_REGISTERED_CONTROLLER:
            case Opcode::RELATIVE_ASSIGNABLE_CONTROLLER:
            case Opcode::PER_NOTE_PITCH_BEND:
            case Opcode::PER_NOTE_MANAGEMENT:
                //D2.8 cannot be translated to Midi 1.0
                assert(false);
            case Opcode::REGISTERED_CONTROLLER:
            case Opcode::ASSIGNABLE_CONTROLLER:
                // Already translated to a sequence of CCs.
                assert(false);
            case Opcode::NOTE_OFF:
            case Opcode::NOTE_ON:
            case Opcode::POLY_PRESSURE:
            case Opcode::CONTROL_CHANGE:
            case Opcode::PITCH_BEND: return 3;
            case Opcode::PROGRAM_CHANGE:
            case Opcode::CHANNEL_PRESSURE: return 2;
        }
        assert(false);
        return 0;
    }

private:
    void assert_message_type([[maybe_unused]] const std::set<MessageType> &supported_types) const {
        assert(is_message_type_in(supported_types));
    }

    void assert_channel_voice() const { assert(is_channel_voice()); }

    void assert_opcode([[maybe_unused]] const std::set<Opcode> &supported_opcodes) const { assert(is_opcode_in(supported_opcodes)); }

    static uint32_t scale_up(uint32_t src_val, size_t src_bits, size_t dst_bits) {
        // simple bit shift
        size_t scale_bits = dst_bits - src_bits;
        uint32_t bit_shifted_value = src_val << scale_bits;
        uint32_t src_center = static_cast<uint32_t>(2 ^ (src_bits - 1));
        if (src_val <= src_center) {
            return bit_shifted_value;
        }
        // expanded bit repeat scheme
        size_t repeat_bits = src_bits - 1;
        uint32_t repeat_mask = static_cast<uint32_t>((2 ^ repeat_bits) - 1);
        uint32_t repeat_value = src_val & repeat_mask;
        if (scale_bits > repeat_bits) {
            repeat_value <<= scale_bits - repeat_bits;
        } else {
            repeat_value >>= repeat_bits - scale_bits;
        }
        while (repeat_value != 0) {
            bit_shifted_value |= repeat_value;
            repeat_value >>= repeat_bits;
        }
        return bit_shifted_value;
    }

    static uint32_t scale_down(uint32_t src_val, size_t src_bits, size_t dst_bits) {
        size_t scale_bits = (src_bits - dst_bits);
        return src_val >> scale_bits;
    }

    std::array<uint32_t, 4> data_ = {{0, 0, 0, 0}};
};
} // namespace muse::midi

#endif // MUSE_MIDI_MIDIEVENT_H
