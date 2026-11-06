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
#pragma once

#include <cstdint>
#include <memory>

#include "../types/fontstypes.h"
#include "global/io/iodevice.h"
#include "global/io/path.h"

namespace muse::draw {
struct IFontsDatabase {
    virtual ~IFontsDatabase() = default;

    virtual void set_default_font(Font::Type type, const FontDataKey &key) = 0;
    virtual void insert_substitution(const String &f1, const String &substitute_name) = 0;
    virtual void remove_substitutions(const String &f1, const std::vector<String> &substitute_names) = 0;

    virtual int add_font(const FontDataKey &key, const io::PathT &path) = 0;
    virtual int add_font(const FontDataKey &key, std::shared_ptr<io::IODevice> device) = 0;

    virtual void remove_font(const FontDataKey &key) = 0;

    virtual FontDataKey actual_font(const FontDataKey &require_key, Font::Type type) const = 0;
    virtual std::vector<FontDataKey> substitution_fonts(const FontDataKey &require_key) const = 0;
    virtual FontData font_data(const FontDataKey &require_key, Font::Type type) const = 0;

    virtual uint64_t revision() const = 0;
};
} // namespace muse::draw
