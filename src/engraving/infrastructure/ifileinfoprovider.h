/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "types/datetime.h"
#include "io/path.h"

namespace mu::engraving {
struct IFileInfoProvider {
    virtual ~IFileInfoProvider() = default;

    virtual bool saved() const = 0;

    virtual muse::io::PathT path() const = 0; //! Absolute path
    virtual muse::io::PathT file_name(bool including_extension = true) const = 0;
    virtual muse::io::PathT absolute_dir_path() const = 0; //! Absolute path of the containing folder

    virtual muse::String display_name() const = 0;

    virtual muse::DateTime birth_time() const = 0;
    virtual muse::DateTime last_modified() const = 0;
    virtual bool is_newly_created() const = 0;
};

using IFileInfoProviderPtr = std::shared_ptr<IFileInfoProvider>;
} // namespace mu::engraving
