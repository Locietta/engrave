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

#include "image.h"

#include "io/file.h"
#include "io/fileinfo.h"

#include "draw/iimageprovider.h" // IWYU pragma: keep
#include "draw/types/pixmap.h"
#include "draw/svgrenderer.h"

#include "../editing/editdata.h"
#include "../editing/elementeditdata.h"

#include "imageStore.h"
#include "masterscore.h"
#include "mscore.h"
#include "score.h"

#include "log.h"

using namespace mu;
using namespace muse::io;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   propertyList
//---------------------------------------------------------

static bool defaultAutoScale = false;
static bool defaultLockAspectRatio = true;
static bool defaultSizeIsSpatium = true;

//---------------------------------------------------------
//   Image
//---------------------------------------------------------

Image::Image(DummyParentOr<EngravingItem> parent) : BSymbol(ElementType::IMAGE, parent, ElementFlag::MOVABLE) {
    imageType_ = ImageType::NONE;
    size_ = SizeF(0.0, 0.0);
    storeItem_ = 0;
    dirty_ = false;
    lockAspectRatio_ = defaultLockAspectRatio;
    autoScale_ = defaultAutoScale;
    sizeIsSpatium_ = defaultSizeIsSpatium;
}

Image::Image(const Image &img) : BSymbol(img) {
    imageType_ = img.imageType_;
    buffer_ = img.buffer_;
    size_ = img.size_;
    lockAspectRatio_ = img.lockAspectRatio_;
    autoScale_ = img.autoScale_;
    dirty_ = img.dirty_;
    storeItem_ = img.storeItem_;
    sizeIsSpatium_ = img.sizeIsSpatium_;
    if (storeItem_) {
        storeItem_->reference(this);
    }
    if (imageType_ == ImageType::RASTER) {
        rasterDoc_ = img.rasterDoc_ ? std::make_shared<Pixmap>(*img.rasterDoc_) : nullptr;
    } else if (imageType_ == ImageType::SVG) {
        svgDoc_ = img.svgDoc_ ? new SvgRenderer(storeItem_->buffer()) : 0;
    }
    setZ(img.z());
}

//---------------------------------------------------------
//   Image
//---------------------------------------------------------

Image::~Image() {
    if (storeItem_) {
        storeItem_->dereference(this);
    }
    if (imageType_ == ImageType::SVG) {
        delete svgDoc_;
    }
}

//---------------------------------------------------------
//   setImageType
//---------------------------------------------------------

void Image::setImageType(ImageType t) {
    imageType_ = t;
    if (imageType_ == ImageType::SVG) {
        svgDoc_ = 0;
    } else if (imageType_ == ImageType::RASTER) {
        rasterDoc_.reset();
    } else {
        LOGD("illegal image type");
    }
}

//---------------------------------------------------------
//   imageSize
//---------------------------------------------------------

muse::SizeF Image::imageSize() const {
    if (!isValid()) {
        return muse::SizeF();
    }

    if (imageType_ == ImageType::RASTER) {
        muse::Size rasterSize = rasterDoc_->size();
        return muse::SizeF(rasterSize.width(), rasterSize.height());
    }

    return svgDoc_->defaultSize();
}

//---------------------------------------------------------
//   isImageFramed
//---------------------------------------------------------

bool Image::isImageFramed() const {
    if (!ownershipParent()) {
        return false;
    }

    return ownershipParent()->isBox();
}

//---------------------------------------------------------
//   imageAspectRatio
//---------------------------------------------------------

double Image::imageAspectRatio() const { return size_.width() / size_.height(); }

//---------------------------------------------------------
//   setImageHeight
//---------------------------------------------------------

void Image::updateImageHeight(const double &height) {
    double aspectRatio = imageAspectRatio();

    size_.setHeight(height);

    if (lockAspectRatio_) {
        size_.setWidth(height * aspectRatio);
    }
}

//---------------------------------------------------------
//   setImageWidth
//---------------------------------------------------------

void Image::updateImageWidth(const double &width) {
    double aspectRatio = imageAspectRatio();

    size_.setWidth(width);

    if (lockAspectRatio_) {
        size_.setHeight(width / aspectRatio);
    }
}

//---------------------------------------------------------
//   imageHeight
//---------------------------------------------------------

double Image::imageHeight() const { return size_.height(); }

//---------------------------------------------------------
//   imageWidth
//---------------------------------------------------------

double Image::imageWidth() const { return size_.width(); }

bool Image::loadFromStore(const std::string &storePath) {
    storePath_ = storePath;
    if (storePath.empty()) {
        return false;
    }

    storeItem_ = imageStore.getImage(storePath);
    if (!storeItem_) {
        return false;
    }
    storeItem_->reference(this);

    std::string suffix = muse::io::suffix(storePath);
    if (suffix == "svg" || suffix == "svgz") {
        setImageType(ImageType::SVG);
    } else {
        setImageType(ImageType::RASTER);
    }

    return true;
}

//---------------------------------------------------------
//   loadFromFile
//    load image from file and put into ImageStore
//    return true on success
//---------------------------------------------------------

bool Image::loadFromFile(muse::io::path_t path) {
    if (path.empty()) {
        return false;
    }

    if (FileInfo(path).isRelative()) {
        // if file path is relative, prepend score path
        path = masterScore()->fileInfo()->absoluteDirPath() + '/' + path;
    }

    File f(path);
    if (!f.open(IODevice::ReadOnly)) {
        LOGD() << "failed load file: " << path;
        return false;
    }
    muse::ByteArray ba = f.readAll();
    f.close();

    storeItem_ = imageStore.add(path.toStdString(), ba);
    storeItem_->reference(this);
    if (path.hasSuffix("svg") || path.hasSuffix("svgz")) {
        setImageType(ImageType::SVG);
    } else {
        setImageType(ImageType::RASTER);
    }
    return true;
}

//---------------------------------------------------------
//   loadFromData
//    load image from data and put into ImageStore
//    return true on success
//---------------------------------------------------------

bool Image::loadFromData(const std::string &name, const muse::ByteArray &ba) {
    storeItem_ = imageStore.add(name, ba);
    storeItem_->reference(this);

    muse::io::path_t ioPath(name);
    if (ioPath.hasSuffix("svg") || ioPath.hasSuffix("svgz")) {
        setImageType(ImageType::SVG);
    } else {
        setImageType(ImageType::RASTER);
    }
    return true;
}

//---------------------------------------------------------
//   pixel2Size
//---------------------------------------------------------

SizeF Image::pixel2size(const SizeF &s) const { return s / (sizeIsSpatium_ ? spatium() : DPMM); }

//---------------------------------------------------------
//   size2pixel
//---------------------------------------------------------

SizeF Image::size2pixel(const SizeF &s) const { return s * (sizeIsSpatium_ ? spatium() : DPMM); }

void Image::init() {
    if (imageType_ == ImageType::SVG && !svgDoc_) {
        if (storeItem_) {
            svgDoc_ = new SvgRenderer(storeItem_->buffer());
        }
    } else if (imageType_ == ImageType::RASTER && !rasterDoc_) {
        if (storeItem_) {
            rasterDoc_ = score()->services().imageProvider->createPixmap(storeItem_->buffer());
            if (!rasterDoc_->isNull()) {
                dirty_ = true;
            }
        }
    }
    if (size_.isNull()) {
        size_ = pixel2size(imageSize());
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Image::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::AUTOSCALE: return autoScale();
        case Pid::SIZE: return PropertyValue::fromValue(size());
        case Pid::IMAGE_HEIGHT: return imageHeight();
        case Pid::IMAGE_WIDTH: return imageWidth();
        case Pid::IMAGE_FRAMED: return isImageFramed();
        case Pid::LOCK_ASPECT_RATIO: return lockAspectRatio();
        case Pid::SIZE_IS_SPATIUM: return sizeIsSpatium();
        default: return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Image::setProperty(Pid propertyId, const PropertyValue &v) {
    bool rv = true;
    score()->addRefresh(canvasBoundingRect());
    switch (propertyId) {
        case Pid::AUTOSCALE: setAutoScale(v.toBool()); break;
        case Pid::SIZE: setSize(v.value<SizeF>()); break;
        case Pid::IMAGE_HEIGHT: updateImageHeight(v.toDouble()); break;
        case Pid::IMAGE_WIDTH: updateImageWidth(v.toDouble()); break;
        case Pid::IMAGE_FRAMED: break;
        case Pid::LOCK_ASPECT_RATIO: setLockAspectRatio(v.toBool()); break;
        case Pid::SIZE_IS_SPATIUM: {
            SizeF s = size2pixel(size_);
            setSizeIsSpatium(v.toBool());
            size_ = pixel2size(s);
        } break;
        default: rv = EngravingItem::setProperty(propertyId, v); break;
    }
    setGenerated(false);
    dirty_ = true;
    triggerLayout();
    return rv;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Image::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::AUTOSCALE: return defaultAutoScale;
        case Pid::SIZE: return PropertyValue::fromValue(pixel2size(imageSize()));
        case Pid::IMAGE_HEIGHT: return pixel2size(imageSize()).height();
        case Pid::IMAGE_WIDTH: return pixel2size(imageSize()).width();
        case Pid::IMAGE_FRAMED: return isImageFramed();
        case Pid::LOCK_ASPECT_RATIO: return defaultLockAspectRatio;
        case Pid::SIZE_IS_SPATIUM: return defaultSizeIsSpatium;
        default: return EngravingItem::propertyDefault(id);
    }
}
} // namespace mu::engraving
