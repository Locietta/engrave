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

#pragma once

#include "textbase.h"

using namespace mu;

namespace mu::engraving {
enum class PedalPosition : unsigned char {
    FLAT,
    NATURAL,
    SHARP,

    UNSET // Only used in setDiagramText to represent the beginning of a score
};

// Use for indexes of _pedalState
enum HarpStringType : unsigned char { D, C, B, E, F, G, A };

// Number of strings per octave on a harp
static constexpr int harp_string_no = 7;

class HarpPedalDiagram final : public TextBase {
    OBJECT_ALLOCATOR(engraving, HarpPedalDiagram)
    DECLARE_CLASSOF(ElementType::HARP_DIAGRAM)

public:
    HarpPedalDiagram(DummyParentOr<Segment> parent);
    HarpPedalDiagram(const HarpPedalDiagram &h);

    HarpPedalDiagram *clone() const override { return new HarpPedalDiagram(*this); }

    Segment *segment() const { return (Segment *) ownership_parent(); }

    Measure *measure() const { return (Measure *) ownership_parent()->ownership_parent(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;

    void set_is_diagram(bool diagram);

    bool is_diagram() const { return is_diagram_; }

    std::array<PedalPosition, ::mu::engraving::harp_string_no> get_pedal_state() const { return pedal_state_; }

    void set_pedal_state(std::array<PedalPosition, ::mu::engraving::harp_string_no> state);

    void set_pedal(HarpStringType harp_string, PedalPosition pedal);

    void set_playable_tpcs();

    String create_diagram_text();
    void update_diagram_text();

    void undo_change_pedal_state(std::array<PedalPosition, ::mu::engraving::harp_string_no> pedal_state);

    bool is_tpc_playable(int tpc);

    const std::set<int> &playable_tpcs() const { return playable_tpcs_; }

    bool position_relative_to_notehead_rest() const override { return true; }

private:

    std::array<PedalPosition, ::mu::engraving::harp_string_no> pedal_state_;

    std::set<int> playable_tpcs_;

    bool is_diagram_ = true;
};
} // namespace mu::engraving
