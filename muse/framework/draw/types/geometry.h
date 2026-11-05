/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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
#ifndef MUSE_DRAW_GEOMETRY_H
#define MUSE_DRAW_GEOMETRY_H

#include <vector>
#include <cmath>
#include <cassert>
#include <string>
#include <sstream>

#include "global/realfn.h"
#include "global/logstream.h"
#include "global/types/number.h"

namespace muse {
template <typename T>
inline bool isEqual(T a1, T a2) {
    if constexpr (std::is_same<T, double>::value) {
        return RealIsEqual(a1, a2);
    } else {
        return a1 == a2;
    }
}

// ====================================
// Point
// ====================================
template <typename T>
class PointX {
public:
    inline PointX() = default;

    inline PointX(T x, T y) : x_(x), y_(y) {}

    inline bool isNull() const { return x_.is_zero() && y_.is_zero(); }

    inline void setX(T x) { x_ = x; }

    inline void setY(T y) { y_ = y; }

    inline void setXY(T x, T y) {
        x_ = x;
        y_ = y;
    }

    inline T x() const { return x_; }

    inline T y() const { return y_; }

    //! NOTE I don't like this methods, but now it a lot of using
    inline number_t<T> &rx() { return x_; }

    inline number_t<T> &ry() { return y_; }

    inline bool operator==(const PointX<T> &p) const { return p.x_ == x_ && p.y_ == y_; }

    inline bool operator!=(const PointX<T> &p) const { return !this->operator==(p); }

    inline PointX<T> operator-() const { return PointX<T>(-x_, -y_); }

    inline PointX<T> operator-(const PointX<T> &p) const { return PointX<T>(x_ - p.x_, y_ - p.y_); }

    inline PointX<T> operator+(const PointX<T> &p) const { return PointX<T>(x_ + p.x_, y_ + p.y_); }

    inline PointX<T> &operator+=(const PointX<T> &p) {
        x_ += p.x_;
        y_ += p.y_;
        return *this;
    }

    inline PointX<T> &operator-=(const PointX<T> &p) {
        x_ -= p.x_;
        y_ -= p.y_;
        return *this;
    }

    inline PointX<T> &operator*=(T c) {
        x_ *= c;
        y_ *= c;
        return *this;
    }

    inline PointX<T> &operator/=(T divisor) {
        x_ /= divisor;
        y_ /= divisor;
        return *this;
    }

    inline T manhattanLength() const { return std::abs(x_) + std::abs(y_); }

    static inline double dotProduct(const PointX<T> &p1, const PointX<T> &p2) { return p1.x_ * p2.x_ + p1.y_ * p2.y_; }

    inline void normalize() {
        // Need some extra precision if the length is very small.
        double len = double(x_) * double(x_) + double(y_) * double(y_);
        if (RealIsNull(len - 1.0f) || RealIsNull(len)) {
            return;
        }

        len = std::sqrt(len);

        x_ /= len;
        y_ /= len;
    }

    inline PointX<T> normalized() const {
        PointX<T> copy = *this;
        copy.normalize();
        return copy;
    }


private:
    //! NOTE We should not swap fields
    //! We should not add new fields
    //! If we really need to do this, then we need to change the implementation of QPainterProvider
    number_t<T> x_;
    number_t<T> y_;
};

template <typename T>
inline void dump(const PointX<T> &p, std::stringstream &ss) {
    ss << "{x: " << p.x() << ", y: " << p.y() << "}";
}

template <typename T>
inline std::string dump(const PointX<T> &p) {
    std::stringstream ss;
    dump(p, ss);
    return ss.str();
}

template <typename T>
inline PointX<T> operator*(const PointX<T> &p, T c) {
    return PointX<T>(p.x() * c, p.y() * c);
}

template <typename T>
inline PointX<T> operator*(T c, const PointX<T> &p) {
    return PointX<T>(p.x() * c, p.y() * c);
}

template <typename T>
inline PointX<T> operator/(const PointX<T> &p, T c) {
    return PointX<T>(p.x() / c, p.y() / c);
}

using PointF = PointX<double>;
using Point = PointX<int>;

// ====================================
// PairF
// ====================================
using PairF = std::pair<double, double>;

// ====================================
// Line
// ====================================
template <typename T>
class LineX {
public:
    inline LineX() = default;

    inline LineX(const PointX<T> &p1, const PointX<T> &p2) : p1_(p1), p2_(p2) {}

    inline LineX(T x1, T y1, T x2, T y2) : p1_(PointX<T>(x1, y1)), p2_(PointX<T>(x2, y2)) {}

    inline bool isNull() const { return p1_.isNull() && p2_.isNull(); }

    inline const PointX<T> &p1() const { return p1_; }

    inline const PointX<T> &p2() const { return p2_; }

    inline double x1() const { return p1_.x(); }

    inline double y1() const { return p1_.y(); }

    inline double x2() const { return p2_.x(); }

    inline double y2() const { return p2_.y(); }

    inline void setP1(const PointX<T> &p) { p1_ = p; }

    inline void setP2(const PointX<T> &p) { p2_ = p; }

    inline void setLine(T aX1, T aY1, T aX2, T aY2) {
        p1_ = PointX<T>(aX1, aY1);
        p2_ = PointX<T>(aX2, aY2);
    }

    inline bool operator==(const LineX<T> &l) const { return p1_ == l.p1_ && p2_ == l.p2_; }

    inline bool operator!=(const LineX<T> &l) const { return !this->operator==(l); }

    inline PointX<T> pointAt(T t) const { return PointX<T>(p1_.x() + (p2_.x() - p1_.x()) * t, p1_.y() + (p2_.y() - p1_.y()) * t); }

    inline void translate(const PointX<T> &point) {
        p1_ += point;
        p2_ += point;
    }

    inline LineX<T> translated(const PointX<T> &p) const { return LineX<T>(p1_ + p, p2_ + p); }


private:
    PointX<T> p1_;
    PointX<T> p2_;
};

using LineF = LineX<double>;
using Line = LineX<int>;

// ====================================
// Size
// ====================================
template <typename T>
class SizeX {
public:
    inline SizeX() = default;

    inline SizeX(T w, T h) : w_(w), h_(h) {}

    inline bool isNull() const { return w_.is_zero() && h_.is_zero(); }

    inline T width() const { return w_; }

    inline T height() const { return h_; }

    inline void setWidth(T w) { w_ = w; }

    inline void setHeight(T h) { h_ = h; }

    inline bool operator==(const SizeX<T> &s) const { return s.w_ == w_ && s.h_ == h_; }

    inline bool operator!=(const SizeX<T> &s) const { return !this->operator==(s); }

    inline SizeX<T> transposed() const { return SizeX<T>(h_, w_); }


private:
    number_t<T> w_;
    number_t<T> h_;
};

template <typename T>
inline SizeX<T> operator*(const SizeX<T> &s, double c) {
    return SizeX<T>(s.width() * c, s.height() * c);
}

template <typename T>
inline SizeX<T> operator*(double c, const SizeX<T> &s) {
    return SizeX<T>(s.width() * c, s.height() * c);
}

template <typename T>
inline SizeX<T> operator/(const SizeX<T> &s, T c) {
    assert(!isEqual(c, T()));
    return SizeX<T>(s.width() / c, s.height() / c);
}

using Size = SizeX<int>;
using SizeF = SizeX<double>;

class ScaleF : public SizeX<double> {
public:
    ScaleF() = default;

    ScaleF(double w, double h) : SizeX<double>(w, h) {}
};

// ====================================
// Rect
// ====================================
template <typename T>
class RectX {
public:
    inline RectX() = default;

    inline RectX(T x, T y, T w, T h) : x_(x), y_(y), w_(w), h_(h) {}

    inline RectX(const PointX<T> &topLeft, const PointX<T> &bottomRight)
        : x_(topLeft.x()), y_(topLeft.y()), w_(bottomRight.x() - topLeft.x()), h_(bottomRight.y() - topLeft.y()) {}

    inline RectX(const PointX<T> &atopLeft, const SizeX<T> &asize)
        : x_(atopLeft.x()), y_(atopLeft.y()), w_(asize.width()), h_(asize.height()) {}

    inline bool isNull() const { return w_.is_zero() && h_.is_zero(); }

    inline bool isEmpty() const { return w_ <= T() || h_ <= T(); }

    inline bool isValid() const { return w_ > T() && h_ > T(); }

    inline T x() const { return x_; }

    inline T y() const { return y_; }

    inline T width() const { return w_; }

    inline T height() const { return h_; }

    inline SizeX<T> size() const { return SizeX<T>(w_, h_); }

    inline T left() const noexcept { return x_; }

    inline T top() const noexcept { return y_; }

    inline T right() const noexcept { return x_ + w_; }

    inline T bottom() const noexcept { return y_ + h_; }

    inline PointX<T> topLeft() const { return PointX<T>(x_, y_); }

    inline PointX<T> bottomRight() const { return PointX<T>(x_ + w_, y_ + h_); }

    inline PointX<T> topRight() const { return PointX<T>(x_ + w_, y_); }

    inline PointX<T> bottomLeft() const { return PointX<T>(x_, y_ + h_); }

    inline PointX<T> center() const { return PointX<T>(x_ + w_ / number_t<T>::cast(2), y_ + h_ / number_t<T>::cast(2)); }

    inline void setCoords(T xp1, T yp1, T xp2, T yp2) {
        x_ = xp1;
        y_ = yp1;
        w_ = xp2 - xp1;
        h_ = yp2 - yp1;
    }

    inline void setRect(double ax, double ay, double aaw, double aah) {
        x_ = ax;
        y_ = ay;
        w_ = aaw;
        h_ = aah;
    }

    inline void setHeight(T h) { h_ = h; }

    inline void setWidth(T w) { w_ = w; }

    inline void setSize(const SizeX<T> &s) {
        w_ = s.width();
        h_ = s.height();
    }

    inline void setLeft(T pos) {
        T diff = pos - x_;
        x_ += diff;
        w_ -= diff;
    }

    inline void setRight(T pos) { w_ = pos - x_; }

    inline void setTop(T pos) {
        T diff = pos - y_;
        y_ += diff;
        h_ -= diff;
    }

    inline void setBottom(T pos) { h_ = pos - y_; }

    inline void setX(T x) { x_ = x; }

    inline void setY(T y) { y_ = y; }

    inline void setTopLeft(const PointX<T> &p) {
        setLeft(p.x());
        setTop(p.y());
    }

    inline void setTopRight(const PointX<T> &p) {
        setRight(p.x());
        setTop(p.y());
    }

    inline void setBottomLeft(const PointX<T> &p) {
        setLeft(p.x());
        setBottom(p.y());
    }

    inline void setBottomRight(const PointX<T> &p) {
        setRight(p.x());
        setBottom(p.y());
    }

    inline bool operator==(const RectX<T> &r) const { return r.x_ == x_ && r.y_ == y_ && r.w_ == w_ && r.h_ == h_; }

    inline bool operator!=(const RectX<T> &r) const { return !this->operator==(r); }

    inline void moveTo(double ax, double ay) {
        x_ = ax;
        y_ = ay;
    }

    inline void moveTo(const PointX<T> &p) {
        x_ = p.x();
        y_ = p.y();
    }

    inline void moveCenter(const PointX<T> &p) {
        x_ = p.x() - w_ / number_t<T>::cast(2);
        y_ = p.y() - h_ / number_t<T>::cast(2);
    }

    inline void moveTop(double pos) { y_ = pos; }

    inline void translate(T dx, T dy) {
        x_ += dx;
        y_ += dy;
    }

    inline void translate(const PointX<T> &p) {
        x_ += p.x();
        y_ += p.y();
    }

    inline RectX<T> translated(T dx, T dy) const { return RectX<T>(x_ + dx, y_ + dy, w_, h_); }

    inline RectX<T> translated(const PointX<T> &p) const { return RectX<T>(x_ + p.x(), y_ + p.y(), w_, h_); }

    inline void adjust(double xp1, double yp1, double xp2, double yp2) {
        x_ += xp1;
        y_ += yp1;
        w_ += xp2 - xp1;
        h_ += yp2 - yp1;
    }

    inline RectX<T> adjusted(T xp1, T yp1, T xp2, T yp2) const { return RectX<T>(x_ + xp1, y_ + yp1, w_ + xp2 - xp1, h_ + yp2 - yp1); }

    inline void pad(double p) { adjust(-p, -p, p, p); }

    inline RectX<T> padded(double p) const { return adjusted(-p, -p, p, p); }

    inline RectX<T> &scale(const SizeX<T> &mag) {
        x_ *= mag.width();
        y_ *= mag.height();
        w_ *= mag.width();
        h_ *= mag.height();
        return *this;
    }

    inline RectX<T> scaled(const SizeX<T> &mag) const {
        return RectX<T>(x_ * mag.width(), y_ * mag.height(), w_ * mag.width(), h_ * mag.height());
    }

    bool contains(const PointX<T> &p) const;
    bool contains(const RectX<T> &r) const;

    T distanceTo(const PointX<T> &p) const;

    bool intersects(const RectX<T> &r) const;

    RectX<T> united(const RectX<T> &r) const;

    inline RectX<T> &unite(const RectX<T> &r) {
        *this = united(r);
        return *this;
    }

    RectX<T> intersected(const RectX<T> &r) const;

    inline RectX<T> &intersect(const RectX<T> &r) {
        *this = intersected(r);
        return *this;
    }

    //! NOTE I don't like this operators, but now it a lot of using
    inline RectX<T> operator|(const RectX<T> &r) const { return united(r); }

    inline RectX<T> operator&(const RectX<T> &r) const { return intersected(r); }

    inline RectX<T> &operator|=(const RectX<T> &r) { return unite(r); }

    inline RectX<T> &operator&=(const RectX<T> &r) { return intersect(r); }

    RectX<T> normalized() const;


private:
    number_t<T> x_;
    number_t<T> y_;
    number_t<T> w_;
    number_t<T> h_;
};

template <typename T>
inline void dump(const RectX<T> &r, std::stringstream &ss) {
    ss << "{x: " << r.x() << ", y: " << r.y() << ", w: " << r.width() << ", h: " << r.height() << "}";
}

template <typename T>
inline std::string dump(const RectX<T> &r) {
    std::stringstream ss;
    dump(r, ss);
    return ss.str();
}

using RectF = RectX<double>;

class Rect : public RectX<int> {
public:
    inline Rect() : RectX<int>() {}

    inline Rect(int x, int y, int w, int h) : RectX<int>(x, y, w, h) {}

    inline RectF toRectF() const { return RectF(x(), y(), width(), height()); }
};

// ====================================
// Polygon
// ====================================
template <typename T>
class PolygonX : public std::vector<PointX<T>> {
public:

    inline PolygonX() = default;

    inline PolygonX(const std::vector<PointX<T>> &v) : std::vector<PointX<T>>(v) {}

    inline PolygonX(size_t size) : std::vector<PointX<T>>(size) {}

    inline PolygonX<T> &operator<<(const PointX<T> &p) {
        this->push_back(p);
        return *this;
    }

    inline void translate(const PointX<T> &offset) {
        if (offset.isNull()) {
            return;
        }
        PointX<T> *p = this->data();
        size_t i = this->size();
        while (i--) {
            *p += offset;
            ++p;
        }
    }

    inline PolygonX<T> translated(T x, T y) const { return translated(PointX<T>(x, y)); }

    inline PolygonX<T> translated(const PointX<T> &offset) const {
        PolygonX<T> copy(*this);
        copy.translate(offset);
        return copy;
    }

    RectX<T> boundingRect() const {
        const PointX<T> *pd = this->data();
        const PointX<T> *pe = pd + this->size();
        if (pd == pe) {
            return RectX<T>(0, 0, 0, 0);
        }
        T minx, maxx, miny, maxy;
        minx = maxx = pd->x();
        miny = maxy = pd->y();
        ++pd;
        while (pd != pe) {
            if (pd->x() < minx) {
                minx = pd->x();
            } else if (pd->x() > maxx) {
                maxx = pd->x();
            }
            if (pd->y() < miny) {
                miny = pd->y();
            } else if (pd->y() > maxy) {
                maxy = pd->y();
            }
            ++pd;
        }
        return RectX<T>(minx, miny, maxx - minx, maxy - miny);
    }
};

using PolygonF = PolygonX<double>;
using Polygon = PolygonX<int>;

// Implementation ==========================================
template <typename T>
RectX<T> RectX<T>::united(const RectX<T> &r) const {
    if (isNull()) {
        return r;
    }

    if (r.isNull()) {
        return *this;
    }

    T left = x_;
    T right = x_;
    if (w_.is_negative()) {
        left += w_;
    } else {
        right += w_;
    }
    if (r.w_.is_negative()) {
        left = std::min(left, r.x_.raw() + r.w_.raw());
        right = std::max(right, r.x_.raw());
    } else {
        left = std::min(left, r.x_.raw());
        right = std::max(right, r.x_.raw() + r.w_.raw());
    }

    double top = y_;
    double bottom = y_;
    if (h_.is_negative()) {
        top += h_;
    } else {
        bottom += h_;
    }
    if (r.h_.is_negative()) {
        top = std::min(top, r.y_.raw() + r.h_.raw());
        bottom = std::max(bottom, r.y_.raw());
    } else {
        top = std::min(top, r.y_.raw());
        bottom = std::max(bottom, r.y_.raw() + r.h_.raw());
    }
    return RectX<T>(left, top, right - left, bottom - top);
}

template <typename T>
RectX<T> RectX<T>::intersected(const RectX<T> &r) const {
    number_t<T> l1 = x_;
    number_t<T> r1 = x_;
    if (w_.is_negative()) {
        l1 += w_;
    } else {
        r1 += w_;
    }
    if (l1 == r1) { // null rect
        return RectX<T>();
    }
    number_t<T> l2 = r.x_;
    number_t<T> r2 = r.x_;
    if (r.w_.is_negative()) {
        l2 += r.w_;
    } else {
        r2 += r.w_;
    }
    if (l2 == r2) { // null rect
        return RectX<T>();
    }
    if (l1 >= r2 || l2 >= r1) {
        return RectX<T>();
    }
    number_t<T> t1 = y_;
    number_t<T> b1 = y_;
    if (h_.is_negative()) {
        t1 += h_;
    } else {
        b1 += h_;
    }
    if (t1 == b1) { // null rect
        return RectX<T>();
    }
    number_t<T> t2 = r.y_;
    number_t<T> b2 = r.y_;
    if (r.h_.is_negative()) {
        t2 += r.h_;
    } else {
        b2 += r.h_;
    }
    if (t2 == b2) { // null rect
        return RectX<T>();
    }
    if (t1 >= b2 || t2 >= b1) {
        return RectX<T>();
    }
    RectX<T> tmp;
    tmp.x_ = std::max(l1, l2);
    tmp.y_ = std::max(t1, t2);
    tmp.w_ = std::min(r1, r2) - tmp.x_;
    tmp.h_ = std::min(b1, b2) - tmp.y_;
    return tmp;
}

template <typename T>
bool RectX<T>::intersects(const RectX<T> &r) const {
    number_t<T> l1 = x_;
    number_t<T> r1 = x_;
    if (w_.is_negative()) {
        l1 += w_;
    } else {
        r1 += w_;
    }
    if (l1 == r1) { // null rect
        return false;
    }
    number_t<T> l2 = r.x_;
    number_t<T> r2 = r.x_;
    if (r.w_.is_negative()) {
        l2 += r.w_;
    } else {
        r2 += r.w_;
    }
    if (l2 == r2) { // null rect
        return false;
    }
    if (l1 >= r2 || l2 >= r1) {
        return false;
    }
    number_t<T> t1 = y_;
    number_t<T> b1 = y_;
    if (h_.is_negative()) {
        t1 += h_;
    } else {
        b1 += h_;
    }
    if (t1 == b1) { // null rect
        return false;
    }
    number_t<T> t2 = r.y_;
    number_t<T> b2 = r.y_;
    if (r.h_.is_negative()) {
        t2 += r.h_;
    } else {
        b2 += r.h_;
    }
    if (t2 == b2) { // null rect
        return false;
    }
    if (t1 >= b2 || t2 >= b1) {
        return false;
    }
    return true;
}

template <typename T>
bool RectX<T>::contains(const PointX<T> &p) const {
    number_t<T> l = x_;
    number_t<T> r = x_;
    if (w_.is_negative()) {
        l += w_;
    } else {
        r += w_;
    }
    if (l == r) { // null rect
        return false;
    }
    if (p.x() < l || p.x() > r) {
        return false;
    }
    number_t<T> t = y_;
    number_t<T> b = y_;
    if (h_.is_negative()) {
        t += h_;
    } else {
        b += h_;
    }
    if (t == b) { // null rect
        return false;
    }
    if (p.y() < t || p.y() > b) {
        return false;
    }
    return true;
}

template <typename T>
bool RectX<T>::contains(const RectX<T> &r) const {
    number_t<T> l1 = x_;
    number_t<T> r1 = x_;
    if (w_.is_negative()) {
        l1 += w_;
    } else {
        r1 += w_;
    }
    if (l1 == r1) { // null rect
        return false;
    }
    number_t<T> l2 = r.x_;
    number_t<T> r2 = r.x_;
    if (r.w_.is_negative()) {
        l2 += r.w_;
    } else {
        r2 += r.w_;
    }
    if (l2 == r2) { // null rect
        return false;
    }
    if (l2 < l1 || r2 > r1) {
        return false;
    }
    number_t<T> t1 = y_;
    number_t<T> b1 = y_;
    if (h_.is_negative()) {
        t1 += h_;
    } else {
        b1 += h_;
    }
    if (t1 == b1) { // null rect
        return false;
    }
    number_t<T> t2 = r.y_;
    number_t<T> b2 = r.y_;
    if (r.h_.is_negative()) {
        t2 += r.h_;
    } else {
        b2 += r.h_;
    }
    if (t2 == b2) { // null rect
        return false;
    }
    if (t2 < t1 || b2 > b1) {
        return false;
    }
    return true;
}

template <typename T>
T RectX<T>::distanceTo(const PointX<T> &p) const {
    number_t<T> dx = std::max({bottomLeft().x() - p.x(), 0.0, p.x() - bottomRight().x()});
    number_t<T> dy = std::max({bottomLeft().y() - p.y(), 0.0, p.y() - topLeft().y()});
    return std::sqrt(dx * dx + dy * dy);
}

template <typename T>
RectX<T> RectX<T>::normalized() const {
    RectX<T> r = *this;
    if (r.w_.is_negative()) {
        r.x_ += r.w_;
        r.w_ = -r.w_;
    }
    if (r.h_.is_negative()) {
        r.y_ += r.h_;
        r.h_ = -r.h_;
    }
    return r;
}
} // namespace muse

template <typename T>
inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::RectX<T> &r) {
    s << muse::dump(r);
    return s;
}

template <typename T>
inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::PointX<T> &p) {
    s << muse::dump(p);
    return s;
}

#endif // MUSE_DRAW_GEOMETRY_H
