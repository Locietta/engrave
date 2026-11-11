/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include <set>
#include <unordered_set>
#include <vector>

#include "../../dom/mscore.h"
#include "../../style/style.h"
#include "../../types/fraction.h"
#include "../../types/types.h"
#include "../layoutoptions.h"
#include "engraving/internal/engravingfont.h"

#define LAYOUT_CALL_CLEAR()
#define LAYOUT_CALL_BEGIN(name, info)
#define LAYOUT_CALL_END()
#define LAYOUT_CALL() \
    if (0) muse::logger::Stream()
#define LAYOUT_ITEM_INFO(item) ""
#define LAYOUT_CALL_PRINT()

namespace mu::engraving {
struct DummyParent;
struct EngravingItem;
struct MeasureBase;
struct Part;
struct Page;
struct Score;
struct Spanner;
struct SpannerMap;
struct System;
struct RangeLocks;
struct Staff;
struct Measure;
struct ChordRest;
struct Segment;
struct BracketItem;
struct PaddingTable;

struct UndoableCommand;
struct EditData;

struct Selection;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct LayoutContext;

struct LayoutConfiguration {
    LayoutConfiguration(LayoutContext *s);

    // Native files retain their view mode; engraving always uses page geometry.
    LayoutMode view_mode() const { return LayoutMode::PAGE; }

    bool is_mode(LayoutMode m) const { return m == LayoutMode::PAGE; }

    bool is_line_mode() const { return is_mode(LayoutMode::LINE); }

    bool is_linear_mode() const { return false; }

    bool is_float_mode() const { return is_mode(LayoutMode::FLOAT); }

    bool is_printing_mode() const;

    bool is_show_v_box() const { return options().is_show_v_box; }

    double note_head_width() const { return options().note_head_width; }

    bool is_show_invisible() const;
    int page_number_offset() const;
    bool is_vertical_spread_enabled() const;
    double max_system_distance() const;
    bool is_show_instrument_names() const;

    std::shared_ptr<const EngravingFont> engraving_font() const;

    const MStyle &style() const;

    const PropertyValue &style_v(Sid idx) const { return style().style_v(idx); }

    Spatium style_s(Sid idx) const { return style().style_s(idx); }

    double style_absolute(Sid idx) const { return style().style_absolute(idx); }

    String style_st(Sid idx) const { return style().style_st(idx); }

    bool style_b(Sid idx) const { return style().style_b(idx); }

    double style_d(Sid idx) const { return style().style_d(idx); }

    int style_i(Sid idx) const { return style().style_i(idx); }

    double spatium() const { return style_d(Sid::SPATIUM); }

    double default_spatium() const { return style().default_spatium(); }

    double point(const Spatium sp) const { return sp.val() * spatium(); }

    double mag_s(double mag) const { return mag * (spatium() / default_spatium()); }

    double lo_width() const { return style_d(Sid::PAGE_WIDTH) * ::mu::engraving::dpi; }

    double lo_height() const { return style_d(Sid::PAGE_HEIGHT) * ::mu::engraving::dpi; }

    bool first_system_indent() const { return style_b(Sid::ENABLE_INDENTATION_ON_FIRST_SYSTEM); }

private:
    const Score *score() const;
    const LayoutOptions &options() const;

    LayoutContext *get_score_ = nullptr;
};

struct DomAccessor {

    DomAccessor(LayoutContext *s);

    // Const access
    const std::vector<Part *> &parts() const;
    size_t visible_part_count() const;
    std::vector<Part *> visible_parts() const;

    size_t npages() const;
    const std::vector<Page *> &pages() const;

    const std::vector<System *> &systems() const;

    size_t nstaves() const;
    size_t visible_staff_count() const;
    const std::vector<Staff *> &staves() const;
    const Staff *staff(staff_idx_t idx) const;
    bool all_staves_invisible() const;

    size_t ntracks() const;

    size_t nmeasures() const;

    const Measure *tick2measure(const Fraction &tick) const;
    const Measure *tick2measure_mm(const Fraction &tick) const;
    const Measure *first_measure() const;
    const Measure *last_measure() const;
    const Measure *last_measure_mm() const;

    const SpannerMap &spanner_map() const;

    const Segment *last_segment() const;

    const ChordRest *find_cr(Fraction tick, track_idx_t track) const;

    const RangeLocks *system_locks() const;
    const RangeLocks *page_locks() const;

    const PaddingTable &padding_table() const;

    const std::vector<BracketItem *> &brackets(staff_idx_t staff_idx) const;

    // Mutable access
    std::vector<Page *> &pages();
    std::vector<System *> &systems();

    MeasureBase *first();
    Measure *first_measure();
    Measure *last_measure();
    Measure *last_measure_mm();
    Measure *tick2measure(const Fraction &tick);
    Measure *tick2measure_mm(const Fraction &tick);

    ChordRest *find_cr(Fraction tick, track_idx_t track);

    // Create/Remove
    const Score *score() const;
    Score *score();
    DummyParent *dummy_parent() const;
    void do_undo_add_element(EngravingItem *);
    void undo_add_element(EngravingItem *item, bool add_to_linked_staves = true, bool ctrl_modifier = false);
    void do_undo_remove_element(EngravingItem *item);
    void undo_remove_element(EngravingItem *item);
    void undo(UndoableCommand *cmd) const;
    void add_element(EngravingItem *item);
    void remove_element(EngravingItem *item);
    void update_locks_on_create_mm_rest(Measure *first, Measure *last);
    void undo_change_parent(EngravingItem *element, EngravingItem *parent, staff_idx_t staff, bool change_links_parents = true);

    void add_unmanaged_spanner(Spanner *s);
    const std::set<Spanner *> &unmanaged_spanners() const;

private:
    LayoutContext *get_score_ = nullptr;
};

struct LayoutState {

    // Const
    bool first_system() const { return first_system_; }

    bool first_system_indent() const { return first_system_indent_; }

    bool start_with_long_names() const { return start_with_long_names_; }

    const Fraction &tick() const { return tick_; }

    const Fraction &start_tick() const { return start_tick_; }

    const Fraction &end_tick() const { return end_tick_; }

    bool is_layout_all() const { return is_layout_all_; }

    const Page *page() const { return page_; }

    page_idx_t page_idx() const { return page_idx_; }

    const std::vector<System *> &system_list() const { return system_list_; }

    const System *prev_system() const { return prev_system_; }

    const System *cur_system() const { return cur_system_; }

    const MeasureBase *prev_measure() const { return prev_measure_; }

    const MeasureBase *cur_measure() const { return cur_measure_; }

    const MeasureBase *next_measure() const { return next_measure_; }

    const MeasureBase *system_old_measure() const { return system_old_measure_; }

    const MeasureBase *page_old_measure() const { return page_old_measure_; }

    int measure_number() const { return measure_number_; }

    bool range_done() const { return range_done_; }

    const std::unordered_set<System *> &systems_needing_centering() const { return systems_needing_centering_; }

    bool must_recompute_headers_footers() const { return must_recompute_headers_footers_; }

    double total_brackets_width() const { return total_brackets_width_; }

    // Mutable
    void set_first_system(bool val) { first_system_ = val; }

    void set_first_system_indent(bool val) { first_system_indent_ = val; }

    void set_start_with_long_names(bool val) { start_with_long_names_ = val; }

    void set_tick(const Fraction &t) { tick_ = t; }

    void set_start_tick(const Fraction &t) { start_tick_ = t; }

    void set_end_tick(const Fraction &t) { end_tick_ = t; }

    void set_is_layout_all(bool v) { is_layout_all_ = v; }

    Page *page() { return page_; }

    void set_page(Page *p) { page_ = p; }

    void set_page_idx(page_idx_t idx) { page_idx_ = idx; }

    std::vector<System *> &system_list() { return system_list_; }

    void set_system_list(const std::vector<System *> &l) { system_list_ = l; }

    System *prev_system() { return prev_system_; }

    void set_prev_system(System *s) { prev_system_ = s; }

    System *cur_system() { return cur_system_; }

    void set_cur_system(System *s) { cur_system_ = s; }

    MeasureBase *prev_measure() { return prev_measure_; }

    void set_prev_measure(MeasureBase *m) { prev_measure_ = m; }

    MeasureBase *cur_measure() { return cur_measure_; }

    void set_cur_measure(MeasureBase *m) { cur_measure_ = m; }

    MeasureBase *next_measure() { return next_measure_; }

    void set_next_measure(MeasureBase *m) { next_measure_ = m; }

    void set_system_old_measure(MeasureBase *m) { system_old_measure_ = m; }

    void set_page_old_measure(MeasureBase *m) { page_old_measure_ = m; }

    void set_measure_number(int n) { measure_number_ = n; }

    void set_range_done(bool val) { range_done_ = val; }

    void add_system_needing_centering(System *s) { systems_needing_centering_.insert(s); }

    void remove_system_needing_centering(System *s) { systems_needing_centering_.erase(s); }

    void clear_systems_needing_centering() { systems_needing_centering_.clear(); }

    void set_must_recompute_headers_footers(bool val) { must_recompute_headers_footers_ = val; }

    void set_total_brackets_width(double val) { total_brackets_width_ = val; }

private:

    bool first_system_ = true;
    bool first_system_indent_ = true;
    bool start_with_long_names_ = true;

    Fraction tick_{0, 1};
    Fraction start_tick_;
    Fraction end_tick_;
    bool is_layout_all_ = false;

    Page *page_ = nullptr;
    page_idx_t page_idx_ = 0; // index in Score->page()s

    std::vector<System *> system_list_; // reusable systems
    System *prev_system_ = nullptr;     // used during page layout
    System *cur_system_ = nullptr;

    MeasureBase *prev_measure_ = nullptr;
    MeasureBase *cur_measure_ = nullptr;
    MeasureBase *next_measure_ = nullptr;
    MeasureBase *system_old_measure_ = nullptr;
    MeasureBase *page_old_measure_ = nullptr;
    int measure_number_ = 0;

    bool range_done_ = false;

    std::unordered_set<System *>
        systems_needing_centering_; // systems which have been laid out and still need SystemLayout::centerElementsBetweenStaves

    bool must_recompute_headers_footers_ =
        false; // we may need to re-compute headers/footers after laying out all pages if they contained a page count

    // cache
    double total_brackets_width_ = -1.0;
};

struct LayoutContext {
    LayoutContext(Score *s);
    ~LayoutContext();

    LayoutContext(const LayoutContext &) = delete;
    LayoutContext &operator=(const LayoutContext &) = delete;

    bool is_valid() const;

    // Conf
    EngravingFontPtr engraving_font() const;

    const LayoutConfiguration &conf() const { return configuration_; }

    // Dom access
    const DomAccessor &dom() const;
    DomAccessor &mut_dom();

    // State
    const LayoutState &state() const;
    LayoutState &mut_state();

    // Other
    const Selection &selection() const;
    void select(EngravingItem *item, SelectType = SelectType::SINGLE, staff_idx_t staff = 0);
    void deselect(EngravingItem *el);

private:

    friend struct LayoutConfiguration;
    friend struct DomAccessor;

    Score *score() { return score_; }

    Score *score_ = nullptr;

    LayoutConfiguration configuration_;
    DomAccessor dom_;
    LayoutState state_;
};
} // namespace mu::engraving::rendering::score
