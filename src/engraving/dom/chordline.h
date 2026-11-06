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

#pragma once

#include <array>

#include "../types/types.h"
#include "draw/types/painterpath.h"
#include "engravingitem.h"

namespace mu::engraving {
struct Factory;

struct Chord;
struct Note;

//---------------------------------------------------------
//   @@ ChordLine
///    bezier line attached to top note of a chord
///    implements fall, doit, plop, bend
//---------------------------------------------------------

struct ChordLine final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, ChordLine)
    DECLARE_CLASSOF(ElementType::CHORDLINE)

public:

    ChordLine *clone() const override { return new ChordLine(*this); }

    Chord *chord() const { return (Chord *) (ownership_parent()); }

    void set_chord_line_type(ChordLineType);

    ChordLineType chord_line_type() const { return chord_line_type_; }

    bool is_straight() const { return straight_; }

    void set_straight(bool straight) { straight_ = straight; }

    bool is_wavy() const { return wavy_; }

    void set_wavy(bool wavy) { wavy_ = wavy; }

    void set_length_x(double length) { length_x_ = length; }

    double length_x() const { return length_x_; }

    void set_length_y(double length) { length_y_ = length; }

    double length_y() const { return length_y_; }

    void set_modified(bool m) { modified_ = m; }

    bool modified() const { return modified_; }

    const TranslatableString &chord_line_type_name() const;

    int subtype() const override;
    TranslatableString subtype_user_name() const override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    void reset_property(Pid) override;

    bool is_to_the_left() const { return chord_line_type_ == ChordLineType::PLOP || chord_line_type_ == ChordLineType::SCOOP; }

    bool is_below() const { return chord_line_type_ == ChordLineType::SCOOP || chord_line_type_ == ChordLineType::FALL; }

    bool play_chord_line() const { return play_chord_line_; }

    void set_play_chord_line(bool val) { play_chord_line_ = val; }

    void set_note(Note *note);

    Note *note() const { return note_; }

    SymId wave_sym() const;

    // Physical drawing-unit sources, independent of the derived layout cache.
    bool has_source_offsets() const { return has_source_offsets_; }

    bool has_default_path() const { return !modified_ && (!has_source_offsets_ || source_offsets_ == std::array<PointF, 4>{}); }

    const std::array<PointF, 4> &source_offsets() const { return source_offsets_; }

    void read_source_offsets(const std::array<PointF, 4> &xml, double unit);
    std::array<PointF, 4> xml_source_offsets() const;
    PainterPath source_offset_path(double spatium, double mag) const;
    void spatium_changed(double old_value, double new_value) override;
    void local_spatium_changed(double old_value, double new_value) override;

    static constexpr std::array<Pid, 8> appearance_properties{
        Pid::COLOR,           Pid::VISIBLE, Pid::Z, Pid::SIZE_SPATIUM_DEPENDENT, Pid::CHORD_LINE_TYPE, Pid::CHORD_LINE_STRAIGHT,
        Pid::CHORD_LINE_WAVY, Pid::PATH};

    struct AppearanceState {
        std::array<PropertyValue, 8> values;
        std::array<PropertyFlags, 8> flags;
        bool linked;
        bool offsets;
        bool modified;
        double length_x;
        double length_y;
        std::array<PointF, 4> controls;
    };

    AppearanceState appearance_state() const;
    void restore_appearance(const AppearanceState &);

    struct LayoutData : public EngravingItem::LayoutData {
        muse::draw::PainterPath path;
    };
    DECLARE_LAYOUTDATA_METHODS(ChordLine)

private:

    friend struct Factory;

    ChordLine(DummyParentOr<Chord> parent);
    ChordLine(const ChordLine &);

    std::array<PointF, 4> offset_points(const std::array<PointF, 4> &, double spatium, double mag) const;
    void scale_offsets(double old_value, double new_value);
    bool has_source_offsets_ = false;
    std::array<PointF, 4> source_offsets_{};

    bool straight_ = false;
    bool wavy_ = false;

    ChordLineType chord_line_type_ = ChordLineType::NOTYPE;
    bool modified_ = false;
    double length_x_ = 0.0;
    double length_y_ = 0.0;
    bool play_chord_line_ = true;
    Note *note_ = nullptr;
};
} // namespace mu::engraving
