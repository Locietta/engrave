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

#include "imageStore.h"

#include "image.h"
#include "io/path.h"
#include "log.h"
#include "score.h"

using namespace mu;
using namespace muse;
using namespace muse::io;

namespace mu::engraving {
ImageStore image_store; // the global image store

//---------------------------------------------------------
//   ImageStoreItem
//---------------------------------------------------------

ImageStoreItem::ImageStoreItem(const std::string &type) : type_(type) {}

//---------------------------------------------------------
//   dereference
//    decrement usage count of image in score
//---------------------------------------------------------

void ImageStoreItem::dereference(Image *image) { references_.remove(image); }

//---------------------------------------------------------
//   reference
//    increment usage count of image in score
//---------------------------------------------------------

void ImageStoreItem::reference(Image *image) { references_.push_back(image); }

//---------------------------------------------------------
//   isUsed
//    check if item is used in score
//---------------------------------------------------------

bool ImageStoreItem::is_used(Score *score) const {
    for (Image *image : references_) {
        if (image->score() == score && image->ownership_parent()) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   hashName
//---------------------------------------------------------

std::string ImageStoreItem::hash_name() const {
    const char hex[17] = "0123456789abcdef";
    char p[33];
    for (int i = 0; i < 16; ++i) {
        p[i * 2] = hex[(hash_[i] >> 4) & 0xf];
        p[i * 2 + 1] = hex[hash_[i] & 0xf];
    }
    p[32] = 0;
    return std::string(p) + "." + type_;
}

//---------------------------------------------------------
//   toInt
//---------------------------------------------------------

inline static int to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    return c - 'a' + 10;
}

//---------------------------------------------------------
//   ~ImageStore
//---------------------------------------------------------

ImageStore::~ImageStore() { muse::delete_all(items_); }

//---------------------------------------------------------
//   getImage
//---------------------------------------------------------

ImageStoreItem *ImageStore::get_image(std::string name) const {
    name = muse::io::complete_basename(name).to_std_string();
    if (name.size() != 32) {
        return nullptr;
    }
    ByteArray hash(16);
    for (int i = 0; i < 16; ++i) {
        hash[i] = to_int(name.at(i * 2)) * 16 + to_int(name.at(i * 2 + 1));
    }
    for (ImageStoreItem *item : items_) {
        if (item->hash() == hash) {
            return item;
        }
    }
    LOGW() << "image not found: " << name;
    return nullptr;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

// The store's 16 byte content key: two FNV-1a passes with distinct offset bases.
// Upstream used an MD4 service that this engine does not provide; the key only
// deduplicates images and names their files in the archive.
static ByteArray content_key(const ByteArray &ba) {
    uint64_t a = 14695981039346656037ull;
    uint64_t b = 0x84222325cbf29ce4ull;
    for (size_t i = 0; i < ba.size(); ++i) {
        a = (a ^ ba.at(i)) * 1099511628211ull;
        b = (b ^ ba.at(i)) * 1099511628211ull;
    }
    uint8_t key[16];
    for (int i = 0; i < 8; ++i) {
        key[i] = static_cast<uint8_t>(a >> (8 * i));
        key[8 + i] = static_cast<uint8_t>(b >> (8 * i));
    }
    return ByteArray(key, sizeof(key));
}

ImageStoreItem *ImageStore::add(const std::string &name, const ByteArray &ba) {
    ByteArray hash = content_key(ba);
    for (ImageStoreItem *item : items_) {
        if (item->hash() == hash) {
            return item;
        }
    }
    ImageStoreItem *item = new ImageStoreItem(muse::io::suffix(name));
    item->set(ba, hash);
    items_.push_back(item);
    return item;
}

//---------------------------------------------------------
//   clearUnused
//---------------------------------------------------------

void ImageStore::clear_unused() {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](ImageStoreItem *i) {
                                    const bool remove = !i->is_used();
                                    if (remove) {
                                        delete i;
                                    }
                                    return remove;
                                }),
                 items_.end());
}
} // namespace mu::engraving
