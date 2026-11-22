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

#include "dom/masterscore.h"
#include "log.h"
#include "readchordlisthook.h"
#include "rw/xmlreader.h"
#include "style/defaultstyle.h"
#include "style/style.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving::compat;
using namespace mu::engraving;

void ReadStyleHook::setup_default_style(Score *score) {
    IF_ASSERT_FAILED(!score->is_master()) { return; }

    int defaults_version = score->master_score()->style().default_style_version();
    score->set_style(DefaultStyle::resolve_style_defaults(defaults_version));
    score->style().set_default_style_version(defaults_version);
}

void ReadStyleHook::read_style_tag(Score *score, XmlReader &e) {
    ReadChordListHook clhook(score);
    score->style().read(e, &clhook, score->msc_version());
}

bool ReadStyleHook::read_style_properties(MStyle *style, XmlReader &e) { return style->read_properties(e); }
