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

#ifndef MU_ENGRAVING_OTTAVA_H
#define MU_ENGRAVING_OTTAVA_H

#include "property.h"
#include "textlinebase.h"
#include "types/translatablestring.h"

namespace mu::engraving {
//---------------------------------------------------------
//   OttavaE
//---------------------------------------------------------

struct OttavaE {
    int offset = 0;
    unsigned start = 0;
    unsigned end = 0;
};

//---------------------------------------------------------
//   OttavaType
//---------------------------------------------------------

enum class OttavaType : unsigned char { OTTAVA_8VA, OTTAVA_8VB, OTTAVA_15MA, OTTAVA_15MB, OTTAVA_22MA, OTTAVA_22MB };

//---------------------------------------------------------
//   OttavaDefault
//---------------------------------------------------------

struct OttavaDefault {
    OttavaType type = OttavaType::OTTAVA_8VA;
    int shift = 0;
    const char *name = nullptr;
    const TranslatableString user_name = TranslatableString();
};

// order is important, should be the same as OttavaType
static const OttavaDefault ottava_default[] = {
    {OttavaType::OTTAVA_8VA, 12, "8va", TranslatableString("engraving/ottavatype", "8va alta")},
    {OttavaType::OTTAVA_8VB, -12, "8vb", TranslatableString("engraving/ottavatype", "8va bassa")},
    {OttavaType::OTTAVA_15MA, 24, "15ma", TranslatableString("engraving/ottavatype", "15ma alta")},
    {OttavaType::OTTAVA_15MB, -24, "15mb", TranslatableString("engraving/ottavatype", "15ma bassa")},
    {OttavaType::OTTAVA_22MA, 36, "22ma", TranslatableString("engraving/ottavatype", "22ma alta")},
    {OttavaType::OTTAVA_22MB, -36, "22mb", TranslatableString("engraving/ottavatype", "22ma bassa")}};

struct Ottava;

//---------------------------------------------------------
//   @@ OttavaSegment
//---------------------------------------------------------

struct OttavaSegment final : TextLineBaseSegment {
private:
    DECLARE_CLASSOF(ElementType::OTTAVA_SEGMENT)

    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;

public:
    OttavaSegment(Ottava *sp);

    OttavaSegment *clone() const override { return new OttavaSegment(*this); }

    Ottava *ottava() const { return (Ottava *) spanner(); }

    EngravingObject *property_delegate(Pid) const override;

    bool can_be_excluded_from_other_parts() const override { return true; }

private:
    void rebase_offsets_on_anchor_changed(Grip grip, const PointF &old_pos, System *sys) override;
};

//---------------------------------------------------------
//   @@ Ottava
//   @P ottavaType  enum (Ottava.OTTAVA_8VA, .OTTAVA_8VB, .OTTAVA_15MA, .OTTAVA_15MB, .OTTAVA_22MA, .OTTAVA_22MB)
//---------------------------------------------------------

struct Ottava final : TextLineBase {
private:
    DECLARE_CLASSOF(ElementType::OTTAVA)

public:
    Ottava(DummyParentOr<EngravingItem> parent);
    Ottava(const Ottava &);

    Ottava *clone() const override { return new Ottava(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    void set_ottava_type(OttavaType val);

    OttavaType ottava_type() const { return ottava_type_; }

    int subtype() const override { return int(ottava_type()); }

    TranslatableString subtype_user_name() const override;

    bool numbers_only() const { return numbers_only_; }

    void set_numbers_only(bool val);

    void set_placement(PlacementV);

    LineSegment *create_line_segment() override;
    int pitch_shift() const;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    static const char *ottava_type_name(OttavaType type);

    PointF line_pos(Grip grip, System **system) const override;

    bool allow_time_anchor() const override { return false; }

protected:
    Sid default_pos_sid() const override;
    void do_compute_end_element() override;

private:

    friend struct OttavaSegment;

    void update_styled_properties();
    Sid get_property_style(Pid) const override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;

    OttavaType ottava_type_ = OttavaType::OTTAVA_8VA;
    bool numbers_only_ = false;
};
} // namespace mu::engraving

#endif
