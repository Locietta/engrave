/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#ifndef MUSE_GLOBAL_CONFIG_H
#define MUSE_GLOBAL_CONFIG_H

#include "types/val.h"

namespace muse {
//! NOTE Some modules have configuration files, this is their model
struct Config {
    Config() = default;

    Config(const ValMap &v) : data_(v) {}

    const Val &value(const std::string &key, const Val &def = Val()) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return def;
    }

    void set(const std::string &key, const Val &val) { data_[key] = val; }

    auto begin() { return data_.begin(); }

    auto end() { return data_.end(); }

    auto begin() const { return data_.begin(); }

    auto end() const { return data_.end(); }

    auto cbegin() const { return data_.cbegin(); }

    auto cend() const { return data_.cend(); }

    const ValMap &data() const { return data_; }

private:
    ValMap data_;
};
} // namespace muse

#endif // MUSE_GLOBAL_CONFIG_H
