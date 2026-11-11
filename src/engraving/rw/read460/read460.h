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

#include "../ireader.h"

namespace mu::engraving {
struct Score;
struct XmlReader;
} // namespace mu::engraving

namespace mu::engraving::read460 {
struct ReadContext;

struct Read460 : rw::IReader {

    muse::Ret read_score_file(Score *score, XmlReader &e, rw::ReadInOutData *data) override;

    static bool read_score_tag(Score *score, XmlReader &e, ReadContext &ctx);

    void read_tremolo_compat(compat::TremoloCompat *item, XmlReader &xml) override;

private:
    bool prepare_paste_duration_element(Score *score, const Fraction &tick, const Fraction &ticks, const track_idx_t track);
    void do_read_item(EngravingItem *item, XmlReader &xml) override;
};
} // namespace mu::engraving::read460
