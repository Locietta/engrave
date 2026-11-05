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

#include "draw/types/font.h"

#include "engravingitem.h"
#include "../types/types.h"

namespace mu::engraving {
class Note;
class Factory;

//---------------------------------------------------------
//   @@ Bend
//---------------------------------------------------------

/**********************************************************
 *    OBSOLETE CLASS
 *    Used to represent bends before version 4.2. Now
 *    replaced by the GuitarBend class.
 *********************************************************/

enum class BendType : unsigned char { BEND = 0, BEND_RELEASE, BEND_RELEASE_BEND, PREBEND, PREBEND_RELEASE, CUSTOM };

class Bend final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Bend)
    DECLARE_CLASSOF(ElementType::BEND)

public:
    Bend *clone() const override { return new Bend(*this); }

    Note *note() const { return toNote(ownershipParent()); }

    static const char *label[13];

    PitchValues &points() { return points_; }

    const PitchValues &points() const { return points_; }

    void addPoint(const PitchValue &pv) { points_.push_back(pv); }

    void setPoints(const PitchValues &p) { points_ = p; }

    bool playBend() const { return playBend_; }

    void setPlayBend(bool v) { playBend_ = v; }

    String fontFace() const { return fontFace_; }

    void setFontFace(String f) { fontFace_ = f; }

    double fontSize() const { return fontSize_; }

    void setFontSize(double s) { fontSize_ = s; }

    FontStyle fontStyle() const { return fontStyle_; }

    void setFontStyle(FontStyle s) { fontStyle_ = s; }

    Spatium lineWidth() const { return lineWidth_; }

    void setLineWidth(Spatium w) { lineWidth_ = w; }

    muse::draw::Font font(double) const;

    // property methods
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    struct LayoutData : public EngravingItem::LayoutData {
        // cache
        PointF notePos;
        double noteWidth = 0.0;
        double noteHeight = 0.0;
    };
    DECLARE_LAYOUTDATA_METHODS(Bend)

private:
    friend class Factory;

    Bend(DummyParentOr<Note> parent);

    BendType parseBendTypeFromCurve() const;
    void updatePointsByBendType(const BendType bendType);

    bool playBend_ = true;
    PitchValues points_;

    String fontFace_;
    double fontSize_ = 0.0;
    FontStyle fontStyle_ = FontStyle::Undefined;
    Spatium lineWidth_;
};
} // namespace mu::engraving
