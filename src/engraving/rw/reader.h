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

#include <memory>
#include <type_traits>
#include <variant>

#include "../types/types.h"
#include "types/ret.h"
#include "xmlreader.h"

namespace mu::engraving {
struct Score;
struct EngravingItem;

struct Accidental;
struct ChordRest;
struct Harmony;
struct Segment;
struct Spanner;
struct StaffTypeChange;
struct Symbol;
struct Text;
struct Tuplet;

struct Chord;
struct TremoloSingleChord;
struct TremoloTwoChord;
} // namespace mu::engraving

namespace mu::engraving::read {
struct ReadContext;
} // namespace mu::engraving::read

namespace mu::engraving::compat {
struct TremoloCompat;
} // namespace mu::engraving::compat

namespace mu::engraving::rw {
struct ReadInOutData;

struct Reader {
    explicit Reader(int format_version) : format_version_(format_version) {}

    muse::Ret read_score_file(Score *score, XmlReader &xml, ReadInOutData *out);

    using Supported =
        std::variant<std::monostate, Accidental *, ChordRest *, Harmony *, Spanner *, StaffTypeChange *, Symbol *, Text *, Tuplet *>;

    template <typename T>
    static void check_supported_static(T item) {
        if constexpr (!std::is_same_v<T, EngravingItem *>) {
            [[maybe_unused]] Supported check(item);
        }
    }

    template <typename T>
    void read_item(T item, XmlReader &xml) {
#ifndef NDEBUG
        check_supported_static(item);
#endif
        do_read_item(static_cast<EngravingItem *>(item), xml);
    }

    void read_tremolo_compat(engraving::compat::TremoloCompat *item, XmlReader &xml);

private:
    static bool read_score_tag(Score *score, XmlReader &xml, read::ReadContext &ctx);
    void do_read_item(EngravingItem *item, XmlReader &xml);

    int format_version_;
};

using ReaderPtr = std::shared_ptr<Reader>;
} // namespace mu::engraving::rw
