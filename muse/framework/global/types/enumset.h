// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <bitset>
#include <cstddef>
#include <initializer_list>
#include <iterator>

namespace muse {
// Enum values are indices starting at -1 (UNDEFINED); traversal is ascending enum order.
template <class E, std::size_t N>
struct EnumSet {
    struct const_iterator {
        using value_type = E;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        const EnumSet *owner = nullptr;
        std::size_t index = N + 1;

        E operator*() const { return static_cast<E>(static_cast<std::ptrdiff_t>(index) - 1); }

        const_iterator &operator++() {
            ++index;
            skip();
            return *this;
        }

        const_iterator operator++(int) {
            auto old = *this;
            ++*this;
            return old;
        }

        bool operator==(const const_iterator &) const = default;

        void skip() {
            while (index < N + 1 && !owner->bits_.test(index)) {
                ++index;
            }
        }
    };

    EnumSet() = default;

    EnumSet(std::initializer_list<E> values) { insert(values.begin(), values.end()); }

    bool contains(E value) const {
        auto i = (static_cast<std::size_t>(value) + 1);
        return i < N + 1 && bits_.test(i);
    }

    bool empty() const { return bits_.none(); }

    std::size_t size() const { return bits_.count(); }

    void insert(E value) { bits_.set((static_cast<std::size_t>(value) + 1)); }

    template <class It>
    void insert(It first, It last) {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    void emplace(E value) { insert(value); }

    void erase(E value) { bits_.reset((static_cast<std::size_t>(value) + 1)); }

    void clear() { bits_.reset(); }

    const_iterator begin() const {
        const_iterator it{this, 0};
        it.skip();
        return it;
    }

    const_iterator end() const { return {this, N + 1}; }

    const_iterator cbegin() const { return begin(); }

    const_iterator cend() const { return end(); }

    const_iterator find(E value) const { return contains(value) ? const_iterator{this, (static_cast<std::size_t>(value) + 1)} : end(); }

    bool operator==(const EnumSet &) const = default;
private:
    std::bitset<N + 1> bits_;
};

template <class E, std::size_t N>
bool contains(const EnumSet<E, N> &set, E value) {
    return set.contains(value);
}
} // namespace muse
