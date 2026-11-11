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
#include "xmlstreamreader.h"

#include <cstring>
#include <sstream>

#include "pugixml.hpp"

#include "log.h"

using namespace muse;
using namespace muse::io;

struct XmlStreamReader::Xml {
    pugi::xml_document doc;
    pugi::xml_node node{};
    pugi::xml_parse_result result{};
    String custom_err;
};

XmlStreamReader::XmlStreamReader() { xml_ = new Xml(); }

XmlStreamReader::XmlStreamReader(IODevice *device) {
    xml_ = new Xml();
    ByteArray data = device->read_all();
    set_data(data);
}

XmlStreamReader::XmlStreamReader(const ByteArray &data) {
    xml_ = new Xml();
    set_data(data);
}

XmlStreamReader::~XmlStreamReader() { delete xml_; }

void XmlStreamReader::set_data(const ByteArray &input_data) {

    xml_->doc.reset();
    xml_->custom_err.clear();
    token_ = TokenType::INVALID;

    if (input_data.size() < 4) {
        xml_->result = pugi::xml_parse_result{}; // zero it
        xml_->result.status = pugi::status_no_document_element;
        xml_->custom_err = String(u"empty document");
        LOGE() << xml_->custom_err;
        return;
    }

    UtfCodec::Encoding enc = UtfCodec::xml_encoding(input_data);
    if (enc == UtfCodec::Encoding::UNKNOWN) {
        xml_->result = pugi::xml_parse_result{};
        xml_->result.status = pugi::status_internal_error;
        xml_->custom_err = String(u"unknown encoding");
        LOGE() << xml_->custom_err;
        return;
    }

    ByteArray data = input_data; // no copy, implicit sharing
    if (enc == UtfCodec::Encoding::UTF_16LE) {
        String u16 = String::from_utf16_le(input_data);
        data = u16.to_utf8();
    } else if (enc == UtfCodec::Encoding::UTF_16BE) {
        String u16 = String::from_utf16_be(input_data);
        data = u16.to_utf8();
    }

    // pugi needs explicit flags to surface declaration/doctype/comments as nodes
    unsigned flags = pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype | pugi::parse_comments;
    xml_->result = xml_->doc.load_buffer(data.const_data(), data.size(), flags);

    if (xml_->result.status == pugi::status_ok) {
        token_ = TokenType::NO_TOKEN;
    } else {
        LOGE() << String::from_utf8(xml_->result.description());
    }
}

bool XmlStreamReader::read_next_start_element() {
    while (read_next() != INVALID) {
        if (is_end_element()) {
            return false;
        } else if (is_start_element()) {
            return true;
        }
    }
    return false;
}

bool XmlStreamReader::at_end() const { return token_ == TokenType::END_DOCUMENT || token_ == TokenType::INVALID; }

static XmlStreamReader::TokenType resolve_token(pugi::xml_node n, bool is_start_element) {
    switch (n.type()) {
        case pugi::node_element:
            return is_start_element ? XmlStreamReader::TokenType::START_ELEMENT : XmlStreamReader::TokenType::END_ELEMENT;

        case pugi::node_pcdata:
        case pugi::node_cdata: return XmlStreamReader::TokenType::CHARACTERS;

        case pugi::node_comment: return XmlStreamReader::TokenType::COMMENT;

        case pugi::node_declaration: // <?xml ... ?>
            return XmlStreamReader::TokenType::START_DOCUMENT;

        case pugi::node_document: // the document node
            return XmlStreamReader::TokenType::END_DOCUMENT;

        case pugi::node_doctype: // <!DOCTYPE ...>
            return XmlStreamReader::TokenType::DTD;

        default: // includes node_pi, node_null, etc.
            return XmlStreamReader::TokenType::UNKNOWN;
    }
}

static std::pair<pugi::xml_node, XmlStreamReader::TokenType> resolve_node(pugi::xml_node current_node,
                                                                          XmlStreamReader::TokenType current_token) {
    if (current_token == XmlStreamReader::TokenType::START_ELEMENT) {
        pugi::xml_node child = current_node.first_child();
        if (child) {
            return {child, resolve_token(child, /*isStartElement=*/true)};
        }

        pugi::xml_node sibling = current_node.next_sibling();
        if (!sibling || sibling.type() == pugi::node_element || sibling.type() == pugi::node_pcdata || sibling.type() == pugi::node_cdata ||
            sibling.type() == pugi::node_comment) {
            // Mirror tiny: close the element before stepping to "content" siblings
            return {current_node, XmlStreamReader::TokenType::END_ELEMENT};
        }
        // else: fall through to handle non-content siblings (doctype, declaration, etc.)
    }

    pugi::xml_node sibling = current_node.next_sibling();
    if (sibling) {
        return {sibling, resolve_token(sibling, /*isStartElement=*/true)};
    }

    pugi::xml_node parent = current_node.parent();
    if (parent) {
        return {parent, resolve_token(parent, /*isStartElement=*/false)};
    }

    return {pugi::xml_node(), XmlStreamReader::TokenType::END_DOCUMENT};
}

XmlStreamReader::TokenType XmlStreamReader::read_next() {
    if (token_ == TokenType::INVALID) {
        return token_;
    }

    if (xml_->result.status != pugi::status_ok || token_ == TokenType::END_DOCUMENT) {
        xml_->node = pugi::xml_node();
        token_ = TokenType::INVALID;
        return token_;
    }

    if (!xml_->node) {
        xml_->node = xml_->doc.first_child();
        if (!xml_->node) {
            // Empty doc — treat as end
            token_ = TokenType::END_DOCUMENT;
            return token_;
        }
        token_ =
            (xml_->node.type() == pugi::node_declaration) ? TokenType::START_DOCUMENT : resolve_token(xml_->node, /*isStartElement=*/true);
        return token_;
    }

    std::pair<pugi::xml_node, XmlStreamReader::TokenType> p = resolve_node(xml_->node, token_);

    xml_->node = p.first;
    token_ = p.second;

    if (token_ == TokenType::DTD) {
        try_parse_entity(xml_);
    }

    return token_;
}

void XmlStreamReader::try_parse_entity(Xml *xml) {
    const char *node_value = xml->node.value();

    if (!node_value || *node_value == '\0') {
        return;
    }

    const char *scan_pos = node_value;
    while (true) {
        const char *entity_pos = std::strstr(scan_pos, "ENTITY");
        if (!entity_pos) {
            break;
        }

        const char *cur = entity_pos + 6; // after "ENTITY"

        // Skip whitespace
        while (*cur == ' ' || *cur == '\t' || *cur == '\r' || *cur == '\n') {
            ++cur;
        }

        // Optional leading '%' (parameter entity)
        if (*cur == '%') {
            ++cur;
            while (*cur == ' ' || *cur == '\t' || *cur == '\r' || *cur == '\n') {
                ++cur;
            }
        }

        // Name token: up to whitespace / quote / '>'
        const char *name_begin = cur;
        while (*cur && *cur != ' ' && *cur != '\t' && *cur != '\r' && *cur != '\n' && *cur != '"' && *cur != '\'' && *cur != '>') {
            ++cur;
        }
        const char *name_end = cur;

        // Handle a leading '%' glued to the name (parameter entity without a space)
        if (name_begin < name_end && *name_begin == '%') {
            ++name_begin;
            while (name_begin < name_end && (*name_begin == ' ' || *name_begin == '\t' || *name_begin == '\r' || *name_begin == '\n')) {
                ++name_begin;
            }
        }

        // Find first quote (either ' or ")
        while (*cur && *cur != '"' && *cur != '\'') {
            ++cur;
        }
        if (*cur != '"' && *cur != '\'') {
            scan_pos = entity_pos + 6;
            continue;
        }

        // Capture quoted value
        const char quote_char = *cur++;
        const char *value_begin = cur;
        while (*cur && *cur != quote_char) {
            ++cur;
        }
        if (*cur != quote_char) {
            scan_pos = entity_pos + 6;
            continue;
        }
        const char *value_end = cur;
        ++cur; // past closing quote

        // Build name/value:
        // - DO NOT remove "SYSTEM"/"PUBLIC" from the name
        // - Trim whitespace
        std::string name_token(name_begin, static_cast<size_t>(name_end - name_begin));
        String name = String::from_utf8(name_token.c_str()).trimmed();

        std::string value_token(value_begin, static_cast<size_t>(value_end - value_begin));
        String value = String::from_utf8(value_token.c_str());

        if (!name.empty()) {
            entities_[u'&' + name + u';'] = value;
        } else {
            LOGW() << "Ignoring malformed ENTITY: " << node_value;
        }

        scan_pos = cur; // continue scanning for more ENTITY decls
    }
}

// emulate tinyxml2::XMLNode::Value
String XmlStreamReader::node_value(Xml *xml) const {
    const pugi::xml_node n = xml->node;

    const char *raw = "";
    switch (n.type()) {
        case pugi::node_element:
        case pugi::node_pi:
        case pugi::node_declaration:
        case pugi::node_doctype:
        case pugi::node_document: // usually empty
            raw = n.name();
            break;

        case pugi::node_pcdata:
        case pugi::node_cdata:
        case pugi::node_comment: raw = n.value(); break;

        default: break;
    }

    String str = String::from_utf8(raw);
    if (!entities_.empty()) {
        for (const auto &p : entities_) {
            str.replace(p.first, p.second);
        }
    }
    return str;
}

XmlStreamReader::TokenType XmlStreamReader::token_type() const { return token_; }

AsciiStringView XmlStreamReader::token_string() const {
    switch (token_) {
        case TokenType::NO_TOKEN: return "NoToken";
        case TokenType::INVALID: return "Invalid";
        case TokenType::START_DOCUMENT: return "StartDocument";
        case TokenType::END_DOCUMENT: return "EndDocument";
        case TokenType::START_ELEMENT: return "StartElement";
        case TokenType::END_ELEMENT: return "EndElement";
        case TokenType::CHARACTERS: return "Characters";
        case TokenType::COMMENT: return "Comment";
        case TokenType::DTD: return "DTD";
        case TokenType::UNKNOWN: return "Unknown";
    }
    return AsciiStringView();
}

bool XmlStreamReader::is_whitespace() const { return false; }

void XmlStreamReader::skip_current_element() {
    int depth = 1;
    while (depth && read_next() != INVALID) {
        if (is_end_element()) {
            --depth;
        } else if (is_start_element()) {
            ++depth;
        }
    }
}

AsciiStringView XmlStreamReader::name() const {
    return (xml_->node && xml_->node.type() == pugi::node_element) ? AsciiStringView(xml_->node.name()) : AsciiStringView();
}

bool XmlStreamReader::has_attribute(const char *name) const {
    if (token_ != TokenType::START_ELEMENT) {
        return false;
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return false;
    }

    return xml_->node.attribute(name);
}

String XmlStreamReader::attribute(const char *name) const {
    if (token_ != TokenType::START_ELEMENT) {
        return String();
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return String();
    }

    pugi::xml_attribute attr = xml_->node.attribute(name);
    if (!attr) {
        return String();
    }

    return String::from_utf8(attr.value());
}

String XmlStreamReader::attribute(const char *name, const String &def) const { return has_attribute(name) ? attribute(name) : def; }

AsciiStringView XmlStreamReader::ascii_attribute(const char *name) const {
    if (token_ != TokenType::START_ELEMENT) {
        return AsciiStringView();
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return AsciiStringView();
    }

    pugi::xml_attribute attr = xml_->node.attribute(name);
    return attr ? AsciiStringView(attr.value()) : AsciiStringView();
}

AsciiStringView XmlStreamReader::ascii_attribute(const char *name, const AsciiStringView &def) const {
    return has_attribute(name) ? ascii_attribute(name) : def;
}

int XmlStreamReader::int_attribute(const char *name) const { return ascii_attribute(name).to_int(); }

int XmlStreamReader::int_attribute(const char *name, int def) const { return has_attribute(name) ? int_attribute(name) : def; }

double XmlStreamReader::double_attribute(const char *name) const { return ascii_attribute(name).to_double(); }

double XmlStreamReader::double_attribute(const char *name, double def) const { return has_attribute(name) ? double_attribute(name) : def; }

std::vector<XmlStreamReader::Attribute> XmlStreamReader::attributes() const {
    std::vector<Attribute> attrs;
    if (token_ != TokenType::START_ELEMENT) {
        return attrs;
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return attrs;
    }

    for (pugi::xml_attribute xa = xml_->node.first_attribute(); xa; xa = xa.next_attribute()) {
        Attribute a;
        a.name = xa.name();
        a.value = String::from_utf8(xa.value());
        attrs.push_back(std::move(a));
    }

    return attrs;
}

bool XmlStreamReader::no_children() const { return xml_->node ? !xml_->node.first_child() && !xml_->node.last_child() : false; }

String XmlStreamReader::read_body() const {
    if (!xml_->node) {
        return String();
    }

    std::ostringstream oss;

    for (pugi::xml_node child = xml_->node.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_element) {
            // Match tinyxml2::XMLPrinter default (no indentation/line breaks)
            child.print(oss, "", pugi::format_raw);
        }
    }

    return String::from_std_string(oss.str());
}

String XmlStreamReader::text() const {
    if (xml_->node) {
        pugi::xml_node_type t = xml_->node.type();
        if (t == pugi::node_pcdata || t == pugi::node_cdata || t == pugi::node_comment) {
            return node_value(xml_);
        }
    }
    return String();
}

AsciiStringView XmlStreamReader::ascii_text() const {
    if (xml_->node) {
        pugi::xml_node_type t = xml_->node.type();
        if (t == pugi::node_pcdata || t == pugi::node_cdata || t == pugi::node_comment) {
            return xml_->node.value();
        }
    }
    return AsciiStringView();
}

String XmlStreamReader::read_text() {
    if (is_start_element()) {
        String result;
        while (1) {
            switch (read_next()) {
                case CHARACTERS: result = node_value(xml_); break;
                case END_ELEMENT: return result;
                case COMMENT: break;
                case START_ELEMENT: break;
                default: break;
            }
        }
    }
    return String();
}

AsciiStringView XmlStreamReader::read_ascii_text() {
    if (is_start_element()) {
        AsciiStringView result;
        while (1) {
            switch (read_next()) {
                case CHARACTERS: result = AsciiStringView(xml_->node.value()); break;
                case END_ELEMENT: return result;
                case COMMENT: break;
                case START_ELEMENT: break;
                default: break;
            }
        }
    }
    return AsciiStringView();
}

int XmlStreamReader::read_int(bool *ok, int base) {
    AsciiStringView s = read_ascii_text();
    return s.to_int(ok, base);
}

double XmlStreamReader::read_double(bool *ok) {
    AsciiStringView s = read_ascii_text();
    return s.to_double(ok);
}

int64_t XmlStreamReader::byte_offset() const {
    if (!xml_->node) {
        return 0;
    }

    // Use byte offset from start of document as a temporary proxy for line number
    return static_cast<int64_t>(xml_->node.offset_debug());
}

XmlStreamReader::Error XmlStreamReader::error() const {
    if (!xml_->custom_err.is_empty()) {
        return CUSTOM_ERROR;
    }

    if (xml_->result.status == pugi::status_ok) {
        return NO_ERROR;
    }

    return NOT_WELL_FORMED_ERROR;
}

bool XmlStreamReader::is_error() const { return error() != NO_ERROR; }

String XmlStreamReader::error_string() const {
    if (!xml_->custom_err.empty()) {
        return xml_->custom_err;
    }
    return String::from_utf8(xml_->result.description());
}

void XmlStreamReader::raise_error(const String &message) { xml_->custom_err = message; }
