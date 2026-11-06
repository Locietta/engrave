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

#include "../types/types.h"
#include "draw/types/color.h"
#include "draw/types/geometry.h"
#include "serialization/xmlstreamreader.h"

namespace mu::engraving {
struct XmlReader : muse::XmlStreamReader {

    XmlReader() = default;

    XmlReader(const muse::ByteArray &d) : XmlStreamReader(d) {}

    XmlReader(muse::io::IODevice *d) : XmlStreamReader(d) {}

    XmlReader(const XmlReader &) = delete;
    XmlReader &operator=(const XmlReader &) = delete;

    ~XmlReader();

    void unknown();

    bool read_bool() { return XmlStreamReader::read_int(); }

    double read_double(bool *ok = nullptr) { return XmlStreamReader::read_double(ok); }

    double read_double(double min, double max);

    PointF read_point();
    SizeF read_size();
    ScaleF read_scale();
    RectF read_rect();
    Color read_color();
    Fraction read_fraction();
    String read_xml();

    void set_doc_name(const String &s) { doc_name_ = s; }

    String doc_name() const { return doc_name_; }

    // for reading old files (< 3.01)
    void set_byte_offset_adjustment(int64_t val) { byte_offset_adjustment_ = val; }

private:

    void html_to_string(int level, String *);

    String doc_name_; // used for error reporting
    int64_t byte_offset_adjustment_ = 0;
};
} // namespace mu::engraving
