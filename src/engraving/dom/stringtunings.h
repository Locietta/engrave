/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#ifndef MU_ENGRAVING_STRING_TUNINGS_H
#define MU_ENGRAVING_STRING_TUNINGS_H

#include "stafftextbase.h"
#include "stringdata.h"

namespace mu::engraving {
struct StringTunings final : StaffTextBase {
private:
    DECLARE_CLASSOF(ElementType::STRING_TUNINGS)

public:
    explicit StringTunings(DummyParentOr<Segment> parent, TextStyleType text_style_type = TextStyleType::STRING_TUNINGS);
    StringTunings(const StringTunings &s);

    StringTunings *clone() const override;

    PropertyValue get_property(Pid id) const override;
    PropertyValue property_default(Pid id) const override;
    bool set_property(Pid id, const PropertyValue &val) override;

    const StringData *string_data() const;
    void set_string_data(const StringData &string_data);
    void undo_string_data(const StringData &string_data);

    const String &preset() const;
    void set_preset(const String &preset);

    const std::vector<string_idx_t> &visible_strings() const;
    void set_visible_strings(const std::vector<string_idx_t> &visible_strings);

    void update_text();

    bool no_string_visible() const;

    void trigger_layout() const override;

private:
    String generate_text() const;

    String preset_;
    std::vector<string_idx_t> visible_strings_;
    StringData string_data_;

    bool no_string_visible_ = false;
    std::optional<int> strings_number_;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_STRING_TUNINGS_H
