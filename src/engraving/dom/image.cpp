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

#include "../editing/editdata.h"
#include "draw/iimageprovider.h" // IWYU pragma: keep
#include "draw/svgrenderer.h"
#include "draw/types/pixmap.h"
#include "imageStore.h"
#include "io/file.h"
#include "io/fileinfo.h"
#include "log.h"
#include "masterscore.h"
#include "mscore.h"
#include "score.h"

using namespace mu;
using namespace muse::io;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   propertyList
//---------------------------------------------------------

static bool default_auto_scale = false;
static bool default_lock_aspect_ratio = true;
static bool default_size_is_spatium = true;

//---------------------------------------------------------
//   Image
//---------------------------------------------------------

Image::Image(DummyParentOr<EngravingItem> parent) : BSymbol(ElementType::IMAGE, parent, ElementFlag::MOVABLE) {
    image_type_ = ImageType::NONE;
    size_ = SizeF(0.0, 0.0);
    store_item_ = 0;
    dirty_ = false;
    lock_aspect_ratio_ = default_lock_aspect_ratio;
    auto_scale_ = default_auto_scale;
    size_is_spatium_ = default_size_is_spatium;
}

Image::Image(const Image &img) : BSymbol(img) {
    image_type_ = img.image_type_;
    buffer_ = img.buffer_;
    size_ = img.size_;
    lock_aspect_ratio_ = img.lock_aspect_ratio_;
    auto_scale_ = img.auto_scale_;
    dirty_ = img.dirty_;
    store_item_ = img.store_item_;
    size_is_spatium_ = img.size_is_spatium_;
    if (store_item_) {
        store_item_->reference(this);
    }
    if (image_type_ == ImageType::RASTER) {
        raster_doc_ = img.raster_doc_ ? std::make_shared<Pixmap>(*img.raster_doc_) : nullptr;
    } else if (image_type_ == ImageType::SVG) {
        svg_doc_ = img.svg_doc_ ? new SvgRenderer(store_item_->buffer()) : 0;
    }
    set_z(img.z());
}

//---------------------------------------------------------
//   Image
//---------------------------------------------------------

Image::~Image() {
    if (store_item_) {
        store_item_->dereference(this);
    }
    if (image_type_ == ImageType::SVG) {
        delete svg_doc_;
    }
}

//---------------------------------------------------------
//   setImageType
//---------------------------------------------------------

void Image::set_image_type(ImageType t) {
    image_type_ = t;
    if (image_type_ == ImageType::SVG) {
        svg_doc_ = 0;
    } else if (image_type_ == ImageType::RASTER) {
        raster_doc_.reset();
    } else {
        LOGD("illegal image type");
    }
}

//---------------------------------------------------------
//   imageSize
//---------------------------------------------------------

muse::SizeF Image::image_size() const {
    if (!is_valid()) {
        return muse::SizeF();
    }

    if (image_type_ == ImageType::RASTER) {
        muse::Size raster_size = raster_doc_->size();
        return muse::SizeF(raster_size.width(), raster_size.height());
    }

    return svg_doc_->default_size();
}

//---------------------------------------------------------
//   isImageFramed
//---------------------------------------------------------

bool Image::is_image_framed() const {
    if (!ownership_parent()) {
        return false;
    }

    return ownership_parent()->is_box();
}

//---------------------------------------------------------
//   imageAspectRatio
//---------------------------------------------------------

double Image::image_aspect_ratio() const { return size_.width() / size_.height(); }

//---------------------------------------------------------
//   setImageHeight
//---------------------------------------------------------

void Image::update_image_height(const double &height) {
    double aspect_ratio = image_aspect_ratio();

    size_.set_height(height);

    if (lock_aspect_ratio_) {
        size_.set_width(height * aspect_ratio);
    }
}

//---------------------------------------------------------
//   setImageWidth
//---------------------------------------------------------

void Image::update_image_width(const double &width) {
    double aspect_ratio = image_aspect_ratio();

    size_.set_width(width);

    if (lock_aspect_ratio_) {
        size_.set_height(width / aspect_ratio);
    }
}

//---------------------------------------------------------
//   imageHeight
//---------------------------------------------------------

double Image::image_height() const { return size_.height(); }

//---------------------------------------------------------
//   imageWidth
//---------------------------------------------------------

double Image::image_width() const { return size_.width(); }

bool Image::load_from_store(const std::string &store_path) {
    store_path_ = store_path;
    if (store_path.empty()) {
        return false;
    }

    store_item_ = image_store.get_image(store_path);
    if (!store_item_) {
        return false;
    }
    store_item_->reference(this);

    std::string suffix = muse::io::suffix(store_path);
    if (suffix == "svg" || suffix == "svgz") {
        set_image_type(ImageType::SVG);
    } else {
        set_image_type(ImageType::RASTER);
    }

    return true;
}

//---------------------------------------------------------
//   loadFromFile
//    load image from file and put into ImageStore
//    return true on success
//---------------------------------------------------------

bool Image::load_from_file(muse::io::PathT path) {
    if (path.empty()) {
        return false;
    }

    if (FileInfo(path).is_relative()) {
        // if file path is relative, prepend score path
        path = master_score()->file_info()->absolute_dir_path() + '/' + path;
    }

    File f(path);
    if (!f.open(IODevice::READ_ONLY)) {
        LOGD() << "failed load file: " << path;
        return false;
    }
    muse::ByteArray ba = f.read_all();
    f.close();

    store_item_ = image_store.add(path.to_std_string(), ba);
    store_item_->reference(this);
    if (path.has_suffix("svg") || path.has_suffix("svgz")) {
        set_image_type(ImageType::SVG);
    } else {
        set_image_type(ImageType::RASTER);
    }
    return true;
}

//---------------------------------------------------------
//   loadFromData
//    load image from data and put into ImageStore
//    return true on success
//---------------------------------------------------------

bool Image::load_from_data(const std::string &name, const muse::ByteArray &ba) {
    store_item_ = image_store.add(name, ba);
    store_item_->reference(this);

    muse::io::PathT io_path(name);
    if (io_path.has_suffix("svg") || io_path.has_suffix("svgz")) {
        set_image_type(ImageType::SVG);
    } else {
        set_image_type(ImageType::RASTER);
    }
    return true;
}

//---------------------------------------------------------
//   pixel2Size
//---------------------------------------------------------

SizeF Image::pixel2size(const SizeF &s) const { return s / (size_is_spatium_ ? spatium() : ::mu::engraving::dpmm); }

//---------------------------------------------------------
//   size2pixel
//---------------------------------------------------------

SizeF Image::size2pixel(const SizeF &s) const { return s * (size_is_spatium_ ? spatium() : ::mu::engraving::dpmm); }

void Image::init() {
    if (image_type_ == ImageType::SVG && !svg_doc_) {
        if (store_item_) {
            svg_doc_ = new SvgRenderer(store_item_->buffer());
        }
    } else if (image_type_ == ImageType::RASTER && !raster_doc_) {
        if (store_item_) {
            raster_doc_ = score()->services().image_provider->create_pixmap(store_item_->buffer());
            if (!raster_doc_->is_null()) {
                dirty_ = true;
            }
        }
    }
    if (size_.is_null()) {
        size_ = pixel2size(image_size());
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Image::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::AUTOSCALE: return auto_scale();
        case Pid::SIZE: return PropertyValue::from_value(size());
        case Pid::IMAGE_HEIGHT: return image_height();
        case Pid::IMAGE_WIDTH: return image_width();
        case Pid::IMAGE_FRAMED: return is_image_framed();
        case Pid::LOCK_ASPECT_RATIO: return lock_aspect_ratio();
        case Pid::SIZE_IS_SPATIUM: return size_is_spatium();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Image::set_property(Pid property_id, const PropertyValue &v) {
    bool rv = true;
    score()->add_refresh(canvas_bounding_rect());
    switch (property_id) {
        case Pid::AUTOSCALE: set_auto_scale(v.to_bool()); break;
        case Pid::SIZE: set_size(v.value<SizeF>()); break;
        case Pid::IMAGE_HEIGHT: update_image_height(v.to_double()); break;
        case Pid::IMAGE_WIDTH: update_image_width(v.to_double()); break;
        case Pid::IMAGE_FRAMED: break;
        case Pid::LOCK_ASPECT_RATIO: set_lock_aspect_ratio(v.to_bool()); break;
        case Pid::SIZE_IS_SPATIUM: {
            SizeF s = size2pixel(size_);
            set_size_is_spatium(v.to_bool());
            size_ = pixel2size(s);
        } break;
        default: rv = EngravingItem::set_property(property_id, v); break;
    }
    set_generated(false);
    dirty_ = true;
    trigger_layout();
    return rv;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Image::property_default(Pid id) const {
    switch (id) {
        case Pid::AUTOSCALE: return default_auto_scale;
        case Pid::SIZE: return PropertyValue::from_value(pixel2size(image_size()));
        case Pid::IMAGE_HEIGHT: return pixel2size(image_size()).height();
        case Pid::IMAGE_WIDTH: return pixel2size(image_size()).width();
        case Pid::IMAGE_FRAMED: return is_image_framed();
        case Pid::LOCK_ASPECT_RATIO: return default_lock_aspect_ratio;
        case Pid::SIZE_IS_SPATIUM: return default_size_is_spatium;
        default: return EngravingItem::property_default(id);
    }
}
} // namespace mu::engraving
