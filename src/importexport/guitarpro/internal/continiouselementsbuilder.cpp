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
#include "continiouselementsbuilder.h"

#include "engraving/dom/chordrest.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/score.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/trill.h"
#include "engraving/dom/vibrato.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
ContiniousElementsBuilder::ContiniousElementsBuilder(Score *score) : score_(score) {}

static mu::engraving::ElementType mu_type_from_import_type(ContiniousElementsBuilder::ImportType import_type) {
    using import_t = ContiniousElementsBuilder::ImportType;

    switch (import_type) {
        case import_t::LET_RING: return ElementType::LET_RING;
        case import_t::PALM_MUTE: return ElementType::PALM_MUTE;
        case import_t::RASGUEADO: return ElementType::RASGUEADO;
        case import_t::PICK_SCRAPE: return ElementType::PICK_SCRAPE;

        case import_t::HARMONIC_ARTIFICIAL:
        case import_t::HARMONIC_PINCH:
        case import_t::HARMONIC_TAP:
        case import_t::HARMONIC_SEMI:
        case import_t::HARMONIC_FEEDBACK: return ElementType::HARMONIC_MARK;

        case import_t::OTTAVA_MA15:
        case import_t::OTTAVA_VA8:
        case import_t::OTTAVA_VB8:
        case import_t::OTTAVA_MB15: return ElementType::OTTAVA;

        case import_t::TRILL: return ElementType::TRILL;
        case import_t::HAMMER_ON_PULL_OFF: return ElementType::HAMMER_ON_PULL_OFF;

        case import_t::HAIRPIN_CRESCENDO:
        case import_t::HAIRPIN_DIMINUENDO: return ElementType::HAIRPIN;

        case import_t::VIBRATO_LEFT_HAND_SLIGHT:
        case import_t::VIBRATO_LEFT_HAND_WIDE:
        case import_t::VIBRATO_W_TREM_BAR_SLIGHT:
        case import_t::VIBRATO_W_TREM_BAR_WIDE: return ElementType::VIBRATO;

        case import_t::NONE: return ElementType::INVALID;
    }

    return ElementType::INVALID;
}

static String harmonic_text(ContiniousElementsBuilder::ImportType type) {
    static const std::unordered_map<ContiniousElementsBuilder::ImportType, String> names{
        {ContiniousElementsBuilder::ImportType::HARMONIC_ARTIFICIAL, u"AH"}, {ContiniousElementsBuilder::ImportType::HARMONIC_PINCH, u"PH"},
        {ContiniousElementsBuilder::ImportType::HARMONIC_TAP, u"TH"},        {ContiniousElementsBuilder::ImportType::HARMONIC_SEMI, u"SH"},
        {ContiniousElementsBuilder::ImportType::HARMONIC_FEEDBACK, u"Fdbk"},
    };

    auto it = names.find(type);
    if (it != names.end()) {
        return it->second;
    }

    LOGE() << "wrong harmonic type";
    return String();
}

static mu::engraving::VibratoType vibrato_type_from_import_type(ContiniousElementsBuilder::ImportType type) {
    static const std::unordered_map<ContiniousElementsBuilder::ImportType, VibratoType> names{
        {ContiniousElementsBuilder::ImportType::VIBRATO_LEFT_HAND_SLIGHT, VibratoType::GUITAR_VIBRATO},
        {ContiniousElementsBuilder::ImportType::VIBRATO_LEFT_HAND_WIDE, VibratoType::GUITAR_VIBRATO_WIDE},
        {ContiniousElementsBuilder::ImportType::VIBRATO_W_TREM_BAR_SLIGHT, VibratoType::VIBRATO_SAWTOOTH},
        {ContiniousElementsBuilder::ImportType::VIBRATO_W_TREM_BAR_WIDE, VibratoType::VIBRATO_SAWTOOTH_WIDE}};

    auto it = names.find(type);
    if (it != names.end()) {
        return it->second;
    }

    LOGE() << "wrong vibrato type";
    return VibratoType::GUITAR_VIBRATO;
}

static std::pair<bool, mu::engraving::OttavaType> ottava_type(ContiniousElementsBuilder::ImportType type) {
    static const std::unordered_map<ContiniousElementsBuilder::ImportType, mu::engraving::OttavaType> types{
        {ContiniousElementsBuilder::ImportType::OTTAVA_VA8, mu::engraving::OttavaType::OTTAVA_8VA},
        {ContiniousElementsBuilder::ImportType::OTTAVA_VB8, mu::engraving::OttavaType::OTTAVA_8VB},
        {ContiniousElementsBuilder::ImportType::OTTAVA_MA15, mu::engraving::OttavaType::OTTAVA_15MA},
        {ContiniousElementsBuilder::ImportType::OTTAVA_MB15, mu::engraving::OttavaType::OTTAVA_15MB}};

    auto it = types.find(type);
    if (it != types.end()) {
        return {true, it->second};
    }

    return {false, mu::engraving::OttavaType::OTTAVA_8VA};
}

/// whenever the continious line elements should end on rest or rest should be included in it
static bool should_split_by_rests(mu::engraving::ElementType mu_type) {
    switch (mu_type) {
        case ElementType::PICK_SCRAPE:
        case ElementType::TRILL:
        case ElementType::HARMONIC_MARK:
        case ElementType::VIBRATO:
        case ElementType::HAMMER_ON_PULL_OFF: return true;

        default: return false;
    }
}

void ContiniousElementsBuilder::build_continious_element(ChordRest *cr, ElementType mu_type, ImportType import_type, bool elem_exists,
                                                         sub_type_t sub_type) {
    auto set_start_cr = [](Spanner *elem, ChordRest *cr) {
        elem->set_tick(cr->tick());
        elem->set_start_element(cr);
    };

    auto set_end_cr = [](Spanner *elem, ChordRest *cr) {
        elem->set_tick2(cr->end_tick());
        elem->set_end_element(cr);
    };

    bool split_by_rests = should_split_by_rests(mu_type);

    track_idx_t track = cr->track();

    auto &elements = elements_by_type_[import_type];
    while (elements.size() < track + 1) {
        elements.push_back(nullptr);
    }

    auto &elem = elements[track];
    auto &last_type_for_track = last_import_types_[mu_type][track][sub_type];

    ContiniousElementState state = calculate_state(cr->is_rest(), elem_exists, split_by_rests, last_type_for_track != import_type);

    /// handling the continious element according to its state
    switch (state) {
        case ContiniousElementState::CHORD_NO_ELEMENT:
            last_type_for_track = ImportType::NONE;
            elem = nullptr;
            break;

        case ContiniousElementState::REST_BREAK:
            last_type_for_track = ImportType::NONE;
            elem = nullptr;
            break;

        case ContiniousElementState::REST_CONTINUE:
        case ContiniousElementState::
            ELEMENT_ON_REST: // currently it's mistake: we cannot indicate Rest with any of continious elements in guitar pro
            if (last_type_for_track != ImportType::NONE) {
                elements_to_add_to_score_[track][last_type_for_track].ended_on_rest = true;
            }

            break;

        case ContiniousElementState::CONTINUE_CURRENT_LINE: {
            ChordRest *last_cr = elem->end_cr();
            if (last_cr == cr) {
                break;
            }

            if (elem->tick2() < cr->tick()) {
                if (last_type_for_track != ImportType::NONE) {
                    auto &last_type_elements_to_add = elements_to_add_to_score_[track][last_type_for_track];

                    /// removing info about the Rest and updating last element's ticks
                    if (last_type_elements_to_add.ended_on_rest) {
                        last_type_elements_to_add.ended_on_rest = false;
                        Spanner *prev_elem = last_type_elements_to_add.elements.back();
                        if (!prev_elem) {
                            LOGE() << "error while importing";
                            return;
                        }

                        elem = prev_elem;
                        set_end_cr(elem, cr);
                    } else {
                        // Simile mark case
                        set_end_cr(elem, cr);
                    }
                }
            } else {
                set_end_cr(elem, cr);
            }

            break;
        }

        case ContiniousElementState::CREATE_NEW_ITEM: {
            EngravingItem *eng_item = Factory::create_item(mu_type, score_->dummy());

            Spanner *new_elem = dynamic_cast<Spanner *>(eng_item);
            IF_ASSERT_FAILED(new_elem) { return; }

            elem = new_elem;

            set_start_cr(new_elem, cr);
            set_end_cr(new_elem, cr);

            new_elem->set_track(track);
            new_elem->set_track2(track);

            elements_to_add_to_score_[track][import_type].elements.push_back(new_elem);
            ordered_added_elements_.push_back(new_elem);
            last_type_for_track = import_type;
            break;
        }

        default: break;
    }

    if (!cr->is_chord()) {
        return;
    }

    setup_added_element(cr->track(), import_type);
}

void ContiniousElementsBuilder::notify_uncompleted_measure() {
    for (auto &track_maps : elements_to_add_to_score_) {
        for (auto &type_maps : track_maps.second) {
            type_maps.second.ended_on_rest = true;
        }
    }
}

static Chord *search_end_chord(Chord *start_chord) {
    ChordRest *next_cr = nullptr;
    if (start_chord->is_grace()) {
        //! this case when start note is a grace note so end note can be next note in grace notes
        //! or parent note of grace notes
        Chord *parent_grace = to_chord(start_chord->parent());

        auto it = parent_grace->grace_notes().begin();
        for (; it != parent_grace->grace_notes().end(); ++it) {
            if (*it == start_chord) {
                break;
            }
        }

        if (it == parent_grace->grace_notes().end()) {
            next_cr = nullptr;
        } else if (std::next(it) == parent_grace->grace_notes().end()) {
            next_cr = parent_grace;
        } else {
            next_cr = *(++it);
        }
    } else {
        next_cr = start_chord->segment()->next1()->next_chord_rest(start_chord->track());
        if (!next_cr) {
            return nullptr;
        }

        if (next_cr->is_chord() && !to_chord(next_cr)->grace_notes().empty()) {
            next_cr = to_chord(next_cr)->grace_notes().front();
        }
    }

    return (next_cr && next_cr->is_chord()) ? to_chord(next_cr) : nullptr;
}

void ContiniousElementsBuilder::add_elements_to_score() {
    for (auto &track_maps : spanners_without_end_element_) {
        for (auto &type_maps : track_maps.second) {
            for (Spanner *elem : type_maps.second) {
                elem->set_end_element(search_end_chord(elem->end_chord()));
            }
        }
    }

    for (Spanner *elem : ordered_added_elements_) {
        score_->add_element(elem);
    }
}

/// indicating the type of behaviour for continious element
ContiniousElementsBuilder::ContiniousElementState
ContiniousElementsBuilder::calculate_state(bool is_rest, bool elem_exists, bool split_by_rests, bool import_type_changed) const {
    ContiniousElementState state = ContiniousElementState::UNDEFINED;

    if (is_rest && split_by_rests) {
        state = ContiniousElementState::REST_BREAK;
    } else {
        if (!elem_exists) {
            if (!is_rest) {
                state = ContiniousElementState::CHORD_NO_ELEMENT;
            } else {
                state = ContiniousElementState::REST_CONTINUE;
            }
        } else {
            // element exists on current beat
            if (is_rest) {
                state = ContiniousElementState::ELEMENT_ON_REST;
            } else if (import_type_changed) {
                state = ContiniousElementState::CREATE_NEW_ITEM;
            } else {
                state = ContiniousElementState::CONTINUE_CURRENT_LINE;
            }
        }
    }

    return state;
}

void ContiniousElementsBuilder::setup_added_element(track_idx_t track_idx, ImportType import_type) {
    ElementType mu_type = mu_type_from_import_type(import_type);

    Spanner *line_elem = elements_by_type_[import_type][track_idx];
    if (!line_elem) {
        return;
    }

    switch (mu_type) {
        case ElementType::TRILL: {
            if (line_elem->is_trill()) {
                to_trill(line_elem)->set_trill_type(TrillType::TRILL_LINE);
            }

            break;
        }
        case ElementType::HAIRPIN: {
            if (line_elem->is_hairpin()) {
                to_hairpin(line_elem)->set_hairpin_type(import_type == ImportType::HAIRPIN_CRESCENDO ? HairpinType::CRESC_HAIRPIN :
                                                                                                       HairpinType::DIM_HAIRPIN);
            }

            break;
        }
        case ElementType::HARMONIC_MARK: {
            if (line_elem->is_text_line_base()) {
                const String &text = harmonic_text(import_type);
                to_text_line_base(line_elem)->set_begin_text(text);
                to_text_line_base(line_elem)->set_continue_text(text);
            }

            break;
        }
        case ElementType::OTTAVA: {
            const auto &[found_ottava, mu_ottava_type] = ottava_type(import_type);
            if (!found_ottava) {
                return;
            }

            if (line_elem->is_ottava()) {
                to_ottava(line_elem)->set_ottava_type(mu_ottava_type);
            }

            break;
        }
        case ElementType::VIBRATO:
            if (line_elem->is_vibrato()) {
                to_vibrato(line_elem)->set_vibrato_type(vibrato_type_from_import_type(import_type));
            }

            break;
        case ElementType::HAMMER_ON_PULL_OFF: spanners_without_end_element_[track_idx][import_type].insert(line_elem); break;
        default: break;
    }
}
} // namespace mu::iex::guitarpro
