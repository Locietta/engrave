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

#include "draw/types/pixmap.h"

#include "bsymbol.h"

namespace muse::draw {
class IImageProvider;
class Pixmap;
class SvgRenderer;
} // namespace muse::draw

namespace muse::io {
struct path_t;
} // namespace muse::io

namespace mu::engraving {
class ImageStoreItem;

enum class ImageType : char { NONE, RASTER, SVG };

//---------------------------------------------------------
//   @@ Image
//---------------------------------------------------------

class Image final : public BSymbol {
    OBJECT_ALLOCATOR(engraving, Image)
    DECLARE_CLASSOF(ElementType::IMAGE)

public:
    Image(DummyParentOr<EngravingItem> parent);
    Image(const Image &);
    ~Image();

    Image *clone() const override { return new Image(*this); }

    bool loadFromStore(const std::string &storePath);
    bool loadFromFile(muse::io::path_t path);
    bool loadFromData(const std::string &suffix, const muse::ByteArray &);

    void init();

    bool isImageFramed() const;
    double imageAspectRatio() const;

    void setSize(const SizeF &s) { size_ = s; }

    const SizeF &size() const { return size_; }

    void updateImageHeight(const double &height);
    void updateImageWidth(const double &width);
    double imageHeight() const;
    double imageWidth() const;

    bool lockAspectRatio() const { return lockAspectRatio_; }

    void setLockAspectRatio(bool v) { lockAspectRatio_ = v; }

    bool autoScale() const { return autoScale_; }

    void setAutoScale(bool v) { autoScale_ = v; }

    ImageStoreItem *storeItem() const { return storeItem_; }

    bool sizeIsSpatium() const { return sizeIsSpatium_; }

    void setSizeIsSpatium(bool val) { sizeIsSpatium_ = val; }

    const std::string &storePath() const { return storePath_; }

    void setStorePath(const std::string &p) { storePath_ = p; }

    PropertyValue getProperty(Pid) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    SizeF imageSize() const;

    void setImageType(ImageType);

    ImageType imageType() const { return imageType_; }

    bool isValid() const { return rasterDoc_ || svgDoc_; }

    muse::draw::SvgRenderer *svgRenderer() const { return svgDoc_; }

    const std::shared_ptr<muse::draw::Pixmap> &rasterImage() const { return rasterDoc_; }

    SizeF pixel2size(const SizeF &s) const;
    SizeF size2pixel(const SizeF &s) const;

    const muse::draw::Pixmap &buffer() const { return buffer_; }

    void setBuffer(const muse::draw::Pixmap &p) const { buffer_ = p; }

    bool dirty() const { return dirty_; }

    void setDirty(bool val) const { dirty_ = val; }

private:

    ImageStoreItem *storeItem_ = nullptr;
    std::string storePath_;             // the path of the img in the ImageStore
    mutable muse::draw::Pixmap buffer_; // cached rendering
    SizeF size_;                        // in absolute or spatium units
    bool lockAspectRatio_ = false;
    bool autoScale_ = false; // fill parent frame
    bool sizeIsSpatium_ = false;
    mutable bool dirty_ = false;

    std::shared_ptr<muse::draw::Pixmap> rasterDoc_;
    muse::draw::SvgRenderer *svgDoc_ = nullptr;

    ImageType imageType_ = ImageType::NONE;
};
} // namespace mu::engraving
