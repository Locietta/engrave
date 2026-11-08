/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sortedrange.h"

//! NOTE useful functions for containers

namespace muse {
static constexpr size_t nidx = static_cast<size_t>(-1);

// vector
template <typename T>
inline bool contains(const std::vector<T> &vec, const T &v) {
    return std::find(vec.cbegin(), vec.cend(), v) != vec.cend();
}

template <typename T, typename Predicate>
inline bool contains_if(const std::vector<T> &vec, Predicate &&pred) {
    return std::find_if(vec.cbegin(), vec.cend(), std::forward<Predicate>(pred)) != vec.cend();
}

template <typename T>
inline T value(const std::vector<T> &vec, size_t idx) {
    if (idx < vec.size()) {
        return vec.at(idx);
    }

    if constexpr (std::is_pointer<T>::value) {
        return nullptr;
    } else {
        return T();
    }
}

template <typename T>
inline bool remove(std::vector<T> &vec, const T &v) {
    size_t orig_size = vec.size();
    vec.erase(std::remove(vec.begin(), vec.end(), v), vec.end());
    return orig_size != vec.size();
}

template <typename T, typename Predicate>
inline bool remove_if(std::vector<T> &vec, Predicate p) {
    size_t orig_size = vec.size();
    vec.erase(std::remove_if(vec.begin(), vec.end(), p), vec.end());
    return orig_size != vec.size();
}

template <typename T>
inline void remove_first(std::vector<T> &vec) {
    vec.erase(vec.begin());
}

template <typename T>
inline T take_at(std::vector<T> &vec, size_t idx) {
    T v = value(vec, idx);
    vec.erase(vec.begin() + idx);
    return v;
}

template <typename T>
inline T take_first(std::vector<T> &vec) {
    return take_at(vec, 0);
}

template <typename T>
inline T take_last(std::vector<T> &vec) {
    return take_at(vec, vec.size() - 1);
}

template <typename T>
inline void swap_items_at(std::vector<T> &vec, size_t idx1, size_t idx2) {
    T v1 = vec[idx1];
    vec[idx1] = vec[idx2];
    vec[idx2] = v1;
}

template <typename T>
inline bool move_item(std::vector<T> &vec, size_t old_idx, size_t new_idx) {
    if (old_idx == muse::nidx || old_idx == new_idx) {
        return false;
    }

    new_idx = std::min(vec.size() - 1, new_idx);

    if (old_idx > new_idx) {
        std::rotate(vec.rend() - old_idx - 1, vec.rend() - old_idx, vec.rend() - new_idx);
    } else {
        std::rotate(vec.begin() + old_idx, vec.begin() + old_idx + 1, vec.begin() + new_idx + 1);
    }

    return true;
}

template <typename T>
std::vector<T> mid(const std::vector<T> &c, size_t pos, int alength = -1) {
    if (c.empty()) {
        return std::vector<T>();
    }

    size_t end = 0;
    if (alength < 0) {
        end = c.size();
    } else {
        end = pos + static_cast<size_t>(alength);
    }

    if (end > (c.size())) {
        end = c.size();
    }

    if (end == 0) {
        return std::vector<T>();
    }

    if (pos >= end) {
        return std::vector<T>();
    }

    std::vector<T> sub(c.begin() + pos, c.begin() + end);
    return sub;
}

template <typename T>
inline void join(std::vector<T> &l1, const std::vector<T> &l2) {
    l1.insert(l1.end(), l2.begin(), l2.end());
}

// list
template <typename T>
inline bool contains(const std::list<T> &l, const T &v) {
    return std::find(l.cbegin(), l.cend(), v) != l.cend();
}

template <typename T>
inline void join(std::list<T> &l1, const std::list<T> &l2) {
    l1.insert(l1.end(), l2.begin(), l2.end());
}

template <typename T>
inline bool remove(std::list<T> &l, const T &v) {
    if (!contains(l, v)) {
        return false;
    }
    l.remove(v);
    return true;
}

template <typename T>
inline T take_first(std::list<T> &l) {
    T v = l.front();
    l.pop_front();
    return v;
}

template <typename T>
inline T take_last(std::list<T> &l) {
    T v = l.back();
    l.pop_back();
    return v;
}

template <typename T>
inline std::pair<bool, T> take(std::list<T> &l, const T &v) {
    auto it = std::find(l.begin(), l.end(), v);
    if (it == l.end()) {
        return std::make_pair(false, T());
    }
    std::pair<bool, T> ret = std::make_pair(true, *it);
    l.erase(it);
    return ret;
}

// ===========================
// Set
// ===========================
template <typename T>
inline bool contains(const std::set<T> &s, const T &v) {
    return s.find(v) != s.cend();
}

template <typename T>
inline bool contains(const std::unordered_set<T> &s, const T &v) {
    return s.find(v) != s.cend();
}

// ===========================
// Array
// ===========================
template <typename T, size_t size>
inline bool contains(const std::array<T, size> &s, const T &v) {
    return std::find(s.begin(), s.end(), v) != s.cend();
}

// ===========================
// General
// ===========================

template <typename Container, typename T>
inline size_t index_of(const Container &c, const T &v) {
    auto it = std::find(c.cbegin(), c.cend(), v);
    if (it != c.cend()) {
        return std::distance(c.cbegin(), it);
    }
    return muse::nidx;
}

template <typename K, typename V>
inline bool contains(const std::map<K, V> &m, const K &k) {
    return m.find(k) != m.cend();
}

template <typename K, typename V>
inline bool contains(const std::multimap<K, V> &m, const K &k) {
    return m.find(k) != m.cend();
}

template <typename K, typename V>
inline bool contains(const std::unordered_map<K, V> &m, const K &k) {
    return m.find(k) != m.cend();
}

template <typename Map>
inline auto values(const Map &m) -> std::vector<typename Map::mapped_type> {
    std::vector<typename Map::mapped_type> result;
    result.reserve(m.size());
    for (const auto &p : m) {
        result.push_back(p.second);
    }
    return result;
}

template <typename Map, typename V, typename K>
inline K key(const Map &m, const V &v, const K &def) {
    for (const auto &p : m) {
        if (p.second == v) {
            return p.first;
        }
    }
    return def;
}

template <typename Map, typename V>
inline auto key(const Map &m, const V &v) -> typename Map::key_type {
    for (const auto &p : m) {
        if (p.second == v) {
            return p.first;
        }
    }
    typename Map::key_type def{};
    return def;
}

template <typename Map>
inline auto value(const Map &m, const typename Map::key_type &k) -> typename Map::mapped_type {
    auto it = m.find(k);
    if (it != m.end()) {
        return it->second;
    }
    typename Map::mapped_type def{};
    return def;
}

template <typename Map>
inline auto value(const Map &m, const typename Map::key_type &k, const typename Map::mapped_type &def) -> typename Map::mapped_type {
    auto it = m.find(k);
    if (it != m.end()) {
        return it->second;
    }
    return def;
}

template <typename Map, typename T>
inline bool remove(Map &c, const T &k) {
    auto it = c.find(k);
    if (it != c.end()) {
        c.erase(it);
        return true;
    }
    return false;
}

template <typename Map, typename Predicate>
inline size_t remove_if(Map &c, Predicate pred) {
    auto old_size = c.size();
    for (auto first = c.begin(), last = c.end(); first != last;) {
        if (pred(*first)) {
            first = c.erase(first);
        } else {
            ++first;
        }
    }
    return old_size - c.size();
}

template <typename Map, typename K>
inline auto take(Map &m, const K &k) -> typename Map::mapped_type {
    auto it = m.find(k);
    if (it != m.end()) {
        auto v = it->second;
        m.erase(it);
        return v;
    }
    typename Map::mapped_type def{};
    return def;
}

template <typename K, typename V>
inline std::set<K> unique_keys(const std::multimap<K, V> &mm) {
    std::set<K> keys;
    for (auto it = mm.begin(); it != mm.end(); ++it) {
        keys.insert(it->first);
    }
    return keys;
}

template <typename K, typename V>
inline auto values(const std::multimap<K, V> &mm, const K &key) -> std::vector<typename std::multimap<K, V>::mapped_type> {
    std::vector<typename std::multimap<K, V>::mapped_type> result;
    const auto range = mm.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    return result;
}

//! NOTE: an associative container searches with its own upper_bound; any other key-sorted
//! range of (key, value) entries is binary-searched on the entry key
template <typename Container, typename Key>
inline auto sorted_upper_bound(Container &c, const Key &k) {
    if constexpr (requires { c.upper_bound(k); }) {
        return c.upper_bound(k);
    } else {
        return std::ranges::upper_bound(c, k, std::ranges::less{}, [](const auto &entry) -> const auto & { return entry_key(entry); });
    }
}

template <typename Container>
inline std::ranges::iterator_t<const Container> find_less_or_equal(const Container &c, const sorted_key_t<Container> &k) {
    if (c.empty()) {
        return c.end();
    }

    auto it = sorted_upper_bound(c, k);
    if (it == c.begin()) {
        return c.end();
    }

    return std::prev(it);
}

template <typename Container>
inline std::ranges::iterator_t<Container> find_less_or_equal(Container &c, const sorted_key_t<Container> &k) {
    if (c.empty()) {
        return c.end();
    }

    auto it = sorted_upper_bound(c, k);
    if (it == c.begin()) {
        return c.end();
    }

    return std::prev(it);
}

template <typename Container>
inline typename Container::const_iterator find_less(const Container &c, const typename Container::key_type &k) {
    if (c.empty()) {
        return c.cend();
    }

    auto it = c.lower_bound(k);
    if (it == c.cbegin()) {
        return c.cend();
    }

    return std::prev(it);
}

template <typename Container>
inline typename Container::iterator find_less(Container &c, const typename Container::key_type &k) {
    if (c.empty()) {
        return c.end();
    }

    auto it = c.lower_bound(k);
    if (it == c.begin()) {
        return c.end();
    }

    return std::prev(it);
}

template <typename ForwardIterator>
inline void delete_all(ForwardIterator begin, ForwardIterator end) {
    while (begin != end) {
        delete *begin;
        ++begin;
    }
}

template <typename Container>
inline void delete_all(const Container &c) {
    delete_all(c.begin(), c.end());
}
} // namespace muse

template <typename T>
inline std::set<T> &operator<<(std::set<T> &s, const T &v) {
    s.insert(v);
    return s;
}
