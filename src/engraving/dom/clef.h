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

#ifndef MU_ENGRAVING_CLEF_H
#define MU_ENGRAVING_CLEF_H

/**
 \file
 Definition of classes Clef
*/

#include "engravingitem.h"

namespace mu::engraving {
struct Transaction;
struct Factory;
struct Segment;

static const int no_clef = -1000;

//---------------------------------------------------------
//   ClefTypeList
//---------------------------------------------------------

struct ClefTypeList {
    ClefType concert_clef = ClefType::G;
    ClefType transposing_clef = ClefType::G;

    ClefTypeList() {}

    ClefTypeList(ClefType a, ClefType b) : concert_clef(a), transposing_clef(b) {}

    ClefTypeList(ClefType a) : concert_clef(a), transposing_clef(a) {}

    bool operator==(const ClefTypeList &t) const;
    bool operator!=(const ClefTypeList &t) const;
};

//---------------------------------------------------------
//   ClefInfo
///   Info about a clef.
//---------------------------------------------------------

struct ClefInfo {
    static const ClefInfo clef_table[];

    ClefType type_ = ClefType::INVALID;
    int line_ = 0;         ///< Line positioning on the staff
    int pitch_offset_ = 0; ///< Pitch offset for line 0.
    signed char lines_[14];
    SymId sym_id_ = SymId::NO_SYM;
    StaffGroup staff_group_ = StaffGroup::STANDARD;

    static int line(ClefType t) { return ::mu::engraving::ClefInfo::clef_table[int(t)].line_; }

    static int pitch_offset(ClefType t) { return ::mu::engraving::ClefInfo::clef_table[int(t)].pitch_offset_; }

    static SymId sym_id(ClefType t) { return ::mu::engraving::ClefInfo::clef_table[int(t)].sym_id_; }

    static const signed char *lines(ClefType t) { return ::mu::engraving::ClefInfo::clef_table[int(t)].lines_; }

    static StaffGroup staff_group(ClefType t) { return ::mu::engraving::ClefInfo::clef_table[int(t)].staff_group_; }
};

//---------------------------------------------------------
//   @@ Clef
///    Graphic representation of a clef.
//
//   @P showCourtesy  bool    show/hide courtesy clef when applicable
//   @P isSmall       bool    small, mid-staff clef (read only, set by layout)
//---------------------------------------------------------

struct Clef final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Clef)
    DECLARE_CLASSOF(ElementType::CLEF)

public:

    Clef *clone() const override { return new Clef(*this); }

    double mag() const override;

    Segment *segment() const { return (Segment *) ownership_parent(); }

    Measure *measure() const { return (Measure *) ownership_parent()->ownership_parent(); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    bool is_small() const { return is_small_; }

    void set_small(bool val);

    bool show_courtesy() const { return show_courtesy_; }

    void set_show_courtesy(bool v) { show_courtesy_ = v; }

    void undo_set_show_courtesy(bool v);
    Clef *other_clef();

    ClefType clef_type() const;
    void set_clef_type(ClefType i);

    bool is_courtesy() const { return is_courtesy_; }

    void set_is_courtesy(const bool v) { is_courtesy_ = v; }

    int subtype() const override { return int(clef_type()); }

    TranslatableString subtype_user_name() const override;

    void set_for_instrument_change(bool for_instrument_change) { for_instrument_change_ = for_instrument_change; }

    bool for_instrument_change() const { return for_instrument_change_; }

    ClefTypeList clef_type_list() const { return clef_types_; }

    ClefType concert_clef() const { return clef_types_.concert_clef; }

    ClefType transposing_clef() const { return clef_types_.transposing_clef; }

    void set_concert_clef(ClefType val);
    void set_transposing_clef(ClefType val);

    void set_clef_type(const ClefTypeList &ctl) { clef_types_ = ctl; }

    void spatium_changed(double old_value, double new_value) override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    void clear();

    void change_clef_to_barline_pos(ClefToBarlinePosition new_pos);

    void undo_change_property(Pid id, const PropertyValue &v) { EngravingObject::undo_change_property(id, v); }

    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) override;

    ClefToBarlinePosition clef_to_barline_position() const { return clef_to_barline_position_; }

    void set_clef_to_barline_position(ClefToBarlinePosition val) { clef_to_barline_position_ = val; }

    bool is_header() const;

    bool is_trailer() const { return is_trailer_; }

    void set_is_trailer(bool val) { is_trailer_ = val; }

    bool is_mid_measure_clef() const;

    bool can_be_excluded_from_other_parts() const override { return !is_header(); }

    void manage_exclusion_from_parts(bool exclude) override;

    struct LayoutData : public EngravingItem::LayoutData {
        SymId sym_id = SymId::NO_SYM;
    };
    DECLARE_LAYOUTDATA_METHODS(Clef)

private:

    friend struct Factory;
    Clef(DummyParentOr<Segment> parent);

    bool show_courtesy_ = true;
    bool is_small_ = false;
    bool for_instrument_change_ = false;
    bool is_courtesy_ = false;
    bool is_trailer_ = false;
    ClefToBarlinePosition clef_to_barline_position_ = ClefToBarlinePosition::AUTO;
    ClefTypeList clef_types_ = ClefType::INVALID;
};
} // namespace mu::engraving
#endif
