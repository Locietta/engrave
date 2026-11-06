/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#include <stdexcept>
#include <vector>

namespace muse {
template <typename T>
struct VectorView {
public:
    VectorView(std::vector<T> &vec) : begin_(vec.begin()), end_(vec.end()) {}

    using iterator = typename std::vector<T>::iterator;
    using reverse_iterator = typename std::vector<T>::reverse_iterator;

    iterator begin() { return begin_; }

    iterator end() { return end_; }

    reverse_iterator rbegin() { return reverse_iterator{end_}; }

    reverse_iterator rend() { return reverse_iterator{begin_}; }

    bool empty() const { return begin_ == end_; }

    T &front() const {
        if (empty()) {
            throw std::out_of_range("VectorView::front(): empty view");
        }
        return *begin_;
    }

    T &pop_front() {
        if (empty()) {
            throw std::out_of_range("VectorView::pop_front(): empty view");
        }
        T &v = *begin_;
        ++begin_;
        return v;
    }

    T &back() const {
        if (empty()) {
            throw std::out_of_range("VectorView::back(): empty view");
        }
        return *(end_ - 1);
    }

    T &pop_back() {
        if (empty()) {
            throw std::out_of_range("VectorView::pop_back(): empty view");
        }
        --end_;
        return *end_;
    }

    bool remove(const T &v) {
        auto original_end = end_;
        end_ = std::remove(begin_, end_, v);
        return end_ != original_end;
    }

    template <typename Predicate>
    bool remove_if(Predicate &&pred) {
        auto original_end = end_;
        end_ = std::remove_if(begin_, end_, std::forward<Predicate>(pred));
        return end_ != original_end;
    }

    bool reverse_remove(const T &v) {
        auto original_begin = begin_;
        begin_ = std::remove(rbegin(), rend(), v).base();
        return begin_ != original_begin;
    }

    template <typename Predicate>
    bool reverse_remove_if(Predicate &&pred) {
        auto original_begin = begin_;
        begin_ = std::remove_if(rbegin(), rend(), std::forward<Predicate>(pred)).base();
        return begin_ != original_begin;
    }

private:
    iterator begin_;
    iterator end_;
};
} // namespace muse
