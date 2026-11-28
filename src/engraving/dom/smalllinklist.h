/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <vector>

namespace mu::engraving {
struct EngravingObject;

struct SmallLinkList {
    static constexpr std::size_t inline_capacity = 8;

    using value_type = EngravingObject *;
    using iterator = EngravingObject **;
    using const_iterator = EngravingObject *const *;

    SmallLinkList() = default;
    SmallLinkList(const SmallLinkList &) = default;
    SmallLinkList &operator=(const SmallLinkList &) = default;

    SmallLinkList(SmallLinkList &&other) noexcept : inline_(other.inline_), overflow_(std::move(other.overflow_)), size_(other.size_) {
        other.overflow_.clear();
        other.size_ = 0;
    }

    SmallLinkList &operator=(SmallLinkList &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        inline_ = other.inline_;
        overflow_ = std::move(other.overflow_);
        size_ = other.size_;
        other.overflow_.clear();
        other.size_ = 0;
        return *this;
    }

    SmallLinkList(std::initializer_list<EngravingObject *> init) : SmallLinkList(init.begin(), init.end()) {}

    template <typename It>
    SmallLinkList(It first, It last) {
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    void push_back(EngravingObject *item) {
        if (!overflow_.empty()) {
            overflow_.push_back(item);
            size_ = overflow_.size();
            return;
        }
        if (size_ < inline_capacity) {
            inline_[size_++] = item;
            return;
        }
        overflow_.reserve(inline_capacity * 2);
        overflow_.assign(inline_.begin(), inline_.end());
        overflow_.push_back(item);
        size_ = overflow_.size();
    }

    EngravingObject **data() noexcept { return overflow_.empty() ? inline_.data() : overflow_.data(); }

    EngravingObject *const *data() const noexcept { return overflow_.empty() ? inline_.data() : overflow_.data(); }

    iterator begin() noexcept { return data(); }

    iterator end() noexcept { return data() + size_; }

    const_iterator begin() const noexcept { return data(); }

    const_iterator end() const noexcept { return data() + size_; }

    const_iterator cbegin() const noexcept { return begin(); }

    const_iterator cend() const noexcept { return end(); }

    std::size_t size() const noexcept { return size_; }

    bool empty() const noexcept { return size_ == 0; }

    EngravingObject *&front() noexcept {
        assert(size_ > 0);
        return data()[0];
    }

    EngravingObject *front() const noexcept {
        assert(size_ > 0);
        return data()[0];
    }

    EngravingObject *&back() noexcept {
        assert(size_ > 0);
        return data()[size_ - 1];
    }

    EngravingObject *back() const noexcept {
        assert(size_ > 0);
        return data()[size_ - 1];
    }

    EngravingObject *&operator[](std::size_t index) noexcept {
        assert(index < size_);
        return data()[index];
    }

    EngravingObject *operator[](std::size_t index) const noexcept {
        assert(index < size_);
        return data()[index];
    }

    const_iterator find(const EngravingObject *item) const noexcept { return std::find(begin(), end(), item); }

    bool contains(const EngravingObject *item) const noexcept { return find(item) != end(); }

private:
    std::array<EngravingObject *, inline_capacity> inline_{};
    std::vector<EngravingObject *> overflow_;
    std::size_t size_ = 0;
};
} // namespace mu::engraving
