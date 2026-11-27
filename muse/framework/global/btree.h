// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtl/btree.hpp>

namespace muse {

// Thin wrapper over gtl::btree_map adding aliasing-safe insert_or_assign and
// centralizing the ordered B-tree backend and allocator parameters.
// Defaults to gtl::Less<Key> to preserve transparent lookup and three-way comparisons.
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

// Indirected string-keyed B-tree map usable with incomplete value types
// (e.g., recursive glz::generic_json). Empty instances hold nullptr with zero
// heap allocation and keep sizeof == sizeof(void*).
template <class Value>
class IndirectStringBTreeMap {
public:
    using map_type = gtl::btree_map<std::string, Value, gtl::Less<std::string>>;
    using key_type = std::string;
    using mapped_type = Value;
    using value_type = std::pair<const std::string, Value>;
    using key_compare = std::less<>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    IndirectStringBTreeMap() noexcept = default;

    IndirectStringBTreeMap(const IndirectStringBTreeMap &other)
        : map_(other.map_ && !other.map_->empty() ? new map_type(*other.map_) : nullptr) {}

    IndirectStringBTreeMap(IndirectStringBTreeMap &&other) noexcept : map_(std::exchange(other.map_, nullptr)) {}

    IndirectStringBTreeMap &operator=(const IndirectStringBTreeMap &other) {
        if (this == &other) return *this;
        if (!other.map_ || other.map_->empty()) {
            if (map_) map_->clear();
            return *this;
        }
        if (map_) {
            *map_ = *other.map_;
        } else {
            map_ = new map_type(*other.map_);
        }
        return *this;
    }

    IndirectStringBTreeMap &operator=(IndirectStringBTreeMap &&other) noexcept {
        if (this != &other) {
            delete map_;
            map_ = std::exchange(other.map_, nullptr);
        }
        return *this;
    }

    ~IndirectStringBTreeMap() noexcept { delete map_; }

    [[nodiscard]] bool empty() const noexcept { return !map_ || map_->empty(); }

    [[nodiscard]] size_type size() const noexcept { return map_ ? map_->size() : 0; }

    void clear() noexcept {
        if (map_) map_->clear();
    }

    void swap(IndirectStringBTreeMap &other) noexcept { std::swap(map_, other.map_); }

    friend void swap(IndirectStringBTreeMap &lhs, IndirectStringBTreeMap &rhs) noexcept { lhs.swap(rhs); }

    auto begin() noexcept { return map_ ? map_->begin() : typename map_type::iterator{}; }

    auto end() noexcept { return map_ ? map_->end() : typename map_type::iterator{}; }

    auto begin() const noexcept { return map_ ? map_->cbegin() : typename map_type::const_iterator{}; }

    auto end() const noexcept { return map_ ? map_->cend() : typename map_type::const_iterator{}; }

    auto cbegin() const noexcept { return begin(); }

    auto cend() const noexcept { return end(); }

    auto find(std::string_view key) { return map_ ? map_->find(key) : typename map_type::iterator{}; }

    auto find(std::string_view key) const { return map_ ? map_->find(key) : typename map_type::const_iterator{}; }

    [[nodiscard]] bool contains(std::string_view key) const { return map_ && map_->find(key) != map_->end(); }

    [[nodiscard]] size_type count(std::string_view key) const { return contains(key) ? 1 : 0; }

    Value &at(std::string_view key) {
        if (map_) {
            if (auto it = map_->find(key); it != map_->end()) return it->second;
        }
        throw std::out_of_range("IndirectStringBTreeMap::at: key not found");
    }

    const Value &at(std::string_view key) const {
        if (map_) {
            if (auto it = map_->find(key); it != map_->end()) return it->second;
        }
        throw std::out_of_range("IndirectStringBTreeMap::at: key not found");
    }

    template <class K>
        requires std::convertible_to<const K &, std::string_view>
    Value &operator[](K &&key) {
        auto &m = ensure();
        if constexpr (std::same_as<K, std::string>) {
            return m[std::move(key)];
        } else {
            const std::string_view sv(key);
            if (auto it = m.find(sv); it != m.end()) return it->second;
            return m.emplace(std::string(sv), Value{}).first->second;
        }
    }

    template <class... Args>
    auto insert(Args &&...args) {
        return ensure().insert(std::forward<Args>(args)...);
    }

    template <class... Args>
    auto emplace(Args &&...args) {
        return ensure().emplace(std::forward<Args>(args)...);
    }

    template <class K, class... Args>
    auto try_emplace(K &&key, Args &&...args) {
        return ensure().try_emplace(std::forward<K>(key), std::forward<Args>(args)...);
    }

    size_type erase(std::string_view key) {
        if (!map_) return 0;
        auto it = map_->find(key);
        if (it == map_->end()) return 0;
        map_->erase(it);
        return 1;
    }

    template <class It>
        requires(!std::convertible_to<const It &, std::string_view>)
    auto erase(It pos) {
        return map_->erase(pos);
    }

    friend bool operator==(const IndirectStringBTreeMap &lhs, const IndirectStringBTreeMap &rhs) {
        if (lhs.size() != rhs.size()) return false;
        if (lhs.empty()) return true;
        return *lhs.map_ == *rhs.map_;
    }

private:
    map_type &ensure() {
        if (!map_) map_ = new map_type();
        return *map_;
    }

    map_type *map_ = nullptr;
};

} // namespace muse
