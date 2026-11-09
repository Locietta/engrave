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

#include <memory>
#include <variant>

#include "../flatmap.h"

namespace muse {
template <typename KeyType, typename ValType>
struct SharedFlatMap {

    // like std
    using key_type = KeyType;
    using mapped_type = ValType;
    using value_type = FlatMapEntry<KeyType, ValType>;

    using PairType = value_type;
    using Data = FlatMap<KeyType, ValType>;
    using DataPtr = std::shared_ptr<Data>;
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;
    typedef typename Data::reverse_iterator reverse_iterator;
    typedef typename Data::const_reverse_iterator const_reverse_iterator;

    SharedFlatMap() = default;

    SharedFlatMap(std::initializer_list<PairType> init_list) {
        if (init_list.size()) {
            data_ptr_ = std::make_shared<Data>(init_list);
        }
    }

    SharedFlatMap &operator=(std::initializer_list<PairType> init_list) {
        data_ptr_.reset();
        if (init_list.size()) {
            data_ptr_ = std::make_shared<Data>(init_list);
        }
        return *this;
    }

    SharedFlatMap(const SharedFlatMap &) = default;
    SharedFlatMap(SharedFlatMap &&) = default;
    SharedFlatMap &operator=(const SharedFlatMap &) = default;
    SharedFlatMap &operator=(SharedFlatMap &&) = default;

    const ValType &at(const KeyType &key) const { return read_data().at(key); }

    ValType &at(const KeyType &key) {
        ensure_detach();
        return data_ptr_->at(key);
    }

    ValType &operator[](const KeyType &key) {
        ensure_detach();
        return data_ptr_->operator[](key);
    }

    ValType &operator[](KeyType &&key) {
        ensure_detach();
        return data_ptr_->operator[](std::forward<KeyType>(key));
    }

    iterator begin() {
        ensure_detach();
        return data_ptr_->begin();
    }

    iterator end() {
        ensure_detach();
        return data_ptr_->end();
    }

    const_iterator begin() const { return read_data().cbegin(); }

    const_iterator end() const { return read_data().cend(); }

    const_iterator cbegin() const { return read_data().cbegin(); }

    const_iterator cend() const { return read_data().cend(); }

    const_reverse_iterator rbegin() const { return read_data().rbegin(); }

    const_reverse_iterator rend() const { return read_data().rend(); }

    const_iterator find(const KeyType &key) const { return read_data().find(key); }

    iterator find(const KeyType &key) {
        ensure_detach();
        return data_ptr_->find(key);
    }

    iterator lower_bound(const KeyType &key) {
        ensure_detach();
        return data_ptr_->lower_bound(key);
    }

    const_iterator lower_bound(const KeyType &key) const { return read_data().lower_bound(key); }

    iterator upper_bound(const KeyType &key) {
        ensure_detach();
        return data_ptr_->upper_bound(key);
    }

    const_iterator upper_bound(const KeyType &key) const { return read_data().upper_bound(key); }

    bool contains(const KeyType &key) const { return find(key) != end(); }

    bool empty() const { return read_data().empty(); }

    size_t size() const { return read_data().size(); }

    bool unique() const { return !data_ptr_ || data_ptr_.use_count() == 1; }

    void insert(const PairType &pair) {
        ensure_detach();
        data_ptr_->insert(pair);
    }

    void insert(PairType &&pair) {
        ensure_detach();
        data_ptr_->insert(std::forward<PairType>(pair));
    }

    void insert(const_iterator first, const_iterator last) {
        // Snapshot before insertion: flat growth invalidates source iterators on self-insert.
        Data values(first, last);
        ensure_detach();
        data_ptr_->insert(values.cbegin(), values.cend());
    }

    iterator insert(const_iterator hint, const PairType &pair) {
        return with_detached_hint(hint, [this, &pair](const_iterator h) { return data_ptr_->insert(h, pair); });
    }

    iterator insert(const_iterator hint, PairType &&pair) {
        return with_detached_hint(hint, [this, &pair](const_iterator h) { return data_ptr_->insert(h, std::forward<PairType>(pair)); });
    }

    void merge(SharedFlatMap &source) {
        if (this == &source) {
            return;
        }
        ensure_detach();
        source.ensure_detach();
        for (auto it = source.data_ptr_->begin(); it != source.data_ptr_->end();) {
            if (data_ptr_->contains(it->first)) {
                ++it;
                continue;
            }
            data_ptr_->insert(std::move(*it));
            it = source.data_ptr_->erase(it);
        }
    }

    void insert_or_assign(const KeyType &key, ValType &&val) {
        ensure_detach();
        data_ptr_->insert_or_assign(key, std::forward<ValType>(val));
    }

    void insert_or_assign(const KeyType &key, const ValType &val) {
        ensure_detach();
        data_ptr_->insert_or_assign(key, val);
    }

    template <typename... Args>
    void emplace(Args &&...args) {
        ensure_detach();
        data_ptr_->insert(PairType{std::forward<Args>(args)...});
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args &&...args) {
        return with_detached_hint(
            hint, [this, &args...](const_iterator h) { return data_ptr_->insert(h, PairType{std::forward<Args>(args)...}); });
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const KeyType &key, Args &&...args) {
        ensure_detach();
        return data_ptr_->try_emplace(key, std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, const KeyType &key, Args &&...args) {
        return with_detached_hint(
            hint, [this, &key, &args...](const_iterator /*h*/) { return data_ptr_->try_emplace(key, std::forward<Args>(args)...).first; });
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, KeyType &&key, Args &&...args) {
        return with_detached_hint(hint, [this, &key, &args...](const_iterator /*h*/) {
            return data_ptr_->try_emplace(std::forward<KeyType>(key), std::forward<Args>(args)...).first;
        });
    }

    void clear() { data_ptr_.reset(); }

    void erase(const KeyType &key) {
        ensure_detach();
        data_ptr_->erase(key);
    }

    iterator erase(const_iterator pos) {
        return with_detached_hint(pos, [this](const_iterator p) { return data_ptr_->erase(p); });
    }

    void erase(const_iterator first, const_iterator last) {
        if (first == last) {
            return;
        }
        if (data_ptr_.use_count() == 1) {
            data_ptr_->erase(first, last);
        } else {
            auto [dfirst, dlast] = detach_and_reanchor_range(first, last);
            data_ptr_->erase(dfirst, dlast);
        }
    }

    bool operator==(const SharedFlatMap &another) const {
        if (data_ptr_ == another.data_ptr_) {
            return true;
        }

        return read_data() == another.read_data();
    }

    bool operator!=(const SharedFlatMap &another) const { return !this->operator==(another); }

    bool operator<(const SharedFlatMap &another) const {
        if (data_ptr_ == another.data_ptr_) {
            return false;
        }

        return read_data() < another.read_data();
    }

    bool operator>(const SharedFlatMap &another) const {
        if (data_ptr_ == another.data_ptr_) {
            return false;
        }

        return read_data() > another.read_data();
    }

private:
    const Data &read_data() const {
        static const Data empty;
        return data_ptr_ ? *data_ptr_ : empty;
    }

    void ensure_detach() {
        if (!data_ptr_) {
            data_ptr_ = std::make_shared<Data>();
            return;
        }

        if (data_ptr_.use_count() == 1) {
            return;
        }

        data_ptr_ = std::make_shared<Data>(*data_ptr_);
    }

    // Capture anchors before detaching; never carry flat iterators across a backing copy.
    struct AtBegin {};

    struct AtEnd {};

    using Anchor = std::variant<AtBegin, AtEnd, KeyType>;

    Anchor describe_anchor(const_iterator it) const {
        if (it == read_data().cbegin()) {
            return AtBegin{};
        }
        if (it == read_data().cend()) {
            return AtEnd{};
        }
        return it->first;
    }

    const_iterator resolve_anchor(const Anchor &a) const {
        if (std::holds_alternative<AtBegin>(a)) {
            return data_ptr_->cbegin();
        }
        if (std::holds_alternative<AtEnd>(a)) {
            return data_ptr_->cend();
        }
        return data_ptr_->find(std::get<KeyType>(a));
    }

    const_iterator detach_and_reanchor(const_iterator hint) {
        const Anchor a = describe_anchor(hint);
        ensure_detach();
        return resolve_anchor(a);
    }

    template <typename Op>
    auto with_detached_hint(const_iterator hint, Op &&op) {
        if (data_ptr_.use_count() == 1) {
            return op(hint);
        }

        const_iterator dhint = detach_and_reanchor(hint);
        return op(dhint);
    }

    std::pair<const_iterator, const_iterator> detach_and_reanchor_range(const_iterator first, const_iterator last) {
        const Anchor first_anchor = describe_anchor(first);
        const Anchor last_anchor = describe_anchor(last);

        ensure_detach();

        return {resolve_anchor(first_anchor), resolve_anchor(last_anchor)};
    }

    DataPtr data_ptr_ = nullptr;
};
} // namespace muse
