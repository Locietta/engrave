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
    String customErr;
};

XmlStreamReader::XmlStreamReader() { xml_ = new Xml(); }

XmlStreamReader::XmlStreamReader(IODevice *device) {
    xml_ = new Xml();
    ByteArray data = device->readAll();
    setData(data);
}

XmlStreamReader::XmlStreamReader(const ByteArray &data) {
    xml_ = new Xml();
    setData(data);
}

XmlStreamReader::~XmlStreamReader() { delete xml_; }

void XmlStreamReader::setData(const ByteArray &data_) {
    TRACEFUNC;

    xml_->doc.reset();
    xml_->customErr.clear();
    token_ = TokenType::Invalid;

    if (data_.size() < 4) {
        xml_->result = pugi::xml_parse_result{}; // zero it
        xml_->result.status = pugi::status_no_document_element;
        xml_->customErr = String(u"empty document");
        LOGE() << xml_->customErr;
        return;
    }

    UtfCodec::Encoding enc = UtfCodec::xmlEncoding(data_);
    if (enc == UtfCodec::Encoding::Unknown) {
        xml_->result = pugi::xml_parse_result{};
        xml_->result.status = pugi::status_internal_error;
        xml_->customErr = String(u"unknown encoding");
        LOGE() << xml_->customErr;
        return;
    }

    ByteArray data = data_; // no copy, implicit sharing
    if (enc == UtfCodec::Encoding::UTF_16LE) {
        String u16 = String::fromUtf16LE(data_);
        data = u16.toUtf8();
    } else if (enc == UtfCodec::Encoding::UTF_16BE) {
        String u16 = String::fromUtf16BE(data_);
        data = u16.toUtf8();
    }

    // pugi needs explicit flags to surface declaration/doctype/comments as nodes
    unsigned flags = pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype | pugi::parse_comments;
    xml_->result = xml_->doc.load_buffer(data.constData(), data.size(), flags);

    if (xml_->result.status == pugi::status_ok) {
        token_ = TokenType::NoToken;
    } else {
        LOGE() << String::fromUtf8(xml_->result.description());
    }
}

bool XmlStreamReader::readNextStartElement() {
    while (readNext() != Invalid) {
        if (isEndElement()) {
            return false;
        } else if (isStartElement()) {
            return true;
        }
    }
    return false;
}

bool XmlStreamReader::atEnd() const { return token_ == TokenType::EndDocument || token_ == TokenType::Invalid; }

static XmlStreamReader::TokenType resolveToken(pugi::xml_node n, bool isStartElement) {
    switch (n.type()) {
        case pugi::node_element: return isStartElement ? XmlStreamReader::TokenType::StartElement : XmlStreamReader::TokenType::EndElement;

        case pugi::node_pcdata:
        case pugi::node_cdata: return XmlStreamReader::TokenType::Characters;

        case pugi::node_comment: return XmlStreamReader::TokenType::Comment;

        case pugi::node_declaration: // <?xml ... ?>
            return XmlStreamReader::TokenType::StartDocument;

        case pugi::node_document: // the document node
            return XmlStreamReader::TokenType::EndDocument;

        case pugi::node_doctype: // <!DOCTYPE ...>
            return XmlStreamReader::TokenType::DTD;

        default: // includes node_pi, node_null, etc.
            return XmlStreamReader::TokenType::Unknown;
    }
}

static std::pair<pugi::xml_node, XmlStreamReader::TokenType> resolveNode(pugi::xml_node currentNode,
                                                                         XmlStreamReader::TokenType currentToken) {
    if (currentToken == XmlStreamReader::TokenType::StartElement) {
        pugi::xml_node child = currentNode.first_child();
        if (child) {
            return {child, resolveToken(child, /*isStartElement=*/true)};
        }

        pugi::xml_node sibling = currentNode.next_sibling();
        if (!sibling || sibling.type() == pugi::node_element || sibling.type() == pugi::node_pcdata || sibling.type() == pugi::node_cdata ||
            sibling.type() == pugi::node_comment) {
            // Mirror tiny: close the element before stepping to "content" siblings
            return {currentNode, XmlStreamReader::TokenType::EndElement};
        }
        // else: fall through to handle non-content siblings (doctype, declaration, etc.)
    }

    pugi::xml_node sibling = currentNode.next_sibling();
    if (sibling) {
        return {sibling, resolveToken(sibling, /*isStartElement=*/true)};
    }

    pugi::xml_node parent = currentNode.parent();
    if (parent) {
        return {parent, resolveToken(parent, /*isStartElement=*/false)};
    }

    return {pugi::xml_node(), XmlStreamReader::TokenType::EndDocument};
}

XmlStreamReader::TokenType XmlStreamReader::readNext() {
    if (token_ == TokenType::Invalid) {
        return token_;
    }

    if (xml_->result.status != pugi::status_ok || token_ == TokenType::EndDocument) {
        xml_->node = pugi::xml_node();
        token_ = TokenType::Invalid;
        return token_;
    }

    if (!xml_->node) {
        xml_->node = xml_->doc.first_child();
        if (!xml_->node) {
            // Empty doc — treat as end
            token_ = TokenType::EndDocument;
            return token_;
        }
        token_ =
            (xml_->node.type() == pugi::node_declaration) ? TokenType::StartDocument : resolveToken(xml_->node, /*isStartElement=*/true);
        return token_;
    }

    std::pair<pugi::xml_node, XmlStreamReader::TokenType> p = resolveNode(xml_->node, token_);

    xml_->node = p.first;
    token_ = p.second;

    if (token_ == TokenType::DTD) {
        tryParseEntity(xml_);
    }

    return token_;
}

void XmlStreamReader::tryParseEntity(Xml *xml) {
    const char *nodeValue = xml->node.value();

    if (!nodeValue || *nodeValue == '\0') {
        return;
    }

    const char *scanPos = nodeValue;
    while (true) {
        const char *entityPos = std::strstr(scanPos, "ENTITY");
        if (!entityPos) {
            break;
        }

        const char *cur = entityPos + 6; // after "ENTITY"

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
        const char *nameBegin = cur;
        while (*cur && *cur != ' ' && *cur != '\t' && *cur != '\r' && *cur != '\n' && *cur != '"' && *cur != '\'' && *cur != '>') {
            ++cur;
        }
        const char *nameEnd = cur;

        // Handle a leading '%' glued to the name (parameter entity without a space)
        if (nameBegin < nameEnd && *nameBegin == '%') {
            ++nameBegin;
            while (nameBegin < nameEnd && (*nameBegin == ' ' || *nameBegin == '\t' || *nameBegin == '\r' || *nameBegin == '\n')) {
                ++nameBegin;
            }
        }

        // Find first quote (either ' or ")
        while (*cur && *cur != '"' && *cur != '\'') {
            ++cur;
        }
        if (*cur != '"' && *cur != '\'') {
            scanPos = entityPos + 6;
            continue;
        }

        // Capture quoted value
        const char quoteChar = *cur++;
        const char *valueBegin = cur;
        while (*cur && *cur != quoteChar) {
            ++cur;
        }
        if (*cur != quoteChar) {
            scanPos = entityPos + 6;
            continue;
        }
        const char *valueEnd = cur;
        ++cur; // past closing quote

        // Build name/value:
        // - DO NOT remove "SYSTEM"/"PUBLIC" from the name
        // - Trim whitespace
        std::string nameToken(nameBegin, static_cast<size_t>(nameEnd - nameBegin));
        String name = String::fromUtf8(nameToken.c_str()).trimmed();

        std::string valueToken(valueBegin, static_cast<size_t>(valueEnd - valueBegin));
        String value = String::fromUtf8(valueToken.c_str());

        if (!name.empty()) {
            entities_[u'&' + name + u';'] = value;
        } else {
            LOGW() << "Ignoring malformed ENTITY: " << nodeValue;
        }

        scanPos = cur; // continue scanning for more ENTITY decls
    }
}

// emulate tinyxml2::XMLNode::Value
String XmlStreamReader::nodeValue(Xml *xml) const {
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

    String str = String::fromUtf8(raw);
    if (!entities_.empty()) {
        for (const auto &p : entities_) {
            str.replace(p.first, p.second);
        }
    }
    return str;
}

XmlStreamReader::TokenType XmlStreamReader::tokenType() const { return token_; }

AsciiStringView XmlStreamReader::tokenString() const {
    switch (token_) {
        case TokenType::NoToken: return "NoToken";
        case TokenType::Invalid: return "Invalid";
        case TokenType::StartDocument: return "StartDocument";
        case TokenType::EndDocument: return "EndDocument";
        case TokenType::StartElement: return "StartElement";
        case TokenType::EndElement: return "EndElement";
        case TokenType::Characters: return "Characters";
        case TokenType::Comment: return "Comment";
        case TokenType::DTD: return "DTD";
        case TokenType::Unknown: return "Unknown";
    }
    return AsciiStringView();
}

bool XmlStreamReader::isWhitespace() const { return false; }

void XmlStreamReader::skipCurrentElement() {
    int depth = 1;
    while (depth && readNext() != Invalid) {
        if (isEndElement()) {
            --depth;
        } else if (isStartElement()) {
            ++depth;
        }
    }
}

AsciiStringView XmlStreamReader::name() const {
    return (xml_->node && xml_->node.type() == pugi::node_element) ? AsciiStringView(xml_->node.name()) : AsciiStringView();
}

bool XmlStreamReader::hasAttribute(const char *name) const {
    if (token_ != TokenType::StartElement) {
        return false;
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return false;
    }

    return xml_->node.attribute(name);
}

String XmlStreamReader::attribute(const char *name) const {
    if (token_ != TokenType::StartElement) {
        return String();
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return String();
    }

    pugi::xml_attribute attr = xml_->node.attribute(name);
    if (!attr) {
        return String();
    }

    return String::fromUtf8(attr.value());
}

String XmlStreamReader::attribute(const char *name, const String &def) const { return hasAttribute(name) ? attribute(name) : def; }

AsciiStringView XmlStreamReader::asciiAttribute(const char *name) const {
    if (token_ != TokenType::StartElement) {
        return AsciiStringView();
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return AsciiStringView();
    }

    pugi::xml_attribute attr = xml_->node.attribute(name);
    return attr ? AsciiStringView(attr.value()) : AsciiStringView();
}

AsciiStringView XmlStreamReader::asciiAttribute(const char *name, const AsciiStringView &def) const {
    return hasAttribute(name) ? asciiAttribute(name) : def;
}

int XmlStreamReader::intAttribute(const char *name) const { return asciiAttribute(name).toInt(); }

int XmlStreamReader::intAttribute(const char *name, int def) const { return hasAttribute(name) ? intAttribute(name) : def; }

double XmlStreamReader::doubleAttribute(const char *name) const { return asciiAttribute(name).toDouble(); }

double XmlStreamReader::doubleAttribute(const char *name, double def) const { return hasAttribute(name) ? doubleAttribute(name) : def; }

std::vector<XmlStreamReader::Attribute> XmlStreamReader::attributes() const {
    std::vector<Attribute> attrs;
    if (token_ != TokenType::StartElement) {
        return attrs;
    }

    if (!xml_->node || xml_->node.type() != pugi::node_element) {
        return attrs;
    }

    for (pugi::xml_attribute xa = xml_->node.first_attribute(); xa; xa = xa.next_attribute()) {
        Attribute a;
        a.name = xa.name();
        a.value = String::fromUtf8(xa.value());
        attrs.push_back(std::move(a));
    }

    return attrs;
}

bool XmlStreamReader::noChildren() const { return xml_->node ? !xml_->node.first_child() && !xml_->node.last_child() : false; }

String XmlStreamReader::readBody() const {
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

    return String::fromStdString(oss.str());
}

String XmlStreamReader::text() const {
    if (xml_->node) {
        pugi::xml_node_type t = xml_->node.type();
        if (t == pugi::node_pcdata || t == pugi::node_cdata || t == pugi::node_comment) {
            return nodeValue(xml_);
        }
    }
    return String();
}

AsciiStringView XmlStreamReader::asciiText() const {
    if (xml_->node) {
        pugi::xml_node_type t = xml_->node.type();
        if (t == pugi::node_pcdata || t == pugi::node_cdata || t == pugi::node_comment) {
            return xml_->node.value();
        }
    }
    return AsciiStringView();
}

String XmlStreamReader::readText() {
    if (isStartElement()) {
        String result;
        while (1) {
            switch (readNext()) {
                case Characters: result = nodeValue(xml_); break;
                case EndElement: return result;
                case Comment: break;
                case StartElement: break;
                default: break;
            }
        }
    }
    return String();
}

AsciiStringView XmlStreamReader::readAsciiText() {
    if (isStartElement()) {
        AsciiStringView result;
        while (1) {
            switch (readNext()) {
                case Characters: result = AsciiStringView(xml_->node.value()); break;
                case EndElement: return result;
                case Comment: break;
                case StartElement: break;
                default: break;
            }
        }
    }
    return AsciiStringView();
}

int XmlStreamReader::readInt(bool *ok, int base) {
    AsciiStringView s = readAsciiText();
    return s.toInt(ok, base);
}

double XmlStreamReader::readDouble(bool *ok) {
    AsciiStringView s = readAsciiText();
    return s.toDouble(ok);
}

int64_t XmlStreamReader::byteOffset() const {
    if (!xml_->node) {
        return 0;
    }

    // Use byte offset from start of document as a temporary proxy for line number
    return static_cast<int64_t>(xml_->node.offset_debug());
}

XmlStreamReader::Error XmlStreamReader::error() const {
    if (!xml_->customErr.isEmpty()) {
        return CustomError;
    }

    if (xml_->result.status == pugi::status_ok) {
        return NoError;
    }

    return NotWellFormedError;
}

bool XmlStreamReader::isError() const { return error() != NoError; }

String XmlStreamReader::errorString() const {
    if (!xml_->customErr.empty()) {
        return xml_->customErr;
    }
    return String::fromUtf8(xml_->result.description());
}

void XmlStreamReader::raiseError(const String &message) { xml_->customErr = message; }
