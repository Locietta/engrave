/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#include "harppedaldiagram.h"

#include "../editing/editharppedaldiagram.h"
#include "log.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "translation.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;

// Position of separator character in diagram
static constexpr size_t separator_idx = 3;

static const String harp_string_type_to_string(HarpStringType type) {
    switch (type) {
        case HarpStringType::A: return String(u"A");
        case HarpStringType::B: return String(u"B");
        case HarpStringType::C: return String(u"C");
        case HarpStringType::D: return String(u"D");
        case HarpStringType::E: return String(u"E");
        case HarpStringType::F: return String(u"F");
        case HarpStringType::G: return String(u"G");
    }
    return String();
}

// STYLE
static const ElementStyle harp_pedal_diagram_style{
    {Sid::HARP_PEDAL_DIAGRAM_PLACEMENT, Pid::PLACEMENT},
    {Sid::HARP_PEDAL_DIAGRAM_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

static const ElementStyle harp_pedal_text_diagram_style{
    {Sid::HARP_PEDAL_TEXT_DIAGRAM_PLACEMENT, Pid::PLACEMENT},
    {Sid::HARP_PEDAL_TEXT_DIAGRAM_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

// HarpPedalDiagram
void HarpPedalDiagram::set_playable_tpcs() {
    std::set<int> playable_tpcs;
    static const std::map<HarpStringType, int> string2step = {{HarpStringType::C, 0}, {HarpStringType::D, 1}, {HarpStringType::E, 2},
                                                              {HarpStringType::F, 3}, {HarpStringType::G, 4}, {HarpStringType::A, 5},
                                                              {HarpStringType::B, 6}};
    static const std::map<PedalPosition, AccidentalVal> position2accidental_val = {{PedalPosition::FLAT, AccidentalVal::FLAT},
                                                                                   {PedalPosition::NATURAL, AccidentalVal::NATURAL},
                                                                                   {PedalPosition::SHARP, AccidentalVal::SHARP}};

    for (size_t i = 0; i < pedal_state_.size(); i++) {
        int string_step = string2step.at(HarpStringType(i));
        AccidentalVal accidental_val = position2accidental_val.at(pedal_state_[i]);
        int res_tpc = step2tpc(string_step, accidental_val);
        playable_tpcs.insert(res_tpc);
    }

    playable_tpcs_ = playable_tpcs;
}

HarpPedalDiagram::HarpPedalDiagram(DummyParentOr<Segment> parent)
    : TextBase(ElementType::HARP_DIAGRAM, parent, TextStyleType::HARP_PEDAL_DIAGRAM, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::harp_pedal_diagram_style);
    pedal_state_ = std::array<PedalPosition, ::mu::engraving::harp_string_no>{
        PedalPosition::NATURAL, PedalPosition::NATURAL, PedalPosition::NATURAL, PedalPosition::NATURAL,
        PedalPosition::NATURAL, PedalPosition::NATURAL, PedalPosition::NATURAL};
    set_playable_tpcs();
}

HarpPedalDiagram::HarpPedalDiagram(const HarpPedalDiagram &h) : TextBase(h) {
    pedal_state_ = h.pedal_state_;
    is_diagram_ = h.is_diagram_;
    set_playable_tpcs();
}

void HarpPedalDiagram::set_pedal_state(std::array<PedalPosition, ::mu::engraving::harp_string_no> state) {
    pedal_state_ = state;
    set_playable_tpcs();
}

void HarpPedalDiagram::set_is_diagram(bool diagram) {
    is_diagram_ = diagram;
    const ElementStyle &style_map = is_diagram_ ? ::harp_pedal_diagram_style : ::harp_pedal_text_diagram_style;

    if (is_diagram_) {
        set_text_style_type(TextStyleType::HARP_PEDAL_DIAGRAM);
    } else {
        set_text_style_type(TextStyleType::HARP_PEDAL_TEXT_DIAGRAM);
    }

    init_element_style(&style_map);
}

void HarpPedalDiagram::set_pedal(HarpStringType harp_string, PedalPosition pedal) {
    pedal_state_.at(harp_string) = pedal;
    set_playable_tpcs();
}

String HarpPedalDiagram::create_diagram_text() {
    String diagram;
    if (is_diagram_) {
        for (size_t idx = 0; idx < pedal_state_.size(); idx++) {
            if (idx == ::separator_idx) {
                // insert separator
                diagram.append(u"<sym>harpPedalDivider</sym>");
            }
            switch (pedal_state_[idx]) {
                case PedalPosition::FLAT: diagram.append(u"<sym>harpPedalRaised</sym>"); break;
                case PedalPosition::NATURAL: diagram.append(u"<sym>harpPedalCentered</sym>"); break;
                case PedalPosition::SHARP: diagram.append(u"<sym>harpPedalLowered</sym>"); break;
                case PedalPosition::UNSET: break;
            }
        }
    } else {
        // find difference between pedal state of previous diagram and this one
        // if no previous, the pedals are "unset" meaning the whole diagram will be displayed

        constexpr std::array<PedalPosition, ::mu::engraving::harp_string_no> init_state = {
            PedalPosition::UNSET, PedalPosition::UNSET, PedalPosition::UNSET, PedalPosition::UNSET,
            PedalPosition::UNSET, PedalPosition::UNSET, PedalPosition::UNSET};
        std::array<PedalPosition, ::mu::engraving::harp_string_no> prev_state;
        HarpPedalDiagram *prev_diagram = nullptr;
        if (origin_part() && segment()) {
            prev_diagram = origin_part()->prev_harp_diagram(segment()->tick());
        }

        if (prev_diagram != nullptr) {
            // If states are the same work backwards until we find the first difference then calculate from there
            // This ensures the diagram is identical to the previous one
            prev_state = prev_diagram->get_pedal_state();
            while (prev_state == pedal_state_) {
                // if states are the same but previous is a diagram write whole config out
                if (prev_diagram->is_diagram()) {
                    prev_state = init_state;
                    break;
                }
                // Check if there's a diagram before.  If not, start of score and use init state
                prev_diagram = origin_part()->prev_harp_diagram(prev_diagram->segment()->tick());
                if (prev_diagram != nullptr) {
                    prev_state = prev_diagram->get_pedal_state();
                } else {
                    // set to init state and break loop
                    prev_state = init_state;
                    break;
                }
            }
        } else {
            prev_state = init_state;
        }

        String top_line, bottom_line;

        for (size_t idx = 0; idx < pedal_state_.size(); idx++) {
            if (pedal_state_[idx] != prev_state[idx]) {
                String str_name = harp_string_type_to_string(HarpStringType(idx));
                switch (pedal_state_[idx]) {
                    case PedalPosition::FLAT: str_name.append(u"<sym>csymAccidentalFlat</sym> "); break;
                    case PedalPosition::NATURAL: str_name.append(u"<sym>csymAccidentalNatural</sym> "); break;
                    case PedalPosition::SHARP: str_name.append(u"<sym>csymAccidentalSharp</sym> "); break;
                    case PedalPosition::UNSET: break;
                }
                if (idx < ::separator_idx) {
                    bottom_line.append(str_name);
                } else {
                    top_line.append(str_name);
                }
            }
        }

        if (top_line.size() == 0) {
            diagram.append(bottom_line);
        } else {
            diagram.append(top_line + u"\n" + bottom_line);
        }
    }

    return diagram;
}

void HarpPedalDiagram::update_diagram_text() { undo_change_property(Pid::TEXT, create_diagram_text(), PropertyFlags::STYLED); }

void HarpPedalDiagram::undo_change_pedal_state(std::array<PedalPosition, ::mu::engraving::harp_string_no> pedal_state) {
    const std::list<EngravingObject *> links = link_list();
    for (EngravingObject *obj : links) {
        if (!obj || !obj->is_harp_pedal_diagram()) {
            continue;
        }

        HarpPedalDiagram *item = to_harp_pedal_diagram(obj);
        Score *linked_score = item->score();
        if (!linked_score) {
            continue;
        }

        linked_score->undo(new ChangeHarpPedalState(item, pedal_state));
    }
}

bool HarpPedalDiagram::is_tpc_playable(int tpc) { return playable_tpcs_.find(tpc) != playable_tpcs_.cend(); }

PropertyValue HarpPedalDiagram::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::HARP_IS_DIAGRAM: return is_diagram_;
        default: return TextBase::get_property(property_id);
    }
}

bool HarpPedalDiagram::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::HARP_IS_DIAGRAM: set_is_diagram(v.to_bool()); break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
    }
    return true;
}

PropertyValue HarpPedalDiagram::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::HARP_PEDAL_DIAGRAM;
        case Pid::HARP_IS_DIAGRAM: return true;
        default: return TextBase::property_default(id);
    }
}
