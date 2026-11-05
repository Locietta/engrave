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
#include "xmlstreamwriter.h"

#include <vector>
#include <limits>

#include <utf8.h>

#include "global/containers.h"
#include "global/io/writebudget.h"
#include "textstream.h"

#include "global/log.h"

using namespace std::literals;
using namespace muse;

struct XmlStreamWriter::Impl {
    std::vector<std::string> stack;
    TextStream stream;

    void putLevel() {
        size_t level = stack.size();
        for (size_t i = 0; i < level * 2; ++i) {
            stream << ' ';
        }
    }
};

XmlStreamWriter::XmlStreamWriter() { impl_ = new Impl(); }

XmlStreamWriter::XmlStreamWriter(io::IODevice *dev) {
    impl_ = new Impl();
    impl_->stream.setDevice(dev);
}

XmlStreamWriter::~XmlStreamWriter() {
    flush();
    delete impl_;
}

void XmlStreamWriter::setDevice(io::IODevice *dev) { impl_->stream.setDevice(dev); }

void XmlStreamWriter::flush() { impl_->stream.flush(); }

void XmlStreamWriter::startDocument() { impl_->stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"; }

void XmlStreamWriter::writeDoctype(const String &type) { impl_->stream << "<!DOCTYPE " << type << '>' << '\n'; }

std::string XmlStreamWriter::escapeCodePoint(const char32_t c) {
    switch (c) {
        case U'<': return "&lt;"s;
        case U'>': return "&gt;"s;
        case U'&': return "&amp;"s;
        case U'\"': return "&quot;"s;
        default:
            // ignore invalid characters in xml 1.0
            if ((c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)) {
                return std::string();
            }
            return utf8::utf32to8(std::u32string_view(&c, 1));
    }
}

std::string XmlStreamWriter::escapeString(const std::string_view str) {
    // XML escaping can expand one input byte to six output bytes. Account for
    // that temporary before constructing it, and poll inside a large value.
    if (io::activeWriteBudget) {
        const auto max = std::numeric_limits<std::uint64_t>::max();
        const auto amount = str.size() > max / 6 ? max : static_cast<std::uint64_t>(str.size()) * 6;
        if (!io::activeWriteBudget->charge(amount)) throw io::WriteBudgetStop{};
    }
    std::string escaped{};
    auto it = str.begin();
    std::size_t checked = 0;
    while (it != str.end()) {
        if ((checked++ & 4095u) == 0 && io::activeWriteBudget && !io::activeWriteBudget->poll()) throw io::WriteBudgetStop{};
        const char32_t c = utf8::next(it, str.end());
        escaped += escapeCodePoint(c);
    }

    return escaped;
}

void XmlStreamWriter::writeValue(const Value &v) {
    // std::monostate, int, unsigned int, signed long int, unsigned long int, signed long long, unsigned long long,
    // double, const char*, std::string_view, String
    switch (v.index()) {
        case 0: break;
        case 1: impl_->stream << int32_t{std::get<int>(v)}; break;
        case 2: impl_->stream << uint32_t{std::get<unsigned int>(v)}; break;
        case 3: impl_->stream << int64_t{std::get<signed long int>(v)}; break;
        case 4: impl_->stream << uint64_t{std::get<unsigned long int>(v)}; break;
        case 5: impl_->stream << int64_t{std::get<signed long long>(v)}; break;
        case 6: impl_->stream << uint64_t{std::get<unsigned long long>(v)}; break;
        case 7: impl_->stream << std::get<double>(v); break;
        case 8: impl_->stream << escapeString(AsciiStringView(std::get<const char *>(v))); break;
        case 9: impl_->stream << escapeString(std::get<std::string_view>(v)); break;
        case 10: impl_->stream << escapeString(std::get<String>(v).toStdString()); break;
        default:
            LOGI() << "index: " << v.index();
            UNREACHABLE;
            break;
    }
}

void XmlStreamWriter::startElement(const std::string_view &name, const Attributes &attrs) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->putLevel();
    impl_->stream << '<' << name;
    for (const Attribute &a : attrs) {
        impl_->stream << ' ' << a.first << '=' << '\"';
        writeValue(a.second);
        impl_->stream << '\"';
    }
    impl_->stream << '>' << '\n';
    impl_->stack.emplace_back(name);
}

void XmlStreamWriter::startElement(const String &name, const Attributes &attrs) {
    ByteArray ba = name.toAscii();
    startElement(AsciiStringView(ba.constChar(), ba.size()), attrs);
}

void XmlStreamWriter::startElementRaw(const String &name) {
    impl_->putLevel();
    impl_->stream << '<' << name << '>' << '\n';
    impl_->stack.push_back(name.split(' ')[0].toStdString());
}

void XmlStreamWriter::endElement() {
    impl_->putLevel();
    impl_->stream << "</" << muse::takeLast(impl_->stack) << '>' << '\n';
}

// <element attr="value" />
void XmlStreamWriter::element(const std::string_view &name, const Attributes &attrs) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->putLevel();
    impl_->stream << '<' << name;
    for (const Attribute &a : attrs) {
        impl_->stream << ' ' << a.first << '=' << '\"';
        writeValue(a.second);
        impl_->stream << '\"';
    }
    impl_->stream << "/>\n";
}

void XmlStreamWriter::element(const std::string_view &name, const Value &body) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->putLevel();
    impl_->stream << '<' << name << '>';
    writeValue(body);
    impl_->stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::element(const std::string_view &name, const Attributes &attrs, const Value &body) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->putLevel();
    impl_->stream << '<' << name;
    for (const Attribute &a : attrs) {
        impl_->stream << ' ' << a.first << '=' << '\"';
        writeValue(a.second);
        impl_->stream << '\"';
    }
    impl_->stream << ">";
    writeValue(body);
    impl_->stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::elementRaw(const String &nameWithAttributes, const Value &body) {
    impl_->putLevel();
    if (body.index() == 0) {
        impl_->stream << '<' << nameWithAttributes << "/>\n";
    } else {
        String ename(String(nameWithAttributes).split(' ')[0]);
        impl_->stream << '<' << nameWithAttributes << '>';
        writeValue(body);
        impl_->stream << "</" << ename << '>' << '\n';
    }
}

void XmlStreamWriter::elementStringRaw(const String &nameWithAttributes, const String &body) {
    impl_->putLevel();
    if (body.isEmpty()) {
        impl_->stream << '<' << nameWithAttributes << "/>\n";
    } else {
        String ename(String(nameWithAttributes).split(' ')[0]);
        impl_->stream << '<' << nameWithAttributes << '>';
        impl_->stream << body;
        impl_->stream << "</" << ename << '>' << '\n';
    }
}

void XmlStreamWriter::comment(const String &text) {
    impl_->putLevel();
    impl_->stream << "<!-- " << text << " -->\n";
}
