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

#include "global/types/ret.h"
#include "infrastructure/mscreader.h"
#include "infrastructure/mscwriter.h"
#include "infrastructure/ifileinfoprovider.h"
#include "types/types.h"

#include "engravingservices.h"

//! NOTE In addition to the score itself, the mscz file also stores other data,
//! such as synthesizer, mixer settings, omr, etc.
//! We should talk not just about the score, but about the Project.
//! So, the Engraving Project contains the score and all other data,
//! such as additional styles, sound settings, sound samples, and others.
//!
//! To simplify logic and reduce combinatorics,
//! we need to strive to ensure that there is work with the project everywhere;
//! accordingly, only the project should create and load the master score.

namespace mu::engraving::rw {
struct ReadInOutData;
} // namespace mu::engraving::rw

namespace mu::engraving::write {
struct WriteContext;
} // namespace mu::engraving::write

namespace mu::engraving {
struct MasterScore;
struct MStyle;

struct EngravingProject : std::enable_shared_from_this<EngravingProject> {
    ~EngravingProject();

    // services must outlive the project.
    static std::shared_ptr<EngravingProject> create(const EngravingServices &services);
    static std::shared_ptr<EngravingProject> create(const MStyle &style, const EngravingServices &services);

    IFileInfoProviderPtr file_info_provider() const;
    void set_file_info_provider(IFileInfoProviderPtr file_info_provider);

    String app_version() const;
    int msc_version() const;

    bool read_only() const;

    MasterScore *master_score() const;
    void set_master_score(MasterScore *score);
    muse::Ret setup_master_score(bool force_mode);

    muse::Ret load_mscz(const MscReader &msc, rw::ReadInOutData *data, bool ignore_version_error);
    bool write_mscz(MscWriter &writer, const write::WriteContext *ctx = nullptr);

    bool is_corrupted_upon_loading() const;
    muse::Ret check_corrupted() const;

private:
    friend struct MasterScore;

    explicit EngravingProject(const EngravingServices &services);

    void init(const MStyle &style);

    const EngravingServices &services_;
    MasterScore *master_score_ = nullptr;

    bool is_corrupted_upon_loading_ = false;
};

using EngravingProjectPtr = std::shared_ptr<EngravingProject>;
} // namespace mu::engraving
