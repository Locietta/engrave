// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>
#include <numeric>
#include <span>
#include <string_view>

#include "mpe/playbacksetupdata.h"

namespace mu::engraving {
struct SetupSubCategories {
    std::array<muse::mpe::SoundSubCategory, 7> values{};
    unsigned count = 0;

    constexpr SetupSubCategories() = default;

    consteval SetupSubCategories(std::initializer_list<muse::mpe::SoundSubCategory> categories) {
        const auto end = std::copy(categories.begin(), categories.end(), values.begin());
        // Match SoundSubCategories' set ordering and uniqueness.
        std::sort(values.begin(), end);
        count = static_cast<unsigned>(std::unique(values.begin(), end) - values.begin());
    }
};

struct SetupDataEntry {
    std::string_view id;

    struct {
        muse::mpe::SoundId sound_id;
        muse::mpe::SoundCategory category;
        SetupSubCategories sub_categories{};
    } data;
};

template <std::size_t N>
consteval auto sorted_setup_data(const SetupDataEntry (&entries)[N]) {
    std::array<std::size_t, N> order{};
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(), [&entries](auto a, auto b) {
        // Lookup must prefer the first source entry when keys repeat.
        const int cmp = entries[a].id.compare(entries[b].id);
        return cmp == 0 ? a < b : cmp < 0;
    });

    std::array<SetupDataEntry, N> sorted{};
    std::transform(order.begin(), order.end(), sorted.begin(), [&entries](auto i) { return entries[i]; });
    return sorted;
}

muse::mpe::PlaybackSetupData resolve_setup_data(std::span<const SetupDataEntry> entries, std::string_view id);
} // namespace mu::engraving
