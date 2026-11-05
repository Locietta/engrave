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

#include "global/allocator.h"

#include "rw/inoutdata.h"
#include "rw/mscloader.h"
#include "rw/mscsaver.h"
#include "style/defaultstyle.h"
#include "dom/masterscore.h"
#include "dom/part.h"

#include "log.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;

std::shared_ptr<EngravingProject> EngravingProject::create(const EngravingServices &services) {
    std::shared_ptr<EngravingProject> p = std::shared_ptr<EngravingProject>(new EngravingProject(services));
    p->init(DefaultStyle::defaultStyle());
    return p;
}

std::shared_ptr<EngravingProject> EngravingProject::create(const MStyle &style, const EngravingServices &services) {
    std::shared_ptr<EngravingProject> p = std::shared_ptr<EngravingProject>(new EngravingProject(services));
    p->init(style);
    return p;
}

EngravingProject::EngravingProject(const EngravingServices &services) : services_(services) { muse::ObjectAllocator::used(); }

EngravingProject::~EngravingProject() {
    delete masterScore_;

    muse::ObjectAllocator::unused();

    // muse::AllocatorsRegister::instance()->printStatistic("=== Destroy engraving project ===");
    //! NOTE At the moment, the allocator is working as leak detector. No need to do cleanup, at the moment it can lead to crashes
    // AllocatorsRegister::instance()->cleanupAll("engraving");
}

void EngravingProject::init(const MStyle &style) { masterScore_ = new MasterScore(services_, style, weak_from_this()); }

IFileInfoProviderPtr EngravingProject::fileInfoProvider() const { return masterScore_->fileInfo(); }

void EngravingProject::setFileInfoProvider(IFileInfoProviderPtr fileInfoProvider) { masterScore_->setFileInfoProvider(fileInfoProvider); }

String EngravingProject::appVersion() const { return masterScore_->mscoreVersion(); }

int EngravingProject::mscVersion() const { return masterScore_->mscVersion(); }

bool EngravingProject::readOnly() const { return masterScore_->readOnly(); }

void EngravingProject::setMasterScore(MasterScore *score) { masterScore_ = score; }

Ret EngravingProject::setupMasterScore(bool forceMode) {
    TRACEFUNC;

    masterScore_->updatePaddingTables();
    for (Score *s : masterScore_->scoreList()) {
        s->updatePaddingTables();
    }

    masterScore_->setLayoutAll();
    masterScore_->update();
    masterScore_->initAutomation();

    Ret ret = checkCorrupted();
    isCorruptedUponLoading_ = !ret;

    return forceMode ? muse::make_ok() : ret;
}

MasterScore *EngravingProject::masterScore() const { return masterScore_; }

Ret EngravingProject::loadMscz(const MscReader &msc, rw::ReadInOutData *data, bool ignoreVersionError) {
    TRACEFUNC;

    MScore::setError(MsError::MS_NO_ERROR);

    MscLoader loader;
    return loader.loadMscz(masterScore_, msc, data, ignoreVersionError);
}

bool EngravingProject::writeMscz(MscWriter &writer, const write::WriteContext *ctx) {
    TRACEFUNC;

    MscSaver saver;
    return saver.writeMscz(masterScore_, writer, ctx);
}

bool EngravingProject::isCorruptedUponLoading() const { return isCorruptedUponLoading_; }

Ret EngravingProject::checkCorrupted() const {
    TRACEFUNC;

    return masterScore_->sanityCheck();
}
