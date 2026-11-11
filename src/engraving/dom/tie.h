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

#include "slurtie.h"

namespace mu::engraving {
struct TieJumpPointList;
struct TieJumpPoint;
//---------------------------------------------------------
//   @@ TieSegment
///    a single segment of a tie
//---------------------------------------------------------

struct TieSegment : SlurTieSegment {
private:
    DECLARE_CLASSOF(ElementType::TIE_SEGMENT)

public:
    TieSegment(Tie *sp);
    TieSegment(const TieSegment &s);

    TieSegment *clone() const override { return new TieSegment(*this); }

    void add_adjustment_offset(const PointF &offset, Grip grip) { adjustment_offsets_[static_cast<size_t>(grip)] += offset; }

    void reset_adjustment_offset() { adjustment_offsets_.fill(PointF()); }

    PointF adjustment_offset(Grip grip) { return adjustment_offsets_[static_cast<size_t>(grip)]; }

    void consolidate_adjustment_offset_into_user_offset();

    bool is_edited() const;

    Tie *tie() const { return (Tie *) spanner(); }

    void set_staff_move(int val) { staff_move_ = val; }

    staff_idx_t v_staff_idx() const override { return staff_idx() + staff_move_; }

    virtual double min_shoulder_height() const;
    virtual double max_shoulder_height() const;
    double end_width() const override;
    double mid_width() const override;
    double dotted_width() const override;

    struct LayoutData : public SlurTieSegment::LayoutData {
        bool all_jump_points_inactive = false;
    };
    DECLARE_LAYOUTDATA_METHODS(TieSegment)

protected:
    TieSegment(const ElementType &type, Tie *sp);

private:
    int staff_move_ = 0;
    std::array<PointF, static_cast<size_t>(Grip::GRIPS)> adjustment_offsets_;
};

//---------------------------------------------------------
//   @@ Tie
//!    a Tie has a Note as startElement/endElement
//---------------------------------------------------------

struct Tie : SlurTie {
private:
    DECLARE_CLASSOF(ElementType::TIE)

public:
    Tie(EngravingObject *parent = 0);
    Tie(const Tie &t);

    Tie *clone() const override { return new Tie(*this); }

    Anchor anchor() const override { return Anchor::NOTE; }

    virtual ~Tie() {}

    virtual Note *start_note() const;
    virtual void set_start_note(Note *note);
    virtual Note *end_note() const;

    virtual void set_end_note(Note *note) { set_end_element((EngravingItem *) note); }

    bool is_inside() const { return is_inside_; }

    void set_is_inside(bool val) { is_inside_ = val; }

    virtual bool is_outer_tie_of_chord(Grip start_or_end) const;
    bool has_tied_second_inside() const;
    bool is_cross_staff() const;

    PropertyValue get_property(Pid property_id) const override;
    PropertyValue property_default(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;

    TieSegment *front_segment() { return to_tie_segment(Spanner::front_segment()); }

    const TieSegment *front_segment() const { return to_tie_segment(Spanner::front_segment()); }

    TieSegment *back_segment() { return to_tie_segment(Spanner::back_segment()); }

    const TieSegment *back_segment() const { return to_tie_segment(Spanner::back_segment()); }

    TieSegment *segment_at(int n) { return to_tie_segment(Spanner::segment_at(n)); }

    const TieSegment *segment_at(int n) const { return to_tie_segment(Spanner::segment_at(n)); }

    SlurTieSegment *new_slur_tie_segment() override { return new TieSegment(this); }

    double scaling_factor() const override;

    const TiePlacement &tie_placement() const { return tie_placement_; }

    void set_tie_placement(const TiePlacement &val) { tie_placement_ = val; }

    // Outgoing ties before repeats
    void update_possible_jump_points();
    void add_ties_to_jump_points();
    void undo_remove_ties_from_jump_points();
    virtual bool all_jump_points_inactive() const;
    virtual TieJumpPointList *tie_jump_points();
    virtual const TieJumpPointList *tie_jump_points() const;

    // Incoming ties after repeats
    void set_jump_point(TieJumpPoint *jump_point) { jump_point_ = jump_point; }

    void update_start_tie_on_removal();

    TieJumpPoint *jump_point() const { return jump_point_; }

    Tie *start_tie() const;

    static void change_tie_type(Tie *old_tie, Note *end_note = nullptr);

protected:
    Tie(const ElementType &type, EngravingObject *parent = nullptr);

    bool is_in_spanner_map() const override { return false; }

    bool is_inside_ = false;
    TiePlacement tie_placement_ = TiePlacement::AUTO;

    // Jump point information for incoming ties after repeats
    TieJumpPoint *jump_point_ = nullptr;
    TieJumpPointList *start_tie_jump_points() const;
};
} // namespace mu::engraving
