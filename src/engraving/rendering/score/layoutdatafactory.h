/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

namespace mu::engraving {
enum class ElementType : unsigned char;
struct ItemLayoutData;

namespace rendering::score {
const ItemLayoutData &empty_layout_data(ElementType type);
} // namespace rendering::score
} // namespace mu::engraving
