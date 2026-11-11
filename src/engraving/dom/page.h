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
struct Factory;
struct System;
struct Text;
struct Measure;
struct RangeLock;

struct Score;
struct MeasureBase;

//---------------------------------------------------------
//   @@ Page
//   @P pagenumber int (read only)
//---------------------------------------------------------

struct Page final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::PAGE)

public:
    ~Page() override;

    Page *clone() const override { return new Page(*this); }

    //! A page is owned by its score. This overload hides EngravingItem::setOwnershipParent,
    //! so that no other parent can be set by accident.
    void set_ownership_parent(Score *score);

    EngravingItem *layout_parent() const override;

    const std::vector<System *> &systems() const { return systems_; }

    std::vector<System *> &systems() { return systems_; }

    System *system(size_t idx) { return systems_[idx]; }

    const System *system(size_t idx) const { return systems_.at(idx); }

    MeasureBase *first_measure_base() const;
    MeasureBase *last_measure_base() const;

    void append_system(System *s);

    page_idx_t page_number() const { return page_number_; }

    void set_page_number(page_idx_t n) { page_number_ = n; }

    int get_display_page_number() const;
    bool is_odd() const;
    double tm() const; // margins in pixel
    double bm() const;
    double lm() const;
    double rm() const;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    std::vector<EngravingItem *> items(const RectF &r);
    std::vector<EngravingItem *> items(const PointF &p);

    void invalidate_bsp_tree() { bsp_tree_valid_ = false; }

    PointF page_pos() const override { return PointF(); } ///< position in page coordinates

    std::vector<EngravingItem *> elements() const; ///< list of visible elements
    RectF tbbox() const;                           // tight bounding box, excluding white space
    Fraction end_tick() const;
    Measure *first_measure() const;

    Text *header_text(int index) const { return header_texts_.at(index); }

    Text *footer_text(int index) const { return footer_texts_.at(index); }

    void set_header_text(int index, Text *t) { header_texts_.at(index) = t; }

    void set_footer_text(int index, Text *t) { footer_texts_.at(index) = t; }

    bool is_locked() const;
    const RangeLock *page_lock() const;


private:
    friend struct Factory;
    Page(Score *parent);

    void do_rebuild_bsp_tree();

    std::vector<System *> systems_;
    page_idx_t page_number_ = 0;

    std::array<Text *, ::mu::engraving::max_headers> header_texts_{};
    std::array<Text *, ::mu::engraving::max_footers> footer_texts_{};

    BspTree bsp_tree;
    bool bsp_tree_valid_ = false;
};
} // namespace mu::engraving
