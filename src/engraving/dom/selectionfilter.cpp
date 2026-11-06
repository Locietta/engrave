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

#include "log.h"
#include "selectionfilter.h"
#include "tuplet.h"

using namespace mu::engraving;

struct FilterParams {
    unsigned int other_filter = 0;
    unsigned int none_mask = 0;
    unsigned int all_mask = 0;
};

static bool is_filtered_impl(const unsigned int &current_filter, const FilterParams &params) {
    if (params.other_filter == params.none_mask || params.other_filter == params.all_mask) {
        return current_filter == params.other_filter;
    }
    return current_filter & params.other_filter;
}

static void set_filtered_impl(unsigned int &current_filter, FilterParams &params, bool filtered) {
    if (params.other_filter == params.none_mask) {
        params.other_filter = params.all_mask;
        set_filtered_impl(current_filter, params, !filtered);
        return;
    }
    filtered ? current_filter |= params.other_filter : current_filter &= ~params.other_filter;
}

bool SelectionFilter::is_filtered(const SelectionFilterTypesVariant &variant) const {
    switch (variant.index()) {
        case 0: {
            const VoicesSelectionFilterTypes type = std::get<VoicesSelectionFilterTypes>(variant);
            const FilterParams params{/*otherFilter*/ static_cast<unsigned int>(type),
                                      /*noneMask*/ static_cast<unsigned int>(VoicesSelectionFilterTypes::NONE),
                                      /*allMask*/ static_cast<unsigned int>(VoicesSelectionFilterTypes::ALL)};
            return is_filtered_impl(filtered_voices_types_, params);
        }
        case 1: {
            const NotesInChordSelectionFilterTypes type = std::get<NotesInChordSelectionFilterTypes>(variant);
            const FilterParams params{/*otherFilter*/ static_cast<unsigned int>(type),
                                      /*noneMask*/ static_cast<unsigned int>(NotesInChordSelectionFilterTypes::NONE),
                                      /*allMask*/ static_cast<unsigned int>(NotesInChordSelectionFilterTypes::ALL)};
            return is_filtered_impl(filtered_notes_in_chord_types_, params);
        }
        case 2: {
            const ElementsSelectionFilterTypes type = std::get<ElementsSelectionFilterTypes>(variant);
            const FilterParams params{/*otherFilter*/ static_cast<unsigned int>(type),
                                      /*noneMask*/ static_cast<unsigned int>(ElementsSelectionFilterTypes::NONE),
                                      /*allMask*/ static_cast<unsigned int>(ElementsSelectionFilterTypes::ALL)};
            return is_filtered_impl(filtered_elements_types_, params);
        }
        default: break;
    }

    UNREACHABLE;
    return true;
}

void SelectionFilter::set_filtered(const SelectionFilterTypesVariant &variant, bool filtered) {
    switch (variant.index()) {
        case 0: {
            const VoicesSelectionFilterTypes type = std::get<VoicesSelectionFilterTypes>(variant);
            FilterParams params{/*otherFilter*/ static_cast<unsigned int>(type),
                                /*noneMask*/ static_cast<unsigned int>(VoicesSelectionFilterTypes::NONE),
                                /*allMask*/ static_cast<unsigned int>(VoicesSelectionFilterTypes::ALL)};
            set_filtered_impl(filtered_voices_types_, params, filtered);
            return;
        }
        case 1: {
            const NotesInChordSelectionFilterTypes type = std::get<NotesInChordSelectionFilterTypes>(variant);
            FilterParams params{/*otherFilter*/ static_cast<unsigned int>(type),
                                /*noneMask*/ static_cast<unsigned int>(NotesInChordSelectionFilterTypes::NONE),
                                /*allMask*/ static_cast<unsigned int>(NotesInChordSelectionFilterTypes::ALL)};
            set_filtered_impl(filtered_notes_in_chord_types_, params, filtered);
            return;
        }
        case 2: {
            const ElementsSelectionFilterTypes type = std::get<ElementsSelectionFilterTypes>(variant);
            FilterParams params{/*otherFilter*/ static_cast<unsigned int>(type),
                                /*noneMask*/ static_cast<unsigned int>(ElementsSelectionFilterTypes::NONE),
                                /*allMask*/ static_cast<unsigned int>(ElementsSelectionFilterTypes::ALL)};
            set_filtered_impl(filtered_elements_types_, params, filtered);
            return;
        }
        default: break;
    }

    UNREACHABLE;
}

bool SelectionFilter::can_select(const EngravingItem *e) const {
    IF_ASSERT_FAILED(!e->is_note() && !e->is_tuplet()) {
        LOGE() << "Using canSelect on a Note or Tuplet - use canSelectNoteIdx or canSelectTuplet instead";
        return true;
    }

    switch (e->type()) {
        case ElementType::DYNAMIC: return is_filtered(ElementsSelectionFilterTypes::DYNAMIC);
        case ElementType::HAIRPIN:
        case ElementType::HAIRPIN_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::HAIRPIN);
        case ElementType::ARTICULATION:
        case ElementType::TAPPING:
        case ElementType::VIBRATO:
        case ElementType::VIBRATO_SEGMENT:
        case ElementType::FERMATA: return is_filtered(ElementsSelectionFilterTypes::ARTICULATION);
        case ElementType::ORNAMENT:
        case ElementType::TRILL:
        case ElementType::TRILL_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::ORNAMENT);
        case ElementType::LYRICS:
        case ElementType::LYRICSLINE:
        case ElementType::LYRICSLINE_SEGMENT:
        case ElementType::PARTIAL_LYRICSLINE:
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::LYRICS);
        case ElementType::FINGERING: return is_filtered(ElementsSelectionFilterTypes::FINGERING);
        case ElementType::HARMONY: return is_filtered(ElementsSelectionFilterTypes::CHORD_SYMBOL);
        case ElementType::SLUR:
        case ElementType::SLUR_SEGMENT:
        case ElementType::HAMMER_ON_PULL_OFF:
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::SLUR);
        case ElementType::TIE:
        case ElementType::TIE_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::TIE);
        case ElementType::FIGURED_BASS: return is_filtered(ElementsSelectionFilterTypes::FIGURED_BASS);
        case ElementType::OTTAVA:
        case ElementType::OTTAVA_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::OTTAVA);
        case ElementType::PEDAL:
        case ElementType::PEDAL_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::PEDAL_LINE);
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET: return is_filtered(ElementsSelectionFilterTypes::ARPEGGIO);
        case ElementType::GLISSANDO:
        case ElementType::GLISSANDO_SEGMENT: return is_filtered(ElementsSelectionFilterTypes::GLISSANDO);
        case ElementType::FRET_DIAGRAM: return is_filtered(ElementsSelectionFilterTypes::FRET_DIAGRAM);
        case ElementType::BREATH: return is_filtered(ElementsSelectionFilterTypes::BREATH);
        case ElementType::TREMOLO_SINGLECHORD:
        case ElementType::TREMOLO_TWOCHORD: return is_filtered(ElementsSelectionFilterTypes::TREMOLO);
        default: break;
    }

    // Special cases...
    if (e->is_text_base()) {
        return is_filtered(ElementsSelectionFilterTypes::OTHER_TEXT);
    }

    if (e->is_s_line() || e->is_s_line_segment()) { // NoteLine, Volta
        return is_filtered(ElementsSelectionFilterTypes::OTHER_LINE);
    }

    if (e->is_chord() && to_chord(e)->is_grace()) {
        return is_filtered(ElementsSelectionFilterTypes::GRACE_NOTE);
    }

    return true;
}

bool SelectionFilter::can_select_note_idx(size_t note_idx, size_t total_notes_in_chord, bool selection_contains_multi_note_chords) const {
    if (total_notes_in_chord == 1) {
        //! NOTE: Always include single notes when the selection consists solely of single notes...
        return include_single_notes_ || !selection_contains_multi_note_chords;
    }

    NotesInChordSelectionFilterTypes type = NotesInChordSelectionFilterTypes::NONE;
    switch (note_idx) {
        case 0: type = NotesInChordSelectionFilterTypes::BOTTOM_NOTE; break;
        case 1: type = NotesInChordSelectionFilterTypes::SECOND_NOTE; break;
        case 2: type = NotesInChordSelectionFilterTypes::THIRD_NOTE; break;
        case 3: type = NotesInChordSelectionFilterTypes::FOURTH_NOTE; break;
        case 4: type = NotesInChordSelectionFilterTypes::FIFTH_NOTE; break;
        case 5: type = NotesInChordSelectionFilterTypes::SIXTH_NOTE; break;
        case 6: type = NotesInChordSelectionFilterTypes::SEVENTH_NOTE; break;
        default: break;
    }

    //! NOTE: Everything above "normal" we handle as top notes...
    const bool idx_is_normal = note_idx < ::mu::engraving::num_notes_in_chord_selection_filter_types - 1;
    if (note_idx == total_notes_in_chord - 1 || !idx_is_normal) {
        return is_filtered(NotesInChordSelectionFilterTypes::TOP_NOTE) || (idx_is_normal && is_filtered(type));
    }

    IF_ASSERT_FAILED(type != NotesInChordSelectionFilterTypes::NONE) { return true; }

    return is_filtered(type);
}

bool SelectionFilter::can_select_tuplet(const Tuplet *tuplet, const Fraction &selection_range_start, const Fraction &selection_range_end,
                                        bool selection_contains_multi_note_chords) const {
    // Tuplets are selectable if all of their contained elements are selectable...
    for (const DurationElement *element : tuplet->elements()) {
        if (element->tick() < selection_range_start || element->tick() >= selection_range_end) {
            return false;
        }
        switch (element->type()) {
            case ElementType::CHORD: {
                const std::vector<Note *> notes = to_chord(element)->notes();
                for (size_t note_idx = 0; note_idx < notes.size(); ++note_idx) {
                    if (!can_select_note_idx(note_idx, notes.size(), selection_contains_multi_note_chords)) {
                        return false;
                    }
                }
                break;
            }
            case ElementType::REST:
                if (!can_select(element)) {
                    return false;
                }
                break;
            case ElementType::TUPLET: {
                // Recursive call...
                if (!can_select_tuplet(to_tuplet(element), selection_range_start, selection_range_end,
                                       selection_contains_multi_note_chords)) {
                    return false;
                }
                break;
            }
            default: {
                UNREACHABLE;
                break;
            }
        }
    }
    return true;
}

bool SelectionFilter::can_select_voice(track_idx_t track) const {
    voice_idx_t voice = track2voice(track);
    switch (voice) {
        case 0: return is_filtered(VoicesSelectionFilterTypes::FIRST_VOICE);
        case 1: return is_filtered(VoicesSelectionFilterTypes::SECOND_VOICE);
        case 2: return is_filtered(VoicesSelectionFilterTypes::THIRD_VOICE);
        case 3: return is_filtered(VoicesSelectionFilterTypes::FOURTH_VOICE);
    }
    return true;
}
