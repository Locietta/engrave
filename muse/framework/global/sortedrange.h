// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <ranges>
#include <type_traits>
#include <utility>

//! NOTE: element access for key-sorted ranges of (key, value) entries. An entry is a std::pair
//! (associative containers) or any two-member aggregate (flat records); both destructure

namespace muse {
template <typename Entry>
constexpr const auto &entry_key(const Entry &entry) noexcept {
    [[maybe_unused]] const auto &[key, value] = entry;
    return key;
}

template <typename Entry>
constexpr const auto &entry_value(const Entry &entry) noexcept {
    [[maybe_unused]] const auto &[key, value] = entry;
    return value;
}

template <typename Range>
using range_entry_t = std::remove_cvref_t<std::ranges::range_reference_t<const Range>>;

//! NOTE: the key type a lookup takes: key_type where the container declares one, else the entry's key
template <typename Range>
struct SortedKey {
    using type = std::remove_cvref_t<decltype(entry_key(std::declval<const range_entry_t<Range> &>()))>;
};

template <typename Range>
    requires requires { typename Range::key_type; }
struct SortedKey<Range> {
    using type = typename Range::key_type;
};

template <typename Range>
using sorted_key_t = typename SortedKey<Range>::type;

template <typename Range>
using sorted_value_t = std::remove_cvref_t<decltype(entry_value(std::declval<const range_entry_t<Range> &>()))>;
} // namespace muse
