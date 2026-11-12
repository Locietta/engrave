/* SPDX-License-Identifier: GPL-3.0-only */
#include "textchange.h"

namespace mu::engraving {
TextFormatInverse capture_text_format(TextBase &text, Pid id) {
    const auto &cursor = *text.cursor();
    return {JournalTarget::capture(text), id,           text.xml_text(), *cursor.format(),
            text.property_flags(id),      cursor.row(), cursor.column(), cursor.select_line(),
            cursor.select_column()};
}

void apply_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags) {
    text.set_property_flags(id, flags);
    text.set_property(id, value);
}

void restore_text_format(const TextFormatInverse &inverse) {
    auto &text = *to_text_base(&inverse.target.object());
    auto &cursor = *text.cursor();
    cursor.set_format(inverse.format);
    text.set_xml_text(inverse.markup);
    text.set_property_flags(inverse.id, inverse.flags);
    cursor.set_row(inverse.row);
    cursor.set_column(inverse.column);
    cursor.set_select_line(inverse.select_row);
    cursor.set_select_column(inverse.select_column);
    text.trigger_layout();
}
} // namespace mu::engraving
