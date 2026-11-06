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
#include "scoreaccess.h"

#include "dom/masterscore.h"
#include "style/defaultstyle.h"

using namespace mu::engraving;
using namespace mu::engraving::compat;

MasterScore *ScoreAccess::create_master_score(const EngravingServices &services) { return new MasterScore(services); }

MasterScore *ScoreAccess::create_master_score_with_base_style(const EngravingServices &services) {
    return new MasterScore(services, DefaultStyle::base_style());
}

MasterScore *ScoreAccess::create_master_score_with_default_style(const EngravingServices &services) {
    return new MasterScore(services, DefaultStyle::default_style());
}

MasterScore *ScoreAccess::create_master_score(const EngravingServices &services, const MStyle &style) {
    return new MasterScore(services, style);
}
