/* SPDX-License-Identifier: GPL-3.0-only */
#include "layoutdatafactory.h"

#include "layoutdatadispatch.h"

namespace mu::engraving::rendering::score {
const EngravingItem::LayoutData &empty_layout_data(ElementType type) {
    return LayoutDataTypes::dispatch(type, []<typename Data>() -> const EngravingItem::LayoutData & {
        static const Data empty;
        return empty;
    });
}
} // namespace mu::engraving::rendering::score
