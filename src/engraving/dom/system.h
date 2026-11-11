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
 Definition of classes SysStaff and System
*/

#include "engravingitem.h"
#include "instrumentname.h"

namespace mu::engraving {
struct Box;
struct Bracket;
struct InstrumentName;
struct MeasureBase;
struct Page;
struct SpannerSegment;
struct StaffVisibilityIndicator;
struct RangeLock;

//---------------------------------------------------------
//   SysStaff
///  One staff in a System.
//---------------------------------------------------------

struct SysStaff {
    SysStaff() {}

    ~SysStaff();

    const std::unordered_map<InstrumentNameRole, InstrumentName *> &instrument_names() const { return instrument_names_; }

    InstrumentName *name(InstrumentNameRole role) const;
    void add_instrument_name(InstrumentName *n);
    void remove_instrument_name(InstrumentNameRole role);

    const RectF &bbox() const { return bbox_; }

    RectF &bbox() { return bbox_; }

    void setbbox(const RectF &r) { bbox_ = r; }

    void setbbox(double x, double y, double w, double h) { bbox_.set_rect(x, y, w, h); }

    double y() const { return bbox_.y() + y_off_; }

    void set_y_off(double offset) { y_off_ = offset; }

    double y_offset() const { return y_off_; }

    double y_bottom() const;

    void save_layout();
    void restore_layout();

    double continuous_dist() const { return continuous_dist_; }

    void set_continuous_dist(double val) { continuous_dist_ = val; }

    bool show() const { return show_; }

    void set_show(bool v) { show_ = v; }

    const Skyline &skyline() const { return skyline_; }

    Skyline &skyline() { return skyline_; }

private:
    std::unordered_map<InstrumentNameRole, InstrumentName *> instrument_names_;

    RectF bbox_; // Bbox of StaffLines.
    Skyline skyline_;
    double y_off_ = 0.0;            // offset of top staff line within bbox
    double y_pos_ = 0.0;            // y position of bbox after System::layout2
    double height_ = 0.0;           // height of bbox after System::layout2
    double continuous_dist_ = -1.0; // distance for continuous mode
    bool show_ = true;              // derived from Staff or false if empty
                                    // staff is hidden
};

//---------------------------------------------------------
//   System
///    One row of measures for all instruments;
///    a complete piece of the timeline.
//---------------------------------------------------------

struct System final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::SYSTEM)

public:
    ~System();

    System *clone() const override { return new System(*this); }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    void change(EngravingItem *o, EngravingItem *n) override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void append_measure(MeasureBase *);
    void remove_measure(MeasureBase *);
    void remove_last_measure();

    //! The page this system is currently laid out on; null if not placed.
    Page *page() const { return page_; }

    void set_page(Page *p) { page_ = p; }

    EngravingItem *layout_parent() const override;

    //! A system is owned by its score; a page merely places it, see setPage().
    //! This overload hides EngravingItem::setOwnershipParent, so that no other
    //! parent can be set by accident.
    void set_ownership_parent(Score *score);

    void clear(); ///< Clear measure list.

    std::vector<SysStaff *> &staves() { return staves_; }

    const std::vector<SysStaff *> &staves() const { return staves_; }

    double staff_ypage(staff_idx_t staff_idx) const;
    double staff_canvas_ypage(staff_idx_t staff_idx) const;
    SysStaff *staff(size_t staff_idx) const;

    bool page_break() const;

    SysStaff *insert_staff(int);
    void remove_staff(int);
    void adjust_staves_number(size_t nstaves);

    staff_idx_t search_staff(double y, staff_idx_t preferred_staff = muse::nidx, double spacing_factor = 0.5) const;
    Fraction snap(const Fraction &tick, const PointF p) const;
    Fraction snap_note(const Fraction &tick, const PointF p, int staff) const;

    const std::vector<MeasureBase *> &measures() const { return ml_; }

    std::vector<MeasureBase *> &measures() { return ml_; }

    MeasureBase *measure(int idx) { return ml_[idx]; }

    MeasureBase *first() const { return ml_.front(); }

    MeasureBase *last() const { return ml_.back(); }

    Measure *first_measure() const;
    Measure *last_measure() const;
    Fraction end_tick() const;

    MeasureBase *next_measure(const MeasureBase *) const;

    double left_margin() const { return left_margin_; }

    void set_left_margin(double val) { left_margin_ = val; }

    double system_height() const { return system_height_; }

    void set_system_height(double h) { system_height_ = h; }

    Box *vbox() const;

    const std::vector<Bracket *> &brackets() const { return brackets_; }

    std::vector<Bracket *> &brackets() { return brackets_; }

    std::list<SpannerSegment *> &spanner_segments() { return spanner_segments_; }

    const std::list<SpannerSegment *> &spanner_segments() const { return spanner_segments_; }

    SystemDivider *system_divider_left() const { return system_divider_left_; }

    SystemDivider *system_divider_right() const { return system_divider_right_; }

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    double min_top() const;
    double min_bottom() const;
    double spacer_distance(bool up) const;
    Spacer *up_spacer(staff_idx_t staff_idx, Spacer *prev_down_spacer) const;
    Spacer *down_spacer(staff_idx_t staff_idx) const;

    double first_note_rest_segment_x(bool leading = false) const;
    double ending_x_for_open_ended_lines() const;
    ChordRest *last_chord_rest(track_idx_t track) const;
    ChordRest *first_chord_rest(track_idx_t track) const;

    bool has_fixed_down_distance() const { return fixed_down_distance_; }

    void set_fixed_down_distance(bool val) const { fixed_down_distance_ = val; }

    staff_idx_t first_visible_staff() const;
    staff_idx_t next_visible_staff(staff_idx_t) const;
    staff_idx_t prev_visible_staff(staff_idx_t) const;
    staff_idx_t last_visible_staff() const;

    double distance() const { return distance_; }

    void set_distance(double d) { distance_ = d; }

    staff_idx_t first_sys_staff_of_part(const Part *part) const;
    staff_idx_t first_visible_sys_staff_of_part(const Part *part) const;
    staff_idx_t first_visible_sys_staff_with_instrument(const String &instrument_id, staff_idx_t start_from);
    staff_idx_t last_sys_staff_of_part(const Part *part) const;
    staff_idx_t last_visible_sys_staff_of_part(const Part *part) const;
    std::vector<staff_idx_t> visible_staves_of_part(const Part *part) const;
    std::vector<Part *> visible_parts_of_group(staff_idx_t start, staff_idx_t end) const;

    size_t get_brackets_columns_count() const;

    void reset_shortest_longest_chord_rest();

    StaffVisibilityIndicator *staff_visibility_indicator() const { return staff_visibility_indicator_; }

    void set_has_staff_visibility_indicator(bool has);

    bool is_locked() const;
    const RangeLock *system_lock() const;

    const std::vector<SystemLockIndicator *> system_lock_indicators() const { return system_lock_indicators_; }

    void add_system_lock_indicator(SystemLockIndicator *sli);
    void delete_system_lock_indicators();

    PageLockIndicator *page_lock_indicator() const { return page_lock_indicator_; }

    void set_page_lock_indicator(PageLockIndicator *pli);
    void delete_page_lock_indicator();

    struct LayoutData : public EngravingItem::LayoutData {
    public:
        bool use_long_names() const { return use_long_names_; }

        void set_use_long_names(bool v) { use_long_names_ = v; }

        double instrument_name_offset() const { return instrument_name_offset_; }

        void set_instrument_name_offset(double v) { instrument_name_offset_ = v; }

        double first_column_width() const { return first_column_width_; }

        void set_first_column_width(double v) { first_column_width_ = v; }

        double second_column_width() const { return second_column_width_; }

        void set_second_column_width(double v) { second_column_width_ = v; }

        double total_names_width() const { return total_names_width_; }

        void set_total_names_width(double v) { total_names_width_ = v; }

        const std::unordered_map<staff_idx_t, double> &group_brackets_width() const { return group_brackets_width_; }

        void set_group_brackets_width_at_staff_idx(staff_idx_t i, double w) { group_brackets_width_[i] = w; }

        double group_brackets_width_at_staff_idx(staff_idx_t i) const {
            return group_brackets_width_.count(i) ? group_brackets_width_.at(i) : 0.0;
        }

        void clear_group_brackets_width() { group_brackets_width_.clear(); }

        const std::unordered_map<Part *, InstrumentName *> &parts_with_group_name() const { return parts_with_group_name_; }

        void add_part_with_group_names(Part *p, InstrumentName *n) { parts_with_group_name_.emplace(p, n); }

        void clear_parts_with_group_names() { parts_with_group_name_.clear(); }

    private:
        bool use_long_names_ = false;
        double instrument_name_offset_ = 0.0;

        double first_column_width_ = 0.0;
        double second_column_width_ = 0.0;
        double total_names_width_ = 0.0;

        std::unordered_map<staff_idx_t, double> group_brackets_width_;

        std::unordered_map<Part *, InstrumentName *> parts_with_group_name_;
    };
    DECLARE_LAYOUTDATA_METHODS(System)

private:
    friend struct Factory;

    System(Score *parent);

    staff_idx_t first_visible_sys_staff() const;
    staff_idx_t last_visible_sys_staff() const;

    staff_idx_t first_visible_staff_from(staff_idx_t start_staff_idx) const;

    Page *page_ = nullptr; // current layout placement; not owned, not copied

    SystemDivider *system_divider_left_ = nullptr; // to the next system
    SystemDivider *system_divider_right_ = nullptr;

    std::vector<MeasureBase *> ml_;
    std::vector<SysStaff *> staves_;
    std::vector<Bracket *> brackets_;
    std::list<SpannerSegment *> spanner_segments_;
    std::vector<SystemLockIndicator *> system_lock_indicators_;
    PageLockIndicator *page_lock_indicator_ = nullptr;

    StaffVisibilityIndicator *staff_visibility_indicator_ = nullptr;

    double left_margin_ = 0.0; // left margin for instrument name, brackets etc.
    mutable bool fixed_down_distance_ = false;
    double distance_ = 0.0; // temp. variable used during layout
    double system_height_ = 0.0;
};

typedef std::vector<System *>::iterator iSystem;
typedef std::vector<System *>::const_iterator ciSystem;
} // namespace mu::engraving
