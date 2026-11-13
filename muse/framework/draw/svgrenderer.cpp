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
#include "svgrenderer.h"

#include <cmath>

#include "log.h"
#include "serialization/xmlstreamreader.h"

using namespace muse::draw;

//! NOTE Perhaps in the future we need to add something like ISvgRenderer

SvgRenderer::SvgRenderer(const ByteArray &data) : data_(data) {}

SvgRenderer::~SvgRenderer() {}

muse::SizeF SvgRenderer::default_size() const {
    muse::XmlStreamReader xml(data_);
    if (!xml.read_next_start_element() || xml.name() != "svg") return {};
    const auto length = [](muse::String value) {
        value = value.trimmed();
        const auto unit = value.right(2);
        double scale = 1.0;
        if (unit == u"in")
            scale = 96.0;
        else if (unit == u"cm")
            scale = 96.0 / 2.54;
        else if (unit == u"mm")
            scale = 96.0 / 25.4;
        else if (unit == u"pt")
            scale = 96.0 / 72.0;
        else if (unit == u"pc")
            scale = 16.0;
        if (scale != 1.0 || unit == u"px") value = value.left(value.size() - 2);
        bool ok = false;
        const double result = value.to_double(&ok) * scale;
        return ok && std::isfinite(result) && result > 0.0 ? result : 0.0;
    };
    double width = length(xml.attribute("width"));
    double height = length(xml.attribute("height"));
    const auto box = xml.attribute("viewBox").replace(u",", u" ").simplified().split(u' ');
    if (box.size() == 4) {
        const double box_width = length(box[2]);
        const double box_height = length(box[3]);
        if (box_width > 0.0 && box_height > 0.0) {
            if (!width && !height) {
                width = box_width;
                height = box_height;
            } else if (!width)
                width = height * box_width / box_height;
            else if (!height)
                height = width * box_height / box_width;
        }
    }
    return SizeF(width, height);
}

void SvgRenderer::render([[maybe_unused]] Painter *painter, [[maybe_unused]] const RectF &rect) { MUSE_NOT_SUPPORTED; }
