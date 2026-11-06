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

#include "groups.h"

namespace mu::engraving {
class Segment;
class Transaction;

//---------------------------------------------------------
//   TimeSigType
//---------------------------------------------------------

enum class TimeSigType : unsigned char {
    NORMAL,     // use sz/sn text
    FOUR_FOUR,  // common time (4/4)
    ALLA_BREVE, // cut time (2/2)
    CUT_BACH,   // cut time (Bach)
    CUT_TRIPLE, // cut triple time (9/8)
};

//---------------------------------------------------------------------------------------
//   @@ TimeSig
///    This class represents a time signature.
//---------------------------------------------------------------------------------------

class TimeSig final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, TimeSig)
    DECLARE_CLASSOF(ElementType::TIMESIG)

    M_PROPERTY2(bool, is_courtesy, set_is_courtesy, false)

public:

    void set_ownership_parent(DummyParentOr<Segment> parent);

    String ssig() const;
    void set_s_sig(const String &);

    TimeSig *clone() const override { return new TimeSig(*this); }

    TimeSigType time_sig_type() const { return time_sig_type_; }

    int subtype() const override;
    TranslatableString subtype_user_name() const override;

    bool operator==(const TimeSig &) const;

    bool operator!=(const TimeSig &ts) const { return !(*this == ts); }

    double mag() const override;

    Fraction sig() const { return sig_; }

    void set_sig(const Fraction &f, TimeSigType st = TimeSigType::NORMAL);

    int numerator() const { return sig_.numerator(); }

    int denominator() const { return sig_.denominator(); }

    Fraction stretch() const { return stretch_; }

    void set_stretch(const Fraction &s) { stretch_ = s; }

    int numerator_stretch() const { return stretch_.numerator(); }

    int denominator_stretch() const { return stretch_.denominator(); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Segment *segment() const { return (Segment *) ownership_parent(); }

    Measure *measure() const { return (Measure *) ownership_parent()->ownership_parent(); }

    bool show_courtesy_sig() const { return show_courtesy_sig_; }

    void set_show_courtesy_sig(bool v) { show_courtesy_sig_ = v; }

    const String &numerator_string() const { return numerator_string_; }

    void set_numerator_string(const String &);

    const String &denominator_string() const { return denominator_string_; }

    void set_denominator_string(const String &);

    bool large_parentheses() const { return large_parentheses_; }

    void set_large_parentheses(bool v) { large_parentheses_ = v; }

    void set_from(const TimeSig *);

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    const Groups &groups() const { return groups_; }

    void set_groups(const Groups &e) { groups_ = e; }

    bool is_local() const { return stretch_ != Fraction(1, 1); }

    PointF staff_offset() const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    void init_element_style(const ElementStyle *) override;
    void style_changed() override;
    Sid get_property_style(Pid id) const override;

    bool show_on_this_staff() const;
    bool is_above_staves() const;
    bool is_across_staves() const;
    TimeSigPlacement time_sig_placement() const;
    TimeSigStyle time_sig_style() const;
    double num_dist() const;
    double y_pos() const;

    const ScaleF &scale() const { return scale_; }

    void set_scale(const ScaleF &s) { scale_ = s; } // TODO: think about what to do with this

    struct LayoutData : public EngravingItem::LayoutData {
        SymIdList ns;
        SymIdList ds;
        PointF pz;
        PointF pn;
        PointF point_large_left_paren;
        PointF point_large_right_paren;
    };
    DECLARE_LAYOUTDATA_METHODS(TimeSig)

protected:
    void added() override;
    void removed() override;

private:

    friend class Factory;
    TimeSig(DummyParentOr<Segment> parent);

    String numerator_string_; // calculated from actualSig() if !customText
    String denominator_string_;

    Fraction sig_;
    Fraction stretch_; // localSig / globalSig
    Groups groups_;

    ScaleF scale_ = ScaleF(1.0, 1.0);
    TimeSigType time_sig_type_ = TimeSigType::NORMAL;
    bool show_courtesy_sig_ = false;
    bool large_parentheses_ = false;
};
} // namespace mu::engraving
