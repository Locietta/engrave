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

#ifndef MUSE_MPE_ARTICULATIONPROFILESREPOSITORY_H
#define MUSE_MPE_ARTICULATIONPROFILESREPOSITORY_H

#include <array>
#include <optional>

#include "io/ifilesystem.h"
#include "mpe/iarticulationprofilesrepository.h"
#include "serialization/json.h"

namespace muse::mpe {
struct ArticulationProfilesRepository : IArticulationProfilesRepository {
    explicit ArticulationProfilesRepository(io::IFileSystem &file_system) : file_system_(file_system) {}

    ArticulationsProfilePtr create_new() const override;
    ArticulationsProfilePtr default_profile(const ArticulationFamily family) const override;
    ArticulationsProfilePtr load_profile(const io::PathT &path) const override;
    void save_profile(const io::PathT &path, const ArticulationsProfilePtr profile_ptr) override;

private:
    io::IFileSystem *file_system() const { return &file_system_; }

    std::vector<ArticulationFamily> supported_families_from_json(const JsonArray &array) const;
    JsonArray supported_families_to_json(const std::vector<ArticulationFamily> &families) const;

    ArticulationPattern patterns_scope_from_json(const JsonArray &array) const;
    JsonArray patterns_scope_to_json(const ArticulationPattern &scope) const;

    ArrangementPattern arrangement_pattern_from_json(const JsonObject &obj) const;
    JsonObject arrangement_pattern_to_json(const ArrangementPattern &pattern) const;

    PitchPattern pitch_pattern_from_json(const JsonObject &obj) const;
    JsonObject pitch_pattern_to_json(const PitchPattern &pattern) const;

    ExpressionPattern expression_pattern_from_json(const JsonObject &obj) const;
    JsonObject expression_pattern_to_json(const ExpressionPattern &pattern) const;

    // optional preserves cached load failures (a cached nullptr is still a cache hit).
    mutable std::array<std::optional<ArticulationsProfilePtr>, static_cast<size_t>(SoundCategory::LAST)> default_profiles_;

    io::IFileSystem &file_system_;
};
} // namespace muse::mpe

#endif // MUSE_MPE_ARTICULATIONPROFILESREPOSITORY_H
