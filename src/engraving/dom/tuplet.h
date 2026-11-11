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

#include <set>

#include "../types/types.h"
#include "durationelement.h"
#include "property.h"

namespace mu::engraving {
struct Text;
struct Spanner;

//------------------------------------------------------------------------
//   @@ Tuplet
//!     Example of 1/8 triplet:
//!       _baseLen     = 1/8  (tuplet is measured in eighth notes)
//!       _ratio       = 3/2  (3 eighth tuplet notes played in the space of 2 regular eighth notes)
//!
//!    Entire tuplet has a duration of _baseLen * _ratio.denominator().
//!    A single tuplet note has duration of _baseLen * _ratio.denominator() / _ratio.numerator().
//------------------------------------------------------------------------

struct Tuplet final : DurationElement {
private:
    DECLARE_CLASSOF(ElementType::TUPLET)

public:
    Tuplet(DummyParentOr<Measure> parent);
    Tuplet(const Tuplet &);
    ~Tuplet();

    void set_ownership_parent(DummyParentOr<Measure> parent);

    Tuplet *clone() const override { return new Tuplet(*this); }

    void set_track(track_idx_t val) override;

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    Text *number() const { return number_; }

    void set_number(Text *t) { number_ = t; }

    void reset_number_property();
    static void reset_number_property(Text *number);

    void set_selected(bool f) override;

    Measure *measure() const override { return to_measure(ownership_parent()); }

    TupletNumberType number_type() const { return number_type_; }

    TupletBracketType bracket_type() const { return bracket_type_; }

    void set_number_type(TupletNumberType val) { number_type_ = val; }

    void set_bracket_type(TupletBracketType val) { bracket_type_ = val; }

    bool has_bracket() const { return has_bracket_; }

    void set_has_bracket(bool b) { has_bracket_ = b; }

    Spatium bracket_width() const { return bracket_width_; }

    void set_bracket_width(Spatium s) { bracket_width_ = s; }

    const Fraction &ratio() const { return ratio_; }

    void set_ratio(const Fraction &r) { ratio_ = r; }

    void set_user_point1(PointF p) { user_p1_ = p; }

    void set_user_point2(PointF p) { user_p2_ = p; }

    const std::vector<DurationElement *> &elements() const { return current_elements_; }

    void clear() { current_elements_.clear(); }

    bool contains(const DurationElement *el) const {
        return std::find(current_elements_.begin(), current_elements_.end(), el) != current_elements_.end();
    }

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void reset() override;

    TDuration base_len() const { return base_len_; }

    void set_base_len(const TDuration &d) { base_len_ = d; }

    void dump() const override;

    void set_direction(DirectionV d) { direction_ = d; }

    DirectionV direction() const { return direction_; }

    bool is_up() const { return is_up_; }

    void set_is_up(bool val) { is_up_ = val; }

    bool is_small() const { return is_small_; }

    void set_is_small(bool val) { is_small_ = val; }

    Fraction tick() const override { return tick_; }

    Fraction rtick() const override;

    void set_tick(const Fraction &v) { tick_ = v; }

    Fraction elements_duration();
    void sort_elements();
    bool cross() const;
    staff_idx_t v_staff_idx() const override;

    const PointF &p1() const { return p1_; }

    PointF &p1() { return p1_; }

    void set_p1(const PointF &p) { p1_ = p; }

    const PointF &p2() const { return p2_; }

    PointF &p2() { return p2_; }

    void set_p2(const PointF &p) { p2_ = p; }

    const PointF &user_p1() const { return user_p1_; }

    const PointF &user_p2() const { return user_p2_; }

    void set_visible(bool f) override;
    void set_color(const Color &col) override;

    EngravingObject *property_delegate(Pid id) const override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;

    void sanitize_tuplet();
    void add_missing_elements();

    bool calc_has_bracket(const DurationElement *cr1, const DurationElement *cr2) const;

    static int compute_tuplet_denominator(int numerator, Fraction total_duration);

    PointF bracket_l[4];
    PointF bracket_r[3];

    EngravingItem *next_element() override;
    EngravingItem *prev_element() override;

private:

    friend struct DurationElement;

    void add_duration_element(DurationElement *de);
    void remove_duration_element(DurationElement *de);

    Fraction add_missing_element(const Fraction &start_tick, const Fraction &end_tick);

    // All DurationElements where `tuplet()` returns this tuplet
    std::set<DurationElement *> all_elements_;

    // Those DurationElements that are currently really part of this tuplet
    std::vector<DurationElement *> current_elements_;

    bool being_destructed_ = false;

    DirectionV direction_ = DirectionV::AUTO;
    TupletNumberType number_type_ = TupletNumberType::SHOW_NUMBER;
    TupletBracketType bracket_type_ = TupletBracketType::AUTO_BRACKET;
    Spatium bracket_width_;

    bool has_bracket_ = false;
    Fraction ratio_;
    TDuration base_len_; // 1/8 for a triplet of 1/8

    bool is_up_ = true;
    bool is_small_ = true;

    Fraction tick_;

    PointF p1_, p2_;
    PointF user_p1_, user_p2_; // user offset

    Text *number_ = nullptr;
};
} // namespace mu::engraving
