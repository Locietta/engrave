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

#include "textbase.h"

namespace muse::draw {
struct Painter;
} // namespace muse::draw

namespace mu::engraving {
struct Transaction;
struct Measure;
struct Segment;

struct Dyn {
    DynamicType type = DynamicType::OTHER;
    int velocity = -1; // associated midi velocity (0-127, -1 = none)
    int change_in_velocity = 0;
    bool accent = 0;            // if true add velocity to current chord velocity
    const char *text = nullptr; // utf8 text of dynamic
};

//-----------------------------------------------------------------------------
//   @@ Dynamic
///    dynamics marker; determines midi velocity
//
//   @P range  enum (Dynamic.STAFF, .PART, .SYSTEM)
//-----------------------------------------------------------------------------

struct Dynamic final : TextBase {
private:
    DECLARE_CLASSOF(ElementType::DYNAMIC)

public:
    struct ChangeSpeedItem {
        DynamicSpeed speed = DynamicSpeed::NORMAL;
        const char *name = nullptr;
    };

    Dynamic(DummyParentOr<Segment> parent);
    Dynamic(const Dynamic &);

    Dynamic *clone() const override { return new Dynamic(*this); }

    Segment *segment() const { return (Segment *) ownership_parent(); }

    Measure *measure() const { return (Measure *) ownership_parent()->ownership_parent(); }

    void set_dynamic_type(DynamicType val) { dynamic_type_ = val; }

    void set_dynamic_type(const String &);

    DynamicType dynamic_type() const { return dynamic_type_; }

    int subtype() const override { return static_cast<int>(dynamic_type_); }

    TranslatableString subtype_user_name() const override;

    void reset() override;

    void set_velocity(int v) { velocity_ = v; }

    int velocity() const;

    int change_in_velocity() const;
    void set_change_in_velocity(int val);
    Fraction velocity_change_length(BeatsPerSecond tempo) const;
    bool is_velocity_change_available() const;

    DynamicSpeed vel_change_speed() const { return vel_change_speed_; }

    void set_vel_change_speed(DynamicSpeed val) { vel_change_speed_ = val; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;
    Sid get_property_style(Pid) const override;

    static String dynamic_text(DynamicType t);

    bool has_custom_text() const { return dynamic_text(dynamic_type_) != xml_text(); }

    Expression *snapped_expression() const;
    HairpinSegment *find_snap_before_hairpin_across_system_break() const;

    bool play_dynamic() const { return play_dynamic_; }

    void set_play_dynamic(bool v) { play_dynamic_ = v; }

    bool accept_drop(EditData &ed) const override;
    EngravingItem *drop(Transaction &tx, EditData &ed) override;

    static int dynamic_velocity(DynamicType t);

    static const std::vector<Dyn> &dynamic_list() { return ::mu::engraving::Dynamic::dyn_list; }

    bool has_voice_assignment_properties() const override { return true; }

    bool position_relative_to_notehead_rest() const override { return true; }

    bool size_is_spatium_dependent() const override { return true; }

    Hairpin *left_hairpin() const { return left_hairpin_; }

    Hairpin *right_hairpin() const { return right_hairpin_; }

    bool has_left_grip() const;
    bool has_right_grip() const;

    void reset_left_drag_offset() { left_drag_offset_ = 0.0; }

    void reset_right_drag_offset() { right_drag_offset_ = 0.0; }

    double left_drag_offset() const { return left_drag_offset_; }

    double right_drag_offset() const { return right_drag_offset_; }

    void find_adjacent_hairpins();

    Shape sym_shape_with_cutouts(SymId id) const override;

    static Dyn dyn_info(DynamicType type);

private:

    M_PROPERTY(bool, avoid_bar_lines, set_avoid_bar_lines)
    M_PROPERTY(bool, center_on_notehead, set_center_on_notehead)

    std::pair<DynamicType, String> parse_dynamic_text(const String &) const;
    DynamicType dynamic_type_ = DynamicType::OTHER;
    bool play_dynamic_ = true;

    mutable PointF drag_offset_;
    int velocity_ = -1; // associated midi velocity 0-127

    int change_in_velocity_ = 128;
    DynamicSpeed vel_change_speed_ = DynamicSpeed::NORMAL;

    static const std::vector<Dyn> dyn_list;

    double left_drag_offset_ = 0.0;
    double right_drag_offset_ = 0.0;

    Hairpin *left_hairpin_ = nullptr;
    Hairpin *right_hairpin_ = nullptr;
};
} // namespace mu::engraving
