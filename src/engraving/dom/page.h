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

#include <array>
#include <vector>

#include "bsp.h"
#include "engravingitem.h"
#include "mscore.h"
#include "text.h"

namespace mu::engraving {
class Factory;
class System;
class Text;
class Measure;
class RangeLock;

class Score;
class MeasureBase;

//---------------------------------------------------------
//   @@ Page
//   @P pagenumber int (read only)
//---------------------------------------------------------

class Page final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Page)
    DECLARE_CLASSOF(ElementType::PAGE)

public:
    ~Page() override;

    Page *clone() const override { return new Page(*this); }

    //! A page is owned by its score. This overload hides EngravingItem::setOwnershipParent,
    //! so that no other parent can be set by accident.
    void setOwnershipParent(Score *score);

    EngravingItem *layoutParent() const override;
    EngravingItem *accessibleParentItem() const override;

    const std::vector<System *> &systems() const { return systems_; }

    std::vector<System *> &systems() { return systems_; }

    System *system(size_t idx) { return systems_[idx]; }

    const System *system(size_t idx) const { return systems_.at(idx); }

    MeasureBase *firstMeasureBase() const;
    MeasureBase *lastMeasureBase() const;

    void appendSystem(System *s);

    page_idx_t pageNumber() const { return pageNumber_; }

    void setPageNumber(page_idx_t n) { pageNumber_ = n; }

    int getDisplayPageNumber() const;
    bool isOdd() const;
    double tm() const; // margins in pixel
    double bm() const;
    double lm() const;
    double rm() const;

    void scanElements(std::function<void(EngravingItem *)> func) override;
    EngravingItemList accessibleChildren() const override;

    std::vector<EngravingItem *> items(const RectF &r);
    std::vector<EngravingItem *> items(const PointF &p);

    void invalidateBspTree() { bspTreeValid_ = false; }

    PointF pagePos() const override { return PointF(); } ///< position in page coordinates

    std::vector<EngravingItem *> elements() const; ///< list of visible elements
    RectF tbbox() const;                           // tight bounding box, excluding white space
    Fraction endTick() const;
    Measure *firstMeasure() const;

    Text *headerText(int index) const { return headerTexts_.at(index); }

    Text *footerText(int index) const { return footerTexts_.at(index); }

    void setHeaderText(int index, Text *t) { headerTexts_.at(index) = t; }

    void setFooterText(int index, Text *t) { footerTexts_.at(index) = t; }

    bool isLocked() const;
    const RangeLock *pageLock() const;


private:
    friend class Factory;
    Page(Score *parent);

    void doRebuildBspTree();

    std::vector<System *> systems_;
    page_idx_t pageNumber_ = 0;

    std::array<Text *, MAX_HEADERS> headerTexts_{};
    std::array<Text *, MAX_FOOTERS> footerTexts_{};

    BspTree bspTree;
    bool bspTreeValid_ = false;
};
} // namespace mu::engraving
