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

#include "io/iodevice.h"
#include "serialization/xmlstreamwriter.h"

#include "../dom/property.h"

namespace mu::engraving {
struct EngravingObject;

struct XmlWriter : muse::XmlStreamWriter {
    XmlWriter() = default;
    XmlWriter(muse::io::IODevice *dev);
    ~XmlWriter();

    const std::vector<std::pair<const EngravingObject *, AsciiStringView>> &elements() const { return elements_value; }

    void set_record_elements(bool record) { record_elements = record; }

    void start_element_raw(const String &name);
    void start_element(const AsciiStringView &name, const Attributes &attrs = {});
    void start_element(const EngravingObject *se, const Attributes &attrs = {});
    void start_element(const AsciiStringView &name, const EngravingObject *se, const Attributes &attrs = {});

    void tag(const AsciiStringView &name, const Attributes &attrs = {});
    void tag(const AsciiStringView &name, const Value &body);
    void tag(const AsciiStringView &name, const Value &val, const Value &def);
    void tag(const AsciiStringView &name, const Attributes &attrs, const Value &body);
    void tag_raw(const String &element_with_attrs, const Value &body = Value());

    void tag_property(Pid id, const PropertyValue &data, const PropertyValue &def = PropertyValue());
    void tag_property(const AsciiStringView &, const PropertyValue &data, const PropertyValue &def = PropertyValue());

    void tag_fraction(const AsciiStringView &name, const Fraction &v, const Fraction &def = Fraction());
    void tag_point(const AsciiStringView &name, const PointF &v);

    void write_xml(const String &, String s);

    void comment(const String &text);

    static String xml_string(const String &);

private:
    void tag_property(const AsciiStringView &name, PType type, const PropertyValue &data);

    std::vector<std::pair<const EngravingObject *, AsciiStringView>> elements_value;
    bool record_elements = false;
};
} // namespace mu::engraving
