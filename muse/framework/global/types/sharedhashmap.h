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
struct SharedHashMap {
    // like std
    using key_type = KeyType;
    using mapped_type = ValType;
    using value_type = std::pair<const KeyType, ValType>;

    using PairType = value_type;
    using Data = std::unordered_map<KeyType, ValType>;
    using DataPtr = std::shared_ptr<Data>;
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;

    SharedHashMap() { data_ptr_ = std::make_shared<Data>(); }

    SharedHashMap(const size_t reserve_size) { data_ptr_ = std::make_shared<Data>(reserve_size); }

    SharedHashMap(std::initializer_list<PairType> init_list) { data_ptr_ = std::make_shared<Data>(init_list); }

    SharedHashMap &operator=(std::initializer_list<PairType> init_list) {
        data_ptr_ = std::make_shared<Data>(init_list);
        return *this;
    }

    SharedHashMap(const SharedHashMap &) = default;
    SharedHashMap(SharedHashMap &&) = default;
    SharedHashMap &operator=(const SharedHashMap &) = default;
    SharedHashMap &operator=(SharedHashMap &&) = default;

    const ValType &at(const KeyType &key) const { return data_ptr_->at(key); }

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

    iterator begin() noexcept {
        ensure_detach();
        return data_ptr_->begin();
    }

    iterator end() noexcept {
        ensure_detach();
        return data_ptr_->end();
    }

    const_iterator begin() const noexcept { return data_ptr_->cbegin(); }

    const_iterator end() const noexcept { return data_ptr_->cend(); }

    const_iterator cbegin() const noexcept { return data_ptr_->cbegin(); }

    const_iterator cend() const noexcept { return data_ptr_->cend(); }

    const_iterator find(const KeyType &key) const noexcept { return data_ptr_->find(key); }

    iterator find(const KeyType &key) noexcept {
        ensure_detach();
        return data_ptr_->find(key);
    }

    bool contains(const KeyType &key) const noexcept { return find(key) != end(); }

    bool empty() const noexcept { return data_ptr_->empty(); }

    size_t size() const noexcept { return data_ptr_->size(); }

    void insert(const PairType &pair) {
        ensure_detach();
        data_ptr_->insert(pair);
    }

    void insert(PairType &&pair) {
        ensure_detach();
        data_ptr_->insert(std::forward<PairType>(pair));
    }

    void insert(const_iterator first, const_iterator last) {
        ensure_detach();
        data_ptr_->insert(first, last);
    }

    iterator insert(const_iterator hint, const PairType &pair) {
        return with_detached_hint(hint, [this, &pair](const_iterator h) { return data_ptr_->insert(h, pair); });
    }

    iterator insert(const_iterator hint, PairType &&pair) {
        return with_detached_hint(hint, [this, &pair](const_iterator h) { return data_ptr_->insert(h, std::forward<PairType>(pair)); });
    }

    void merge(SharedHashMap &source) {
        ensure_detach();
        source.ensure_detach();
        data_ptr_->merge(*source.data_ptr_);
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
        data_ptr_->emplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator hint, Args &&...args) {
        return with_detached_hint(hint,
                                  [this, &args...](const_iterator h) { return data_ptr_->emplace_hint(h, std::forward<Args>(args)...); });
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const KeyType &key, Args &&...args) {
        ensure_detach();
        return data_ptr_->try_emplace(key, std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, const KeyType &key, Args &&...args) {
        return with_detached_hint(
            hint, [this, &key, &args...](const_iterator h) { return data_ptr_->try_emplace(h, key, std::forward<Args>(args)...); });
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, KeyType &&key, Args &&...args) {
        return with_detached_hint(hint, [this, &key, &args...](const_iterator h) {
            return data_ptr_->try_emplace(h, std::forward<KeyType>(key), std::forward<Args>(args)...);
        });
    }

    void clear() noexcept {
        ensure_detach();
        data_ptr_->clear();
    }

    void erase(const KeyType &key) {
        ensure_detach();
        data_ptr_->erase(key);
    }

    iterator erase(const_iterator pos) {
        return with_detached_hint(pos, [this](const_iterator p) { return data_ptr_->erase(p); });
    }

    void erase(const_iterator first, const_iterator last) {
        if (data_ptr_.use_count() == 1) {
            data_ptr_->erase(first, last);
        } else {
            auto [dfirst, dlast] = detach_and_reanchor_range(first, last);
            data_ptr_->erase(dfirst, dlast);
        }
    }

    bool operator==(const SharedHashMap &another) const noexcept {
        if (data_ptr_ == another.data_ptr_) {
            return true;
        }

        return *data_ptr_ == *another.data_ptr_;
    }

    bool operator!=(const SharedHashMap &another) const noexcept { return !this->operator==(another); }

private:
    void ensure_detach() {
        if (!data_ptr_) {
            return;
        }

        if (data_ptr_.use_count() == 1) {
            return;
        }

        data_ptr_ = std::make_shared<Data>(*data_ptr_);
    }

    // Detach-safe descriptor of a position
    struct AtEnd {};

    using Anchor = std::variant<AtEnd, KeyType>;

    Anchor describe_anchor(const_iterator it) const {
        if (it == data_ptr_->cend()) {
            return AtEnd{};
        }
        return it->first;
    }

    const_iterator resolve_anchor(const Anchor &a) const {
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
