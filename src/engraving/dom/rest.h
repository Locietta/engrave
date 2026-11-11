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

#include "chordrest.h"
#include "containers.h"
#include "notedot.h"

namespace mu::engraving {
struct TDuration;
struct Transaction;

struct RestVerticalClearance {
public:
    void reset() {
        above_ = 1000; // arbitrary high value
        below_ = 1000; // arbitrary high value
        locked_ = false;
    }

    int above() const { return above_; }

    int below() const { return below_; }

    void set_above(int v) { above_ = std::max(std::min(above_, v), 0); }

    void set_below(int v) { below_ = std::max(std::min(below_, v), 0); }

    bool locked() const { return locked_; }

    void set_locked(bool v) { locked_ = v; }

private:
    int above_ = 0.0; // In space units
    int below_ = 0.0; // In space units
    bool locked_ = false;
};

//---------------------------------------------------------
//    @@ Rest
///     This class implements a rest.
//---------------------------------------------------------

struct Rest : ChordRest {
private:
    DECLARE_CLASSOF(ElementType::REST)

public:

    ~Rest() { muse::delete_all(dots_); }

    void hack_to_rest_type();

    Rest &operator=(const Rest &) = delete;

    Rest *clone() const override { return new Rest(*this, false); }

    EngravingItem *linked_clone() override { return new Rest(*this, true); }

    Measure *measure() const override { return ownership_parent() ? to_measure(ownership_parent()->ownership_parent()) : 0; }

    double mag() const override;
    double intrinsic_mag() const override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;
    void set_track(track_idx_t val) override;

    bool is_gap() const { return gap_; }

    virtual void set_gap(bool v) { gap_ = v; }

    virtual void add(EngravingItem *) override;
    virtual void remove(EngravingItem *) override;

    void check_dots();

    NoteDot *dot(int n);
    const std::vector<NoteDot *> &dot_list() const;

    int dot_line() const { return dotline_; }

    void set_dot_line(int l) { dotline_ = l; }

    static int get_dotline(DurationType duration_type);
    bool accent();
    void set_accent(bool flag);

    bool is_whole_rest() const;
    bool is_breve_rest() const;

    DeadSlapped *dead_slapped() const { return dead_slapped_; }

    double right_edge() const override;
    double center_x() const;

    bool align_with_other_rests() const { return align_with_other_rests_; }

    void set_align_with_other_rests(bool v) { align_with_other_rests_ = v; }

    void local_spatium_changed(double old_value, double new_value) override;
    PropertyValue property_default(Pid) const override;
    void reset_property(Pid id) override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue get_property(Pid property_id) const override;
    void undo_change_dots_visible(bool v);

    EngravingItem *next_element() override;
    EngravingItem *prev_element() override;

    bool should_not_be_drawn() const;
    bool debug_draw_gap() const;

    RestVerticalClearance &vertical_clearance() { return vertical_clearance_; }

    struct LayoutData : public ChordRest::LayoutData {
        std::vector<Rest *> merged_rests; // Rests from other voices that may be merged with this
        ld_field<SymId> sym = {"[Rest] sym", SymId::REST_QUARTER};
    };
    DECLARE_LAYOUTDATA_METHODS(Rest)

    SymId get_symbol(DurationType type, int line, int lines) const;
    double sym_width_no_ledger_lines(LayoutData *ldata) const;

protected:
    Rest(const ElementType &type, DummyParentOr<Segment> parent);
    Rest(const ElementType &type, DummyParentOr<Segment> parent, const TDuration &);
    Rest(const Rest &, bool link = false);

    Sid get_property_style(Pid pid) const override;

    virtual RectF number_rect() const { return RectF(); } // TODO: add style to show number over 1-measure rests

private:

    friend struct Factory;
    Rest(DummyParentOr<Segment> parent);
    Rest(DummyParentOr<Segment> parent, const TDuration &);

    double up_pos() const override;
    double down_pos() const override;
    void set_offset(const PointF &o) override;

    // values calculated by layout:

    int dotline_ = -1; // depends on rest symbol
    bool gap_ = false; // invisible and not selectable for user
    std::vector<NoteDot *> dots_;
    DeadSlapped *dead_slapped_ = nullptr;

    RestVerticalClearance vertical_clearance_;

    bool align_with_other_rests_ = true;
};
} // namespace mu::engraving
