// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <functional>
#include <utility>

#include "global/flatmap.h"

namespace mu::engraving {
// Immutable contiguous lookup. Stable sorting preserves the first initializer for duplicate keys.
template <class K, class V, std::size_t N>
struct SortedTable {
    std::array<muse::FlatMapEntry<K, V>, N> entries;

    explicit SortedTable(muse::FlatMapEntry<K, V> (&&values)[N]) : entries(std::to_array(std::move(values))) {
        std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return std::less<K>{}(a.first, b.first); });
    }

    auto cend() const { return entries.cend(); }

    auto end() const { return entries.end(); }

    auto find(const K &key) const {
        auto it = std::lower_bound(entries.begin(), entries.end(), key,
                                   [](const auto &entry, const K &k) { return std::less<K>{}(entry.first, k); });
        return it != entries.end() && !std::less<K>{}(key, it->first) ? it : entries.end();
    }
};

template <class K, class V, std::size_t N>
auto make_sorted_table(muse::FlatMapEntry<K, V> (&&values)[N]) {
    return SortedTable<K, V, N>(std::move(values));
}

template <class T, std::size_t N>
constexpr auto sorted_values(T (&&values)[N]) {
    auto result = std::to_array(std::move(values));
    std::sort(result.begin(), result.end());
    return result;
}
} // namespace mu::engraving
