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
#include "xmldom.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

#include "pugixml.hpp"

#include "log.h"

using namespace muse;

struct muse::XmlDomImplData {
    pugi::xml_document doc;
    pugi::xml_parse_result result{};
    bool triedload = false;
};

using xml_node_impl = pugi::xml_node;
using xml_attr_impl = pugi::xml_attribute;

// ================================================
// generic pack/unpack
// ================================================

namespace {
template <class T>
inline xml_handle pack_handle(const T &t) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "Backend handle must be trivially copyable");
    static_assert(sizeof(T) <= sizeof(xml_handle), "Increase xml_handle slots/size");
    xml_handle h{}; // zero both slots
    const auto *src = reinterpret_cast<const unsigned char *>(&t);
    auto *dst = reinterpret_cast<unsigned char *>(&h);
    std::copy_n(src, sizeof(T), dst);
    return h;
}

template <class T>
inline T unpack_handle(xml_handle h) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "Backend handle must be trivially copyable");
    static_assert(sizeof(T) <= sizeof(xml_handle), "Increase xml_handle slots/size");
    T t{}; // zero-init destination
    const auto *src = reinterpret_cast<const unsigned char *>(&h);
    auto *dst = reinterpret_cast<unsigned char *>(&t);
    std::copy_n(src, sizeof(T), dst);
    return t;
}
} // anonymous namespace

// ================================================
// XmlDomNode
// ================================================

XmlDomNode::XmlDomNode(const std::shared_ptr<XmlDomImplData> &xml, xml_node_handle node) : xml_(xml), node_(node) {}

bool XmlDomNode::isNull() const { return !static_cast<bool>(node_); }

String XmlDomNode::nodeName() const {
    if (!node_) {
        return String();
    }

    xml_node_impl n = unpack_handle<xml_node_impl>(node_);
    switch (n.type()) {
        case pugi::node_element:
        case pugi::node_pi:
        case pugi::node_declaration:
        case pugi::node_doctype:
        case pugi::node_document: // usually empty
            return String::fromUtf8(n.name());
        case pugi::node_pcdata:
        case pugi::node_cdata: return String::fromUtf8(n.value());
        case pugi::node_comment: return String::fromUtf8(n.value());
        case pugi::node_null:
        default: return String();
    }
}

bool XmlDomNode::hasChildNodes() const { return node_ ? static_cast<bool>(unpack_handle<xml_node_impl>(node_).first_child()) : false; }

XmlDomNode XmlDomNode::firstChild() const {
    if (!node_) {
        return XmlDomNode(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_).first_child();
    return XmlDomNode(xml_, pack_handle(n));
}

XmlDomElement XmlDomNode::firstChildElement(const char *name) const {
    if (!node_) {
        return XmlDomElement(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_);
    xml_node_impl c = name ? n.child(name) : n.find_child([](xml_node_impl x) { return x.type() == pugi::node_element; });
    if (c && c.type() == pugi::node_element) {
        return XmlDomElement(xml_, pack_handle(c));
    }
    return XmlDomElement(xml_, xml_node_handle());
}

XmlDomNode XmlDomNode::nextSibling() const {
    if (!node_) {
        return XmlDomNode(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_).next_sibling();
    return XmlDomNode(xml_, pack_handle(n));
}

XmlDomNode XmlDomNode::previousSibling() const {
    if (!node_) {
        return XmlDomNode(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_).previous_sibling();
    return XmlDomNode(xml_, pack_handle(n));
}

XmlDomNode XmlDomNode::parent() const {
    if (!node_) {
        return XmlDomNode(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_).parent();
    return XmlDomNode(xml_, pack_handle(n));
}

XmlDomElement XmlDomNode::nextSiblingElement(const char *name) const {
    if (!node_) {
        return XmlDomElement(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_);
    xml_node_impl s = name ? n.next_sibling(name) : n.next_sibling();
    if (!name) {
        while (s && s.type() != pugi::node_element) {
            s = s.next_sibling();
        }
    }
    if (s && s.type() == pugi::node_element) {
        return XmlDomElement(xml_, pack_handle(s));
    }
    return XmlDomElement(xml_, xml_node_handle());
}

XmlDomElement XmlDomNode::previousSiblingElement(const char *name) const {
    if (!node_) {
        return XmlDomElement(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_);
    xml_node_impl s = name ? n.previous_sibling(name) : n.previous_sibling();
    if (!name) {
        while (s && s.type() != pugi::node_element) {
            s = s.previous_sibling();
        }
    }
    if (s && s.type() == pugi::node_element) {
        return XmlDomElement(xml_, pack_handle(s));
    }
    return XmlDomElement(xml_, xml_node_handle());
}

XmlDomElement XmlDomNode::toElement() const {
    if (!node_) {
        return XmlDomElement(xml_, xml_node_handle());
    }
    xml_node_impl n = unpack_handle<xml_node_impl>(node_);
    if (n.type() == pugi::node_element) {
        return XmlDomElement(xml_, pack_handle(n));
    }
    return XmlDomElement(xml_, xml_node_handle());
}

// ================================================
// XmlDomAttribute
// ================================================

XmlDomAttribute::XmlDomAttribute(const std::shared_ptr<XmlDomImplData> &data, xml_attr_handle attribute)
    : xml_(data), attribute_(attribute) {}

bool XmlDomAttribute::isNull() const { return !static_cast<bool>(attribute_); }

String XmlDomAttribute::attributeName() const {
    if (!attribute_) {
        return String();
    }
    xml_attr_impl a = unpack_handle<xml_attr_impl>(attribute_);
    return String::fromUtf8(a.name());
}

String XmlDomAttribute::value() const {
    if (!attribute_) {
        return String();
    }
    xml_attr_impl a = unpack_handle<xml_attr_impl>(attribute_);
    return String::fromUtf8(a.value());
}

XmlDomAttribute XmlDomAttribute::nextAttribute() const {
    if (!attribute_) {
        return XmlDomAttribute(xml_, xml_attr_handle());
    }
    xml_attr_impl a = unpack_handle<xml_attr_impl>(attribute_).next_attribute();
    return XmlDomAttribute(xml_, pack_handle(a));
}

// ================================================
// XmlDomElement
// ================================================

XmlDomElement::XmlDomElement(const std::shared_ptr<XmlDomImplData> &data, xml_node_handle node) : XmlDomNode(data, node) {}

String XmlDomElement::text() const {
    xml_node_impl e = unpack_handle<xml_node_impl>(node_);
    if (!e || e.type() != pugi::node_element) {
        return String();
    }
    String result;
    for (xml_node_impl c = e.first_child(); c; c = c.next_sibling()) {
        const pugi::xml_node_type t = c.type();
        if (t == pugi::node_pcdata || t == pugi::node_cdata) {
            result += String::fromUtf8(c.value());
        }
    }
    return result;
}

XmlDomAttribute XmlDomElement::firstAttribute() const {
    if (!node_) {
        return XmlDomAttribute(xml_, xml_attr_handle());
    }
    xml_node_impl e = unpack_handle<xml_node_impl>(node_);
    if (e && e.type() == pugi::node_element) {
        return XmlDomAttribute(xml_, pack_handle(e.first_attribute()));
    }
    return XmlDomAttribute(xml_, xml_attr_handle());
}

XmlDomAttribute XmlDomElement::attribute(const char *name) const {
    if (!node_) {
        return XmlDomAttribute(xml_, xml_attr_handle());
    }
    xml_node_impl e = unpack_handle<pugi::xml_node>(node_);
    if (e && e.type() == pugi::node_element) {
        return XmlDomAttribute(xml_, pack_handle(e.attribute(name)));
    }
    return XmlDomAttribute(xml_, xml_attr_handle());
}

// ================================================
// XmlDomDocument
// ================================================

XmlDomDocument::XmlDomDocument() { xml_ = std::make_shared<XmlDomImplData>(); }

void XmlDomDocument::setContent(const ByteArray &data) {
    TRACEFUNC;

    xml_->doc.reset();
    xml_->result = xml_->doc.load_buffer(data.constData(), data.size());
    xml_->triedload = true;

    if (xml_->result.status != pugi::status_ok) {
        LOGE() << errorString();
    }
}

XmlDomElement XmlDomDocument::rootElement() const {
    xml_node_impl e = xml_->doc.document_element();
    return XmlDomElement(xml_, pack_handle(e));
}

bool XmlDomDocument::hasError() const { return xml_->triedload && xml_->result.status != pugi::status_ok; }

String XmlDomDocument::errorString() const {
    if (xml_->triedload) {
        return String::fromUtf8(xml_->result.description());
    }
    return String();
}
