/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "dom/textbase.h"
#include "sourcechange.h"

namespace mu::engraving {
struct TextFormatInverse {
    JournalTarget target;
    Pid id;
    String markup;
    CharFormat format;
    PropertyFlags flags;
    size_t row;
    size_t column;
    size_t select_row;
    size_t select_column;
};

TextFormatInverse capture_text_format(TextBase &text, Pid id);
void apply_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags);
void restore_text_format(const TextFormatInverse &inverse);
} // namespace mu::engraving
