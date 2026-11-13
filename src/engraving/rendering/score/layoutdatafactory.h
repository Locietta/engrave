/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <memory>

#include "dom/engravingitem.h"

namespace mu::engraving::rendering::score {
// Closed payload dispatch, separate from the lookup implementation so layout
// translation units never include the dense hash implementation.
std::unique_ptr<EngravingItem::LayoutData> make_layout_data(ElementType type);
const EngravingItem::LayoutData &empty_layout_data(ElementType type);
} // namespace mu::engraving::rendering::score
