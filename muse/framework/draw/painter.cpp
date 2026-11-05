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
#include "painter.h"

#include "types/brush.h"
#include "types/painterpath.h"

#include "log.h"

using namespace muse;
using namespace muse::draw;

IPaintProviderPtr Painter::extended;
bool PainterItemMarker::enabled = true;

Painter::Painter(IPaintProviderPtr provider, const std::string &name) : provider_(provider), name_(name) { init(); }

Painter::~Painter() { endTarget(false); }

void Painter::init() {
    provider_->beginTarget(name_);
    if (extended) {
        extended->beginTarget(name_);
    }

    State st;
    st.worldTransform = provider_->transform();
    st.isWxF = true;
    st.transform = st.worldTransform;

    states_ = std::stack<State>();
    states_.push(std::move(st));
}

IPaintProviderPtr Painter::provider() const { return provider_; }

void Painter::setProvider(const IPaintProviderPtr &p, bool reinit) {
    provider_ = p;
    if (reinit) {
        init();
    }
}

bool Painter::endDraw() { return endTarget(true); }

bool Painter::endTarget(bool endDraw) {
    provider_->beforeEndTargetHook(this);
    if (extended) {
        extended->beforeEndTargetHook(this);
    }

    bool ok = provider_->endTarget(endDraw);
    if (extended) {
        extended->endTarget(endDraw);
    }
    return ok;
}

bool Painter::isActive() const { return provider_->isActive(); }

void Painter::beginObject(const std::string &name) {
    provider_->beginObject(name);
    if (extended) {
        extended->beginObject(name);
    }
}

void Painter::endObject() {
    provider_->endObject();
    if (extended) {
        extended->endObject();
    }
}

void Painter::setAntialiasing(bool arg) {
    provider_->setAntialiasing(arg);
    if (extended) {
        extended->setAntialiasing(arg);
    }
}

void Painter::setCompositionMode(CompositionMode mode) {
    provider_->setCompositionMode(mode);
    if (extended) {
        extended->setCompositionMode(mode);
    }
}

void Painter::setFont(const Font &font) {
    provider_->setFont(font);
    if (extended) {
        extended->setFont(font);
    }
}

const Font &Painter::font() const { return provider_->font(); }

void Painter::setPen(const Pen &pen) {
    provider_->setPen(pen);
    if (extended) {
        extended->setPen(pen);
    }
}

void Painter::setNoPen() { setPen(Pen(PenStyle::NoPen)); }

const Pen &Painter::pen() const { return provider_->pen(); }

void Painter::setBrush(const Brush &brush) {
    provider_->setBrush(brush);
    if (extended) {
        extended->setBrush(brush);
    }
}

const Brush &Painter::brush() const { return provider_->brush(); }

void Painter::save() {
    State newSt = states_.top();
    states_.push(newSt);

    provider_->save();
    if (extended) {
        extended->save();
    }
}

void Painter::restore() {
    if (states_.size() > 0) {
        states_.pop();
    }

    provider_->restore();
    if (extended) {
        extended->restore();
    }
}

double Painter::deviceLogicalDpi() const { return provider_->deviceLogicalDpi(); }

void Painter::setWorldTransform(const Transform &matrix, bool combine) {
    State &st = editableState();
    if (combine) {
        st.worldTransform = matrix * st.worldTransform; // combines
    } else {
        st.worldTransform = matrix; // set new matrix
    }
    st.isWxF = true;
    updateMatrix();
}

const Transform &Painter::worldTransform() const { return state().worldTransform; }

void Painter::scale(double sx, double sy) {
    State &st = editableState();
    st.worldTransform.scale(sx, sy);
    st.isWxF = true;
    updateMatrix();
}

void Painter::rotate(double angle) {
    State &st = editableState();
    st.worldTransform.rotate(angle);
    st.isWxF = true;
    updateMatrix();
}

void Painter::translate(double dx, double dy) {
    State &st = editableState();
    st.worldTransform.translate(dx, dy);
    st.isWxF = true;
    updateMatrix();
}

RectF Painter::window() const { return state().window; }

void Painter::setWindow(const RectF &window) {
    State &st = editableState();
    st.window = window;
    st.isVxF = true;
    updateViewTransform();

    // for debug purpose
    provider_->setWindow(window);
    if (extended) {
        extended->setWindow(window);
    }
}

RectF Painter::viewport() const { return state().viewport; }

void Painter::setViewport(const RectF &viewport) {
    State &st = editableState();
    st.viewport = viewport;
    st.isVxF = true;
    updateViewTransform();

    // for debug purpose
    provider_->setViewport(viewport);
    if (extended) {
        extended->setViewport(viewport);
    }
}

// drawing functions

void Painter::fillPath(const PainterPath &path, const Brush &brush) {
    Pen oldPen = this->pen();
    Brush oldBrush = this->brush();
    setPen(Pen(PenStyle::NoPen));
    setBrush(brush);

    drawPath(path);

    setPen(oldPen);
    setBrush(oldBrush);
}

void Painter::strokePath(const PainterPath &path, const Pen &pen) {
    Pen oldPen = this->pen();
    Brush oldBrush = this->brush();
    setPen(pen);
    setBrush(BrushStyle::NoBrush);

    drawPath(path);

    setPen(oldPen);
    setBrush(oldBrush);
}

void Painter::drawPath(const PainterPath &path) {
    provider_->drawPath(path);
    if (extended) {
        extended->drawPath(path);
    }
}

void Painter::drawLines(const LineF *lines, size_t lineCount) {
    for (size_t i = 0; i < lineCount; ++i) {
        PointF pts[2] = {lines[i].p1(), lines[i].p2()};
        if (pts[0] == pts[1]) {
            LOGE() << "draw point not implemented";
        }
        drawPolyline(pts, 2);
    }
}

void Painter::drawLines(const PointF *pointPairs, size_t lineCount) {
    static_assert(sizeof(LineF) == 2 * sizeof(PointF), "must be: sizeof(LineF) == 2 * sizeof(PointF)");

    drawLines((const LineF *) pointPairs, lineCount);
}

void Painter::drawRects(const RectF *rects, size_t rectCount) {
    for (size_t i = 0; i < rectCount; ++i) {
        PainterPath path;
        path.addRect(rects[i]);
        if (path.isEmpty()) {
            continue;
        }
        drawPath(path);
    }
}

void Painter::drawEllipse(const RectF &rect) {
    PainterPath path;
    path.addEllipse(rect);
    provider_->drawPath(path);
    if (extended) {
        extended->drawPath(path);
    }
}

void Painter::drawPolyline(const PointF *points, size_t pointCount) {
    provider_->drawPolygon(points, pointCount, PolygonMode::Polyline);
    if (extended) {
        extended->drawPolygon(points, pointCount, PolygonMode::Polyline);
    }
}

void Painter::drawPolygon(const PointF *points, size_t pointCount, FillRule fillRule) {
    PolygonMode mode = (fillRule == FillRule::OddEvenFill) ? PolygonMode::OddEven : PolygonMode::Winding;
    provider_->drawPolygon(points, pointCount, mode);
    if (extended) {
        extended->drawPolygon(points, pointCount, mode);
    }
}

void Painter::drawConvexPolygon(const PointF *points, size_t pointCount) {
    provider_->drawPolygon(points, pointCount, PolygonMode::Convex);
    if (extended) {
        extended->drawPolygon(points, pointCount, PolygonMode::Convex);
    }
}

void Painter::drawArc(const RectF &r, int a, int alen) {
    //! NOTE Copied from QPainter source code

    RectF rect = r.normalized();

    PainterPath path;
    path.arcMoveTo(rect, a / 16.0);
    path.arcTo(rect, a / 16.0, alen / 16.0);
    strokePath(path, pen());
}

void Painter::drawRoundedRect(const RectF &rect, double xRadius, double yRadius) {
    if (xRadius <= 0 || yRadius <= 0) { // draw normal rectangle
        drawRect(rect);
        return;
    }

    PainterPath path;
    path.addRoundedRect(rect, xRadius, yRadius);
    drawPath(path);
}

void Painter::applyFontSizeScaling() {
    static constexpr double MU_ENGRAVING_DPI = 1200; // Same as mu::engraving::DPI. TODO: pass as parameter.
    Font f = font();
    double scaledPointSize = f.pointSizeF() * MU_ENGRAVING_DPI / deviceLogicalDpi();
    f.setPointSizeF(scaledPointSize);
    setFont(f);
}

void Painter::drawText(const PointF &point, const String &text) {
    applyFontSizeScaling();

    provider_->drawText(point, text);
    if (extended) {
        extended->drawText(point, text);
    }
}

void Painter::drawText(const RectF &rect, Alignment alignment, TextFlags textFlags, const String &text) {
    applyFontSizeScaling();

    provider_->drawText(rect, alignment, textFlags, text);
    if (extended) {
        extended->drawText(rect, alignment, textFlags, text);
    }
}

void Painter::drawSymbol(const PointF &point, char32_t ucs4Code) {
    applyFontSizeScaling();

    provider_->drawSymbol(point, ucs4Code);
    if (extended) {
        extended->drawSymbol(point, ucs4Code);
    }
}

void Painter::fillRect(const RectF &rect, const Brush &brush) {
    Pen oldPen = this->pen();
    Brush oldBrush = this->brush();
    setPen(Pen(PenStyle::NoPen));
    setBrush(brush);

    drawRect(rect);

    setBrush(oldBrush);
    setPen(oldPen);
}

void Painter::drawPixmap(const PointF &point, const Pixmap &pm) {
    provider_->drawPixmap(point, pm);
    if (extended) {
        extended->drawPixmap(point, pm);
    }
}

void Painter::drawTiledPixmap(const RectF &rect, const Pixmap &pm, const PointF &offset) {
    provider_->drawTiledPixmap(rect, pm, offset);
    if (extended) {
        extended->drawTiledPixmap(rect, pm, offset);
    }
}

Painter::State &Painter::editableState() { return states_.top(); }

const Painter::State &Painter::state() const { return states_.top(); }

void Painter::updateViewTransform() {
    State &st = editableState();
    if (st.window.isNull()) {
        st.viewTransform = Transform();
        updateMatrix();
        return;
    }

    double scaleW = double(st.viewport.width()) / double(st.window.width());
    double scaleH = double(st.viewport.height()) / double(st.window.height());
    st.viewTransform = Transform(scaleW, 0, 0, scaleH, st.viewport.x() - st.window.x() * scaleW, st.viewport.y() - st.window.y() * scaleH);
    updateMatrix();
}

void Painter::updateMatrix() {
    Painter::State &st = editableState();
    st.transform = st.isWxF ? st.worldTransform : Transform();
    if (st.isVxF) {
        st.transform *= st.viewTransform;
    }

    provider_->setTransform(st.transform);
    if (extended) {
        extended->setTransform(st.transform);
    }
}

bool Painter::hasClipping() const { return provider_->hasClipping(); }

void Painter::setClipRect(const RectF &rect) { provider_->setClipRect(rect); }

void Painter::setMask(const RectF &background, const std::vector<RectF> &maskRects) { provider_->setMask(background, maskRects); }

void Painter::setClipping(bool enable) { provider_->setClipping(enable); }
