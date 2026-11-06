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

#include <cmath>

#include "bsp.h"
#include "engravingitem.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   InsertItemBspTreeVisitor
//---------------------------------------------------------

struct InsertItemBspTreeVisitor : BspTreeVisitor {
private:
    OBJECT_ALLOCATOR(engraving, InsertItemBspTreeVisitor)
public:
    EngravingItem *item;

    inline void visit(std::vector<EngravingItem *> &items) override { items.push_back(item); }
};

//---------------------------------------------------------
//   RemoveItemBspTreeVisitor
//---------------------------------------------------------

struct RemoveItemBspTreeVisitor : BspTreeVisitor {
private:
    OBJECT_ALLOCATOR(engraving, RemoveItemBspTreeVisitor)
public:
    EngravingItem *item;

    inline void visit(std::vector<EngravingItem *> &items) override { muse::remove(items, item); }
};

//---------------------------------------------------------
//   FindItemBspTreeVisitor
//---------------------------------------------------------

struct FindItemBspTreeVisitor : BspTreeVisitor {
private:
    OBJECT_ALLOCATOR(engraving, FindItemBspTreeVisitor)
public:
    std::vector<EngravingItem *> found_items;

    void visit(std::vector<EngravingItem *> &items) override {
        for (auto item : items) {
            if (!item->item_discovered) {
                item->item_discovered = true;
                found_items.push_back(item);
            }
        }
    }
};

//---------------------------------------------------------
//   BspTree
//---------------------------------------------------------

BspTree::BspTree() : leaf_cnt_(0) { depth_ = 0; }

//---------------------------------------------------------
//   intmaxlog
//---------------------------------------------------------

static inline int intmaxlog(int n) { return n > 0 ? std::max(int(::ceil(::log(double(n)) / ::log(double(2)))), 5) : 0; }

//---------------------------------------------------------
//   initialize
//---------------------------------------------------------

void BspTree::initialize(const RectF &rec, int n) {
    depth_ = intmaxlog(n);
    this->rect_ = rec;
    leaf_cnt_ = 0;

    nodes_.resize((1 << (depth_ + 1)) - 1);
    leaves_.assign(1LL << depth_, std::vector<EngravingItem *>());
    initialize(rec, depth_, 0);
}

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void BspTree::clear() {
    leaf_cnt_ = 0;
    nodes_.clear();
    leaves_.clear();
}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

void BspTree::insert(EngravingItem *element) {
    InsertItemBspTreeVisitor insert_visitor;
    insert_visitor.item = element;
    climb_tree(&insert_visitor, element->page_bounding_rect());
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void BspTree::remove(EngravingItem *element) {
    RemoveItemBspTreeVisitor remove_visitor;
    remove_visitor.item = element;
    climb_tree(&remove_visitor, element->page_bounding_rect());
}

//---------------------------------------------------------
//   items
//---------------------------------------------------------

std::vector<EngravingItem *> BspTree::items(const RectF &rec) {
    FindItemBspTreeVisitor find_visitor;
    climb_tree(&find_visitor, rec);
    std::vector<EngravingItem *> l;
    for (EngravingItem *e : find_visitor.found_items) {
        e->item_discovered = false;
        if (e->page_bounding_rect().intersects(rec)) {
            l.push_back(e);
        }
    }
    return l;
}

//---------------------------------------------------------
//   items
//---------------------------------------------------------

std::vector<EngravingItem *> BspTree::items(const PointF &pos) {
    FindItemBspTreeVisitor find_visitor;
    climb_tree(&find_visitor, pos);

    std::vector<EngravingItem *> l;
    for (EngravingItem *e : find_visitor.found_items) {
        e->item_discovered = false;
        if (e->contains(pos)) {
            l.push_back(e);
        }
    }
    return l;
}

//---------------------------------------------------------
//   nearestNeighbor (public)
//---------------------------------------------------------

EngravingItem *BspTree::nearest_neighbor(const PointF &pos) {
    EngravingItem *nn = nullptr;
    double best_distance = std::numeric_limits<double>::max();
    nearest_neighbor(pos, &nn, best_distance);
    return nn;
}

//---------------------------------------------------------
//   nearestNeighbor (private)
//---------------------------------------------------------

void BspTree::nearest_neighbor(const PointF &pos, EngravingItem **best_item, double &best_distance, int node_index) {
    if (nodes_.empty()) {
        return;
    }

    Node *node = &nodes_[node_index];

    // Base case: go through the items in the leaf node (if any), and update bestItem/bestDistance accordingly
    if (node->type == Node::Type::LEAF) {
        for (auto item : leaves_[node->leaf_index]) {
            PointF item_pos = item->page_bounding_rect().center();
            double curr_distance = std::sqrt(std::pow(pos.x() - item_pos.x(), 2) + std::pow(pos.y() - item_pos.y(), 2));
            if (curr_distance < best_distance) {
                *best_item = item;
                best_distance = curr_distance;
            }
        }
        return;
    }

    // Find which child contains pos and which is the "sibling"
    int container_idx = first_child_index(node_index);
    int sibling_idx = container_idx + 1;
    if (node->type == Node::Type::VERTICAL) {
        if (pos.x() >= node->offset) {
            ++container_idx;
            --sibling_idx;
        }
    } else if (pos.y() >= node->offset) {
        ++container_idx;
        --sibling_idx;
    }

    // Recursion on container node
    nearest_neighbor(pos, best_item, best_distance, container_idx);

    // If the distance to the "offset" is shorter than the best distance we've found so far, then it's possible that the nearest
    // neighbour is in the sibling node (so we should search there too).
    double distance_to_offset;
    if (node->type == Node::Type::HORIZONTAL) {
        distance_to_offset = std::abs(pos.y() - node->offset);
    } else {
        distance_to_offset = std::abs(pos.x() - node->offset);
    }

    if (distance_to_offset < best_distance) {
        // Recursion on sibling node
        nearest_neighbor(pos, best_item, best_distance, sibling_idx);
    }
}

#ifndef NDEBUG
//---------------------------------------------------------
//   debug
//---------------------------------------------------------

String BspTree::debug(int index) const {
    const Node *node = &nodes_.at(index);

    String tmp;
    if (node->type == Node::Type::LEAF) {
        RectF rec = rect_for_index(index);
        if (!leaves_[node->leaf_index].empty()) {
            tmp += String(u"[%1, %2, %3, %4] contains %5 items\n")
                       .arg(rec.left())
                       .arg(rec.top())
                       .arg(rec.width())
                       .arg(rec.height())
                       .arg(leaves_[node->leaf_index].size());
        }
    } else {
        if (node->type == Node::Type::HORIZONTAL) {
            tmp += debug(first_child_index(index));
            tmp += debug(first_child_index(index) + 1);
        } else {
            tmp += debug(first_child_index(index));
            tmp += debug(first_child_index(index) + 1);
        }
    }
    return tmp;
}

#endif

//---------------------------------------------------------
//   initialize
//---------------------------------------------------------

void BspTree::initialize(const RectF &rec, int dep, int index) {
    Node *node = &nodes_[index];
    if (index == 0) {
        node->type = Node::Type::HORIZONTAL;
        node->offset = rec.center().y();
    }

    if (dep) {
        Node::Type type;
        RectF rect1, rect2;
        double offset1, offset2;

        if (node->type == Node::Type::HORIZONTAL) {
            type = Node::Type::VERTICAL;
            rect1.set_rect(rec.left(), rec.top(), rec.width(), rec.height() * .5);
            rect2.set_rect(rect1.left(), rect1.bottom(), rect1.width(), rec.height() - rect1.height());
            offset1 = rect1.center().x();
            offset2 = rect2.center().x();
        } else {
            type = Node::Type::HORIZONTAL;
            rect1.set_rect(rec.left(), rec.top(), rec.width() * .5, rec.height());
            rect2.set_rect(rect1.right(), rect1.top(), rec.width() - rect1.width(), rect1.height());
            offset1 = rect1.center().y();
            offset2 = rect2.center().y();
        }

        int child_index = first_child_index(index);

        Node *child = &nodes_[child_index];
        child->offset = offset1;
        child->type = type;

        child = &nodes_[child_index + 1];
        child->offset = offset2;
        child->type = type;

        initialize(rect1, dep - 1, child_index);
        initialize(rect2, dep - 1, child_index + 1);
    } else {
        node->type = Node::Type::LEAF;
        node->leaf_index = leaf_cnt_++;
    }
}

//---------------------------------------------------------
//   climbTree
//---------------------------------------------------------

void BspTree::climb_tree(BspTreeVisitor *visitor, const PointF &pos, int index) {
    if (nodes_.empty()) {
        return;
    }

    Node *node = &nodes_[index];
    int child_index = first_child_index(index);

    switch (node->type) {
        case Node::Type::LEAF: visitor->visit(leaves_[node->leaf_index]); break;
        case Node::Type::VERTICAL:
            if (pos.x() < node->offset) {
                climb_tree(visitor, pos, child_index);
            } else {
                climb_tree(visitor, pos, child_index + 1);
            }
            break;
        case Node::Type::HORIZONTAL:
            if (pos.y() < node->offset) {
                climb_tree(visitor, pos, child_index);
            } else {
                climb_tree(visitor, pos, child_index + 1);
            }
            break;
    }
}

//---------------------------------------------------------
//   climbTree
//---------------------------------------------------------

void BspTree::climb_tree(BspTreeVisitor *visitor, const RectF &rec, int index) {
    if (nodes_.empty()) {
        return;
    }

    Node *node = &nodes_[index];
    int child_index = first_child_index(index);

    switch (node->type) {
        case Node::Type::LEAF: visitor->visit(leaves_[node->leaf_index]); break;
        case Node::Type::VERTICAL:
            if (rec.left() < node->offset) {
                climb_tree(visitor, rec, child_index);
                if (rec.right() >= node->offset) {
                    climb_tree(visitor, rec, child_index + 1);
                }
            } else {
                climb_tree(visitor, rec, child_index + 1);
            }
            break;
        case Node::Type::HORIZONTAL:
            if (rec.top() < node->offset) {
                climb_tree(visitor, rec, child_index);
                if (rec.bottom() >= node->offset) {
                    climb_tree(visitor, rec, child_index + 1);
                }
            } else {
                climb_tree(visitor, rec, child_index + 1);
            }
    }
}

//---------------------------------------------------------
//   rectForIndex
//---------------------------------------------------------

RectF BspTree::rect_for_index(int index) const {
    if (index <= 0) {
        return rect_;
    }

    int parent_idx = parent_index(index);
    RectF rec = rect_for_index(parent_idx);
    const Node *parent = &nodes_.at(parent_idx);

    if (parent->type == Node::Type::HORIZONTAL) {
        if (index & 1) {
            rec.set_right(parent->offset);
        } else {
            rec.set_left(parent->offset);
        }
    } else {
        if (index & 1) {
            rec.set_bottom(parent->offset);
        } else {
            rec.set_top(parent->offset);
        }
    }
    return rec;
}
} // namespace mu::engraving
