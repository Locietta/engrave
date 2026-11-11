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
//---------------------------------------------------------
//   @@ SlurSegment
///    a single segment of slur; also used for Tie
//---------------------------------------------------------

struct SlurSegment : SlurTieSegment {
private:
    DECLARE_CLASSOF(ElementType::SLUR_SEGMENT)

    M_PROPERTY2(double, extra_height, set_extra_height, 0.0)
    M_PROPERTY2(PointF, end_point_off1, set_end_point_off1, PointF(0.0, 0.0))
    M_PROPERTY2(PointF, end_point_off2, set_end_point_off2, PointF(0.0, 0.0))

public:
    SlurSegment(Slur *sp, ElementType type = ElementType::SLUR_SEGMENT);
    SlurSegment(const SlurSegment &ss);

    SlurSegment *clone() const override { return new SlurSegment(*this); }

    bool is_edited() const;
    bool is_end_points_edited() const;

    Slur *slur() const { return to_slur(spanner()); }

    double end_width() const override;
    double mid_width() const override;
    double dotted_width() const override;

    Color cur_color(const rendering::PaintOptions &opt) const override;

protected:
};

//---------------------------------------------------------
//   @@ Slur
//---------------------------------------------------------

struct Slur : SlurTie {
private:
    DECLARE_CLASSOF(ElementType::SLUR)

public:
    Slur(DummyParentOr<EngravingItem> parent, ElementType type = ElementType::SLUR);
    Slur(const Slur &);

    Anchor anchor() const override { return Anchor::CHORDREST; }

    struct StemFloated {
        bool left = false;
        bool right = false;

        void reset() {
            left = false;
            right = false;
        }
    };

    /// temporary HACK for correct guitar pro import
    enum ConnectedElement : unsigned char { NONE, GLISSANDO };

    ~Slur() {}

    Slur *clone() const override { return new Slur(*this); }

    void set_track(track_idx_t val) override;

    PropertyValue get_property(Pid property_id) const override;
    PropertyValue property_default(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;

    SlurSegment *front_segment() { return to_slur_segment(Spanner::front_segment()); }

    const SlurSegment *front_segment() const { return to_slur_segment(Spanner::front_segment()); }

    SlurSegment *back_segment() { return to_slur_segment(Spanner::back_segment()); }

    const SlurSegment *back_segment() const { return to_slur_segment(Spanner::back_segment()); }

    SlurSegment *segment_at(int n) { return to_slur_segment(Spanner::segment_at(n)); }

    const SlurSegment *segment_at(int n) const { return to_slur_segment(Spanner::segment_at(n)); }

    bool is_cross_staff();
    bool has_cross_beams();

    const StemFloated &stem_floated() const { return stem_floated_; }

    StemFloated &stem_floated() { return stem_floated_; }

    SlurTieSegment *new_slur_tie_segment() override { return new SlurSegment(this); }

    double scaling_factor() const override;

    void undo_set_incoming(bool incoming);
    void undo_set_outgoing(bool outgoing);
    void set_incoming(bool incoming);
    void set_outgoing(bool outgoing);
    bool is_incoming() const;
    bool is_outgoing() const;

    void undo_change_start_end_elements(ChordRest *scr, ChordRest *ecr);

private:
    M_PROPERTY2(ConnectedElement, connected_element, set_connected_element, ConnectedElement::NONE)
    M_PROPERTY2(PartialSpannerDirection, partial_spanner_direction, set_partial_spanner_direction, PartialSpannerDirection::NONE)

    PartialSpannerDirection calc_incoming_direction(bool incoming);
    PartialSpannerDirection calc_outgoing_direction(bool outgoing);

    friend struct Factory;

    StemFloated stem_floated_; // end point position is attached to stem but floated towards the note
};
} // namespace mu::engraving
