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
#include <unordered_map>
#include <variant>

namespace muse {
template <typename KeyType, typename ValType>
class SharedHashMap {
public:
    // like std
    using key_type = KeyType;
    using mapped_type = ValType;
    using value_type = std::pair<const KeyType, ValType>;

    using PairType = value_type;
    using Data = std::unordered_map<KeyType, ValType>;
    using DataPtr = std::shared_ptr<Data>;
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;

    SharedHashMap() { dataPtr_ = std::make_shared<Data>(); }

    SharedHashMap(const size_t reserveSize) { dataPtr_ = std::make_shared<Data>(reserveSize); }

    SharedHashMap(std::initializer_list<PairType> initList) { dataPtr_ = std::make_shared<Data>(initList); }

    SharedHashMap &operator=(std::initializer_list<PairType> initList) {
        dataPtr_ = std::make_shared<Data>(initList);
        return *this;
    }

    SharedHashMap(const SharedHashMap &) = default;
    SharedHashMap(SharedHashMap &&) = default;
    SharedHashMap &operator=(const SharedHashMap &) = default;
    SharedHashMap &operator=(SharedHashMap &&) = default;

    const ValType &at(const KeyType &key) const { return dataPtr_->at(key); }

    ValType &at(const KeyType &key) {
        ensureDetach();
        return dataPtr_->at(key);
    }

    ValType &operator[](const KeyType &key) {
        ensureDetach();
        return dataPtr_->operator[](key);
    }

    ValType &operator[](KeyType &&key) {
        ensureDetach();
        return dataPtr_->operator[](std::forward<KeyType>(key));
    }

    iterator begin() noexcept {
        ensureDetach();
        return dataPtr_->begin();
    }

    iterator end() noexcept {
        ensureDetach();
        return dataPtr_->end();
    }

    const_iterator begin() const noexcept { return dataPtr_->cbegin(); }

    const_iterator end() const noexcept { return dataPtr_->cend(); }

    const_iterator cbegin() const noexcept { return dataPtr_->cbegin(); }

    const_iterator cend() const noexcept { return dataPtr_->cend(); }

    const_iterator find(const KeyType &key) const noexcept { return dataPtr_->find(key); }

    iterator find(const KeyType &key) noexcept {
        ensureDetach();
        return dataPtr_->find(key);
    }

    bool contains(const KeyType &key) const noexcept { return find(key) != end(); }

    bool empty() const noexcept { return dataPtr_->empty(); }

    size_t size() const noexcept { return dataPtr_->size(); }

    void insert(const PairType &pair) {
        ensureDetach();
        dataPtr_->insert(pair);
    }

    void insert(PairType &&pair) {
        ensureDetach();
        dataPtr_->insert(std::forward<PairType>(pair));
    }

    void insert(const_iterator first, const_iterator last) {
        ensureDetach();
        dataPtr_->insert(first, last);
    }

    iterator insert(const_iterator hint, const PairType &pair) {
        return withDetachedHint(hint, [this, &pair](const_iterator h) { return dataPtr_->insert(h, pair); });
    }

    iterator insert(const_iterator hint, PairType &&pair) {
        return withDetachedHint(hint, [this, &pair](const_iterator h) { return dataPtr_->insert(h, std::forward<PairType>(pair)); });
    }

    void merge(SharedHashMap &source) {
        ensureDetach();
        source.ensureDetach();
        dataPtr_->merge(*source.dataPtr_);
    }

    void insert_or_assign(const KeyType &key, ValType &&val) {
        ensureDetach();
        dataPtr_->insert_or_assign(key, std::forward<ValType>(val));
    }

    void insert_or_assign(const KeyType &key, const ValType &val) {
        ensureDetach();
        dataPtr_->insert_or_assign(key, val);
    }

    template <typename... Args>
    void emplace(Args &&...args) {
        ensureDetach();
        dataPtr_->emplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args &&...args) {
        return withDetachedHint(hint,
                                [this, &args...](const_iterator h) { return dataPtr_->emplace_hint(h, std::forward<Args>(args)...); });
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const KeyType &key, Args &&...args) {
        ensureDetach();
        return dataPtr_->try_emplace(key, std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, const KeyType &key, Args &&...args) {
        return withDetachedHint(
            hint, [this, &key, &args...](const_iterator h) { return dataPtr_->try_emplace(h, key, std::forward<Args>(args)...); });
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, KeyType &&key, Args &&...args) {
        return withDetachedHint(hint, [this, &key, &args...](const_iterator h) {
            return dataPtr_->try_emplace(h, std::forward<KeyType>(key), std::forward<Args>(args)...);
        });
    }

    void clear() noexcept {
        ensureDetach();
        dataPtr_->clear();
    }

    void erase(const KeyType &key) {
        ensureDetach();
        dataPtr_->erase(key);
    }

    iterator erase(const_iterator pos) {
        return withDetachedHint(pos, [this](const_iterator p) { return dataPtr_->erase(p); });
    }

    void erase(const_iterator first, const_iterator last) {
        if (dataPtr_.use_count() == 1) {
            dataPtr_->erase(first, last);
        } else {
            auto [dfirst, dlast] = detachAndReanchorRange(first, last);
            dataPtr_->erase(dfirst, dlast);
        }
    }

    bool operator==(const SharedHashMap &another) const noexcept {
        if (dataPtr_ == another.dataPtr_) {
            return true;
        }

        return *dataPtr_ == *another.dataPtr_;
    }

    bool operator!=(const SharedHashMap &another) const noexcept { return !this->operator==(another); }

private:
    void ensureDetach() {
        if (!dataPtr_) {
            return;
        }

        if (dataPtr_.use_count() == 1) {
            return;
        }

        dataPtr_ = std::make_shared<Data>(*dataPtr_);
    }

    // Detach-safe descriptor of a position
    struct AtEnd {};

    using Anchor = std::variant<AtEnd, KeyType>;

    Anchor describeAnchor(const_iterator it) const {
        if (it == dataPtr_->cend()) {
            return AtEnd{};
        }
        return it->first;
    }

    const_iterator resolveAnchor(const Anchor &a) const {
        if (std::holds_alternative<AtEnd>(a)) {
            return dataPtr_->cend();
        }
        return dataPtr_->find(std::get<KeyType>(a));
    }

    const_iterator detachAndReanchor(const_iterator hint) {
        const Anchor a = describeAnchor(hint);
        ensureDetach();
        return resolveAnchor(a);
    }

    template <typename Op>
    auto withDetachedHint(const_iterator hint, Op &&op) {
        if (dataPtr_.use_count() == 1) {
            return op(hint);
        }

        const_iterator dhint = detachAndReanchor(hint);
        return op(dhint);
    }

    std::pair<const_iterator, const_iterator> detachAndReanchorRange(const_iterator first, const_iterator last) {
        const Anchor firstAnchor = describeAnchor(first);
        const Anchor lastAnchor = describeAnchor(last);

        ensureDetach();

        return {resolveAnchor(firstAnchor), resolveAnchor(lastAnchor)};
    }

    DataPtr dataPtr_ = nullptr;
};
} // namespace muse
