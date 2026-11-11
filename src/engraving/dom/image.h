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

#include "bsymbol.h"
#include "draw/types/pixmap.h"

namespace muse::draw {

struct Pixmap;
struct SvgRenderer;
} // namespace muse::draw

namespace muse::io {
struct PathT;
} // namespace muse::io

namespace mu::engraving {
struct ImageStoreItem;

enum class ImageType : char { NONE, RASTER, SVG };

//---------------------------------------------------------
//   @@ Image
//---------------------------------------------------------

struct Image final : BSymbol {
private:
    DECLARE_CLASSOF(ElementType::IMAGE)

public:
    Image(DummyParentOr<EngravingItem> parent);
    Image(const Image &);
    ~Image();

    Image *clone() const override { return new Image(*this); }

    bool load_from_store(const std::string &store_path);
    bool load_from_file(muse::io::PathT path);
    bool load_from_data(const std::string &suffix, const muse::ByteArray &);

    void init();

    bool is_image_framed() const;
    double image_aspect_ratio() const;

    void set_size(const SizeF &s) { size_ = s; }

    const SizeF &size() const { return size_; }

    void update_image_height(const double &height);
    void update_image_width(const double &width);
    double image_height() const;
    double image_width() const;

    bool lock_aspect_ratio() const { return lock_aspect_ratio_; }

    void set_lock_aspect_ratio(bool v) { lock_aspect_ratio_ = v; }

    bool auto_scale() const { return auto_scale_; }

    void set_auto_scale(bool v) { auto_scale_ = v; }

    ImageStoreItem *store_item() const { return store_item_; }

    bool size_is_spatium() const { return size_is_spatium_; }

    void set_size_is_spatium(bool val) { size_is_spatium_ = val; }

    const std::string &store_path() const { return store_path_; }

    void set_store_path(const std::string &p) { store_path_ = p; }

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    SizeF image_size() const;

    void set_image_type(ImageType);

    ImageType image_type() const { return image_type_; }

    bool is_valid() const { return raster_doc_ || svg_doc_; }

    muse::draw::SvgRenderer *svg_renderer() const { return svg_doc_; }

    const std::shared_ptr<muse::draw::Pixmap> &raster_image() const { return raster_doc_; }

    SizeF pixel2size(const SizeF &s) const;
    SizeF size2pixel(const SizeF &s) const;

    const muse::draw::Pixmap &buffer() const { return buffer_; }

    void set_buffer(const muse::draw::Pixmap &p) const { buffer_ = p; }

    bool dirty() const { return dirty_; }

    void set_dirty(bool val) const { dirty_ = val; }

private:

    ImageStoreItem *store_item_ = nullptr;
    std::string store_path_;            // the path of the img in the ImageStore
    mutable muse::draw::Pixmap buffer_; // cached rendering
    SizeF size_;                        // in absolute or spatium units
    bool lock_aspect_ratio_ = false;
    bool auto_scale_ = false; // fill parent frame
    bool size_is_spatium_ = false;
    mutable bool dirty_ = false;

    std::shared_ptr<muse::draw::Pixmap> raster_doc_;
    muse::draw::SvgRenderer *svg_doc_ = nullptr;

    ImageType image_type_ = ImageType::NONE;
};
} // namespace mu::engraving
