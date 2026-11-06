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
#ifndef MUSE_GLOBAL_XMLSTREAMREADER_H
#define MUSE_GLOBAL_XMLSTREAMREADER_H

#include <vector>
#include <map>

#include "io/iodevice.h"
#include "types/bytearray.h"
#include "types/string.h"

namespace muse {
class XmlStreamReader {
public:

    enum TokenType { NO_TOKEN = 0, INVALID, START_DOCUMENT, END_DOCUMENT, START_ELEMENT, END_ELEMENT, CHARACTERS, COMMENT, DTD, UNKNOWN };

    enum Error { NO_ERROR, UNEXPECTED_ELEMENT_ERROR, CUSTOM_ERROR, NOT_WELL_FORMED_ERROR, PREMATURE_END_OF_DOCUMENT_ERROR };

    struct Attribute {
        AsciiStringView name;
        String value;
    };

    XmlStreamReader();
    explicit XmlStreamReader(io::IODevice *device);
    explicit XmlStreamReader(const ByteArray &data);
    virtual ~XmlStreamReader();

    XmlStreamReader(const XmlStreamReader &) = delete;
    XmlStreamReader &operator=(const XmlStreamReader &) = delete;

    void set_data(const ByteArray &data);

    bool read_next_start_element();
    bool at_end() const;
    void skip_current_element();
    TokenType read_next();
    TokenType token_type() const;
    AsciiStringView token_string() const;

    inline bool is_start_document() const { return token_type() == START_DOCUMENT; }

    inline bool is_end_document() const { return token_type() == END_DOCUMENT; }

    inline bool is_start_element() const { return token_type() == START_ELEMENT; }

    inline bool is_end_element() const { return token_type() == END_ELEMENT; }

    inline bool is_characters() const { return token_type() == CHARACTERS; }

    bool is_whitespace() const;

    AsciiStringView name() const;

    bool has_attribute(const char *name) const;
    String attribute(const char *name) const;
    String attribute(const char *name, const String &def) const;
    AsciiStringView ascii_attribute(const char *name) const;
    AsciiStringView ascii_attribute(const char *name, const AsciiStringView &def) const;
    int int_attribute(const char *name) const;
    int int_attribute(const char *name, int def) const;
    double double_attribute(const char *name) const;
    double double_attribute(const char *name, double def) const;
    std::vector<Attribute> attributes() const;

    bool no_children() const;

    String read_body() const;

    String text() const;
    AsciiStringView ascii_text() const;
    String read_text();
    AsciiStringView read_ascii_text();
    int read_int(bool *ok = nullptr, int base = 10);
    double read_double(bool *ok = nullptr);

    int64_t byte_offset() const;
    Error error() const;
    bool is_error() const;
    String error_string() const;
    void raise_error(const String &message);

private:
    struct Xml;

    void try_parse_entity(Xml *xml);
    String node_value(Xml *xml) const;

    Xml *xml_ = nullptr;
    TokenType token_ = TokenType::NO_TOKEN;

    std::map<String, String> entities_;
};
} // namespace muse

#endif // MUSE_GLOBAL_XMLSTREAMREADER_H
