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

#include <vector>

#include "global/allocator.h"
#include "types/string.h"
#include "../types/types.h"

namespace mu::engraving {
class BspTreeVisitor;
class InsertItemBspTreeVisitor;
class RemoveItemBspTreeVisitor;
class FindItemBspTreeVisitor;

class EngravingItem;

//---------------------------------------------------------
//   BspTree
//    binary space partitioning
//---------------------------------------------------------

class BspTree {
public:
    struct Node {
        enum class Type : char { HORIZONTAL, VERTICAL, LEAF };

        union {
            double offset;
            int leaf_index;
        };

        Type type;
    };
private:

    void initialize(const RectF &rect, int depth, int index);
    void climb_tree(BspTreeVisitor *visitor, const PointF &pos, int index = 0);
    void climb_tree(BspTreeVisitor *visitor, const RectF &rect, int index = 0);

    void nearest_neighbor(const PointF &pos, EngravingItem **best_item, double &best_distance, int node_index = 0);

    RectF rect_for_index(int index) const;

    unsigned int depth_ = 0;
    std::vector<Node> nodes_;
    std::vector<std::vector<EngravingItem *>> leaves_;
    int leaf_cnt_ = 0;
    RectF rect_;

public:
    BspTree();

    void initialize(const RectF &rect, int depth);
    void clear();

    void insert(EngravingItem *item);
    void remove(EngravingItem *item);

    std::vector<EngravingItem *> items(const RectF &rect);
    std::vector<EngravingItem *> items(const PointF &pos);

    EngravingItem *nearest_neighbor(const PointF &pos);

    int leaf_count() const { return leaf_cnt_; }

    inline int first_child_index(int index) const { return index * 2 + 1; }

    inline int parent_index(int index) const { return index > 0 ? ((index & 1) ? ((index - 1) / 2) : ((index - 2) / 2)) : -1; }

#ifndef NDEBUG
    String debug(int index) const;
#endif
};

//---------------------------------------------------------
//   BspTreeVisitor
//---------------------------------------------------------

class BspTreeVisitor {
    OBJECT_ALLOCATOR(engraving, BspTreeVisitor)
public:
    virtual ~BspTreeVisitor() {}

    virtual void visit(std::vector<EngravingItem *> &items) = 0;
};
} // namespace mu::engraving
