// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

#include <ankerl/unordered_dense.h>

namespace muse {

template <class Key>
using Hash = ankerl::unordered_dense::hash<Key>;

// Transparent hash for std::string / std::string_view / non-null C-string lookups.
struct TransparentStringHash {
    using is_transparent = void;
    using is_avalanching = void;

    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept { return Hash<std::string_view>{}(sv); }
};

template <class Key>
using DefaultHash = std::conditional_t<std::same_as<Key, std::string>, TransparentStringHash, Hash<Key>>;

template <class Key>
using DefaultKeyEqual = std::conditional_t<std::same_as<Key, std::string>, std::equal_to<>, std::equal_to<Key>>;

template <class Key, class Value, class Hash = DefaultHash<Key>, class KeyEqual = DefaultKeyEqual<Key>>
using HashMap = ankerl::unordered_dense::map<Key, Value, Hash, KeyEqual>;

template <class Key, class Hash = DefaultHash<Key>, class KeyEqual = DefaultKeyEqual<Key>>
using HashSet = ankerl::unordered_dense::set<Key, Hash, KeyEqual>;

template <class Key, class Value, class Hash = DefaultHash<Key>, class KeyEqual = DefaultKeyEqual<Key>>
using SegmentedHashMap = ankerl::unordered_dense::segmented_map<Key, Value, Hash, KeyEqual>;

template <class Key, class Hash = DefaultHash<Key>, class KeyEqual = DefaultKeyEqual<Key>>
using SegmentedHashSet = ankerl::unordered_dense::segmented_set<Key, Hash, KeyEqual>;

} // namespace muse
