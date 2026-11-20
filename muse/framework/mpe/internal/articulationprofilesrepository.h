/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#include <array>
#include <optional>

#include "engrave/src/runtime/resource_filesystem.h"
#include "io/path.h"
#include "mpe/mpetypes.h"

namespace muse::mpe {
struct ArticulationProfilesRepository {
    std::optional<ArticulationsProfilePtr> default_profile_override;

    explicit ArticulationProfilesRepository(io::NativeFileSystem &file_system) : file_system_(file_system) {}

    ArticulationsProfilePtr create_new() const;
    ArticulationsProfilePtr default_profile(const ArticulationFamily family) const;
    ArticulationsProfilePtr load_profile(const io::PathT &path) const;
    void save_profile(const io::PathT &path, const ArticulationsProfilePtr profile_ptr);

private:
    io::NativeFileSystem *file_system() const { return &file_system_; }

    // optional preserves cached load failures (a cached nullptr is still a cache hit).
    mutable std::array<std::optional<ArticulationsProfilePtr>, static_cast<size_t>(SoundCategory::LAST)> default_profiles_;

    io::NativeFileSystem &file_system_;
};
} // namespace muse::mpe
