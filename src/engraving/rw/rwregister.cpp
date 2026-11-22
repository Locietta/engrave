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
#include "rwregister.h"

#include "types/constants.h"
#include "write/writer.h"

using namespace mu::engraving;
using namespace mu::engraving::rw;

ReaderPtr RWRegister::reader(int version) {
    if (version < 0) {
        version = Constants::msc_version;
    }

    if (version < 460) {
        return nullptr;
    }

    return std::make_shared<Reader>(version);
}

std::unique_ptr<write::Writer> RWRegister::writer() { return std::make_unique<write::Writer>(); }
