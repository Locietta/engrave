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

#include "writecontext.h"

#include "engraving/internal/engravingconfiguration.h"

using namespace mu::engraving;
using namespace mu::engraving::write;

EngravingConfiguration *WriteContext::configuration() const { return score_->configuration(); }

bool WriteContext::can_write(const EngravingItem *) const { return true; }

bool WriteContext::can_write_note_idx(size_t, size_t) const { return true; }

bool WriteContext::can_write_voice(track_idx_t) const { return true; }
