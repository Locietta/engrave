// SPDX-License-Identifier: GPL-3.0-only
#include "setupdatatable.h"

namespace mu::engraving {
muse::mpe::PlaybackSetupData resolve_setup_data(std::span<const SetupDataEntry> entries, std::string_view id) {
    const auto it = std::ranges::lower_bound(entries, id, {}, &SetupDataEntry::id);
    if (it == entries.end() || it->id != id) {
        return {};
    }

    const auto &data = it->data;
    const auto sub_categories = std::span(data.sub_categories.values).first(data.sub_categories.count);
    muse::mpe::PlaybackSetupData result{data.sound_id, data.category};
    result.sub_categories.reserve(sub_categories.size());
    for (auto category : sub_categories) {
        result.sub_categories.push_back(muse::mpe::sound_sub_category_to_string(category));
    }
    return result;
}
} // namespace mu::engraving
