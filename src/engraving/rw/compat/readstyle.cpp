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
#include "readstyle.h"

#include "types/constants.h"

#include "style/defaultstyle.h"
#include "style/style.h"

#include "rw/xmlreader.h"

#include "dom/masterscore.h"

#include "readchordlisthook.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving::compat;
using namespace mu::engraving;

static int readStyleDefaultsVersion(MasterScore *score, const ByteArray &scoreData, const String &completeBaseName) {
    XmlReader e(scoreData);
    e.setDocName(completeBaseName);

    while (!e.atEnd()) {
        e.readNext();
        if (e.name() == "defaultsVersion") {
            return e.readInt();
        }
    }

    return ReadStyleHook::styleDefaultByMscVersion(score->mscVersion());
}

ReadStyleHook::ReadStyleHook(Score *score, const ByteArray &scoreData, const String &completeBaseName)
    : score_(score), scoreData_(scoreData), completeBaseName_(completeBaseName) {}

int ReadStyleHook::styleDefaultByMscVersion(const int mscVer) {
    constexpr int LEGACY_MSC_VERSION_V400 = 400;
    constexpr int LEGACY_MSC_VERSION_V302 = 302;
    constexpr int LEGACY_MSC_VERSION_V3 = 301;
    constexpr int LEGACY_MSC_VERSION_V2 = 206;
    constexpr int LEGACY_MSC_VERSION_V1 = 114;

    if (mscVer > LEGACY_MSC_VERSION_V3 && mscVer < LEGACY_MSC_VERSION_V400) {
        return LEGACY_MSC_VERSION_V302;
    }
    if (mscVer > LEGACY_MSC_VERSION_V2 && mscVer <= LEGACY_MSC_VERSION_V3) {
        return LEGACY_MSC_VERSION_V3;
    }

    if (mscVer > LEGACY_MSC_VERSION_V1 && mscVer <= LEGACY_MSC_VERSION_V2) {
        return LEGACY_MSC_VERSION_V2;
    }

    if (mscVer <= LEGACY_MSC_VERSION_V1) {
        return LEGACY_MSC_VERSION_V1;
    }

    return Constants::MSC_VERSION;
}

void ReadStyleHook::setupDefaultStyle() {
    IF_ASSERT_FAILED(score_) { return; }

    int defaultsVersion = -1;
    if (score_->isMaster()) {
        defaultsVersion = readStyleDefaultsVersion(score_->masterScore(), scoreData_, completeBaseName_);
    } else {
        defaultsVersion = score_->masterScore()->style().defaultStyleVersion();
    }

    score_->setStyle(DefaultStyle::resolveStyleDefaults(defaultsVersion));
    score_->style().setDefaultStyleVersion(defaultsVersion);
}

void ReadStyleHook::setupDefaultStyle(Score *score) {
    IF_ASSERT_FAILED(!score->isMaster()) { return; }

    int defaultsVersion = score->masterScore()->style().defaultStyleVersion();
    score->setStyle(DefaultStyle::resolveStyleDefaults(defaultsVersion));
    score->style().setDefaultStyleVersion(defaultsVersion);
}

void ReadStyleHook::readStyleTag(XmlReader &e) { readStyleTag(score_, e); }

void ReadStyleHook::readStyleTag(Score *score, XmlReader &e) {
    ReadChordListHook clhook(score);
    score->style().read(e, &clhook, score->mscVersion());
}

bool ReadStyleHook::readStyleProperties(MStyle *style, XmlReader &e) { return style->readProperties(e); }
