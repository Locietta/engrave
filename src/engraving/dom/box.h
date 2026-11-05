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

#include "measurebase.h"
#include "property.h"

namespace mu::engraving {
class Transaction;

//---------------------------------------------------------
//   Box
//   virtual base class for frames "boxes"
//---------------------------------------------------------

class Box : public MeasureBase {
    OBJECT_ALLOCATOR(engraving, Box)

public:
    Box(const ElementType &type, Score *parent);

    virtual bool acceptDrop(EditData &) const override;
    virtual EngravingItem *drop(Transaction &tx, EditData &) override;
    virtual void add(EngravingItem *e) override;
    virtual double absoluteFromSpatium(const Spatium &val) const override;

    RectF contentRect() const;

    Spatium boxWidth() const { return boxWidth_; }

    void setBoxWidth(Spatium val) { boxWidth_ = val; }

    Spatium boxHeight() const { return boxHeight_; }

    void setBoxHeight(Spatium val) { boxHeight_ = val; }

    double leftMargin() const { return leftMargin_; }

    double rightMargin() const { return rightMargin_; }

    double topMargin() const { return topMargin_; }

    double bottomMargin() const { return bottomMargin_; }

    void setLeftMargin(double val) { leftMargin_ = val; }

    void setRightMargin(double val) { rightMargin_ = val; }

    void setTopMargin(double val) { topMargin_ = val; }

    void setBottomMargin(double val) { bottomMargin_ = val; }

    Spatium topGap() const { return topGap_; }

    void setTopGap(Spatium val) { topGap_ = val; }

    Spatium bottomGap() const { return bottomGap_; }

    void setBottomGap(Spatium val) { bottomGap_ = val; }

    bool isAutoSizeEnabled() const { return isAutoSizeEnabled_; }

    void setAutoSizeEnabled(const bool val) { isAutoSizeEnabled_ = val; }

    void copyValues(Box *origin);
    bool isTitleFrame() const;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    // TODO: add a grip for moving the entire box
    // overridden in descendants

    bool canBeExcludedFromOtherParts() const override { return true; }

    void manageExclusionFromParts(bool exclude) override;

private:
    Spatium boxWidth_;  // only valid for HBox
    Spatium boxHeight_; // only valid for VBox
    Spatium topGap_;    // distance from previous system (left border for hbox)
                        // initialized with Sid::systemFrameDistance
    Spatium bottomGap_; // distance to next system (right border for hbox)
                        // initialized with Sid::frameSystemDistance
    double leftMargin_ = 0.0;
    double rightMargin_ = 0.0; // inner margins in metric mm
    double topMargin_ = 0.0;
    double bottomMargin_ = 0.0;
    bool isAutoSizeEnabled_ = true;
};

//---------------------------------------------------------
//   HBox
//   horizontal frame
//---------------------------------------------------------

class HBox final : public Box {
    OBJECT_ALLOCATOR(engraving, HBox)
    DECLARE_CLASSOF(ElementType::HBOX)

public:
    HBox(Score *parent);

    HBox *clone() const override { return new HBox(*this); }

    bool isMovable() const override;
    void computeMinWidth() override;

    bool createSystemHeader() const { return createSystemHeader_; }

    void setCreateSystemHeader(bool val) { createSystemHeader_ = val; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

private:
    bool createSystemHeader_ = true;
};

//---------------------------------------------------------
//   VBox
//   vertical frame
//---------------------------------------------------------

class VBox : public Box {
    OBJECT_ALLOCATOR(engraving, VBox)
    DECLARE_CLASSOF(ElementType::VBOX)

public:
    VBox(Score *parent);

    VBox *clone() const override { return new VBox(*this); }

    void add(EngravingItem *e) override;

    double minHeight() const;
    double maxHeight() const;

    PropertyValue getProperty(Pid propertyId) const override;
    PropertyValue propertyDefault(Pid) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;

    bool acceptDrop(EditData &) const override;

    Spatium paddingToNotationAbove() const { return paddingToNotationAbove_; }

    Spatium paddingToNotationBelow() const { return paddingToNotationBelow_; }

protected:
    VBox(const ElementType &type, Score *parent);

private:
    Spatium paddingToNotationAbove_;
    Spatium paddingToNotationBelow_;
};

//---------------------------------------------------------
//   FBox
//   frame containing fret diagrams
//---------------------------------------------------------

class FBox : public VBox {
    OBJECT_ALLOCATOR(engraving, FBox)
    DECLARE_CLASSOF(ElementType::FBOX)

public:
    FBox(Score *parent);

    FBox *clone() const override { return new FBox(*this); }

    void init();

    void add(EngravingItem *) override;
    void addAtIdx(FretDiagram *fretDiagram, size_t idx);

    double textScale() const { return textScale_; }

    double diagramScale() const { return diagramScale_; }

    Spatium columnGap() const { return columnGap_; }

    Spatium rowGap() const { return rowGap_; }

    int chordsPerRow() const { return chordsPerRow_; }

    AlignH contentHorizontalAlignment() const { return contentAlignmentH_; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &val) override;
    PropertyValue propertyDefault(Pid propertyId) const override;

    void undoReorderElements(const StringList &newOrder);
    void reorderElements(const StringList &newOrder);
    StringList diagramsOrder() const;

    bool needsRebuild() const { return needsRebuild_; }

    void setNeedsRebuild(bool v) { needsRebuild_ = v; }

private:

    void updateInvisibleDiagrams(const StringList &currentDiagrams);
    size_t computeInsertionIdx(const String &nameOfDiagramBeforeThis);

    double textScale_ = 0.0;
    double diagramScale_ = 0.0;
    Spatium columnGap_;
    Spatium rowGap_;
    int chordsPerRow_ = 0;

    bool needsRebuild_ = false;

    AlignH contentAlignmentH_ = AlignH::HCENTER;

    StringList diagramsOrderInScore_;
};

//---------------------------------------------------------
//   TBox
//   Text frame.
//---------------------------------------------------------

class Text;

class TBox : public VBox {
    OBJECT_ALLOCATOR(engraving, TBox)
    DECLARE_CLASSOF(ElementType::TBOX)

public:
    TBox(Score *parent);
    TBox(const TBox &);
    ~TBox() override;

    Text *text() const { return text_; }

    void scanElements(std::function<void(EngravingItem *)> func) override;

    TBox *clone() const override { return new TBox(*this); }

    EngravingItem *drop(Transaction &tx, EditData &) override;
    void add(EngravingItem *e) override;
    void remove(EngravingItem *el) override;

    PropertyValue propertyDefault(Pid) const override;


private:
    Text *text_ = nullptr;
};
} // namespace mu::engraving
