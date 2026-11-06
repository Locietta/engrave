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

#include "ifileinfoprovider.h"

#include "io/ifilesystem.h"

namespace mu::engraving {
class LocalFileInfoProvider : public IFileInfoProvider {
public:
    explicit LocalFileInfoProvider(const muse::io::PathT &file_path);

    bool saved() const override;

    muse::io::PathT path() const override;
    muse::io::PathT file_name(bool including_extension = true) const override;
    muse::io::PathT absolute_dir_path() const override;

    muse::String display_name() const override;

    muse::DateTime birth_time() const override;
    muse::DateTime last_modified() const override;
    bool is_newly_created() const override;

private:
    muse::io::PathT path_;
};
} // namespace mu::engraving
