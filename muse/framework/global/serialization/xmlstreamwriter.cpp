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

#include <algorithm>
#include <limits>
#include <vector>

#include <utf8.h>

#include "global/containers.h"
#include "global/io/writebudget.h"
#include "global/log.h"
#include "textstream.h"

using namespace std::literals;
using namespace muse;

namespace {
size_t utf16_chunk_size(std::u16string_view text) {
    size_t count = std::min<size_t>(text.size(), 4096);
    if (count < text.size() && text[count] >= 0xdc00 && text[count] <= 0xdfff) --count;
    return count;
}
} // namespace

struct XmlStreamWriter::Impl {
    std::vector<std::string> stack;
    TextStream stream;
    bool indent = true;

    void put_level() {
        if (!indent) return;
        size_t level = stack.size();
        for (size_t i = 0; i < level * 2; ++i) {
            stream << ' ';
        }
    }
};

XmlStreamWriter::XmlStreamWriter() { impl_ = new Impl(); }

XmlStreamWriter::XmlStreamWriter(io::IODevice *dev) {
    impl_ = new Impl();
    impl_->stream.set_device(dev);
}

XmlStreamWriter::~XmlStreamWriter() {
    flush();
    delete impl_;
}

void XmlStreamWriter::set_indent(bool indent) { impl_->indent = indent; }

bool XmlStreamWriter::indent() const { return impl_->indent; }

void XmlStreamWriter::set_round_trip_doubles(bool round_trip_doubles) { impl_->stream.set_round_trip_doubles(round_trip_doubles); }

bool XmlStreamWriter::round_trip_doubles() const { return impl_->stream.round_trip_doubles(); }

void XmlStreamWriter::set_device(io::IODevice *dev) { impl_->stream.set_device(dev); }

void XmlStreamWriter::flush() { impl_->stream.flush(); }

void XmlStreamWriter::start_document() { impl_->stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"; }

void XmlStreamWriter::write_doctype(const String &type) { impl_->stream << "<!DOCTYPE " << type << '>' << '\n'; }

std::string XmlStreamWriter::escape_code_point(const char32_t c) {
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

std::string XmlStreamWriter::escape_string(const std::string_view str, bool compact) {
    // XML escaping can expand one input byte to six output bytes. Account for
    // that temporary before constructing it, and poll inside a large value.
    auto *budget = io::active_write_budget;
    if (budget) {
        const auto max = std::numeric_limits<std::uint64_t>::max();
        const auto amount = str.size() > max / 6 ? max : static_cast<std::uint64_t>(str.size()) * 6;
        if (!budget->charge(amount)) throw io::WriteBudgetStop{};
    }
    std::string escaped{};
    auto it = str.begin();
    auto next_poll = it;
    while (it != str.end()) {
        if (it >= next_poll) {
            if (budget && !budget->poll()) throw io::WriteBudgetStop{};
            next_poll = it + std::min<std::ptrdiff_t>(4093, str.end() - it);
        }
        const char32_t c = utf8::next(it, str.end());
        if (compact && c >= U' ' && c < 0x80 && c != U'<' && c != U'>' && c != U'&' && c != U'"')
            escaped.push_back(static_cast<char>(c));
        else
            escaped += escape_code_point(c);
    }

    return escaped;
}

void XmlStreamWriter::write_escaped(std::string_view value) {
    const bool compact = !impl_->indent;
    if (const auto *budget = io::active_write_budget; budget && budget->archive_entry) {
        while (value.size() > 4096) {
            std::size_t length = 4096;
            // Keep a multibyte code point in the next chunk if it straddles the boundary.
            while (length > 4093 && (static_cast<unsigned char>(value[length]) & 0xc0) == 0x80) --length;
            impl_->stream << escape_string(value.substr(0, length), compact);
            value.remove_prefix(length);
        }
    }
    impl_->stream << escape_string(value, compact);
}

void XmlStreamWriter::write_value(const Value &v) {
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
        case 8: write_escaped(AsciiStringView(std::get<const char *>(v))); break;
        case 9: write_escaped(std::get<std::string_view>(v)); break;
        case 10: {
            const auto &value = std::get<String>(v);
            if (const auto *budget = io::active_write_budget; budget && budget->archive_entry && value.size() > 4096) {
                auto text = value.view();
                std::string utf8;
                while (!text.empty()) {
                    impl_->stream.require_entry_space(text.size());
                    const auto count = utf16_chunk_size(text);
                    utf8.clear();
                    UtfCodec::utf16to8(text.substr(0, count), utf8);
                    write_escaped(utf8);
                    text.remove_prefix(count);
                }
            } else {
                write_escaped(value.to_std_string());
            }
            break;
        }
        default:
            LOGI() << "index: " << v.index();
            UNREACHABLE;
            break;
    }
}

void XmlStreamWriter::start_element(const std::string_view &name, const Attributes &attrs) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->put_level();
    impl_->stream << '<' << name;
    for (const Attribute &a : attrs) {
        impl_->stream << ' ' << a.first << '=' << '\"';
        write_value(a.second);
        impl_->stream << '\"';
    }
    impl_->stream << '>' << '\n';
    impl_->stack.emplace_back(name);
}

void XmlStreamWriter::start_element(const String &name, const Attributes &attrs) {
    ByteArray ba = name.to_ascii();
    start_element(AsciiStringView(ba.const_char(), ba.size()), attrs);
}

void XmlStreamWriter::start_element_raw(const String &name) {
    impl_->put_level();
    impl_->stream << '<' << name << '>' << '\n';
    impl_->stack.push_back(name.split(' ')[0].to_std_string());
}

void XmlStreamWriter::end_element() {
    impl_->put_level();
    impl_->stream << "</" << muse::take_last(impl_->stack) << '>' << '\n';
}

// <element attr="value" />
void XmlStreamWriter::element(const std::string_view &name, const Attributes &attrs) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->put_level();
    impl_->stream << '<' << name;
    for (const Attribute &a : attrs) {
        impl_->stream << ' ' << a.first << '=' << '\"';
        write_value(a.second);
        impl_->stream << '\"';
    }
    impl_->stream << "/>\n";
}

void XmlStreamWriter::element(const std::string_view &name, const Value &body) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->put_level();
    impl_->stream << '<' << name << '>';
    write_value(body);
    impl_->stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::element(const std::string_view &name, const Attributes &attrs, const Value &body) {
    IF_ASSERT_FAILED(name.find(' ') == std::string_view::npos) {}

    impl_->put_level();
    impl_->stream << '<' << name;
    for (const Attribute &a : attrs) {
        impl_->stream << ' ' << a.first << '=' << '\"';
        write_value(a.second);
        impl_->stream << '\"';
    }
    impl_->stream << ">";
    write_value(body);
    impl_->stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::element_raw(const String &name_with_attributes, const Value &body) {
    impl_->put_level();
    if (body.index() == 0) {
        impl_->stream << '<' << name_with_attributes << "/>\n";
    } else {
        String ename(String(name_with_attributes).split(' ')[0]);
        impl_->stream << '<' << name_with_attributes << '>';
        write_value(body);
        impl_->stream << "</" << ename << '>' << '\n';
    }
}

void XmlStreamWriter::element_string_raw(const String &name_with_attributes, const String &body) {
    impl_->put_level();
    if (body.is_empty()) {
        impl_->stream << '<' << name_with_attributes << "/>\n";
    } else {
        String ename(String(name_with_attributes).split(' ')[0]);
        impl_->stream << '<' << name_with_attributes << '>';
        impl_->stream << body;
        impl_->stream << "</" << ename << '>' << '\n';
    }
}

void XmlStreamWriter::element_text_raw(const String &name, std::u16string_view body, bool line_breaks) {
    // Every code unit contributes at least one UTF-8 byte after control replacement.
    impl_->stream.require_entry_space(body.size());
    impl_->put_level();
    impl_->stream << '<' << name;
    if (body.empty()) {
        impl_->stream << "/>\n";
        return;
    }
    impl_->stream << '>';
    std::u16string chunk;
    std::string utf8;
    while (!body.empty()) {
        impl_->stream.require_entry_space(body.size());
        const auto count = utf16_chunk_size(body);
        chunk.clear();
        for (char16_t ch : body.substr(0, count)) {
            if (line_breaks && ch == u'\n')
                chunk += u"<br/>";
            else
                chunk += ch < 0x20 && ch != u'\t' && ch != u'\n' && ch != u'\r' ? u'?' : ch;
        }
        utf8.clear();
        UtfCodec::utf16to8(chunk, utf8);
        impl_->stream << std::string_view(utf8);
        body.remove_prefix(count);
    }
    impl_->stream << "</" << name << ">\n";
}

void XmlStreamWriter::comment(const String &text) {
    impl_->put_level();
    impl_->stream << "<!-- " << text << " -->\n";
}
