/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "dom/bracketitem.h"
#include "sourcechange.h"

namespace mu::engraving {
struct BracketPropertyInverse {
    JournalTarget staff;
    size_t level;
    size_t applied_level;
    Pid id;
    PropertyValue before;
    PropertyFlags flags;
    amc::vector<size_t> columns;
};

struct StyledValueBefore {
    Pid id;
    PropertyValue value;
    PropertyFlags flags;
};

struct TextLinePropertyInverse {
    PropertyInverse property;
    amc::vector<StyledValueBefore> styled;
};

BracketPropertyInverse capture_bracket_property(BracketItem &item, Pid id);
void apply_bracket_property(BracketPropertyInverse &inverse, const PropertyValue &value, PropertyFlags flags);
void restore_bracket_property(BracketPropertyInverse &inverse);
TextLinePropertyInverse capture_text_line_property(EngravingObject &object);
void apply_text_line_property(EngravingObject &object, const PropertyValue &value);
void restore_text_line_property(const TextLinePropertyInverse &inverse);
} // namespace mu::engraving
