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

#include <string>

#include "types/val.h"
#include "io/path.h"

//! NOTE We are gradually abandoning Qt in non-GUI classes.
//! This settings interface is almost independent of Qt,
//! QSettings are used only in the implementation for compatibility with current settings.
//! Perhaps in the future this will be changed.

class QSettings;

namespace muse {
class Settings {
public:
    static Settings *instance();

    struct Key {
        std::string module_name;
        std::string key;

        Key() = default;
        Key(std::string module_name, std::string key);

        bool is_null() const;
        bool operator==(const Key &k) const;
        bool operator<(const Key &k) const;
    };

    struct Item {
        Key key;
        Val value;
        Val default_value;
        std::string description;

        bool can_be_manually_edited = false;
        Val min_value;
        Val max_value;

        bool is_null() const { return key.is_null(); }
    };

    using Items = std::map<Key, Item>;

    const Items &items() const;

    void reload();
    void load();

    void reset(bool keep_default_settings = false, bool notify_about_changes = true, bool notify_other_instances = true);

    const Val &value(const Key &key) const;
    const Val &default_value(const Key &key) const;

    //! NOTE Will be write to global config and sync between all instances
    void set_shared_value(const Key &key, const Val &value);

    //! NOTE Will be write to global config and NOT sync between instances
    void set_local_value(const Key &key, const Val &value);

    void set_default_value(const Key &key, const Val &value);

    void set_description(const Key &key, const std::string &value);

    void set_can_be_manually_edited(const Settings::Key &key, bool can_be_manually_edited, const Val &min_value = Val(),
                                    const Val &max_value = Val());

    void begin_transaction(bool notify_to_other_instances = true);
    void commit_transaction(bool notify_to_other_instances = true);
    void rollback_transaction(bool notify_to_other_instances = true);

    io::PathT file_path() const;

private:
    Settings();
    ~Settings();

    Item &find_item(const Key &key) const;

    void insert_new_item(const Key &key, const Val &value);

    Items read_items() const;
    void write_value(const Key &key, const Val &value);

    QSettings *settings_ = nullptr;
    mutable Items items_;
    mutable Items local_settings_;
    mutable bool is_transaction_started_ = false;
};

inline Settings *settings() { return Settings::instance(); }
} // namespace muse
