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

#include "../types/types.h"
#include "line.h"
#include "textbase.h"

namespace mu::engraving {
class Transaction;

//---------------------------------------------------------
//   Lyrics
//---------------------------------------------------------

class LyricsLine;

class Lyrics final : public TextBase {
    OBJECT_ALLOCATOR(engraving, Lyrics)
    DECLARE_CLASSOF(ElementType::LYRICS)

public:

    // MELISMA FIRST UNDERSCORE:
    // used as_ticks value to mark a melisma for which only the first chord has been spanned so far
    // and to give the user a visible feedback that the underscore has been actually entered;
    // it should be cleared to 0 at some point, so that it will not be carried over
    // if the melisma is not extended beyond a single chord, but no suitable place to do this
    // has been identified yet.
    static constexpr Fraction TEMP_MELISMA_TICKS =
        Fraction::eps(); // THIS WAS A HORRIBLE HACK. At some point we must remove it and replace it with a proper solution. (M.S.)

    ~Lyrics();

    Lyrics *clone() const override { return new Lyrics(*this); }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Segment *segment() const { return toSegment(ownershipParent()->ownershipParent()); }

    Measure *measure() const { return toMeasure(ownershipParent()->ownershipParent()->ownershipParent()); }

    ChordRest *chordRest() const { return toChordRest(ownershipParent()); }

    int subtype() const override { return verse_; }

    TranslatableString subtypeUserName() const override;

    void setVerse(int n) { verse_ = n; }

    int verse() const { return verse_; }

    bool isEven() const { return verse_ % 2; }

    void setSyllabic(LyricsSyllabic s) { syllabic_ = s; }

    LyricsSyllabic syllabic() const { return syllabic_; }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    bool positionRelativeToNoteheadRest() const override { return true; }

    const Fraction &ticks() const { return ticks_; }

    void setTicks(const Fraction &tick) { ticks_ = tick; }

    Fraction endTick() const;
    void removeFromScore();

    void adjustPrevious();

    bool needRemoveInvalidSegments() const { return needRemoveInvalidSegments_; }

    void setNeedRemoveInvalidSegments();
    void removeInvalidSegments();

    LyricsLine *separator() const { return separator_; }

    void setSeparator(LyricsLine *s) { separator_ = s; }

    bool isMelisma() const;

    bool allowTimeAnchor() const override { return false; }

    using EngravingObject::undoChangeProperty;
    void paste(const String &txt) override;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;
    void undoChangeProperty(Pid id, const PropertyValue &, PropertyFlags ps) override;
    void reset() override;
    void triggerLayout() const override;

    double yRelativeToStaff() const;
    void setYRelativeToStaff(double y);

    bool avoidBarlines() const { return avoidBarlines_; }

    void setAvoidBarlines(bool v) { avoidBarlines_ = v; }

private:

    friend class Factory;
    Lyrics(DummyParentOr<ChordRest> parent);
    Lyrics(const Lyrics &);

    int verse_ = 0;  // row index
    Fraction ticks_; // if > 0 then draw an underline to tick() + _ticks (melisma)
    LyricsSyllabic syllabic_ = LyricsSyllabic::SINGLE;
    LyricsLine *separator_ = nullptr;
    bool needRemoveInvalidSegments_ = false;
    bool avoidBarlines_ = true;
};

//---------------------------------------------------------
//   LyricsLine
///   \cond PLUGIN_API \private \endcond
//---------------------------------------------------------

class LyricsLine : public SLine {
    OBJECT_ALLOCATOR(engraving, LyricsLine)
    DECLARE_CLASSOF(ElementType::LYRICSLINE)

public:
    LyricsLine(DummyParentOr<EngravingItem> parent);
    LyricsLine(const LyricsLine &);

    LyricsLine *clone() const override { return new LyricsLine(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *createLineSegment() override;
    void removeUnmanaged() override;

    virtual Lyrics *lyrics() const { return toLyrics(ownershipParent()); }

    Lyrics *nextLyrics() const { return nextLyrics_; }

    void setNextLyrics(Lyrics *l) { nextLyrics_ = l; }

    virtual bool isEndMelisma() const { return lyrics() && lyrics()->ticks().isNotZero(); }

    bool isDash() const { return !isEndMelisma(); }

    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue propertyDefault(Pid id) const override;
    Sid getPropertyStyle(Pid) const override;

protected:
    LyricsLine(const ElementType &type, DummyParentOr<EngravingItem> parent, ElementFlags = ElementFlag::NOTHING);

    bool isInSpannerMap() const override { return false; }

    Lyrics *nextLyrics_ = nullptr;

    void doComputeEndElement() override;
};

//---------------------------------------------------------
//   LyricsLineSegment
///   \cond PLUGIN_API \private \endcond
//---------------------------------------------------------

class LyricsLineSegment : public LineSegment {
    OBJECT_ALLOCATOR(engraving, LyricsLineSegment)
    DECLARE_CLASSOF(ElementType::LYRICSLINE_SEGMENT)

public:
    LyricsLineSegment(LyricsLine *);

    LyricsLineSegment *clone() const override { return new LyricsLineSegment(*this); }

    LyricsLine *lyricsLine() const { return toLyricsLine(spanner()); }

    virtual Lyrics *lyrics() const { return lyricsLine()->lyrics(); }

    virtual double baseLineShift() const;

    virtual int verse() const { return lyrics()->verse(); }

    virtual bool lyricsPlaceAbove() const { return lyrics()->placeAbove(); }

    virtual bool lyricsAddToSkyline() const { return lyrics()->addToSkyline(); }

    virtual double lineSpacing() const { return lyrics()->lineSpacing(); }

    Color color() const override { return lyrics()->color(); }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid propertyId) const override;
    EngravingObject *propertyDelegate(Pid propertyId) const override;

    bool allowTimeAnchor() const override { return false; }

    struct LayoutData : public LineSegment::LayoutData {
    public:
        const std::vector<LineF> &dashes() const { return dashes_; }

        void clearDashes() { dashes_.clear(); }

        void addDash(const LineF &dash) { dashes_.push_back(dash); }
    private:
        std::vector<LineF> dashes_;
    };
    DECLARE_LAYOUTDATA_METHODS(LyricsLineSegment)

protected:
    LyricsLineSegment(const ElementType &type, LyricsLine *sp, ElementFlags f = ElementFlag::NOTHING);
};

class PartialLyricsLine final : public LyricsLine {
    OBJECT_ALLOCATOR(engraving, PartialLyricsLine)
    DECLARE_CLASSOF(ElementType::PARTIAL_LYRICSLINE)

public:
    PartialLyricsLine(DummyParentOr<EngravingItem> parent);
    PartialLyricsLine(const PartialLyricsLine &);

    PartialLyricsLine *clone() const override { return new PartialLyricsLine(*this); }

    LineSegment *createLineSegment() override;

    Lyrics *lyrics() const override { return nullptr; }

    void setIsEndMelisma(bool val) { isEndMelisma_ = val; }

    bool isEndMelisma() const override { return isEndMelisma_; }

    void setVerse(int val) { verse_ = val; }

    int verse() const { return verse_; }

    void setCenterBetweenStaves(AutoOnOff v) { centerBetweenStaves_ = v; }

    AutoOnOff centerBetweenStaves() const { return centerBetweenStaves_; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid propertyId) const override;
    Sid getPropertyStyle(Pid propertyId) const override;
    void undoChangeProperty(Pid id, const PropertyValue &, PropertyFlags ps) override;
    void reset() override;

    Lyrics *findLyricsInPreviousRepeatSeg() const;
    Lyrics *findAdjacentLyricsOrDefault() const;

protected:
    void doComputeEndElement() override;

    bool isInSpannerMap() const override { return true; }

private:
    bool isEndMelisma_ = false;
    int verse_ = 0;
    AutoOnOff centerBetweenStaves_ = AutoOnOff::AUTO;
};

class PartialLyricsLineSegment final : public LyricsLineSegment {
    OBJECT_ALLOCATOR(engraving, PartialLyricsLineSegment)
    DECLARE_CLASSOF(ElementType::PARTIAL_LYRICSLINE_SEGMENT)

public:
    PartialLyricsLineSegment(PartialLyricsLine *);

    LyricsLineSegment *clone() const override { return new PartialLyricsLineSegment(*this); }

    PartialLyricsLine *lyricsLine() const { return toPartialLyricsLine(spanner()); }

    Lyrics *lyrics() const override { return nullptr; }

    int verse() const override { return lyricsLine()->verse(); }

    double lineSpacing() const override;

    bool lyricsPlaceAbove() const override { return lyricsLine()->placeAbove(); } // Delegate?

    bool lyricsAddToSkyline() const override { return lyricsLine()->addToSkyline(); }

    Color color() const override { return lyricsLine()->color(); }

    double baseLineShift() const override;

    EngravingObject *propertyDelegate(Pid) const override;
};
} // namespace mu::engraving
