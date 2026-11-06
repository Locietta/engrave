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
#ifndef MU_ENGRAVING_IWRITER_H
#define MU_ENGRAVING_IWRITER_H

#include <memory>
#include <variant>
#include <vector>

#include "global/io/iodevice.h"
#include "global/types/bytearray.h"
#include "xmlwriter.h"

namespace mu::engraving {
class Score;
class EngravingItem;
class Segment;
class SelectionFilter;
struct SelectedSymbol;
} // namespace mu::engraving

namespace mu::engraving::write {
class WriteContext;
} // namespace mu::engraving::write

namespace mu::engraving::rw {
struct WriteInOutData;

class IWriter {
public:
    virtual ~IWriter() = default;

    virtual bool write_score(Score *score, muse::io::IODevice *device, WriteInOutData *out = nullptr) = 0;

    virtual muse::ByteArray write_staff_selection(Score *score, const SelectionFilter &filter, staff_idx_t staff_start,
                                                  staff_idx_t staff_end, const Fraction &tick_start, const Fraction &tick_end,
                                                  Segment *start_segment, Segment *end_segment) = 0;

    virtual muse::ByteArray write_symbol_list_selection(track_idx_t from_track, track_idx_t to_track,
                                                        const std::vector<SelectedSymbol> &symbols) = 0;

    using Supported = std::variant<std::monostate>;

    template <typename T>
    static void check_supported_static(T item) {
        if constexpr (std::is_same<T, const EngravingItem *>::value || std::is_same<T, EngravingItem *>::value) {
            // supported
        } else {
            Supported check(item);
            (void) check;
        }
    }

    template <typename T>
    void write_item(const T item, XmlWriter &xml) {
#ifndef NDEBUG
        check_supported_static(item);
#endif
        do_write_item(static_cast<const EngravingItem *>(item), xml);
    }

private:
    virtual void do_write_item(const EngravingItem *item, XmlWriter &xml) = 0;
    virtual void write_segments(XmlWriter &xml, write::WriteContext &ctx, track_idx_t st, track_idx_t et, Segment *sseg, Segment *eseg,
                                bool, bool) = 0;
};

using IWriterPtr = std::shared_ptr<IWriter>;
} // namespace mu::engraving::rw

#endif // MU_ENGRAVING_IWRITER_H
