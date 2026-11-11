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

#include "beambase.h"
#include "engravingitem.h"
#include "property.h"

namespace mu::engraving {
struct Chord;
struct ChordRest;
struct Factory;
struct Skyline;
struct System;
struct Transaction;
enum class ActionIconType : signed char;
enum class SpannerSegmentType : unsigned char;

struct TremAnchor {
    ChordRest *chord1 = nullptr;
    double y1 = 0.;
    double y2 = 0.;
};

//---------------------------------------------------------
//   @@ Beam
//---------------------------------------------------------

struct Beam final : BeamBase {
private:
    DECLARE_CLASSOF(ElementType::BEAM)

public:
    ~Beam();

    Beam *clone() const override { return new Beam(*this); }

    Fraction tick() const override;
    Fraction rtick() const override;
    Fraction ticks() const;

    void spatium_changed(double /*oldValue*/, double /*newValue*/) override;

    void reset() override;

    //! A beam has no owner: its lifetime is managed by the chord/rests it beams,
    //! which drop it through ChordRest::removeDeleteBeam() once it runs empty.
    //! The system it is drawn on is that of its first chord/rest.
    System *system() const;
    EngravingItem *layout_parent() const override;
    EngravingItem *accessible_parent_item() const override;

    const std::vector<ChordRest *> &elements() const { return elements_; }

    std::vector<ChordRest *> &elements() { return elements_; }

    void clear() { elements_.clear(); }

    bool empty() const { return elements_.empty(); }

    bool contains(const ChordRest *cr) const { return std::find(elements_.begin(), elements_.end(), cr) != elements_.end(); }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    void move(const PointF &) override;

    void set_direction(DirectionV d) override;

    void calc_beam_breaks(const ChordRest *chord, const ChordRest *prev_chord, int level, bool &is_broken32, bool &is_broken64) const;

    //!Note Unfortunately we have no FEATHERED_BEAM_MODE for now int BeamMode enum, so we'll handle this locally
    void set_as_feathered(const bool slower);
    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    double grow_left() const { return grow_left_; }

    double grow_right() const { return grow_right_; }

    void set_grow_left(double val) { grow_left_ = val; }

    void set_grow_right(double val) { grow_right_ = val; }

    PairF beam_pos() const;
    void set_beam_pos(const PairF &bp);

    double beam_dist() const { return beam_dist_; }

    void set_beam_dist(double d) { beam_dist_ = d; }

    double beam_width() const { return beam_width_; }

    void set_beam_width(double w) { beam_width_ = w; }

    int beam_spacing() const { return beam_spacing_; }

    void set_beam_spacing(int val) { beam_spacing_ = val; }

    bool no_slope() const { return no_slope_; }

    void set_no_slope(bool b);

    double slope() const { return slope_; }

    void compute_and_set_slope();

    void set_slope(double val) { slope_ = val; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    void set_is_grace(bool val) { is_grace_ = val; }

    bool is_grace() const { return is_grace_; }

    bool cross() const { return cross_; }

    void set_cross(bool val) { cross_ = val; }

    bool full_cross() const { return full_cross_; }

    void set_full_cross(bool v) { full_cross_ = v; }

    int min_cr_move() const override { return min_cr_move_; }

    void set_min_move(int val) { min_cr_move_ = val; }

    int max_cr_move() const override { return max_cr_move_; }

    void set_max_move(int val) { max_cr_move_ = val; }

    void add_skyline(Skyline &);

    void trigger_layout() const override;

    static ActionIconType action_icon_type_for_beam_mode(BeamMode);

    bool is_movable() const override;

    bool has_all_rests();

    const std::vector<TremAnchor> &trem_anchors() const { return trem_anchors_; }

    std::vector<TremAnchor> &trem_anchors() { return trem_anchors_; }

    const std::vector<BeamFragment *> &beam_fragments() const { return fragments_; }

    std::vector<BeamFragment *> &beam_fragments() { return fragments_; }

    void add_beam_fragment(BeamFragment *f) { fragments_.push_back(f); }

    void clear_beam_segments() override;

    const StaffType *tab() const { return tab_; }

    void set_tab(const StaffType *t) { tab_ = t; }

    bool is_beside_tab_staff() const { return is_beside_tab_staff_; }

    void set_is_beside_tab_staff(bool val) { is_beside_tab_staff_ = val; }

    const std::vector<NotePosition> &note_positions() const { return note_positions_; }

    std::vector<NotePosition> &note_positions() { return note_positions_; }

    const Chord *find_chord_with_custom_stem_direction() const;

    const BeamSegment *top_level_segment_for_element(const ChordRest *element) const;

private:

    friend struct Factory;
    friend struct BeamSegment;
    Beam(Score *parent);
    Beam(const Beam &);

    static constexpr std::array max_slopes = {0, 1, 2, 3, 4, 5, 6, 7};

    void add_chord_rest(ChordRest *a);
    void remove_chord_rest(ChordRest *a);

    std::vector<ChordRest *> elements_; // must be sorted by tick

    bool is_grace_ = false;
    bool cross_ = false;
    bool full_cross_ = false;

    double grow_left_ = 1.0; // define "feather" beams
    double grow_right_ = 1.0;
    double beam_dist_ = 0.0;
    int beam_spacing_ = 3;    // how far apart beams are spaced in quarter spaces
    double beam_width_ = 0.0; // how wide each beam is

    // for tabs
    bool is_beside_tab_staff_ = false;
    const StaffType *tab_ = nullptr;

    std::vector<BeamFragment *> fragments_; // beam splits across systems

    int min_cr_move_ = 0; // set in layout1()
    int max_cr_move_ = 0;
    int cross_beam_pos_ = 0;

    bool no_slope_ = false;
    real_t slope_ = 0.0;

    std::vector<NotePosition> note_positions_;
    std::vector<TremAnchor> trem_anchors_;
};
} // namespace mu::engraving
