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

#ifndef MUSE_MPE_IARTICULATIONPROFILESREPOSITORY_H
#define MUSE_MPE_IARTICULATIONPROFILESREPOSITORY_H

#include "io/path.h"
#include "mpetypes.h"

namespace muse::mpe {
struct IArticulationProfilesRepository {
    virtual ~IArticulationProfilesRepository() = default;

    virtual ArticulationsProfilePtr create_new() const = 0;
    virtual ArticulationsProfilePtr default_profile(const ArticulationFamily family) const = 0;
    virtual ArticulationsProfilePtr load_profile(const io::PathT &path) const = 0;
    virtual void save_profile(const io::PathT &path, const ArticulationsProfilePtr profile_ptr) = 0;
};
} // namespace muse::mpe

#endif // MUSE_MPE_IARTICULATIONPROFILESREPOSITORY_H
