// SPDX-License-Identifier: GPL-3.0-only
#pragma once

//! NOTE: FlatMap (playback DoD design 4.3). Sorted contiguous (key, value) vector backed by
//! amc::vector when FlatMapEntry<K, V> is trivially copyable and std::vector otherwise. Compatible
//! with the sorted-range helpers in global/sortedrange.h via two-member aggregate destructuring

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <amc/vector.hpp>

#include "sortedrange.h"

namespace muse {
//! Thrown when a flat playback buffer or an offset in it would leave the uint32 format; the
//! export maps it to RESOURCE_LIMIT. Release builds keep the check
struct StoreLimit {
    const char *message = "playback buffer exceeds uint32 offset limit";
};

//! Trivially copyable payloads only: amc relocates by memcpy. Nothing on the playback path declares
//! trivially_relocatable
template <class T>
    requires std::is_trivially_copyable_v<T>
using Buf = amc::vector<T>;

template <class T = std::byte>
constexpr std::uint32_t checked_grow(std::uint64_t size, std::uint64_t count) {
    static_assert(std::is_trivially_copyable_v<T>);
    constexpr std::uint64_t limit = std::numeric_limits<std::uint32_t>::max();
    if (size > limit || count > limit - size) {
        throw StoreLimit{};
    }
    const std::uint64_t total = size + count;
    if (total > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
        throw StoreLimit{};
    }
    return static_cast<std::uint32_t>(total);
}

template <class T>
void reserve_checked(Buf<T> &buf, std::uint64_t count) {
    buf.reserve(checked_grow<T>(0, count));
}

template <class T, class... Args>
T &emplace_checked(Buf<T> &buf, Args &&...args) {
    std::ignore = checked_grow<T>(buf.size(), 1);
    return buf.emplace_back(std::forward<Args>(args)...);
}

//! Aggregate (key, value) entry that stays trivially copyable whenever K and V are, unlike
//! libstdc++'s std::pair which defines user-provided assignment operators
template <class K, class V>
struct FlatMapEntry {
    K first;
    V second;

    bool operator==(const FlatMapEntry &other) const = default;

    bool operator<(const FlatMapEntry &other) const {
        if (first < other.first) {
            return true;
        }
        if (other.first < first) {
            return false;
        }
        return second < other.second;
    }
};

template <class K, class V, class Compare = std::less<K>>
class FlatMap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = FlatMapEntry<K, V>;
    using key_compare = Compare;
    static constexpr bool is_amc_storage = std::is_trivially_copyable_v<value_type>;
    static_assert(is_amc_storage == (std::is_trivially_copyable_v<K> && std::is_trivially_copyable_v<V>) );
    using Storage = std::conditional_t<is_amc_storage, amc::vector<value_type>, std::vector<value_type>>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    FlatMap() = default;

    explicit FlatMap(Compare comp) : comp_(std::move(comp)) {}

    FlatMap(std::initializer_list<value_type> init, Compare comp = Compare{}) : comp_(std::move(comp)) {
        reserve(init.size());
        for (const value_type &entry : init) {
            insert(entry);
        }
    }

    template <class InputIt>
    FlatMap(InputIt first, InputIt last, Compare comp = Compare{}) : comp_(std::move(comp)) {
        insert(first, last);
    }

    iterator begin() noexcept { return data_.begin(); }

    const_iterator begin() const noexcept { return data_.begin(); }

    const_iterator cbegin() const noexcept { return data_.cbegin(); }

    iterator end() noexcept { return data_.end(); }

    const_iterator end() const noexcept { return data_.end(); }

    const_iterator cend() const noexcept { return data_.cend(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    bool empty() const noexcept { return data_.empty(); }

    size_type size() const noexcept { return static_cast<size_type>(data_.size()); }

    void clear() noexcept { data_.clear(); }

    key_compare key_comp() const { return comp_; }

    void reserve(size_type count) {
        if constexpr (is_amc_storage) {
            data_.reserve(checked_grow<value_type>(0, count));
        } else {
            data_.reserve(count);
        }
    }

    iterator lower_bound(const K &key) {
        return std::lower_bound(data_.begin(), data_.end(), key,
                                [this](const value_type &entry, const K &k) { return comp_(entry.first, k); });
    }

    const_iterator lower_bound(const K &key) const {
        return std::lower_bound(data_.begin(), data_.end(), key,
                                [this](const value_type &entry, const K &k) { return comp_(entry.first, k); });
    }

    iterator upper_bound(const K &key) {
        return std::upper_bound(data_.begin(), data_.end(), key,
                                [this](const K &k, const value_type &entry) { return comp_(k, entry.first); });
    }

    const_iterator upper_bound(const K &key) const {
        return std::upper_bound(data_.begin(), data_.end(), key,
                                [this](const K &k, const value_type &entry) { return comp_(k, entry.first); });
    }

    iterator find(const K &key) {
        const iterator it = lower_bound(key);
        return (it != data_.end() && !comp_(key, it->first)) ? it : data_.end();
    }

    const_iterator find(const K &key) const {
        const const_iterator it = lower_bound(key);
        return (it != data_.end() && !comp_(key, it->first)) ? it : data_.end();
    }

    bool contains(const K &key) const { return find(key) != data_.end(); }

    size_type count(const K &key) const { return contains(key) ? 1 : 0; }

    V &at(const K &key) {
        const iterator it = find(key);
        if (it == data_.end()) {
            throw std::out_of_range("FlatMap::at: key not found");
        }
        return it->second;
    }

    const V &at(const K &key) const {
        const const_iterator it = find(key);
        if (it == data_.end()) {
            throw std::out_of_range("FlatMap::at: key not found");
        }
        return it->second;
    }

    V &operator[](const K &key) { return try_emplace(key).first->second; }

    V &operator[](K &&key) { return try_emplace(std::move(key)).first->second; }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(const K &key, Args &&...args) {
        const iterator it = lower_bound(key);
        if (it != data_.end() && !comp_(key, it->first)) {
            return {it, false};
        }
        check_grow();
        return {data_.insert(it, value_type{key, V(std::forward<Args>(args)...)}), true};
    }

    template <class... Args>
    std::pair<iterator, bool> try_emplace(K &&key, Args &&...args) {
        const iterator it = lower_bound(key);
        if (it != data_.end() && !comp_(key, it->first)) {
            return {it, false};
        }
        check_grow();
        return {data_.insert(it, value_type{std::move(key), V(std::forward<Args>(args)...)}), true};
    }

    template <class KArg, class... Args>
    std::pair<iterator, bool> emplace(KArg &&key, Args &&...args) {
        return try_emplace(K(std::forward<KArg>(key)), std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert(const value_type &value) {
        const iterator it = lower_bound(value.first);
        if (it != data_.end() && !comp_(value.first, it->first)) {
            return {it, false};
        }
        check_grow();
        return {data_.insert(it, value), true};
    }

    std::pair<iterator, bool> insert(value_type &&value) {
        const iterator it = lower_bound(value.first);
        if (it != data_.end() && !comp_(value.first, it->first)) {
            return {it, false};
        }
        check_grow();
        return {data_.insert(it, std::move(value)), true};
    }

    iterator insert(const_iterator /*hint*/, const value_type &value) { return insert(value).first; }

    iterator insert(const_iterator /*hint*/, value_type &&value) { return insert(std::move(value)).first; }

    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            insert(value_type{first->first, first->second});
        }
    }

    void insert(std::initializer_list<value_type> init) { insert(init.begin(), init.end()); }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(const K &key, M &&obj) {
        const iterator it = lower_bound(key);
        if (it != data_.end() && !comp_(key, it->first)) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        check_grow();
        return {data_.insert(it, value_type{key, V(std::forward<M>(obj))}), true};
    }

    template <class M>
    std::pair<iterator, bool> insert_or_assign(K &&key, M &&obj) {
        const iterator it = lower_bound(key);
        if (it != data_.end() && !comp_(key, it->first)) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        check_grow();
        return {data_.insert(it, value_type{std::move(key), V(std::forward<M>(obj))}), true};
    }

    iterator erase(const_iterator pos) { return data_.erase(pos); }

    iterator erase(const_iterator first, const_iterator last) { return data_.erase(first, last); }

    size_type erase(const K &key) {
        const iterator it = find(key);
        if (it == data_.end()) {
            return 0;
        }
        data_.erase(it);
        return 1;
    }

    bool operator==(const FlatMap &other) const { return data_ == other.data_; }

    bool operator!=(const FlatMap &other) const { return !(*this == other); }

    bool operator<(const FlatMap &other) const { return data_ < other.data_; }

    bool operator>(const FlatMap &other) const { return other < *this; }

    bool operator<=(const FlatMap &other) const { return !(other < *this); }

    bool operator>=(const FlatMap &other) const { return !(*this < other); }

private:
    void check_grow(std::uint64_t count = 1) const {
        if constexpr (is_amc_storage) {
            std::ignore = checked_grow<value_type>(data_.size(), count);
        }
    }

    Storage data_;
    [[no_unique_address]] Compare comp_{};
};
} // namespace muse
