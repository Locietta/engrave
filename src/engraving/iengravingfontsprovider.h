/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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
#include <string>

#include "global/io/path.h"
#include "global/io/iodevice.h"

#include "iengravingfont.h"

namespace mu::engraving {
struct IEngravingFontsProvider {
    virtual ~IEngravingFontsProvider() = default;

    virtual void add_internal_font(const std::string &name, const std::string &family, const muse::io::PathT &file_path) = 0;
    virtual void add_external_font(const std::string &name, const std::string &family, const muse::io::PathT &file_path,
                                   const muse::io::PathT &metadata_path) = 0;
    virtual void add_external_font(const std::string &name, const std::string &family, std::shared_ptr<muse::io::IODevice> font,
                                   std::shared_ptr<muse::io::IODevice> metadata) = 0;
    virtual IEngravingFontPtr font_by_name(const std::string &name) const = 0;
    virtual std::vector<IEngravingFontPtr> fonts() const = 0;

    virtual void set_fallback_font(const std::string &name) = 0;
    virtual IEngravingFontPtr fallback_font() const = 0;
    virtual bool is_fallback_font(const IEngravingFont *f) const = 0;

    virtual void clear_external_fonts() = 0;

    virtual void load_all_fonts() = 0;
};
} // namespace mu::engraving
