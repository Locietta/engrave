/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include "stafftextbase.h"

namespace mu::engraving {
class Capo final : public StaffTextBase {
    OBJECT_ALLOCATOR(engraving, Capo)
    DECLARE_CLASSOF(ElementType::CAPO)

public:
    Capo(DummyParentOr<Segment> parent, TextStyleType text_style_type = TextStyleType::STAFF);

    Capo *clone() const override;

    PropertyValue get_property(Pid id) const override;
    PropertyValue property_default(Pid id) const override;
    bool set_property(Pid id, const PropertyValue &val) override;
    void set_xml_text(const String &text) override;

    const CapoParams &params() const;
    void set_params(const CapoParams &params);

    bool should_automatically_generate_text() const;
    String generate_text(size_t string_count) const;

    void added() override;
    void removed() override;

private:
    CapoParams params_;
    bool should_automatically_generate_text_ = true;
    String custom_text_;
};
} // namespace mu::engraving
