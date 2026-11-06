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

#include "textbase.h"

namespace mu::engraving {
//---------------------------------------------------------
//   @@ Marker
//
//   @P label       string
//   @P markerType  enum (Marker.CODA, .CODETTA, .FINE, .SEGNO, .TOCODA, .USER, .VARCODA, .VARSEGNO)
//---------------------------------------------------------

class Marker final : public TextBase {
    OBJECT_ALLOCATOR(engraving, Marker)
    DECLARE_CLASSOF(ElementType::MARKER)

public:
    Marker(DummyParentOr<EngravingItem> parent);
    Marker(DummyParentOr<EngravingItem> parent, TextStyleType);

    void set_marker_type(MarkerType t);

    MarkerType marker_type() const { return marker_type_; }

    String marker_type_user_name() const;

    inline bool is_segno() const { return marker_type_ == MarkerType::SEGNO || marker_type_ == MarkerType::VARSEGNO; }

    inline bool is_coda() const {
        return marker_type_ == MarkerType::CODA || marker_type_ == MarkerType::VARCODA || marker_type_ == MarkerType::CODETTA;
    }

    inline bool is_to_coda() const {
        return marker_type_ == MarkerType::TOCODA || marker_type_ == MarkerType::TOCODASYM || marker_type_ == MarkerType::DA_CODA ||
               marker_type_ == MarkerType::DA_DBLCODA;
    }

    inline bool is_right_marker() const { return muse::contains(Marker::right_markers, marker_type_); }

    Marker *clone() const override { return new Marker(*this); }

    int subtype() const override { return int(marker_type_); }

    TranslatableString subtype_user_name() const override;

    Measure *measure() const { return (Measure *) ownership_parent(); }

    String label() const { return label_; }

    void set_label(const String &s) { label_ = s; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    bool position_relative_to_notehead_rest() const override { return false; }

    bool center_on_symbol() const { return center_on_symbol_; }

    void set_center_on_symbol(bool val) { center_on_symbol_ = val; }

    static constexpr std::array<MarkerType, 5> right_markers{
        MarkerType::FINE, MarkerType::TOCODA, MarkerType::TOCODASYM, MarkerType::DA_CODA, MarkerType::DA_DBLCODA,
    };

    String symbol_string() const;

private:
    MarkerType marker_type_ = MarkerType::FINE;
    String label_ = u"fine"; ///< referenced from Jump() element

    bool center_on_symbol_ = true;
};

struct MarkerTypeTableItem {
    MarkerType type;
    AsciiStringView text;
    AsciiStringView label;
    bool right_aligned;
};

extern const std::vector<MarkerTypeTableItem> marker_type_table;
} // namespace mu::engraving
