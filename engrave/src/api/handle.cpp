#include <engrave/handle.h>

#include "engraving/infrastructure/eid.h"

namespace engrave {

std::string to_string(const ElementId &id) { return mu::engraving::EID::from_parts(id.hi, id.lo).to_std_string(); }

std::optional<ElementId> element_id_from_string(std::string_view text) {
    const mu::engraving::EID eid = mu::engraving::EID::from_std_string(text);
    if (!eid.is_valid()) return std::nullopt;
    return ElementId{eid.first(), eid.second()};
}

} // namespace engrave
