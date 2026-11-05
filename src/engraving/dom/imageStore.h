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

#include <list>
#include <string>

#include "types/string.h"
#include "types/bytearray.h"
#include "io/path.h"

namespace mu::engraving {
class Image;
class Score;

//---------------------------------------------------------
//   ImageStoreItem
//---------------------------------------------------------

class ImageStoreItem {
public:
    ImageStoreItem(const std::string &p);
    void dereference(Image *);
    void reference(Image *);

    muse::ByteArray &buffer() { return buffer_; }

    const muse::ByteArray &buffer() const { return buffer_; }

    bool loaded() const { return !buffer_.empty(); }

    bool isUsed(Score *) const;

    bool isUsed() const { return !references_.empty(); }

    std::string hashName() const;

    const muse::ByteArray &hash() const { return hash_; }

    const std::string &type() const { return type_; }

    void set(const muse::ByteArray &b, const muse::ByteArray &h) {
        buffer_ = b;
        hash_ = h;
    }

private:

    std::list<Image *> references_;
    std::string type_; // image type (file extension)
    muse::ByteArray buffer_;
    muse::ByteArray hash_; // 16 byte content key of buffer_
};

//---------------------------------------------------------
//   ImageStore
//---------------------------------------------------------

class ImageStore {
public:
    ImageStore() = default;
    ImageStore(const ImageStore &) = delete;
    ImageStore &operator=(const ImageStore &) = delete;
    ~ImageStore();

    ImageStoreItem *getImage(std::string name) const;
    ImageStoreItem *add(const std::string &name, const muse::ByteArray &);
    void clearUnused();

    typedef std::vector<ImageStoreItem *> ItemList;
    typedef ItemList::iterator iterator;
    typedef ItemList::const_iterator const_iterator;

    iterator begin() { return items_.begin(); }

    const_iterator begin() const { return items_.begin(); }

    iterator end() { return items_.end(); }

    const_iterator end() const { return items_.end(); }

private:

    ItemList items_;
};

extern ImageStore imageStore; // this is the global imageStore
} // namespace mu::engraving
