/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#ifndef MU_ENGRAVING_SHAPE_H
#define MU_ENGRAVING_SHAPE_H

#include <functional>
#include <optional>

#include "draw/types/geometry.h"
#include "draw/types/transform.h"
#include "engraving/types/types.h"

namespace muse::draw {
struct Painter;
struct Transform;
} // namespace muse::draw

namespace mu::engraving {
struct EngravingItem;

//---------------------------------------------------------
//   ShapeElement
//---------------------------------------------------------

struct ShapeElement : public RectF {
public:

    ShapeElement(const RectF &f, const EngravingItem *p, bool ignore_for_layout)
        : RectF(f), item_(p), ignore_for_layout_(ignore_for_layout) {}

    ShapeElement(const RectF &f, const EngravingItem *p) : RectF(f), item_(p) {}

    ShapeElement(const RectF &f) : RectF(f) {}

    const EngravingItem *item() const { return item_; }

    void set_item(const EngravingItem *item) { item_ = item; }

    bool ignore_for_layout() const { return ignore_for_layout_; }

private:
    const EngravingItem *item_ = nullptr;
    bool ignore_for_layout_ = false;
};

//---------------------------------------------------------
//   Shape
//---------------------------------------------------------

struct Shape {

    enum class Type : unsigned char {
        FIXED,    // fixed size, like just bbox
                  //        FixedX,     // not implemented (reserved)
                  //        FixedY,     // not implemented (reserved)
        COMPOSITE // composed of other shapes
    };

    Shape(Type t = Type::FIXED) : type_(t) {}

    Shape(const RectF &r, const EngravingItem *p = nullptr, Type t = Type::FIXED) : type_(t) { set_b_box(r, p); }

    Shape(const std::vector<RectF> &rects, const EngravingItem *p = nullptr);

    Type type() const { return type_; }

    bool is_composite() const { return type_ == Type::COMPOSITE; }

    size_t size() const { return elements_.size(); }

    bool empty() const { return elements_.empty(); }

    void clear() { elements_.clear(); }

    bool equal(const Shape &sh) const {
        if (type_ != sh.type_) {
            return false;
        }

        switch (type_) {
            case Type::FIXED: return this->bbox() == sh.bbox();
            case Type::COMPOSITE: return this->elements_ == sh.elements_;
        }

        return true;
    }

    // Fixed
    void set_b_box(const RectF &r, const EngravingItem *p = nullptr);
    void add_b_box(const RectF &r);

    // Composite
    void add(const Shape &s);
    void add(const ShapeElement &shape_el);

    void add(const RectF &r, const EngravingItem *p, bool ignore_for_layout) { add(ShapeElement(r, p, ignore_for_layout)); }

    void add(const RectF &r, const EngravingItem *p) { add(ShapeElement(r, p)); }

    void add(const RectF &r) { add(ShapeElement(r)); }

    void remove(const RectF &);
    void remove(const Shape &);

    template <typename Predicate>
    inline bool remove_if(Predicate p) {
        size_t orig_size = elements_.size();
        elements_.erase(std::remove_if(elements_.begin(), elements_.end(), p), elements_.end());
        invalidate_b_box();
        return orig_size != elements_.size();
    }

    // ---

    const std::vector<ShapeElement> &elements() const { return elements_; }

    std::vector<ShapeElement> &elements() { return elements_; }

    std::vector<RectF> to_rects() const;

    std::optional<ShapeElement> find_if(const std::function<bool(const ShapeElement &)> &func) const;
    std::optional<ShapeElement> find_first(ElementType type) const;
    std::optional<ShapeElement> get_first() const;

    void remove_invisibles();
    void remove_types(const std::set<ElementType> &types);

    void add_horizontal_spacing(EngravingItem *item, double left, double right);

    Shape &translate(const PointF &);
    void translate_x(double);
    void translate_y(double);
    Shape translated(const PointF &) const;
    Shape &scale(const SizeF &);
    Shape scaled(const SizeF &) const;
    Shape &adjust(double xp1, double yp1, double xp2, double yp2);
    Shape adjusted(double xp1, double yp1, double xp2, double yp2) const;
    Shape &pad(double p);
    Shape padded(double p) const;
    Shape &transform(const muse::draw::Transform &t);
    Shape transformed(const muse::draw::Transform &t);

    const RectF &bbox() const;
    double min_vertical_distance(const Shape &, double min_horizontal_clearance = 0.0) const;
    double vertical_clearance(const Shape &, double min_horizontal_distance = 0.0) const;
    double top_distance(const PointF &) const;
    double bottom_distance(const PointF &) const;
    double left() const;
    double right() const;
    double top() const;
    double bottom() const;
    double width() const;
    double height() const;
    double top_at_x(double x) const;
    double bottom_at_x(double x) const;
    double right_most_edge_at_height(double y_above, double y_below) const;
    double left_most_edge_at_height(double y_above, double y_below) const;
    double left_most_edge_at_top() const;

    bool contains(const PointF &) const;
    bool intersects(const RectF &rr) const;
    bool intersects(const Shape &other) const;
    bool clears_vertically(const Shape &a) const;

    void paint(muse::draw::Painter &painter) const;

private:

    void invalidate_b_box();

    Type type_ = Type::FIXED;
    std::vector<ShapeElement> elements_;
    mutable RectF bbox_; // cache
};

void dump(const ShapeElement &sh, std::stringstream &ss);
void dump(const Shape &sh, std::stringstream &ss);
std::string dump(const Shape &sh);

//---------------------------------------------------------
//   intersects
//---------------------------------------------------------

inline static bool intersects(double a, double b, double c, double d, double min_clearance = 0.0) {
    if (a == b || c == d) {
        return false;
    }
    return (b + min_clearance > c) && (a < d + min_clearance);
}
} // namespace mu::engraving

#endif
