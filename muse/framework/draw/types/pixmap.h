/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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
#ifndef MUSE_DRAW_PIXMAP_H
#define MUSE_DRAW_PIXMAP_H

#include "global/types/bytearray.h"

#include "geometry.h"

namespace muse::draw {
class Pixmap {
public:

    Pixmap() = default;

    Pixmap(Size size) : size_(size) {}

    Pixmap(const ByteArray &data, Size size) : size_(size) { set_data(data); }

    Pixmap(int w, int h) : size_(Size(w, h)) {}

    int width() const { return size_.width(); }

    int height() const { return size_.height(); }

    Size size() const { return size_; }

    ByteArray data() const { return data_; }

    bool is_null() const { return data_.empty(); }

    unsigned int key() const { return key_; }

    bool operator==(const Pixmap &o) const { return size_ == o.size_ && key_ == o.key_ && data_ == o.data_; }

    bool operator!=(const Pixmap &o) const { return !this->operator==(o); }


private:

    inline unsigned int do_key(const uint8_t *p, size_t len) const {
        unsigned int h = 0;
        for (size_t i = 0; i < len; ++i) {
            h = 31 * h + p[i];
        }
        return h;
    }

    void set_data(const ByteArray &data) {
        data_ = data;
        key_ = do_key(data.const_data(), data.size());
    }

    Size size_;
    ByteArray data_; //! usually png
    unsigned int key_ = 0;
};
} // namespace muse::draw

#endif // MUSE_DRAW_PIXMAP_H
