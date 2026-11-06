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

static int read_style_defaults_version(MasterScore *score, const ByteArray &score_data, const String &complete_base_name) {
    XmlReader e(score_data);
    e.set_doc_name(complete_base_name);

    while (!e.at_end()) {
        e.read_next();
        if (e.name() == "defaultsVersion") {
            return e.read_int();
        }
    }

    return ReadStyleHook::style_default_by_msc_version(score->msc_version());
}

ReadStyleHook::ReadStyleHook(Score *score, const ByteArray &score_data, const String &complete_base_name)
    : score_(score), score_data_(score_data), complete_base_name_(complete_base_name) {}

int ReadStyleHook::style_default_by_msc_version(const int msc_ver) {
    constexpr int legacy_msc_version_v400 = 400;
    constexpr int legacy_msc_version_v302 = 302;
    constexpr int legacy_msc_version_v3 = 301;
    constexpr int legacy_msc_version_v2 = 206;
    constexpr int legacy_msc_version_v1 = 114;

    if (msc_ver > legacy_msc_version_v3 && msc_ver < legacy_msc_version_v400) {
        return legacy_msc_version_v302;
    }
    if (msc_ver > legacy_msc_version_v2 && msc_ver <= legacy_msc_version_v3) {
        return legacy_msc_version_v3;
    }

    if (msc_ver > legacy_msc_version_v1 && msc_ver <= legacy_msc_version_v2) {
        return legacy_msc_version_v2;
    }

    if (msc_ver <= legacy_msc_version_v1) {
        return legacy_msc_version_v1;
    }

    return Constants::msc_version;
}

void ReadStyleHook::setup_default_style() {
    IF_ASSERT_FAILED(score_) { return; }

    int defaults_version = -1;
    if (score_->is_master()) {
        defaults_version = read_style_defaults_version(score_->master_score(), score_data_, complete_base_name_);
    } else {
        defaults_version = score_->master_score()->style().default_style_version();
    }

    score_->set_style(DefaultStyle::resolve_style_defaults(defaults_version));
    score_->style().set_default_style_version(defaults_version);
}

void ReadStyleHook::setup_default_style(Score *score) {
    IF_ASSERT_FAILED(!score->is_master()) { return; }

    int defaults_version = score->master_score()->style().default_style_version();
    score->set_style(DefaultStyle::resolve_style_defaults(defaults_version));
    score->style().set_default_style_version(defaults_version);
}

void ReadStyleHook::read_style_tag(XmlReader &e) { read_style_tag(score_, e); }

void ReadStyleHook::read_style_tag(Score *score, XmlReader &e) {
    ReadChordListHook clhook(score);
    score->style().read(e, &clhook, score->msc_version());
}

bool ReadStyleHook::read_style_properties(MStyle *style, XmlReader &e) { return style->read_properties(e); }
