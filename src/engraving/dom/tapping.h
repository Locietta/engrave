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

#include "engraving/types/types.h"

#include "articulation.h"
#include "slur.h"
#include "textbase.h"

namespace mu::engraving {
class TappingHalfSlur;
class TappingText;

class Tapping : public Articulation {
    OBJECT_ALLOCATOR(engraving, Tapping)
    DECLARE_CLASSOF(ElementType::TAPPING)

public:
    Tapping(const Tapping &);

    Tapping *clone() const override { return new Tapping(*this); }

    struct LayoutData : public Articulation::LayoutData {};
    DECLARE_LAYOUTDATA_METHODS(Tapping)

    TappingHand hand() const { return hand_; }

    void setHand(TappingHand h) { hand_ = h; }

    TappingText *text() const { return text_; }

    void setText(TappingText *text) { text_ = text; }

    void setSelected(bool f) override;

    TappingHalfSlur *halfSlurAbove() const { return halfSlurAbove_; }

    void setHalfSlurAbove(TappingHalfSlur *s) { halfSlurAbove_ = s; }

    TappingHalfSlur *halfSlurBelow() const { return halfSlurBelow_; }

    void setHalfSlurBelow(TappingHalfSlur *s) { halfSlurBelow_ = s; }

    LHTappingShowItems lhShowItems() const;

    void styleChanged() override;

    double mag() const override;

    int subtype() const override;

    TranslatableString typeUserName() const override;
    TranslatableString subtypeUserName() const override;

protected:
    friend class mu::engraving::Factory;
    Tapping(DummyParentOr<ChordRest> parent);

private:
    TappingHand hand_ = TappingHand::INVALID;
    TappingText *text_ = nullptr;
    TappingHalfSlur *halfSlurAbove_ = nullptr;
    TappingHalfSlur *halfSlurBelow_ = nullptr;
};

class TappingHalfSlurSegment : public SlurSegment {
    OBJECT_ALLOCATOR(engraving, TappingHalfSlurSegment)
    DECLARE_CLASSOF(ElementType::TAPPING_HALF_SLUR_SEGMENT)

public:
    TappingHalfSlurSegment(TappingHalfSlur *sp);
    TappingHalfSlurSegment(const TappingHalfSlurSegment &other);

    TappingHalfSlurSegment *clone() const override { return new TappingHalfSlurSegment(*this); }

    TappingHalfSlur *tappingHalfSlur() const { return toTappingHalfSlur(spanner()); }
};

class TappingHalfSlur : public Slur {
    OBJECT_ALLOCATOR(engraving, TappingHalfSlur)
    DECLARE_CLASSOF(ElementType::TAPPING_HALF_SLUR)

    bool isHalfSlurAbove() const { return isHalfSlurAbove_; }

    void setIsHalfSlurAbove(bool v) { isHalfSlurAbove_ = v; }

public:
    TappingHalfSlur(EngravingItem *parent);
    TappingHalfSlur(const TappingHalfSlur &);

    TappingHalfSlur *clone() const override { return new TappingHalfSlur(*this); }

    TappingHalfSlurSegment *newSlurTieSegment() override { return new TappingHalfSlurSegment(this); }

    Tapping *tapping() const { return toTapping(ownershipParent()); }

protected:
    bool isInSpannerMap() const override { return false; }

private:
    bool isHalfSlurAbove_ = true;
};

class TappingText final : public TextBase {
    OBJECT_ALLOCATOR(engraving, TappingText)
    DECLARE_CLASSOF(ElementType::TAPPING_TEXT)

public:
    TappingText(Tapping *parent = nullptr);
    TappingText(const TappingText &t);

    TappingText *clone() const override { return new TappingText(*this); }

    Tapping *tapping() const { return toTapping(ownershipParent()); }

    Color curColor(const rendering::PaintOptions &opt) const override;

    bool positionRelativeToNoteheadRest() const override { return true; }
};
} // namespace mu::engraving
