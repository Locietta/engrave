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

    void setMarkerType(MarkerType t);

    MarkerType markerType() const { return markerType_; }

    String markerTypeUserName() const;

    inline bool isSegno() const { return markerType_ == MarkerType::SEGNO || markerType_ == MarkerType::VARSEGNO; }

    inline bool isCoda() const {
        return markerType_ == MarkerType::CODA || markerType_ == MarkerType::VARCODA || markerType_ == MarkerType::CODETTA;
    }

    inline bool isToCoda() const {
        return markerType_ == MarkerType::TOCODA || markerType_ == MarkerType::TOCODASYM || markerType_ == MarkerType::DA_CODA ||
               markerType_ == MarkerType::DA_DBLCODA;
    }

    inline bool isRightMarker() const { return muse::contains(Marker::RIGHT_MARKERS, markerType_); }

    Marker *clone() const override { return new Marker(*this); }

    int subtype() const override { return int(markerType_); }

    TranslatableString subtypeUserName() const override;

    Measure *measure() const { return (Measure *) ownershipParent(); }

    String label() const { return label_; }

    void setLabel(const String &s) { label_ = s; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    bool positionRelativeToNoteheadRest() const override { return false; }

    bool centerOnSymbol() const { return centerOnSymbol_; }

    void setCenterOnSymbol(bool val) { centerOnSymbol_ = val; }

    static constexpr std::array<MarkerType, 5> RIGHT_MARKERS{
        MarkerType::FINE, MarkerType::TOCODA, MarkerType::TOCODASYM, MarkerType::DA_CODA, MarkerType::DA_DBLCODA,
    };

    String symbolString() const;

private:
    MarkerType markerType_ = MarkerType::FINE;
    String label_ = u"fine"; ///< referenced from Jump() element

    bool centerOnSymbol_ = true;
};

struct MarkerTypeTableItem {
    MarkerType type;
    AsciiStringView text;
    AsciiStringView label;
    bool rightAligned;
};

extern const std::vector<MarkerTypeTableItem> markerTypeTable;
} // namespace mu::engraving
