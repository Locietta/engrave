// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <utility>

#include <gtl/btree.hpp>

namespace muse {

// Thin wrapper over gtl::btree_map adding aliasing-safe insert_or_assign and
// centralizing the ordered B-tree backend and allocator parameters.
template <class Key, class Value, class Compare = gtl::Less<Key>>
struct BTreeMap : gtl::btree_map<Key, Value, Compare> {
    using Base = gtl::btree_map<Key, Value, Compare>;
    using typename Base::const_iterator;
    using typename Base::iterator;
    using typename Base::key_type;
    using typename Base::mapped_type;

    using Base::Base;
    using Base::operator=;

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const key_type &key, M &&obj) {
        if (auto it = this->find(key); it != this->end()) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        // gtl::btree_map::emplace materializes value_type before splitting nodes.
        return this->emplace(key, std::forward<M>(obj));
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(key_type &&key, M &&obj) {
        if (auto it = this->find(key); it != this->end()) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        return this->emplace(std::move(key), std::forward<M>(obj));
    }

    friend void swap(BTreeMap &lhs, BTreeMap &rhs) noexcept(noexcept(lhs.swap(rhs))) { lhs.swap(rhs); }
};

template <class Key, class Compare = gtl::Less<Key>>
using BTreeSet = gtl::btree_set<Key, Compare>;

template <class Key, class Value, class Compare = gtl::Less<Key>>
using BTreeMultiMap = gtl::btree_multimap<Key, Value, Compare>;

template <class Key, class Compare = gtl::Less<Key>>
using BTreeMultiSet = gtl::btree_multiset<Key, Compare>;

} // namespace muse
