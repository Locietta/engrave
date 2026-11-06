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
#ifndef MUSE_GLOBAL_XMLDOM_H
#define MUSE_GLOBAL_XMLDOM_H

#include <memory>

#include "types/bytearray.h"
#include "types/string.h"

namespace muse {
struct XmlDomImplData;
// avoid external dependency on a particular xml library
// Opaque, backend-agnostic handle storage (two machine words)
using xml_handle_slot = std::uintptr_t;

struct XmlHandle {
    xml_handle_slot slot0 = 0;
    xml_handle_slot slot1 = 0;

    explicit operator bool() const noexcept { return !(slot0 == 0 && slot1 == 0); }
};

static_assert(std::is_trivially_copyable_v<XmlHandle>, "Xml handle struct must be trivially copyable");
using xml_node_handle = XmlHandle;
using xml_attr_handle = XmlHandle;

struct XmlDomElement;
struct XmlDomAttribute;

struct XmlDomNode {

    XmlDomNode() = default;

    bool is_null() const;
    String node_name() const;

    bool has_child_nodes() const;
    XmlDomNode first_child() const;
    XmlDomElement first_child_element(const char *name) const;
    XmlDomNode next_sibling() const;
    XmlDomNode previous_sibling() const;
    XmlDomNode parent() const;

    XmlDomElement next_sibling_element(const char *name = nullptr) const;
    XmlDomElement previous_sibling_element(const char *name = nullptr) const;
    XmlDomElement to_element() const;

protected:
    friend struct XmlDomDocument;
    friend struct XmlDomElement;

    XmlDomNode(const std::shared_ptr<XmlDomImplData> &xml, xml_node_handle node);

    std::shared_ptr<XmlDomImplData> xml_ = nullptr;
    xml_node_handle node_{};
};

struct XmlDomElement : XmlDomNode {

    String text() const;

    XmlDomAttribute first_attribute() const;
    XmlDomAttribute attribute(const char *name) const;

private:

    friend struct XmlDomDocument;
    friend struct XmlDomNode;

    XmlDomElement(const std::shared_ptr<XmlDomImplData> &data, xml_node_handle node);
};

struct XmlDomAttribute {

    bool is_null() const;
    String attribute_name() const;

    String value() const;

    XmlDomAttribute next_attribute() const;

private:
    friend struct XmlDomElement;

    explicit XmlDomAttribute(const std::shared_ptr<XmlDomImplData> &data, xml_attr_handle attribute);

    std::shared_ptr<XmlDomImplData> xml_ = nullptr;
    xml_attr_handle attribute_{};
};

struct XmlDomDocument {
    XmlDomDocument();

    void set_content(const ByteArray &data);

    XmlDomElement root_element() const;

    bool has_error() const;
    String error_string() const;

private:
    std::shared_ptr<XmlDomImplData> xml_ = nullptr;
};
} // namespace muse

#endif // MUSE_GLOBAL_XMLDOM_H
