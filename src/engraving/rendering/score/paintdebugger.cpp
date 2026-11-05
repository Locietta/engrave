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
#include "paintdebugger.h"

using namespace mu;
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

PaintDebugger::PaintDebugger(IPaintProviderPtr real) : real_(real) {}

IPaintProviderPtr PaintDebugger::realProvider() const { return real_; }

void PaintDebugger::setDebugPenColor(const Color &c) {
    debugPenColor_ = c;
    const Pen &originalPen = pen();
    originalPenColor_ = originalPen.color();
    Pen p = originalPen;
    p.setColor(c);
    setPen(p);
}

void PaintDebugger::restorePenColor() {
    Pen p = pen();
    p.setColor(originalPenColor_);
    setPen(p);
    debugPenColor_ = Color();
}

void PaintDebugger::beginTarget(const std::string &name) { real_->beginTarget(name); }

void PaintDebugger::beforeEndTargetHook(Painter *p) { real_->beforeEndTargetHook(p); }

bool PaintDebugger::endTarget(bool endDraw) { return real_->endTarget(endDraw); }

bool PaintDebugger::isActive() const { return real_->isActive(); }

void PaintDebugger::beginObject(const std::string &name) { real_->beginObject(name); }

void PaintDebugger::endObject() { real_->endObject(); }

void PaintDebugger::setAntialiasing(bool arg) { real_->setAntialiasing(arg); }

void PaintDebugger::setCompositionMode(CompositionMode mode) { real_->setCompositionMode(mode); }

void PaintDebugger::setWindow(const RectF &window) { real_->setWindow(window); }

void PaintDebugger::setViewport(const RectF &viewport) { real_->setViewport(viewport); }

void PaintDebugger::setFont(const Font &f) { real_->setFont(f); }

const Font &PaintDebugger::font() const { return real_->font(); }

void PaintDebugger::setPen(const Pen &pen) {
    Pen p = pen;
    if (debugPenColor_.isValid()) {
        p.setColor(debugPenColor_);
    }
    real_->setPen(p);
}

void PaintDebugger::setNoPen() { real_->setNoPen(); }

const Pen &PaintDebugger::pen() const { return real_->pen(); }

void PaintDebugger::setBrush(const Brush &brush) { real_->setBrush(brush); }

const Brush &PaintDebugger::brush() const { return real_->brush(); }

void PaintDebugger::save() { real_->save(); }

void PaintDebugger::restore() { real_->restore(); }

void PaintDebugger::setTransform(const Transform &transform) { real_->setTransform(transform); }

const Transform &PaintDebugger::transform() const { return real_->transform(); }

// drawing functions

void PaintDebugger::drawPath(const PainterPath &path) { real_->drawPath(path); }

void PaintDebugger::drawPolygon(const PointF *points, size_t pointCount, PolygonMode mode) { real_->drawPolygon(points, pointCount, mode); }

void PaintDebugger::drawText(const PointF &point, const String &text) { real_->drawText(point, text); }

void PaintDebugger::drawText(const RectF &rect, Alignment alignment, TextFlags textFlags, const String &text) {
    real_->drawText(rect, alignment, textFlags, text);
}

void PaintDebugger::drawSymbol(const PointF &point, char32_t ucs4Code) { real_->drawSymbol(point, ucs4Code); }

void PaintDebugger::drawPixmap(const PointF &p, const Pixmap &pm) { real_->drawPixmap(p, pm); }

void PaintDebugger::drawTiledPixmap(const RectF &rect, const Pixmap &pm, const PointF &offset) { real_->drawTiledPixmap(rect, pm, offset); }

bool PaintDebugger::hasClipping() const { return real_->hasClipping(); }

void PaintDebugger::setClipRect(const RectF &rect) { real_->setClipRect(rect); }

void PaintDebugger::setMask(const RectF &background, const std::vector<RectF> &maskRects) { real_->setMask(background, maskRects); }

void PaintDebugger::setClipping(bool enable) { real_->setClipping(enable); }

double mu::engraving::rendering::score::PaintDebugger::deviceLogicalDpi() const { return real_->deviceLogicalDpi(); }
