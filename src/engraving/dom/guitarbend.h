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
#include "line.h"
#include "property.h"
#include "textbase.h"

namespace mu::engraving {
enum class GuitarBendType : unsigned char {
    BEND,
    PRE_BEND,
    GRACE_NOTE_BEND,
    SLIGHT_BEND,

    DIVE,
    PRE_DIVE,
    DIP,
    SCOOP,
};

enum class GuitarBendShowHoldLine : unsigned char {
    AUTO,
    SHOW,
    HIDE,
};

enum class ActionIconType : signed char;

class GuitarBend final : public SLine {
    OBJECT_ALLOCATOR(engraving, GuitarBend)
    DECLARE_CLASSOF(ElementType::GUITAR_BEND)

    M_PROPERTY2(DirectionV, direction, setDirection, DirectionV::AUTO)
    M_PROPERTY2(int, bendAmountInQuarterTones, setBendAmountInQuarterTones, 4)
    M_PROPERTY2(GuitarBendShowHoldLine, showHoldLine, setShowHoldLine, GuitarBendShowHoldLine::AUTO)
    M_PROPERTY2(float, startTimeFactor, setStartTimeFactor, 0.f)
    M_PROPERTY2(float, endTimeFactor, setEndTimeFactor, 1.f)

public:
    GuitarBend(EngravingObject *parent);
    GuitarBend(const GuitarBend &);
    ~GuitarBend() override;

    GuitarBend *clone() const override { return new GuitarBend(*this); }

    LineSegment *createLineSegment() override;

    Anchor anchor() const override { return Anchor::NOTE; }

    bool allowTimeAnchor() const override { return false; }

    Note *startNote() const;
    Note *startNoteOfChain() const;

    Note *endNote() const;
    void changeBendAmount(int bendAmount, int startBendAmount);
    void setEndNotePitch(int pitch, int quarterToneOffset);

    bool isReleaseBend() const;
    bool isFullRelease() const;
    bool angledPreBend() const;
    bool isDive() const;
    bool isFullReleaseDive() const;

    static void fixNotesFrettingForGraceBend(Note *grace, Note *main);
    static void fixNotesFrettingForStandardBend(Note *startNote, Note *endNote);
    static Note *createEndNote(Note *startNote, GuitarBendType bendType = GuitarBendType::BEND);

    PropertyValue getProperty(Pid id) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue propertyDefault(Pid id) const override;

    static constexpr int SLACK_BEND_AMOUNT = -33;
    void computeBendAmount();
    int totBendAmountIncludingPrecedingBends() const;
    void computeBendText();
    void computeIsInvalidOrNeedsWarning();

    bool isInvalid() const { return isInvalid_; }

    bool isBorderlineUnplayable() const { return isBorderlineUnplayable_; }

    GuitarBend *overlappingBendOrDive() const;

    GuitarBend *findPrecedingBend() const;
    GuitarBend *findFollowingPreBendOrDive() const;
    WhammyBar *findOverlappingWhammyBar(Fraction startTick, Fraction endTick) const;

    void updateHoldLine();

    void setHoldLine(GuitarBendHold *hold) { holdLine_ = hold; }

    GuitarBendHold *holdLine() const { return holdLine_; }

    double lineWidth() const;

    Color curColor(const rendering::PaintOptions &opt) const override;

    DirectionV diveTabPos() const { return diveTabPos_; }

    void setDiveTabPos(DirectionV v) { diveTabPos_ = v; }

    static void adaptBendsFromTabToStandardStaff(const Staff *staff);

    static GuitarBendType bendTypeFromActionIcon(ActionIconType actionIconType);

    GuitarBendType bendType() const { return bendType_; }

    void setBendType(GuitarBendType t);

    VibratoType dipVibratoType() const { return dipVibratoType_; }

    void setDipVibratoType(VibratoType v) { dipVibratoType_ = v; }

    bool isSlack() const { return isSlack_; }

    void setIsSlack(bool v) { isSlack_ = v; }

    std::optional<float> targetTimeFactor() const { return targetTimeFactor_; }

    void setTargetTimeFactor(float v);

    struct LayoutData : public SLine::LayoutData {
    public:
        bool up() const { return up_; }

        void setUp(bool v) { up_ = v; }

        bool isInside() const { return isInside_; }

        void setIsInside(bool v) { isInside_ = v; }

        const String &bendDigit() const { return bendDigit_; }

        void setBendDigit(const String &s) { bendDigit_ = s; }

        const std::set<int> &diveLevels() const { return diveLevels_; }

        void resetDiveLevels() { diveLevels_.clear(); }

        void setDiveLevels(const std::set<int> &v) { diveLevels_ = v; }

        bool aboveStaff() const { return aboveStaff_; }

        void setAboveStaff(bool v) { aboveStaff_ = v; }

    private:
        bool up_ = true;
        bool isInside_ = false;
        String bendDigit_;
        std::set<int> diveLevels_;
        bool aboveStaff_ = false;
    };
    DECLARE_LAYOUTDATA_METHODS(GuitarBend)

protected:
    bool isInSpannerMap() const override { return false; }

private:
    GuitarBendType bendType_ = GuitarBendType::BEND;
    GuitarBendHold *holdLine_ = nullptr;
    bool isInvalid_ = false;
    bool isBorderlineUnplayable_ = false;
    DirectionV diveTabPos_ = DirectionV::AUTO;
    VibratoType dipVibratoType_ = VibratoType::NONE;
    bool isSlack_ = false;
    std::optional<float> targetTimeFactor_;
};

class GuitarBendText; // forward decl

class GuitarBendSegment final : public LineSegment {
    OBJECT_ALLOCATOR(engraving, GuitarBendSegment)
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_SEGMENT)

public:
    GuitarBendSegment(GuitarBend *sp);
    GuitarBendSegment(const GuitarBendSegment &);
    ~GuitarBendSegment() override;

    GuitarBend *guitarBend() const { return toGuitarBend(spanner()); }

    GuitarBendSegment *clone() const override { return new GuitarBendSegment(*this); }

    PropertyValue getProperty(Pid id) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue propertyDefault(Pid id) const override;
    EngravingObject *propertyDelegate(Pid id) const override;

    PointF vertexPointOff() const { return vertexPointOff_; }

    void setVertexPointOff(PointF p) { vertexPointOff_ = p; }

    void reset() override;

    double lineWidth() const;

    void scanElements(std::function<void(EngravingItem *)> func) override;

    GuitarBendText *bendText() const { return text_; }

    void setBendText(GuitarBendText *t) { text_ = t; }

    bool isUserModified() const override;

    Color curColor(const rendering::PaintOptions &opt) const override { return guitarBend()->curColor(opt); }

    struct LayoutData : public LineSegment::LayoutData {
    public:
        const PointF &vertexPoint() const { return vertexPoint_; }

        void setVertexPoint(PointF p) { vertexPoint_ = p; }

        const PainterPath &path() const { return path_; }

        void setPath(const PainterPath &path) { path_ = path; }

        const PolygonF &arrow() const { return arrow_; }

        PolygonF &arrow() { return arrow_; }

        void setArrow(const PolygonF &a) { arrow_ = a; }

    private:
        PointF vertexPoint_ = PointF(); // Coordinates relative to pos()
        PainterPath path_ = PainterPath();
        PolygonF arrow_ = PolygonF();
    };
    DECLARE_LAYOUTDATA_METHODS(GuitarBendSegment)

private:

    PointF vertexPointOff_ = PointF();
    GuitarBendText *text_ = nullptr;
};

class GuitarBendHold final : public SLine {
    OBJECT_ALLOCATOR(engraving, GuitarBendHold)
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_HOLD)

public:
    GuitarBendHold(GuitarBend *parent);
    GuitarBendHold(const GuitarBendHold &);

    GuitarBendHold *clone() const override { return new GuitarBendHold(*this); }

    Anchor anchor() const override { return Anchor::NOTE; }

    LineSegment *createLineSegment() override;

    PropertyValue propertyDefault(Pid id) const override;

    bool allowTimeAnchor() const override { return false; }

    Note *startNote() const;
    Note *endNote() const;

    GuitarBend *guitarBend() const { return toGuitarBend(ownershipParent()); }

    double lineWidth() const;

protected:
    bool isInSpannerMap() const override { return false; }
};

class GuitarBendHoldSegment final : public LineSegment {
    OBJECT_ALLOCATOR(engraving, GuitarBendHoldSegment)
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_HOLD_SEGMENT)

public:
    GuitarBendHoldSegment(GuitarBendHold *sp);

    GuitarBendHold *guitarBendHold() const { return toGuitarBendHold(spanner()); }

    GuitarBendHoldSegment *clone() const override { return new GuitarBendHoldSegment(*this); }

    GuitarBendSegment *startBendSeg() const { return startBendSeg_; }

    GuitarBendSegment *endBendSeg() const { return endBendSeg_; }

    void setStartBendSeg(GuitarBendSegment *s) { startBendSeg_ = s; }

    void setEndBendSeg(GuitarBendSegment *s) { endBendSeg_ = s; }

    double dashLength() const { return dashLength_; }

    double lineWidth() const { return guitarBendHold()->lineWidth(); }

    struct LayoutData : LineSegment::LayoutData {
    public:
        const SymIdList &symIds() const { return symIds_; }

        void setSymIds(const SymIdList &v) { symIds_ = v; }

    private:
        SymIdList symIds_;
    };
    DECLARE_LAYOUTDATA_METHODS(GuitarBendHoldSegment)

private:

    double dashLength_ = 3.0;

    GuitarBendSegment *startBendSeg_ = nullptr;
    GuitarBendSegment *endBendSeg_ = nullptr;
};

class GuitarBendText final : public TextBase {
    OBJECT_ALLOCATOR(engraving, GuitarBendText)
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_TEXT)

public:
    GuitarBendText(GuitarBendSegment *parent);

    GuitarBendText *clone() const override { return new GuitarBendText(*this); }

    bool positionRelativeToNoteheadRest() const override { return false; }
};
} // namespace mu::engraving
