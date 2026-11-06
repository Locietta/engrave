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
#ifndef MU_ENGRAVING_BEAMBASE_H
#define MU_ENGRAVING_BEAMBASE_H

#include "engravingitem.h"

namespace mu::engraving {
class Beam;
class TremoloTwoChord;

enum class BeamType : unsigned char { INVALID, BEAM, TREMOLO };

//---------------------------------------------------------
//   BeamFragment
//    position of primary beam
//    idx 0 - DirectionV::AUTO or DirectionV::DOWN
//        1 - DirectionV::UP
//---------------------------------------------------------

struct BeamFragment {
    double py1[2];
    double py2[2];
};

enum class ChordBeamAnchorType : unsigned char { START, END, MIDDLE };

class BeamSegment {
    OBJECT_ALLOCATOR(engraving, BeamSegment)
public:
    LineF line;
    int level = 0;
    bool above = false; // above level 0 or below? (meaningless for level 0)
    Fraction start_tick;
    Fraction end_tick;
    bool is_beamlet = false;
    bool is_before = false;

    Shape shape() const;
    EngravingItem *parent_element = nullptr;

    BeamSegment(EngravingItem *b);
};

class BeamBase : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, BeamBase)
    DECLARE_CLASSOF(ElementType::INVALID) // dummy

    M_PROPERTY2(int, cross_staff_move, set_cross_staff_move, 0)

public:

    virtual int max_cr_move() const = 0;
    virtual int min_cr_move() const = 0;

    virtual PropertyValue get_property(Pid property_id) const override;
    virtual bool set_property(Pid property_id, const PropertyValue &) override;
    virtual PropertyValue property_default(Pid property_id) const override;

    int cross_staff_idx() const;
    int default_cross_staff_idx() const;
    bool accept_cross_staff_move(int move) const;

    bool up() const { return up_; }

    void set_up(bool v) { up_ = v; }

    bool user_modified() const;
    void set_user_modified(bool val);

    DirectionV direction() const { return direction_; }

    void do_set_direction(DirectionV val) { direction_ = val; }

    virtual void set_direction(DirectionV v) = 0;

    inline int direction_idx() const { return (direction_ == DirectionV::AUTO || direction_ == DirectionV::DOWN) ? 0 : 1; }

    const std::vector<BeamSegment *> &beam_segments() const { return beam_segments_; }

    std::vector<BeamSegment *> &beam_segments() { return beam_segments_; }

    virtual void clear_beam_segments();

    const PointF &start_anchor() const { return start_anchor_; }

    PointF &start_anchor() { return start_anchor_; }

    void set_start_anchor(const PointF &p) { start_anchor_ = p; }

    const PointF &end_anchor() const { return end_anchor_; }

    PointF &end_anchor() { return end_anchor_; }

    void set_end_anchor(const PointF &p) { end_anchor_ = p; }

    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps = PropertyFlags::NOSTYLE) override;

    struct NotePosition {
        int line;
        staff_idx_t staff;

        NotePosition(int note, staff_idx_t staff) : line(note), staff(staff) {}

        NotePosition() : line(0), staff(0) {}

        inline bool operator<(const NotePosition &other) const {
            return staff < other.staff || (staff == other.staff && line < other.line);
        }

        inline bool operator>(const NotePosition &other) const { return other < *this; }

        inline bool operator<=(const NotePosition &other) const { return !(other < *this); }

        inline bool operator>=(const NotePosition &other) const { return !(*this < other); }

        inline bool operator==(const NotePosition &other) const { return line == other.line && staff == other.staff; }

        inline bool operator!=(const NotePosition &other) const { return line != other.line || staff != other.staff; }
    };

    enum class CrossStaffBeamPosition : signed char { INVALID, BETWEEN, ABOVE, BELOW };

    struct LayoutData : public EngravingItem::LayoutData {
        BeamType beam_type = BeamType::INVALID;
        const Beam *beam = nullptr;
        const TremoloTwoChord *trem = nullptr;
        bool up = false;
        Fraction tick = Fraction(0, 1);
        double spatium = 0.;
        PointF start_anchor;
        PointF end_anchor;
        double slope = 0.;
        bool is_grace = false;
        int beam_spacing = 0;
        double beam_dist = 0.0;
        double beam_width = 0.0;
        std::vector<ChordRest *> elements;
        std::vector<NotePosition> note_positions;
        const StaffType *tab = nullptr;
        bool is_beside_tab_staff = false;
        CrossStaffBeamPosition cross_staff_beam_pos = CrossStaffBeamPosition::INVALID;
        const Chord *limiting_chord_above = nullptr; // <-
        const Chord *limiting_chord_below = nullptr; // <- For cross-staff spacing and centering

        void set_anchors(PointF start_a, PointF end_a) {
            start_anchor = start_a;
            end_anchor = end_a;
        }

        bool is_valid() const override { return !(beam_type == BeamType::INVALID); }
    };
    DECLARE_LAYOUTDATA_METHODS(BeamBase)

protected:
    BeamBase(const ElementType &type, EngravingObject *parent, ElementFlags flags = ElementFlag::ON_STAFF);
    BeamBase(const BeamBase &);
    std::vector<BeamSegment *> beam_segments_;
    PointF start_anchor_;
    PointF end_anchor_;

private:
    bool up_ = true;
    bool user_modified_[2]{false}; // 0: auto/down  1: up
    DirectionV direction_ = DirectionV::AUTO;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_BEAMBASE_H
