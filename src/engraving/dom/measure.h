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

/**
 \file
 Definition of class Measure.
*/

#include "measurebase.h"
#include "segmentlist.h"

namespace mu::engraving::read400 {
struct MeasureRead;
} // namespace mu::engraving::read400

namespace mu::engraving::read410 {
struct MeasureRead;
} // namespace mu::engraving::read410

namespace mu::engraving::read460 {
struct MeasureRead;
} // namespace mu::engraving::read460

namespace mu::engraving::read500 {
struct MeasureRead;
} // namespace mu::engraving::read500

namespace mu::engraving::write {
struct MeasureWrite;
} // namespace mu::engraving::write

namespace mu::engraving {
struct AccidentalState;
struct Chord;
struct ChordRest;
struct MMRestRange;
struct MeasureNumber;
struct MeasureRepeat;
struct Note;
struct Part;
struct Score;
struct Spacer;
struct Staff;
struct System;
struct TieMap;
struct Transaction;

//---------------------------------------------------------
//   MeasureNumberMode
//---------------------------------------------------------

enum class MeasureNumberMode : char {
    AUTO, // show measure number depending on style
    SHOW, // always show measure number
    HIDE  // don’t show measure number
};

//---------------------------------------------------------
//   MStaff
///   Per staff values of measure.
//---------------------------------------------------------

struct MStaff {
    MStaff() {}

    ~MStaff();
    MStaff(const MStaff &);

    void set_score(Score *);
    void set_track(track_idx_t);

    MeasureNumber *measure_number() const { return measure_number_; }

    void set_measure_number(MeasureNumber *t) { measure_number_ = t; }

    MMRestRange *mm_range_text() const { return mm_range_text_; }

    void set_mm_range_text(MMRestRange *r) { mm_range_text_ = r; }

    StaffLines *lines() const { return lines_; }

    void set_lines(StaffLines *l) { lines_ = l; }

    Spacer *vspacer_up() const { return vspacer_up_; }

    void set_vspacer_up(Spacer *s) { vspacer_up_ = s; }

    Spacer *vspacer_down() const { return vspacer_down_; }

    void set_vspacer_down(Spacer *s) { vspacer_down_ = s; }

    bool has_voices() const { return has_voices_; }

    void set_has_voices(bool val) { has_voices_ = val; }

    bool visible() const { return visible_; }

    void set_visible(bool val) { visible_ = val; }

    bool stemless() const { return stemless_; }

    void set_stemless(bool val) { stemless_ = val; }

    AutoOnOff hide_if_empty() const { return hide_if_empty_; }

    void set_hide_if_empty(AutoOnOff val) { hide_if_empty_ = val; }

    bool corrupted() const { return corrupted_; }

    void set_corrupted(bool val) { corrupted_ = val; }

    int measure_repeat_count() const { return measure_repeat_count_; }

    void set_measure_repeat_count(int n) { measure_repeat_count_ = n; }

private:
    MeasureNumber *measure_number_ = nullptr; // Measure number text object
    MMRestRange *mm_range_text_ = nullptr;    // Multi measure rest range text object
    StaffLines *lines_ = nullptr;
    Spacer *vspacer_up_ = nullptr;
    Spacer *vspacer_down_ = nullptr;
    bool has_voices_ = false; // indicates that MStaff contains more than one voice,
                              // this changes some layout rules

    bool visible_ = true;
    bool stemless_ = false;
    AutoOnOff hide_if_empty_ = AutoOnOff::AUTO; // whether this MStaff wants its staff to be hidden on its system
                                                // when the staff is empty on that system

    bool corrupted_ = false;
    int measure_repeat_count_ = 0;
};

//---------------------------------------------------------
//   @@ Measure
///    one measure in a system
//
//   @P firstSegment    Segment       the first segment of the measure (read-only)
//   @P lastSegment     Segment       the last segment of the measure (read-only)
//---------------------------------------------------------

struct Measure final : MeasureBase {
private:
    DECLARE_CLASSOF(ElementType::MEASURE)

public:

    ~Measure();

    Measure *clone() const override { return new Measure(*this); }

    void set_score(Score *s) override;
    Measure *clone_measure(Score *, const Fraction &tick, TieMap *);

    void check_measure(staff_idx_t idx, bool use_gap_rests = true);

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    void change(EngravingItem *o, EngravingItem *n) override;
    void spatium_changed(double old_value, double new_value) override;

    bool has_voices(staff_idx_t staff_idx, Fraction stick, Fraction len, bool consider_invisible = false) const;
    bool has_voices(staff_idx_t staff_idx) const;
    void set_has_voices(staff_idx_t staff_idx, bool v);

    StaffLines *staff_lines(staff_idx_t staff_idx) const;
    Spacer *vspacer_down(staff_idx_t staff_idx) const;
    Spacer *vspacer_up(staff_idx_t staff_idx) const;
    void set_staff_visible(staff_idx_t staff_idx, bool visible);
    void set_staff_stemless(staff_idx_t staff_idx, bool stemless);
    AutoOnOff hide_staff_if_empty(staff_idx_t staff_idx) const;
    void set_hide_staff_if_empty(staff_idx_t staff_idx, AutoOnOff hide_if_empty);

    bool corrupted(staff_idx_t staff_idx) const { return mstaves_[staff_idx]->corrupted(); }

    void set_corrupted(staff_idx_t staff_idx, bool val) { mstaves_[staff_idx]->set_corrupted(val); }

    MeasureNumber *measure_number(staff_idx_t staff_idx) const { return mstaves_[staff_idx]->measure_number(); }

    void set_measure_number(staff_idx_t staff_idx, MeasureNumber *t) { mstaves_[staff_idx]->set_measure_number(t); }

    const std::vector<MStaff *> &mstaves() const { return mstaves_; }

    std::vector<MStaff *> &mstaves() { return mstaves_; }

    void set_mm_range_text(staff_idx_t staff_idx, MMRestRange *);
    MMRestRange *mm_range_text(staff_idx_t staff_idx) const;

    void create_staves(staff_idx_t);

    Fraction timesig() const { return timesig_; }

    void set_timesig(const Fraction &f) { timesig_ = f; }

    Fraction stretched_len(const Staff *) const;

    bool is_irregular() const { return timesig_ != len_; }

    int measure_number() const { return measure_number_; }

    void set_measure_number(int n) { measure_number_ = n; }

    int measure_number_offset() const { return measure_number_offset_; }

    void set_measure_number_offset(int n) { measure_number_offset_ = n; }

    bool exclude_from_numbering() const { return flag(ElementFlag::EXCLUDE_FROM_NUMBERING); }

    void set_exclude_from_numbering(bool v) { set_flag(ElementFlag::EXCLUDE_FROM_NUMBERING, v); }

    MeasureNumberMode measure_number_mode() const { return measure_number_mode_; }

    void set_measure_number_mode(MeasureNumberMode v) { measure_number_mode_ = v; }

    bool repeat_end() const { return flag(ElementFlag::REPEAT_END); }

    void set_repeat_end(bool v) { set_flag(ElementFlag::REPEAT_END, v); }

    bool repeat_start() const { return flag(ElementFlag::REPEAT_START); }

    void set_repeat_start(bool v) { set_flag(ElementFlag::REPEAT_START, v); }

    bool repeat_jump() const { return flag(ElementFlag::REPEAT_JUMP); }

    void set_repeat_jump(bool v) { set_flag(ElementFlag::REPEAT_JUMP, v); }

    bool has_courtesy_key_sig() const { return flag(ElementFlag::COURTESY_KEYSIG); }

    void set_has_courtesy_key_sig(bool v) { set_flag(ElementFlag::COURTESY_KEYSIG, v); }

    bool has_courtesy_time_sig() const { return flag(ElementFlag::COURTESY_TIMESIG); }

    void set_has_courtesy_time_sig(bool v) const { set_flag(ElementFlag::COURTESY_TIMESIG, v); }

    bool has_courtesy_clef() const { return flag(ElementFlag::COURTESY_CLEF); }

    void set_has_courtesy_clef(bool v) const { set_flag(ElementFlag::COURTESY_CLEF, v); }

    bool end_of_measure_change() const { return flag(ElementFlag::END_OF_MEASURE_CHANGE); }

    void set_end_of_measure_change(bool val) const { set_flag(ElementFlag::END_OF_MEASURE_CHANGE, val); }

    Segment *first() const { return segments_.first(); }

    Segment *first(SegmentType t) const { return segments_.first(t); }

    Segment *first_enabled() const { return segments_.first(ElementFlag::ENABLED); }

    Segment *first_active() const { return segments_.first_active(); }

    Segment *last() const { return segments_.last(); }

    Segment *last(SegmentType t) const { return segments_.last(t); }

    Segment *last_enabled() const { return segments_.last(ElementFlag::ENABLED); }

    SegmentList &segments() { return segments_; }

    const SegmentList &segments() const { return segments_; }

    double user_stretch() const;

    void set_user_stretch(double v) { user_stretch_ = v; }

    void compute_ticks();
    Fraction anacrusis_offset() const;
    Fraction max_ticks() const;

    bool show_measure_number() const;
    bool show_measure_number_in_auto_mode() const;
    bool show_measure_number_on_staff(staff_idx_t staff_idx) const;

    Chord *find_chord(Fraction tick, track_idx_t track) const;
    ChordRest *find_chord_rest(Fraction tick, track_idx_t track) const;
    Fraction snap(const Fraction &tick, const PointF p) const;
    Fraction snap_note(const Fraction &tick, const PointF p, int staff) const;

    Segment *search_segment(double x, SegmentType st, track_idx_t strack, track_idx_t etrack, const Segment *preferred_segment = nullptr,
                            double spacing_factor = 0.5) const;

    void insert_staff(Staff *, staff_idx_t staff);
    void insert_m_staff(MStaff *staff, staff_idx_t idx);
    void remove_m_staff(MStaff *staff, staff_idx_t idx);

    void move_ticks(const Fraction &diff) override;

    void cmd_remove_staves(staff_idx_t s, staff_idx_t e);
    void cmd_add_staves(staff_idx_t s, staff_idx_t e, bool create_rest);
    void remove_staves(staff_idx_t s, staff_idx_t e);
    void insert_staves(staff_idx_t s, staff_idx_t e);

    double tick2pos(Fraction) const;
    Segment *tick2segment(const Fraction &tick, SegmentType st = SegmentType::CHORD_REST);

    void sort_staves(std::vector<staff_idx_t> &dst);

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    int repeat_count() const { return repeat_count_; }

    void set_repeat_count(int val) { repeat_count_ = val; }

    Segment *find_segment_r(SegmentType st, const Fraction &) const;
    Segment *undo_get_segment_r(SegmentType st, const Fraction &f);
    Segment *get_segment_r(SegmentType st, const Fraction &f);
    Segment *find_first_r(SegmentType st, const Fraction &rtick) const;

    // segment routines with absolute tick values
    Segment *find_segment(SegmentType st, const Fraction &f) const { return find_segment_r(st, f - tick()); }

    Segment *undo_get_segment(SegmentType st, const Fraction &f) { return undo_get_segment_r(st, f - tick()); }

    Segment *get_segment(SegmentType st, const Fraction &f) { return get_segment_r(st, f - tick()); }

    Segment *undo_get_chord_rest_or_time_tick_segment(const Fraction &f);
    Segment *get_chord_rest_or_time_tick_segment(const Fraction &f);

    void connect_tremolo();

    void set_end_bar_line_type(BarLineType val, track_idx_t track, bool visible = true, Color color = Color());

    void scan_elements(std::function<void(EngravingItem *)> func) override;
    void create_voice(int track);
    void adjust_to_len(Fraction, bool append_rests_if_necessary = true);

    AccidentalVal find_accidental(const Note *) const;
    AccidentalVal find_accidental(const Segment *s, staff_idx_t staff_idx, int line, bool &error) const;
    void check_multi_voices(staff_idx_t staff_idx);
    bool has_voice(track_idx_t track) const;
    bool is_empty(staff_idx_t staff_idx) const;
    bool is_cutaway_clef(staff_idx_t staff_idx) const;
    bool is_full_measure_rest() const;
    bool visible(staff_idx_t staff_idx) const;
    bool stemless(staff_idx_t staff_idx) const;
    bool is_final_measure_of_section() const;
    LayoutBreak *section_break_element(bool include_next_frames = true) const;
    bool is_anacrusis() const;
    bool is_first_in_system() const;
    bool is_last_in_system() const;
    bool is_first_in_section() const;

    bool break_multi_measure_rest() const { return break_multi_measure_rest_; }

    void set_break_multi_measure_rest(bool val) { break_multi_measure_rest_ = val; }

    bool empty() const;
    bool is_only_rests(track_idx_t track) const;
    bool is_only_gap_rests(track_idx_t track) const;

    int playback_count() const { return playback_count_; }

    void set_playback_count(int val) { playback_count_ = val; }

    RectF staff_page_bounding_rect(staff_idx_t staff_idx) const;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    void undo_change_property(Pid id, const PropertyValue &new_value);
    void undo_change_property(Pid id, const PropertyValue &new_value, PropertyFlags ps) override;

    bool has_mm_rest() const { return mm_rest_ != nullptr; }

    bool is_mm_rest() const { return mm_rest_count_ > 0; }

    Measure *mm_rest() const { return mm_rest_; }

    Measure *covering_mm_rest_or_this();
    const Measure *covering_mm_rest_or_this() const;

    void set_mm_rest(Measure *m) { mm_rest_ = m; }

    int mm_rest_count() const { return mm_rest_count_; } // number of measures mmRest_ spans

    void set_mm_rest_count(int n) { mm_rest_count_ = n; }

    Measure *mm_rest_first() const;
    Measure *mm_rest_last() const;

    int measure_repeat_count(staff_idx_t staff_idx) const;
    bool contains_measure_repeat(const staff_idx_t staff_idx_from, const staff_idx_t staff_idx_to) const;
    void set_measure_repeat_count(int n, staff_idx_t staff_idx);
    bool is_measure_repeat_group(staff_idx_t staff_idx) const;
    bool is_measure_repeat_group_with_next_m(staff_idx_t staff_idx) const;
    bool is_measure_repeat_group_with_prev_m(staff_idx_t staff_idx) const;
    Measure *first_of_measure_repeat_group(staff_idx_t staff_idx) const; // used to find beginning of group
    MeasureRepeat *measure_repeat_element(staff_idx_t staff_idx) const;  // get measure repeat element from anywhere within group
    int measure_repeat_num_measures(staff_idx_t staff_idx) const;
    bool is_one_measure_repeat(staff_idx_t staff_idx) const;
    bool next_is_one_measure_repeat(staff_idx_t staffidx) const;
    bool prev_is_one_measure_repeat(staff_idx_t staff_idx) const;

    ChordRest *last_chord_rest(track_idx_t track) const;
    ChordRest *first_chord_rest(track_idx_t track) const;

    EngravingItem *next_element_staff(staff_idx_t staff, EngravingItem *from_item = nullptr);
    EngravingItem *prev_element_staff(staff_idx_t staff, EngravingItem *from_item = nullptr);

    double first_note_rest_segment_x(bool leading = false) const;
    double ending_x_for_open_ended_lines() const;

    void style_changed() override;

    const BarLine *end_bar_line() const;
    const BarLine *end_bar_line(staff_idx_t staff_idx, bool first = false) const;
    BarLineType end_bar_line_type() const;
    bool end_bar_line_visible() const;
    const BarLine *start_bar_line() const;
    const BarLine *start_bar_line(staff_idx_t staff_idx, bool first = false) const;
    void trigger_layout() const override;
    void trigger_layout(staff_idx_t staff_idx) const;

    void check_header();
    void check_trailer();
    void check_end_of_measure_change();

    void respace_segments();

    bool can_add_string_tunings(staff_idx_t staff_idx) const;
    bool can_add_staff_type_change(staff_idx_t staff_idx) const;

    struct LayoutData : public MeasureBase::LayoutData {
    private:
        bool need_layout_ = true;
    public:
        bool need_layout() const { return need_layout_; }

        void set_need_layout(bool v) { need_layout_ = v; }
    };
    DECLARE_LAYOUTDATA_METHODS(Measure)

private:

    friend struct Factory;

    Measure(Score *parent);
    Measure(const Measure &);

    void push_back(Segment *e);
    void push_front(Segment *e);

    void fill_gap(const Fraction &pos, const Fraction &len, track_idx_t track, const Fraction &stretch, bool use_gap_rests = true);

    MStaff *mstaff(staff_idx_t staff_index) const;

    std::vector<MStaff *> mstaves_;
    SegmentList segments_;
    Measure *mm_rest_ = nullptr; // multi measure rest which replaces a measure range

    double user_stretch_ = 0.0;

    Fraction timesig_;

    int mm_rest_count_ = 0; // number of measures an mmrest spans

    int playback_count_ = 0; // temp. value used in RepeatList
                             // counts how many times this measure was already played

    int repeat_count_ = 0; // end repeat marker and repeat count

    int measure_number_ = 0; // counting from zero
    int measure_number_offset_ = 0;
    MeasureNumberMode measure_number_mode_ = MeasureNumberMode::AUTO;
    bool break_multi_measure_rest_ = false;
};
} // namespace mu::engraving
