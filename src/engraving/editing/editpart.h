/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "../compat/midi/midipatch.h"
#include "../dom/drumset.h"
#include "../dom/instrument.h"
#include "../dom/part.h"
#include "../dom/score.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct InsertPart : UndoableCommand {
private:

    Part *part_ = nullptr;
    size_t target_part_idx_ = 0;

public:
    InsertPart(Part *p, size_t target_part_idx);
    void undo() override;
    void redo() override;
    void cleanup(bool) override;

    UNDO_TYPE(CommandType::INSERT_PART)
    UNDO_NAME("InsertPart")
    UNDO_CHANGED_OBJECTS({part_})
};

struct RemovePart : UndoableCommand {
private:

    Part *part_ = nullptr;
    size_t part_idx_ = muse::nidx;

public:
    RemovePart(Part *, size_t part_idx);
    void undo() override;
    void redo() override;
    void cleanup(bool) override;

    UNDO_TYPE(CommandType::REMOVE_PART)
    UNDO_NAME("RemovePart")
    UNDO_CHANGED_OBJECTS({part_})
};

struct SetSoloist : UndoableCommand {
private:

    Part *part = nullptr;
    bool soloist = false;

public:
    SetSoloist(Part *p, bool b);
    void undo() override;
    void redo() override;

    UNDO_TYPE(CommandType::SET_SOLOIST)
    UNDO_NAME("SetSoloist")
    UNDO_CHANGED_OBJECTS({part})
};

struct ChangePart : UndoableCommand {
private:

    Part *part = nullptr;
    Instrument *instrument = nullptr;

    void flip() override;

public:
    ChangePart(Part *, Instrument *);
    void cleanup(bool) override;

    UNDO_TYPE(CommandType::CHANGE_PART)
    UNDO_NAME("ChangePart")
    UNDO_CHANGED_OBJECTS({part})
};

struct ChangeInstrumentLong : UndoableCommand {
private:

    Part *part = nullptr;
    Fraction tick;
    String long_name;

    void flip() override;

public:
    ChangeInstrumentLong(const Fraction &, Part *, const String &);

    UNDO_TYPE(CommandType::CHANGE_INSTRUMENT_LONG)
    UNDO_NAME("ChangeInstrumentLong")
    UNDO_CHANGED_OBJECTS({part})
};

struct ChangeInstrumentShort : UndoableCommand {
private:

    Part *part = nullptr;
    Fraction tick;
    String short_name;

    void flip() override;

public:
    ChangeInstrumentShort(const Fraction &, Part *, const String &);

    UNDO_TYPE(CommandType::CHANGE_INSTRUMENT_SHORT)
    UNDO_NAME("ChangeInstrumentShort")
    UNDO_CHANGED_OBJECTS({part})
};

struct ChangeInstrumentGroupOptions : UndoableCommand {
private:

    Part *part = nullptr;
    Fraction tick;
    bool use_custom;
    String long_name;
    String short_name;

    void flip() override;

public:
    ChangeInstrumentGroupOptions(const Fraction &, Part *, bool, const String &, const String &);

    UNDO_TYPE(CommandType::CHANGE_INSTRUMENT_GROUP_OPTIONS)
    UNDO_NAME("ChangeInstrumentGroupOptions")
    UNDO_CHANGED_OBJECTS({part})
};

struct ChangeInstrumentNumber : UndoableCommand {
private:

    Part *part = nullptr;
    Fraction tick;
    int number;

    void flip() override;

public:
    ChangeInstrumentNumber(const Fraction &, Part *, int v);

    UNDO_TYPE(CommandType::CHANGE_INSTRUMENT_NUMBER)
    UNDO_NAME("ChangeInstrumentNumber")
    UNDO_CHANGED_OBJECTS({part})
};

struct ChangeDrumset : UndoableCommand {
private:

    Instrument *instrument = nullptr;
    Drumset drumset;
    Part *part = nullptr;

    void flip() override;

public:
    ChangeDrumset(Instrument *i, const Drumset &d, Part *p) : instrument(i), drumset(d), part(p) {}

    UNDO_TYPE(CommandType::CHANGE_DRUMSET)
    UNDO_NAME("ChangeDrumset")
};

struct ChangeStringData : UndoableCommand {
private:
    Instrument *instrument_ = nullptr;
    StringTunings *string_tunings_ = nullptr;
    StringData string_data_;

public:
    ChangeStringData(StringTunings *string_tunings, const StringData &string_data)
        : string_tunings_(string_tunings), string_data_(string_data) {}

    ChangeStringData(Instrument *instrument, const StringData &string_data) : instrument_(instrument), string_data_(string_data) {}

    void flip() override;
    UNDO_NAME("ChangeStringData")
};

struct ChangePatch : UndoableCommand {
private:

    Score *score = nullptr;
    InstrChannel *channel = nullptr;
    MidiPatch patch;

    void flip() override;

public:
    ChangePatch(Score *s, InstrChannel *c, const MidiPatch &pt) : score(s), channel(c), patch(pt) {}
    UNDO_NAME("ChangePatch")
    UNDO_CHANGED_OBJECTS({score})
};

struct SetUserBankController : UndoableCommand {
private:

    InstrChannel *channel = nullptr;
    bool val = false;

    void flip() override;

public:
    SetUserBankController(InstrChannel *c, bool v) : channel(c), val(v) {}
    UNDO_NAME("SetUserBankController")
};

struct InstrumentTemplate;
struct ScoreOrder;
struct Staff;
struct StaffType;
enum class PreferSharpFlat : char;
enum class StaffTypes : signed char;

struct EditPart {
    static void replace_part_instrument(Score *score, Part *part, const Instrument &new_instrument,
                                        const StaffType *new_staff_type = nullptr);

    static bool replace_instrument_at_tick(Score *score, Part *part, const Fraction &tick, const Instrument &new_instrument);

    static void set_part_visible(Score *score, Part *part, bool visible);
    static void set_staff_visible(Score *score, Staff *staff, bool visible);
    static void set_part_sharp_flat(Score *score, Part *part, PreferSharpFlat sharp_flat);
    static void set_instrument_name(Score *score, Part *part, const Fraction &tick, const String &name);
    static void set_instrument_abbreviature(Score *score, Part *part, const Fraction &tick, const String &abbreviature);
    static void set_instrument_group_name_options(Score *score, Part *part, const Fraction &tick, bool use_custom, const String &long_name,
                                                  const String &short_name);
    static void set_instrument_custom_group_abbreviature(Score *score, Part *part, const Fraction &tick, const String &abbreviature);
    static void set_staff_type(Score *score, Staff *staff, StaffTypes type_id);

    static void remove_parts(Score *score, const std::vector<Part *> &parts);
    static void remove_staves(Score *score, const std::vector<Staff *> &staves);
    static void move_parts(Score *score, const std::vector<Part *> &source_parts, Part *destination_part, bool insert_after);
    static void move_staves(Score *score, const std::vector<Staff *> &source_staves, Staff *destination_staff, bool insert_after);

    static void add_system_objects(Score *score, const std::vector<Staff *> &staves);
    static void remove_system_objects(Score *score, const std::vector<Staff *> &staves);
    static void move_system_objects(Score *score, Staff *source_staff, Staff *destination_staff);

    static Staff *append_staff(Score *score, Part *destination_part);
    static Staff *append_linked_staff(Score *score, Staff *source_staff, Part *destination_part);

    static bool set_voice_visible(Score *score, Staff *staff, int voice_index, bool visible);

    static void replace_drumset(Score *score, Part *part, const Fraction &tick, const Drumset &new_drumset);

    static void insert_part(Score *score, const InstrumentTemplate *templ, size_t index);
    static void replace_part(Score *score, Part *old_part, const InstrumentTemplate *templ);

    static void set_score_order(Score *score, const ScoreOrder &order);

private:
    static void do_append_staff(Score *score, Staff *staff, Part *destination_part, bool create_rests = true);
};
} // namespace mu::engraving
