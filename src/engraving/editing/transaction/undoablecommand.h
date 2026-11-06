/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <optional>
#include <vector>

#include "engraving/types/types.h"

namespace mu::engraving {
struct EngravingItem;
struct EngravingObject;

enum class CommandType : signed char {
    UNKNOWN = -1,

    // Parts
    INSERT_PART,
    REMOVE_PART,
    ADD_PART_TO_EXCERPT,
    SET_SOLOIST,
    CHANGE_PART,
    CONNECT_SHARED_PART,
    DISCONNECT_SHARED_PART,

    // Staves
    INSERT_STAFF,
    REMOVE_STAFF,
    ADD_SYSTEM_OBJECT_STAFF,
    REMOVE_SYSTEM_OBJECT_STAFF,
    SORT_STAVES,
    CHANGE_STAFF,
    CHANGE_STAFF_TYPE,

    // MStaves
    INSERT_M_STAFF,
    REMOVE_M_STAFF,
    INSERT_STAVES,
    REMOVE_STAVES,
    CHANGE_M_STAFF_PROPERTIES,
    CHANGE_M_STAFF_HIDE_IF_EMPTY,

    // Instruments
    CHANGE_INSTRUMENT_SHORT,
    CHANGE_INSTRUMENT_LONG,
    CHANGE_INSTRUMENT_GROUP_OPTIONS,
    CHANGE_INSTRUMENT_NUMBER,
    CHANGE_INSTRUMENT,
    CHANGE_DRUMSET,

    // Measures
    REMOVE_MEASURES,
    INSERT_MEASURES,
    CHANGE_MEASURE_LEN,
    CHANGE_MM_REST,
    CHANGE_MEASURE_REPEAT_COUNT,

    // Elements
    ADD_ELEMENT,
    REMOVE_ELEMENT,
    UNLINK,
    LINK,
    CHANGE_ELEMENT,
    CHANGE_PARENT,

    // Notes
    CHANGE_PITCH,
    CHANGE_FRETTING,
    CHANGE_VELOCITY,

    // ChordRest
    CHANGE_CHORD_STAFF_MOVE,
    SWAP_CR,
    ADD_NOTE_PARENTHESES_INFO,
    REMOVE_NOTE_PARENTHESES_INFO,
    REMOVE_SINGLE_NOTE_PARENTHESES,

    // Brackets
    REMOVE_BRACKET,
    ADD_BRACKET,

    // Fret
    FRET_DATA_CHANGE,
    FRET_DOT,
    FRET_MARKER,
    FRET_BARRE,
    FRET_CLEAR,
    ADD_FRET_DIAGRAM_TO_FRET_BOX,
    REMOVE_FRET_DIAGRAM_FROM_FRET_BOX,

    // Harmony
    TRANSPOSE_HARMONY,

    // KeySig
    CHANGE_KEY_SIG,

    // Clef
    CHANGE_CLEF_TYPE,

    // Tremolo
    MOVE_TREMOLO,

    // Spanners
    CHANGE_SPANNER_ELEMENTS,
    INSERT_TIME_UNMANAGED_SPANNER,
    CHANGE_START_END_SPANNER,

    // Ties
    CHANGE_TIE_END_POINT_ACTIVE,

    // Style
    CHANGE_STYLE,
    CHANGE_STYLE_VALUES,

    // Property
    CHANGE_PROPERTY,

    // Voices
    EXCHANGE_VOICE,

    // Excerpts
    ADD_EXCERPT,
    REMOVE_EXCERPT,
    SWAP_EXCERPT,
    CHANGE_EXCERPT_TITLE,

    // Meta info
    CHANGE_META_INFO,

    // Text
    TEXT_EDIT,

    // Automation
    EDIT_AUTOMATION_POINTS,

    // Other
    INSERT_TIME,
    CHANGE_SCORE_ORDER,
};

enum class UndoableCommandFilter : unsigned char {
    TEXT_EDIT,
    ADD_ELEMENT,
    ADD_ELEMENT_LINKED,
    LINK,
    REMOVE_ELEMENT,
    REMOVE_ELEMENT_LINKED,
    CHANGE_PROPERTY_LINKED,
};

#define UNDO_TYPE(t) \
    CommandType type() const override { return t; }
#define UNDO_NAME(a) \
    const char *name() const override { return a; }
#define UNDO_CHANGED_OBJECTS(...) \
    std::vector<EngravingObject *> object_items() const override { return __VA_ARGS__; }

struct ChangedRange {
    Fraction tick_from;
    Fraction tick_to;
    staff_idx_t staff_idx_from = muse::nidx;
    staff_idx_t staff_idx_to = muse::nidx;
};

struct UndoableCommand {
    virtual ~UndoableCommand() = default;

    virtual void undo();
    virtual void redo();

    virtual void cleanup(bool /*wasDone*/) {}

    virtual std::vector<EngravingObject *> object_items() const { return {}; }

    virtual const char *name() const { return "UndoableCommand"; }

    virtual CommandType type() const { return CommandType::UNKNOWN; }

    virtual std::optional<ChangedRange> changed_range() const { return std::nullopt; }

    virtual bool matches_filter(UndoableCommandFilter, const EngravingItem * /* target */) const { return false; }

protected:
    virtual void flip() {}
};

std::vector<EngravingObject *> compound_objects(EngravingObject *object);
} // namespace mu::engraving
