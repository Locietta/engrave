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

#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "global/types/string.h"
#include "global/io/iodevice.h"

namespace muse {
struct TextStream;

struct XmlStreamWriter {
    XmlStreamWriter();
    explicit XmlStreamWriter(io::IODevice *dev);
    virtual ~XmlStreamWriter();

    XmlStreamWriter(const XmlStreamWriter &) = delete;
    XmlStreamWriter &operator=(const XmlStreamWriter &) = delete;

    using Value = std::variant<std::monostate, int, unsigned int, signed long int, unsigned long int, signed long long, unsigned long long,
                               double, const char *, std::string_view, String>;
    using Attribute = std::pair<std::string_view, Value>;
    using Attributes = std::vector<Attribute>;

    void set_device(io::IODevice *dev);
    void flush();

    void start_document();
    void write_doctype(const String &type);

    void start_element(const std::string_view &name, const Attributes &attrs = {});
    void start_element(const String &name, const Attributes &attrs = {});
    void end_element();

    void element(const std::string_view &name, const Attributes &attrs = {});               // <element attr="value" />
    void element(const std::string_view &name, const Value &body);                          // <element>body</element>
    void element(const std::string_view &name, const Attributes &attrs, const Value &body); // <element attr="value" >body</element>

    void comment(const String &text);

    static std::string escape_code_point(char32_t);
    static std::string escape_string(std::string_view);

protected:
    void start_element_raw(const String &name);
    void element_raw(const String &name_with_attributes, const Value &body);
    void element_string_raw(const String &name_with_attributes, const String &body);

private:

    void write_value(const Value &v);

    struct Impl;
    Impl *impl_ = nullptr;
};
} // namespace muse
