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
#include "engravingproject.h"

#include "dom/masterscore.h"
#include "dom/part.h"
#include "global/allocator.h"
#include "log.h"
#include "rw/inoutdata.h"
#include "rw/mscloader.h"
#include "rw/mscsaver.h"
#include "style/defaultstyle.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;

std::shared_ptr<EngravingProject> EngravingProject::create(const EngravingServices &services) {
    std::shared_ptr<EngravingProject> p = std::shared_ptr<EngravingProject>(new EngravingProject(services));
    p->init(DefaultStyle::default_style());
    return p;
}

std::shared_ptr<EngravingProject> EngravingProject::create(const MStyle &style, const EngravingServices &services) {
    std::shared_ptr<EngravingProject> p = std::shared_ptr<EngravingProject>(new EngravingProject(services));
    p->init(style);
    return p;
}

EngravingProject::EngravingProject(const EngravingServices &services) : services_(services) { muse::ObjectAllocator::used(); }

EngravingProject::~EngravingProject() {
    delete master_score_;

    muse::ObjectAllocator::unused();

    // muse::AllocatorsRegister::instance()->printStatistic("=== Destroy engraving project ===");
    //! NOTE At the moment, the allocator is working as leak detector. No need to do cleanup, at the moment it can lead to crashes
    // AllocatorsRegister::instance()->cleanupAll("engraving");
}

void EngravingProject::init(const MStyle &style) { master_score_ = new MasterScore(services_, style, weak_from_this()); }

IFileInfoProviderPtr EngravingProject::file_info_provider() const { return master_score_->file_info(); }

void EngravingProject::set_file_info_provider(IFileInfoProviderPtr file_info_provider) {
    master_score_->set_file_info_provider(file_info_provider);
}

String EngravingProject::app_version() const { return master_score_->mscore_version(); }

int EngravingProject::msc_version() const { return master_score_->msc_version(); }

bool EngravingProject::read_only() const { return master_score_->read_only(); }

void EngravingProject::set_master_score(MasterScore *score) { master_score_ = score; }

Ret EngravingProject::setup_master_score(bool force_mode) {

    master_score_->update_padding_tables();
    for (Score *s : master_score_->score_list()) {
        s->update_padding_tables();
    }

    master_score_->set_layout_all();
    master_score_->update();
    master_score_->init_automation();

    Ret ret = check_corrupted();
    is_corrupted_upon_loading_ = !ret;

    return force_mode ? muse::make_ok() : ret;
}

MasterScore *EngravingProject::master_score() const { return master_score_; }

Ret EngravingProject::load_mscz(const MscReader &msc, rw::ReadInOutData *data, bool ignore_version_error) {

    MScore::set_error(MsError::MS_NO_ERROR);

    MscLoader loader;
    return loader.load_mscz(master_score_, msc, data, ignore_version_error);
}

bool EngravingProject::write_mscz(MscWriter &writer, const write::WriteContext *ctx) {

    MscSaver saver;
    return saver.write_mscz(master_score_, writer, ctx);
}

bool EngravingProject::is_corrupted_upon_loading() const { return is_corrupted_upon_loading_; }

Ret EngravingProject::check_corrupted() const { return master_score_->sanity_check(); }
