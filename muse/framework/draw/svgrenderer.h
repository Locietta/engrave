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
#ifndef MUSE_DRAW_SVGRENDERER_H
#define MUSE_DRAW_SVGRENDERER_H

#include "global/types/bytearray.h"
#include "painter.h"
#include "types/geometry.h"

struct QSvgRenderer;

namespace muse::draw {
struct SvgRenderer {
    SvgRenderer(const ByteArray &data);
    ~SvgRenderer();

    SizeF default_size() const;

    void render(Painter *painter, const RectF &rect);

private:
    QSvgRenderer *q_svg_renderer_ = nullptr;
};
} // namespace muse::draw

#endif // MUSE_DRAW_SVGRENDERER_H
