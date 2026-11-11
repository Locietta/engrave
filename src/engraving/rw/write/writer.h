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
#include <vector>

#include "../compat/writescorehook.h"
#include "engraving/rw/xmlwriter.h"
#include "global/io/iodevice.h"
#include "global/types/bytearray.h"
#include "writecontext.h"

namespace mu::engraving {
struct Score;
struct EngravingItem;
struct Segment;
} // namespace mu::engraving

namespace mu::engraving::write {
struct WriteContext;
} // namespace mu::engraving::write

namespace mu::engraving::rw {
struct WriteInOutData;
} // namespace mu::engraving::rw

namespace mu::engraving::write {

struct Writer {
    using Supported = std::variant<std::monostate>;

    template <typename T>
    static void check_supported_static(T item) {
        if constexpr (std::is_same<T, const EngravingItem *>::value || std::is_same<T, EngravingItem *>::value) {
            // supported
        } else {
            [[maybe_unused]] Supported check(item);
        }
    }

    template <typename T>
    void write_item(const T item, XmlWriter &xml) {
#ifndef NDEBUG
        check_supported_static(item);
#endif
        do_write_item(static_cast<const EngravingItem *>(item), xml);
    }

    Writer();

    bool write_score(Score *score, muse::io::IODevice *device, rw::WriteInOutData *out = nullptr);

    static void write(Score *score, XmlWriter &xml, WriteContext &ctx, compat::WriteScoreHook &hook);

private:
    void write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t st, track_idx_t et, Segment *sseg, Segment *eseg, bool, bool);
    void do_write_item(const EngravingItem *item, XmlWriter &xml);
};
} // namespace mu::engraving::write
