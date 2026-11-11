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

#include <deque>

#include "../types/types.h"
#include "draw/types/color.h"
#include "engravingitem.h"

namespace mu::engraving {
struct Spanner;

//---------------------------------------------------------
//   @@ SpannerSegment
//!    parent: the Spanner it belongs to; placed on a System during layout
//---------------------------------------------------------

struct SpannerSegment : EngravingItem {

public:

    ~SpannerSegment() override;

    virtual double mag() const override;
    virtual Fraction tick() const override;

    //! The spanner that owns this segment - derived from the ownership parent,
    //! so that it can never go stale.
    Spanner *spanner() const { return to_spanner(ownership_parent()); }

    void set_spanner(Spanner *val);

    void set_spanner_segment_type(SpannerSegmentType s) { spanner_segment_type_ = s; }

    SpannerSegmentType spanner_segment_type() const { return spanner_segment_type_; }

    bool is_single_type() const { return spanner_segment_type() == SpannerSegmentType::SINGLE; }

    bool is_begin_type() const { return spanner_segment_type() == SpannerSegmentType::BEGIN; }

    bool is_single_begin_type() const { return is_single_type() || is_begin_type(); }

    bool is_single_end_type() const { return is_single_type() || is_end_type(); }

    bool is_middle_type() const { return spanner_segment_type() == SpannerSegmentType::MIDDLE; }

    bool is_end_type() const { return spanner_segment_type() == SpannerSegmentType::END; }

    int subtype() const override;
    TranslatableString subtype_user_name() const override;

    //! The system this segment is currently laid out on; null if not placed.
    System *system() const { return system_; }

    //! Records the placement only. Reserved for System itself, which maintains its own
    //! segment list; everyone else must use moveToSystem(), so that a non-null system()
    //! always implies that the segment is in that system's segment list - otherwise the
    //! pointer is left dangling when the system is destroyed.
    void set_system(System *s) { system_ = s; }

    //! Detach from the current system's segment list and attach to the given one.
    void move_to_system(System *s);
    EngravingItem *layout_parent() const override;

    //! A segment is owned by its spanner; a system merely places it, see
    //! setSystem()/moveToSystem(). This overload hides EngravingItem::setOwnershipParent,
    //! so that no other parent can be set by accident.
    void set_ownership_parent(Spanner *spanner);

    const PointF &user_off2() const { return offset2_; }

    void set_user_off2(const PointF &o) { offset2_ = o; }

    void set_user_xoffset2(double x) { offset2_.set_x(x); }

    void set_user_yoffset2(double y) { offset2_.set_y(y); }

    real_t &r_user_xoffset2() { return offset2_.rx(); }

    real_t &r_user_yoffset2() { return offset2_.ry(); }

    void set_pos2(const PointF &p) { p2_ = p; }

    //TODO: rename to spanSegPosWithUserOffset()
    PointF pos2() const { return p2_ + offset2_; }

    //TODO: rename to spanSegPos()
    const PointF &ipos2() const { return p2_; }

    PointF &rpos2() { return p2_; }

    real_t &rxpos2() { return p2_.rx(); }

    real_t &rypos2() { return p2_.ry(); }

    muse::ByteArray mime_data(const PointF &drag_offset) const override;

    void spatium_changed(double ov, double nv) override;

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;
    virtual EngravingObject *property_delegate(Pid) const override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;
    using EngravingObject::undo_change_property;

    PointF default_pos() const override;

    Sid get_property_style(Pid id) const override;
    void reset_property(Pid id) override;
    void style_changed() override;
    void reset() override;

    void set_selected(bool f) override;
    void set_visible(bool f) override;
    void set_color(const Color &col) override;
    void set_z(int val) override;

    bool collect_for_drawing() const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    void trigger_layout() const override;

    std::list<EngravingObject *> link_list_for_property_propagation() const override;
    bool is_property_linked_to_master(Pid id) const override;

    virtual bool is_user_modified() const override;

    bool allow_time_anchor() const override;

protected:

    SpannerSegment(const ElementType &type, Spanner *, ElementFlags f = ElementFlag::ON_STAFF | ElementFlag::MOVABLE);
    SpannerSegment(const SpannerSegment &);

    PointF p2_;
    PointF offset2_;

private:
    String format_bars_and_beats() const override;
    String format_start_bars_and_beats(const Segment *segment) const;
    String format_end_bars_and_beats(const Segment *segment) const;

    System *system_ = nullptr; // current layout placement; not owned, not copied
    SpannerSegmentType spanner_segment_type_ = SpannerSegmentType::SINGLE;
};

//----------------------------------------------------------------------------------
//   @@ Spanner
///   Virtual base class for slurs, ties, lines etc.
//
//    @P anchor         enum (Spanner.CHORD, Spanner.MEASURE, Spanner.NOTE, Spanner.SEGMENT)
//    @P endElement     EngravingItem           the element the spanner end is anchored to (read-only)
//    @P startElement   EngravingItem           the element the spanner start is anchored to (read-only)
//    @P tick           int               tick start position
//    @P tick2          int               tick end position
//----------------------------------------------------------------------------------

struct Spanner : EngravingItem {
public:
    enum class Anchor : unsigned char { SEGMENT, MEASURE, CHORDREST, NOTE };

    ~Spanner() override;

    virtual double mag() const override;

    virtual void set_score(Score *s) override;

    virtual Fraction tick() const override { return tick_; }

    Fraction tick2() const { return tick_ + ticks_; }

    Fraction ticks() const { return ticks_; }

    void set_tick(const Fraction &);
    void set_tick2(const Fraction &);
    void set_ticks(const Fraction &);

    bool is_voice_specific() const;
    track_idx_t track2() const;
    void set_track2(track_idx_t v);
    track_idx_t effective_track2() const;

    bool play_spanner() const { return play_spanner_; }

    void set_play_spanner(bool p) { play_spanner_ = p; }

    virtual Anchor anchor() const = 0;

    const std::vector<SpannerSegment *> &spanner_segments() const { return segments_; }

    void set_spanner_segments(const std::vector<SpannerSegment *> &s) { segments_ = s; }

    SpannerSegment *front_segment() { return segments_.front(); }

    const SpannerSegment *front_segment() const { return segments_.front(); }

    SpannerSegment *back_segment() { return segments_.back(); }

    const SpannerSegment *back_segment() const { return segments_.back(); }

    SpannerSegment *segment_at(int n) { return segments_[n]; }

    const SpannerSegment *segment_at(int n) const { return segments_[n]; }

    size_t nsegments() const { return segments_.size(); }

    bool segments_empty() const { return segments_.empty(); }

    void erase_spanner_segments();
    bool either_end_visible() const;

    virtual void trigger_layout() const override;
    virtual void add(EngravingItem *) override;
    virtual void remove(EngravingItem *) override;
    virtual void removed() override;

    virtual void scan_elements(std::function<void(EngravingItem *)>) override {}

    bool remove_spanner_back();
    virtual void remove_unmanaged();
    virtual void insert_time_unmanaged(const Fraction &tick, const Fraction &len);

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid property_id) const override;

    virtual void compute_start_element();
    void compute_end_element();

    static Note *end_element_from_spanner(Spanner *sp, EngravingItem *new_start);
    static Note *start_element_from_spanner(Spanner *sp, EngravingItem *new_end);
    void set_note_span(Note *start_note, Note *end_note);

    EngravingItem *start_element() const { return start_element_; }

    EngravingItem *end_element() const { return end_element_; }

    Measure *start_measure() const;
    Measure *end_measure() const;

    Measure *find_start_measure() const;
    Measure *find_end_measure() const;

    void set_start_element(EngravingItem *e);
    virtual void set_end_element(EngravingItem *e);

    ChordRest *start_cr();
    ChordRest *end_cr();

    Chord *start_chord();
    Chord *end_chord();

    ChordRest *find_start_cr() const;
    ChordRest *find_end_cr() const;

    Chord *find_start_chord() const;
    Chord *find_end_chord() const;

    Segment *start_segment() const;
    Segment *end_segment() const;

    bool element_applies_to_track(const track_idx_t ref_track) const override;

    virtual void set_selected(bool f) override;
    virtual void set_visible(bool f) override;
    virtual void set_autoplace(bool f) override;
    virtual void set_color(const Color &col) override;
    virtual void set_z(int val) override;
    Spanner *next_spanner(EngravingItem *e, staff_idx_t active_staff);
    Spanner *prev_spanner(EngravingItem *e, staff_idx_t active_staff);
    virtual EngravingItem *next_segment_element() override;
    virtual EngravingItem *prev_segment_element() override;

    using EngravingObject::undo_change_property;

    void push_unused_segment(SpannerSegment *seg);
    SpannerSegment *pop_unused_segment();
    void reuse(SpannerSegment *seg); // called when segment from unusedSegments
                                     // is added back to the spanner.
    int reuse_segments(int number);
    void fixup_segments(unsigned int target_number, std::function<SpannerSegment *()> create_segment);

    bool is_user_modified() const override;

    virtual bool allow_time_anchor() const override { return false; }

protected:

    Spanner(const ElementType &type, EngravingObject *parent, ElementFlags = ElementFlag::NOTHING);
    Spanner(const Spanner &);

    virtual void do_compute_start_element();
    virtual void do_compute_end_element();

    virtual bool is_in_spanner_map() const { return true; }

private:
    bool can_be_cross_staff() const;

    friend struct SpannerSegment;

    EngravingItem *start_element_ = nullptr;
    EngravingItem *end_element_ = nullptr;

    bool play_spanner_ = true;

    Fraction tick_ = Fraction(-1, 1);
    Fraction ticks_ = Fraction(0, 1);
    track_idx_t track2_ = muse::nidx;

    std::vector<SpannerSegment *> segments_;
    std::deque<SpannerSegment *> unused_segments_; // Currently unused segments which can be reused later.
                                                   // We cannot just delete them as they can be referenced
                                                   // in undo stack or other places already.
};
} // namespace mu::engraving
