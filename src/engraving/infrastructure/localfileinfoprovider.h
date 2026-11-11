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

#include "engrave/src/runtime/resource_filesystem.h"
#include "io/path.h"
#include "types/datetime.h"

namespace mu::engraving {
struct LocalFileInfoProvider {

    explicit LocalFileInfoProvider(const muse::io::PathT &file_path);

    bool saved() const;

    muse::io::PathT path() const;
    muse::io::PathT file_name(bool including_extension = true) const;
    muse::io::PathT absolute_dir_path() const;

    muse::String display_name() const;

    muse::DateTime birth_time() const;
    muse::DateTime last_modified() const;
    bool is_newly_created() const;

private:
    muse::io::PathT path_;
};

using LocalFileInfoProviderPtr = std::shared_ptr<LocalFileInfoProvider>;
} // namespace mu::engraving
