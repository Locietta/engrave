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

#include "../types/types.h"

#include "textlinebase.h"

namespace mu::engraving {
class Hairpin;
class Transaction;

enum class HairpinType : signed char { INVALID = -1, CRESC_HAIRPIN, DIM_HAIRPIN, CRESC_LINE, DIM_LINE };

//---------------------------------------------------------
//   @@ HairpinSegment
//---------------------------------------------------------

class HairpinSegment final : public TextLineBaseSegment {
    OBJECT_ALLOCATOR(engraving, HairpinSegment)
    DECLARE_CLASSOF(ElementType::HAIRPIN_SEGMENT)

public:
    HairpinSegment(Hairpin *sp);

    HairpinSegment *clone() const override { return new HairpinSegment(*this); }

    Hairpin *hairpin() const { return (Hairpin *) spanner(); }

    bool draw_circled_tip() const { return draw_circled_tip_; }

    void set_draw_circled_tip(bool arg) { draw_circled_tip_ = arg; }

    double circled_tip_radius() const { return circled_tip_radius_; }

    void set_circled_tip_radius(double r) { circled_tip_radius_ = r; }

    PointF circled_tip() const { return circled_tip_; }

    void set_circled_tip(const PointF &p) { circled_tip_ = p; }

    EngravingObject *property_delegate(Pid) const override;

    bool has_voice_assignment_properties() const override { return spanner()->has_voice_assignment_properties(); }

    EngravingItem *find_element_to_snap_before(bool ignore_invisible = true) const;
    EngravingItem *find_element_to_snap_after(bool ignore_invisible = true, bool require_playable = false) const;

private:
    TextBase *find_start_dynamic_or_expression(bool ignore_invisible = true) const;
    TextBase *find_end_dynamic_or_expression(bool ignore_invisible = true, bool require_playable = false) const;

    Sid get_property_style(Pid) const override;

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void set_property_flags(Pid id, PropertyFlags f) override;

    bool draw_circled_tip_ = false;
    PointF circled_tip_;
    double circled_tip_radius_ = 0.0;
};

//---------------------------------------------------------
//   @@ Hairpin
//   @P hairpinType  enum (Hairpin.CRESCENDO, Hairpin.DIMINUENDO)
//   @P veloChange   int
//---------------------------------------------------------

class Hairpin final : public TextLineBase {
    OBJECT_ALLOCATOR(engraving, Hairpin)
    DECLARE_CLASSOF(ElementType::HAIRPIN)

public:
    Hairpin(DummyParentOr<EngravingItem> parent);

    Hairpin *clone() const override { return new Hairpin(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    DynamicType dynamic_type_from() const;
    DynamicType dynamic_type_to() const;

    const Dynamic *dynamic_snapped_before() const;
    const Dynamic *dynamic_snapped_after() const;

    HairpinType hairpin_type() const { return hairpin_type_; }

    void set_hairpin_type(HairpinType val);

    LineSegment *create_line_segment() override;

    bool hairpin_circled_tip() const { return hairpin_circled_tip_; }

    void set_hairpin_circled_tip(bool val) { hairpin_circled_tip_ = val; }

    int velo_change() const { return velo_change_; }

    void set_velo_change(int v) { velo_change_ = v; }

    Spatium hairpin_height() const { return hairpin_height_; }

    void set_hairpin_height(Spatium val) { hairpin_height_ = val; }

    Spatium hairpin_cont_height() const { return hairpin_cont_height_; }

    void set_hairpin_cont_height(Spatium val) { hairpin_cont_height_ = val; }

    bool single_note_dynamics() const { return single_note_dynamics_; }

    void set_single_note_dynamics(bool val) { single_note_dynamics_ = val; }

    ChangeMethod velo_change_method() const { return velo_change_method_; }

    void set_velo_change_method(ChangeMethod val) { velo_change_method_ = val; }

    bool is_crescendo() const { return hairpin_type_ == HairpinType::CRESC_HAIRPIN || hairpin_type_ == HairpinType::CRESC_LINE; }

    bool is_decrescendo() const { return hairpin_type_ == HairpinType::DIM_HAIRPIN || hairpin_type_ == HairpinType::DIM_LINE; }

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    bool is_line_type() const { return hairpin_type_ == HairpinType::CRESC_LINE || hairpin_type_ == HairpinType::DIM_LINE; }

    PointF line_pos(Grip grip, System **system) const override;

    bool has_voice_assignment_properties() const override { return true; }

    void reset() override;

    void set_voice_assignment(VoiceAssignment v) { voice_assignment_ = v; }

    VoiceAssignment voice_assignment() const { return voice_assignment_; }

    void set_direction(DirectionV v) { direction_ = v; }

    DirectionV direction() const { return direction_; }

    void set_center_between_staves(AutoOnOff v) { center_between_staves_ = v; }

    AutoOnOff center_between_staves() const { return center_between_staves_; }

    bool snap_to_item_before() const { return snap_to_item_before_; }

    void set_snap_to_item_before(bool v) { snap_to_item_before_ = v; }

    bool snap_to_item_after() const { return snap_to_item_after_; }

    void set_snap_to_item_after(bool v) { snap_to_item_after_ = v; }

    int subtype() const override { return int(hairpin_type_); }

    TranslatableString subtype_user_name() const override;

protected:
    Sid default_pos_sid() const override;

private:

    Sid get_property_style(Pid) const override;

    HairpinType hairpin_type_ = HairpinType::INVALID;
    int velo_change_ = 0;
    bool hairpin_circled_tip_ = false;
    bool single_note_dynamics_ = false;
    ChangeMethod velo_change_method_ = ChangeMethod::NORMAL;

    Spatium hairpin_height_;
    Spatium hairpin_cont_height_;

    VoiceAssignment voice_assignment_ = VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
    DirectionV direction_ = DirectionV::AUTO;
    AutoOnOff center_between_staves_ = AutoOnOff::AUTO;

    bool snap_to_item_before_ = true;
    bool snap_to_item_after_ = true;
};
} // namespace mu::engraving
