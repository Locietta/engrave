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

namespace mu::engraving::compat {
struct TremoloCompat;
} // namespace mu::engraving::compat

namespace mu::engraving::rw {
struct ReadInOutData;

struct IReader {
    virtual ~IReader() = default;

    virtual muse::Ret read_score_file(Score *score, XmlReader &xml, rw::ReadInOutData *out) = 0;

    using Supported =
        std::variant<std::monostate, Accidental *, ChordRest *, Harmony *, Spanner *, StaffTypeChange *, Symbol *, Text *, Tuplet *>;

    template <typename T>
    static void check_supported_static(T item) {
        if constexpr (std::is_same<T, EngravingItem *>::value) {
            // supported
        } else {
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

    //! NOTE Needs refactoring - reading should be separated from insertion
    //! (we read the elements into some structure, then inserted them)

    // compat
    virtual void read_tremolo_compat(engraving::compat::TremoloCompat *item, XmlReader &xml) = 0;

private:
    virtual void do_read_item(EngravingItem *item, XmlReader &xml) = 0;
};

using IReaderPtr = std::shared_ptr<IReader>;
} // namespace mu::engraving::rw
