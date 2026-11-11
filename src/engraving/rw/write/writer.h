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

#include "../compat/writescorehook.h"
#include "../iwriter.h"
#include "writecontext.h"

namespace mu::engraving::write {
struct Writer : rw::IWriter {

    Writer();

    bool write_score(Score *score, muse::io::IODevice *device, rw::WriteInOutData *out) override;

    static void write(Score *score, XmlWriter &xml, WriteContext &ctx, compat::WriteScoreHook &hook);



private:
    void write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t st, track_idx_t et, Segment *sseg, Segment *eseg, bool,
                        bool) override;
    void do_write_item(const EngravingItem *item, XmlWriter &xml) override;
};
} // namespace mu::engraving::write
