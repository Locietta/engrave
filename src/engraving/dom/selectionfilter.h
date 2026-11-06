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

#include <variant>

#include "mscore.h"
#include "chord.h"

namespace mu::engraving {
static constexpr size_t num_voices_selection_filter_types = 4;
enum class VoicesSelectionFilterTypes : unsigned int {
    NONE = 0,
    FIRST_VOICE = 1 << 0,
    SECOND_VOICE = 1 << 1,
    THIRD_VOICE = 1 << 2,
    FOURTH_VOICE = 1 << 3,
    ALL = ~(~0u << ::mu::engraving::num_voices_selection_filter_types)
};

static constexpr size_t num_notes_in_chord_selection_filter_types = 8;
enum class NotesInChordSelectionFilterTypes : unsigned int {
    NONE = 0,
    TOP_NOTE = 1 << 0,
    SEVENTH_NOTE = 1 << 1,
    SIXTH_NOTE = 1 << 2,
    FIFTH_NOTE = 1 << 3,
    FOURTH_NOTE = 1 << 4,
    THIRD_NOTE = 1 << 5,
    SECOND_NOTE = 1 << 6,
    BOTTOM_NOTE = 1 << 7,
    ALL = ~(~0u << ::mu::engraving::num_notes_in_chord_selection_filter_types)
};

static constexpr size_t num_elements_selection_filter_types = 20;
enum class ElementsSelectionFilterTypes : unsigned int {
    NONE = 0,
    DYNAMIC = 1 << 0,
    HAIRPIN = 1 << 1,
    FINGERING = 1 << 2,
    LYRICS = 1 << 3,
    CHORD_SYMBOL = 1 << 4,
    OTHER_TEXT = 1 << 5,
    ARTICULATION = 1 << 6,
    ORNAMENT = 1 << 7,
    SLUR = 1 << 8,
    TIE = 1 << 9,
    FIGURED_BASS = 1 << 10,
    OTTAVA = 1 << 11,
    PEDAL_LINE = 1 << 12,
    OTHER_LINE = 1 << 13,
    ARPEGGIO = 1 << 14,
    GLISSANDO = 1 << 15,
    FRET_DIAGRAM = 1 << 16,
    BREATH = 1 << 17,
    TREMOLO = 1 << 18,
    GRACE_NOTE = 1 << 19,
    ALL = ~(~0u << ::mu::engraving::num_elements_selection_filter_types)
};

using SelectionFilterTypesVariant =
    std::variant<VoicesSelectionFilterTypes, NotesInChordSelectionFilterTypes, ElementsSelectionFilterTypes>;

struct SelectionFilter {
    SelectionFilter() = default;

    inline bool operator==(const SelectionFilter &f) const {
        return filtered_voices_types_ == f.filtered_voices_types_ && filtered_notes_in_chord_types_ == f.filtered_notes_in_chord_types_ &&
               filtered_elements_types_ == f.filtered_elements_types_;
    }

    inline bool operator!=(const SelectionFilter &f) const { return !this->operator==(f); }

    bool is_filtered(const SelectionFilterTypesVariant &variant) const;
    void set_filtered(const SelectionFilterTypesVariant &variant, bool filtered);

    bool can_select(const EngravingItem *element) const;
    bool can_select_note_idx(size_t note_idx, size_t total_notes_in_chord, bool selection_contains_multi_note_chords) const;
    bool can_select_tuplet(const Tuplet *tuplet, const Fraction &selection_range_start, const Fraction &selection_range_end,
                           bool selection_contains_multi_note_chords) const;
    bool can_select_voice(track_idx_t track) const;

    //! IMPORTANT NOTE: This flag should only ever be used for selections that contain multi-note Chords. It should always be
    //! ignored in selections that exclusively consist of single notes...
    bool include_single_notes() const { return include_single_notes_; }

    void set_include_single_notes(bool include) { include_single_notes_ = include; }

private:
    unsigned int filtered_voices_types_ = static_cast<unsigned int>(VoicesSelectionFilterTypes::ALL);
    unsigned int filtered_notes_in_chord_types_ = static_cast<unsigned int>(NotesInChordSelectionFilterTypes::ALL);
    unsigned int filtered_elements_types_ = static_cast<unsigned int>(ElementsSelectionFilterTypes::ALL);

    bool include_single_notes_ = true;
};
} // namespace mu::engraving
