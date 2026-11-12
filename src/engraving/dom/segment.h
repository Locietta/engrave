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

#include "engravingitem.h"

namespace mu::engraving {
struct Factory;
struct Measure;
struct Segment;
struct ChordRest;
struct Spanner;
struct System;

//-------------------------------------------------------------------
//   SegmentType
//
//    Type values determine the order of segments for a given tick
//-------------------------------------------------------------------

enum class SegmentType {
    ///.\{
    INVALID = 0x0,
    BEGIN_BAR_LINE = 0x1,
    HEADER_CLEF = 0x2,
    KEY_SIG = 0x4,
    AMBITUS = 0x8,
    BREATH = 0x10,
    TIME_SIG = 0x20,
    START_REPEAT_BAR_LINE = 0x40,
    CLEF_START_REPEAT_ANNOUNCE = 0x80,
    KEY_SIG_START_REPEAT_ANNOUNCE = 0x100,
    TIME_SIG_START_REPEAT_ANNOUNCE = 0x200,
    CLEF = 0x400,
    BAR_LINE = 0x800,
    //--
    TIME_TICK = 0x1000,
    CHORD_REST = 0x2000,
    //--
    CLEF_REPEAT_ANNOUNCE = 0x4000,
    KEY_SIG_REPEAT_ANNOUNCE = 0x8000,
    TIME_SIG_REPEAT_ANNOUNCE = 0x10000,
    //--
    END_BAR_LINE = 0x20000,
    KEY_SIG_ANNOUNCE = 0x40000,
    TIME_SIG_ANNOUNCE = 0x80000,
    //--
    ALL = -1, ///< Includes all barline types
    /// Alias for `BeginBarLine | StartRepeatBarLine | BarLine | EndBarLine`
    BAR_LINE_TYPES = BEGIN_BAR_LINE | START_REPEAT_BAR_LINE | BAR_LINE | END_BAR_LINE,
    COURTESY_TIME_SIG_TYPES = TIME_SIG_ANNOUNCE | TIME_SIG_REPEAT_ANNOUNCE | TIME_SIG_START_REPEAT_ANNOUNCE,
    COURTESY_KEY_SIG_TYPES = KEY_SIG_ANNOUNCE | KEY_SIG_REPEAT_ANNOUNCE | KEY_SIG_START_REPEAT_ANNOUNCE,
    COURTESY_CLEF_TYPES = CLEF_REPEAT_ANNOUNCE | CLEF_START_REPEAT_ANNOUNCE,
    TIME_SIG_TYPES = TIME_SIG | COURTESY_TIME_SIG_TYPES,
    KEY_SIG_TYPES = KEY_SIG | COURTESY_KEY_SIG_TYPES,
    CLEF_TYPES = CLEF | HEADER_CLEF | COURTESY_CLEF_TYPES,
    // Only types with non-null duration
    DURATION = TIME_TICK | CHORD_REST,
    ///\}
};

constexpr SegmentType operator|(const SegmentType t1, const SegmentType t2) {
    return static_cast<SegmentType>(static_cast<int>(t1) | static_cast<int>(t2));
}

constexpr bool operator&(const SegmentType t1, const SegmentType t2) { return static_cast<int>(t1) & static_cast<int>(t2); }

constexpr SegmentType operator~(const SegmentType &t) { return static_cast<SegmentType>(~static_cast<int>(t)); }

//------------------------------------------------------------------------
//   @@ Segment
//    A segment holds all vertical aligned staff elements.
//    Segments are typed and contain only Elements of the same type.
//
//    All Elements in a segment start at the same tick. The Segment can store one EngravingItem for
//    each voice in each staff in the score.
//    Some elements (Clef, KeySig, TimeSig etc.) are assumed to always have voice zero
//    and can be found in _elist[staffIdx * VOICES];

//    Segments are children of Measures and store Clefs, KeySigs, TimeSigs,
//    BarLines and ChordRests.
//
//   @P annotations     array[EngravingItem]    the list of annotations (read only)
//   @P next            Segment           the next segment in the whole score; null at last score segment (read-only)
//   @P nextInMeasure   Segment           the next segment in measure; null at last measure segment (read-only)
//   @P prev            Segment           the previous segment in the whole score; null at first score segment (read-only)
//   @P prevInMeasure   Segment           the previous segment in measure; null at first measure segment (read-only)
//   @P segmentType     enum (Segment.All, .Ambitus, .BarLine, .Breath, .ChordRest, .Clef, .EndBarLine, .Invalid, .KeySig, .KeySigAnnounce, .StartRepeatBarLine, .TimeSig, .TimeSigAnnounce)
//   @P tick            int               midi tick position (read only)
//------------------------------------------------------------------------

struct Segment final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::SEGMENT)

protected:
    EngravingItem *get_element(staff_idx_t staff) const; //??

public:

    ~Segment();

    void set_ownership_parent(DummyParentOr<Measure> parent);

    Segment *clone() const override { return new Segment(*this); }

    void set_score(Score *) override;

    inline bool is_active() const { return !is_time_tick_type() && enabled() && visible(); }

    Segment *next() const { return next_; }

    Segment *next(SegmentType) const;
    Segment *next_active() const;
    Segment *next_enabled() const;
    Segment *next_in_staff(staff_idx_t staff_idx, SegmentType t = SegmentType::CHORD_REST) const;

    void set_next(Segment *e) { next_ = e; }

    Segment *prev() const { return prev_; }

    Segment *prev(SegmentType) const;
    Segment *prev_with_elements_on_track(track_idx_t track_idx, SegmentType seg_type = SegmentType::CHORD_REST) const;
    Segment *prev_active() const;
    Segment *prev_enabled() const;

    void set_prev(Segment *e) { prev_ = e; }

    // don’t stop at measure boundary:
    Segment *next1() const;
    Segment *next1enabled() const;
    Segment *next1_mm() const;
    Segment *next1_m_menabled() const;
    Segment *next1(SegmentType) const;
    Segment *next1_with_elems_on_staff(staff_idx_t staff_idx, SegmentType seg_type = SegmentType::CHORD_REST) const;
    Segment *next1_with_elems_on_track(track_idx_t track_idx, SegmentType seg_type = SegmentType::CHORD_REST) const;
    Segment *next1_mm(SegmentType) const;

    Segment *prev1() const;
    Segment *prev1_with_elems_on_staff(staff_idx_t staff_idx, SegmentType seg_type = SegmentType::CHORD_REST) const;
    Segment *prev1_with_elems_on_track(track_idx_t track_idx, SegmentType seg_type = SegmentType::CHORD_REST) const;
    Segment *prev1enabled() const;
    Segment *prev1_mm() const;
    Segment *prev1_m_menabled() const;
    Segment *prev1(SegmentType) const;
    Segment *prev1_mm(SegmentType) const;

    Segment *next_cr(track_idx_t track = muse::nidx, bool same_staff = false) const;

    ChordRest *next_chord_rest(track_idx_t track, bool backwards = false, bool stop_at_measure_boundary = false) const;

    EngravingItem *element(track_idx_t track) const;

    const std::vector<EngravingItem *> &elist() const { return elist_; }

    std::vector<EngravingItem *> &elist() { return elist_; }

    void remove_element(track_idx_t track);
    void set_element(track_idx_t track, EngravingItem *el);
    void scan_elements(std::function<void(EngravingItem *)> func) override;

    Measure *measure() const { return to_measure(ownership_parent()); }

    System *system() const;

    double x() const override { return ldata()->pos().x(); }

    void insert_staff(staff_idx_t staff);
    void remove_staff(staff_idx_t staff);

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    void swap_elements(track_idx_t i1, track_idx_t i2);

    void sort_staves(std::vector<staff_idx_t> &dst);
    const char *sub_type_name() const;

    static const char *sub_type_name(SegmentType);
    static SegmentType segment_type(ElementType type);

    SegmentType segment_type() const { return segment_type_; }

    void set_segment_type(SegmentType t);

    bool empty() const { return flag(ElementFlag::EMPTY); }

    bool written() const { return flag(ElementFlag::WRITTEN); }

    void set_written(bool val) const { set_flag(ElementFlag::WRITTEN, val); }

    bool end_of_measure_change() const {
        return flag(ElementFlag::END_OF_MEASURE_CHANGE);
    } // Key/time sigs which should be placed at the end of the measure

    void set_end_of_measure_change(bool val) const { set_flag(ElementFlag::END_OF_MEASURE_CHANGE, val); }

    void fix_staff_idx();

    double stretch() const { return stretch_; }

    void set_stretch(double v) { stretch_ = v; }

    Fraction rtick() const override { return tick_; }

    void set_rtick(const Fraction &v) {
        assert(v >= Fraction(0, 1));
        tick_ = v;
    }

    Fraction tick() const override;

    Fraction ticks() const { return ticks_; }

    void set_ticks(const Fraction &v) { ticks_ = v; }

    double width_in_staff(staff_idx_t staff_idx, SegmentType next_seg_type = SegmentType::CHORD_REST) const;
    Fraction ticks_in_staff(staff_idx_t staff_idx) const;

    bool splits_tuplet() const;

    const std::vector<EngravingItem *> &annotations() const { return annotations_; }

    // Display-only storage operations: no staff/instrument cache registration.
    void add_layout_annotation(EngravingItem *item);
    void remove_layout_copy(EngravingItem *item);
    void clear_annotations();
    void remove_annotation(EngravingItem *e);
    bool has_annotation_or_element(ElementType type, track_idx_t min_track, track_idx_t max_track) const;
    bool has_annotation_or_element(staff_idx_t staff_idx) const;
    EngravingItem *find_annotation(ElementType type, track_idx_t min_track, track_idx_t max_track) const;
    std::vector<EngravingItem *> find_annotations(ElementType type, track_idx_t min_track, track_idx_t max_track) const;
    bool has_elements() const;
    bool has_elements(track_idx_t min_track, track_idx_t max_track) const;
    bool has_elements(staff_idx_t staff_idx) const;
    bool all_elements_invisible() const;

    Spatium extra_leading_space() const { return extra_leading_space_; }

    void set_extra_leading_space(Spatium v) { extra_leading_space_ = v; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    bool operator<(const Segment &) const;
    bool operator>(const Segment &) const;

    EngravingItem *first_in_next_segments(staff_idx_t active_staff) const; //<
    EngravingItem *last_in_prev_segments(staff_idx_t active_staff) const;  //<
    EngravingItem *first_element_for_navigation(staff_idx_t staff) const;  //<  These methods are used for navigation
    EngravingItem *last_element_for_navigation(staff_idx_t staff) const;   //<  for next-element and prev-element
    EngravingItem *first_element_of_segment(staff_idx_t active_staff) const;
    EngravingItem *next_element_of_segment(EngravingItem *e, staff_idx_t active_staff) const;
    EngravingItem *prev_element_of_segment(EngravingItem *e, staff_idx_t active_staff) const;
    EngravingItem *last_element_of_segment(staff_idx_t active_staff) const;
    EngravingItem *next_annotation(EngravingItem *e) const;
    EngravingItem *prev_annotation(EngravingItem *e) const;
    EngravingItem *first_annotation(staff_idx_t active_staff) const;
    EngravingItem *last_annotation(staff_idx_t active_staff) const;
    Spanner *first_spanner(staff_idx_t active_staff) const;
    Spanner *last_spanner(staff_idx_t active_staff) const;
    bool not_chord_rest_type() const;
    using EngravingItem::next_element;
    EngravingItem *next_element(staff_idx_t active_staff);
    using EngravingItem::prev_element;
    EngravingItem *prev_element(staff_idx_t active_staff);

    EngravingItem *first_element(staff_idx_t staff_idx) const;

    std::vector<Shape> shapes() { return shapes_; }

    const std::vector<Shape> &shapes() const { return shapes_; }

    const Shape &staff_shape(staff_idx_t staff_idx) const { return shapes_[staff_idx]; }

    Shape &staff_shape(staff_idx_t staff_idx) { return shapes_[staff_idx]; }

    void create_shapes();
    void create_shape(staff_idx_t staff_idx);
    double min_right() const;
    double min_left() const;

    double width_offset() const { return width_offset_; }

    void clear_width_offset() { width_offset_ = 0.0; }

    void add_width_offset(double w) { width_offset_ += w; }

    void set_width_offset(double w) { width_offset_ = w; }

    double elements_top_offset_from_skyline(staff_idx_t staff_index) const;
    double elements_bottom_offset_from_skyline(staff_idx_t staff_index) const;

    //! spacing is additional width of segment, for example accidental needs this spacing to avoid overlapping
    void set_spacing(double);
    double spacing() const;

    // some helper function
    ChordRest *cr(track_idx_t track) const { return to_chord_rest(elist_[track]); }

    bool is_type(const SegmentType t) const { return int(segment_type_) & int(t); }

    bool is_just_type(const SegmentType t) const { return segment_type_ == t; }

    bool is_begin_bar_line_type() const { return segment_type_ == SegmentType::BEGIN_BAR_LINE; }

    bool is_clef_type() const { return segment_type_ == SegmentType::CLEF; }

    bool is_header_clef_type() const { return segment_type_ == SegmentType::HEADER_CLEF; }

    bool is_key_sig_type() const { return segment_type_ == SegmentType::KEY_SIG; }

    bool is_ambitus_type() const { return segment_type_ == SegmentType::AMBITUS; }

    bool is_time_sig_type() const { return segment_type_ == SegmentType::TIME_SIG; }

    bool has_time_sig_above_staves() const;
    bool make_space_for_time_sig_above_staves() const;
    bool has_time_sig_across_staves() const;

    bool is_start_repeat_bar_line_type() const { return segment_type_ == SegmentType::START_REPEAT_BAR_LINE; }

    bool is_bar_line_type() const { return segment_type_ == SegmentType::BAR_LINE; }

    bool is_breath_type() const { return segment_type_ == SegmentType::BREATH; }

    bool is_chord_rest_type() const { return segment_type_ == SegmentType::CHORD_REST; }

    bool is_clef_repeat_announce_type() const { return segment_type_ == SegmentType::CLEF_REPEAT_ANNOUNCE; }

    bool is_key_sig_repeat_announce_type() const { return segment_type_ == SegmentType::KEY_SIG_REPEAT_ANNOUNCE; }

    bool is_time_sig_repeat_announce_type() const { return segment_type_ == SegmentType::TIME_SIG_REPEAT_ANNOUNCE; }

    bool is_end_bar_line_type() const { return segment_type_ == SegmentType::END_BAR_LINE; }

    bool is_key_sig_announce_type() const { return segment_type_ == SegmentType::KEY_SIG_ANNOUNCE; }

    bool is_time_sig_announce_type() const { return segment_type_ == SegmentType::TIME_SIG_ANNOUNCE; }

    bool is_courtesy_segment() const {
        return segment_type_ &
               (SegmentType::COURTESY_TIME_SIG_TYPES | SegmentType::COURTESY_KEY_SIG_TYPES | SegmentType::COURTESY_CLEF_TYPES);
    }

    bool is_time_tick_type() const { return segment_type_ == SegmentType::TIME_TICK; }

    bool is_right_aligned() const { return is_clef_type() || is_breath_type(); }

    bool is_mm_rest_segment() const { return is_chord_rest_type() && elist_.front() && elist_.front()->is_mm_rest(); }

    bool can_write_spanner_start_end(track_idx_t track, const Spanner *spanner) const;

    Fraction shortest_chord_rest() const;

    bool has_accidentals() const;

    const std::vector<EngravingItem *> &pre_appended_items() const { return pre_appended_items_; }

    EngravingItem *pre_appended_item(track_idx_t track) const { return pre_appended_items_[track]; }

    void pre_append(EngravingItem *item, track_idx_t track) { pre_appended_items_[track] = item; }

    void clear_pre_appended(track_idx_t track) { pre_appended_items_[track] = nullptr; }

    bool goes_before(const Segment *next_segment) const;

    void check_empty() const;

    double x_pos_in_system_coords() const;
    void set_x_pos_in_system_coords(double x);

    bool is_tuplet_subdivision_on_staff(staff_idx_t staff_idx) const;
    bool is_inside_tuplet_on_staff(staff_idx_t staff_idx) const;

private:

    void add_articulations_to_shape(const Chord *chord, Shape &shape);

    friend struct Factory;
    Segment(DummyParentOr<Measure> m);
    Segment(DummyParentOr<Measure>, SegmentType, const Fraction &);
    Segment(const Segment &);

    void init();
    void check_element(EngravingItem *, track_idx_t track);

    void set_empty(bool val) const { set_flag(ElementFlag::EMPTY, val); }

    SegmentType segment_type_ = SegmentType::INVALID;
    Fraction tick_;  // { Fraction(0, 1) };
    Fraction ticks_; // { Fraction(0, 1) };
    Spatium extra_leading_space_;
    double stretch_ = 1.0;
    double width_offset_ = 0.0; // part of the segment width that will not be stretched during system justification

    Segment *next_ = nullptr; // linked list of segments inside a measure
    Segment *prev_ = nullptr;

    std::vector<EngravingItem *> annotations_;
    std::vector<EngravingItem *> elist_; // EngravingItem storage, size = staves * VOICES.
    std::vector<EngravingItem *>
        pre_appended_items_;    // Container for items appended to the left of this segment (example: grace notes), size = staves * VOICES.
    std::vector<Shape> shapes_; // size = staves
    double spacing_ = 0;
};
} // namespace mu::engraving
