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
#include "defaultstyle.h"

#include "io/file.h"
#include "log.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

static const int legacy_msc_version_v450 = 450;
static const int legacy_msc_version_v302 = 302;
static const int legacy_msc_version_v3 = 301;
static const int legacy_msc_version_v2 = 206;
static const int legacy_msc_version_v1 = 114;

static const String legacy_mss_v1_path(u":/engraving/styles/legacy-style-defaults-v1.mss");
static const String legacy_mss_v2_path(u":/engraving/styles/legacy-style-defaults-v2.mss");
static const String legacy_mss_v3_path(u":/engraving/styles/legacy-style-defaults-v3.mss");
static const String legacy_mss_v302_path(u":/engraving/styles/legacy-style-defaults-v302.mss");
static const String legacy_mss_v450_path(u":/engraving/styles/legacy-style-defaults-v450.mss");

DefaultStyle *DefaultStyle::instance() {
    static DefaultStyle s;
    return &s;
}

static void apply_page_size_to_style(MStyle *style, const SizeF &page_size) {
    double old_width = style->style_d(Sid::PAGE_WIDTH);
    double new_printable_width = style->style_d(Sid::PAGE_PRINTABLE_WIDTH) + (page_size.width() - old_width);

    style->set(Sid::PAGE_WIDTH, page_size.width());
    style->set(Sid::PAGE_HEIGHT, page_size.height());
    style->set(Sid::PAGE_PRINTABLE_WIDTH, new_printable_width);
}

void DefaultStyle::init(const PathT &default_style_file_path, const PathT &part_style_file_path, const SizeF &default_page_size) {
    base_style_.precompute_values();

    {
        apply_page_size_to_style(&default_style_, default_page_size);

        if (!default_style_file_path.empty()) {
            bool ok = do_load_style(&default_style_, default_style_file_path);
            if (!ok) {
                LOGW() << "Failed to load default style file from " << default_style_file_path;
            }
        }

        default_style_.precompute_values();
    }

    if (!part_style_file_path.empty()) {
        default_style_for_parts_ = new MStyle();

        apply_page_size_to_style(default_style_for_parts_, default_page_size);

        bool ok = do_load_style(default_style_for_parts_, part_style_file_path);
        if (!ok) {
            LOGW() << "Failed to load default part style file from " << part_style_file_path;
            delete default_style_for_parts_;
            default_style_for_parts_ = nullptr;
        } else {
            default_style_for_parts_->precompute_values();
        }
    }
}

bool DefaultStyle::do_load_style(MStyle *style, const PathT &file_path) {
    File file(file_path);
    if (!file.open(IODevice::READ_ONLY)) {
        LOGE() << "failed load style: " << file_path;
        return false;
    }

    return style->read(&file);
}

// Static

const MStyle &DefaultStyle::base_style() { return instance()->base_style_; }

const MStyle &DefaultStyle::default_style() { return instance()->default_style_; }

const MStyle *DefaultStyle::default_style_for_parts() { return instance()->default_style_for_parts_; }

const MStyle &DefaultStyle::resolve_style_defaults(const int defaults_version) {
    static auto loaded_style = [](MStyle &style, const String &path, bool &loaded_flag) -> const MStyle & {
        if (loaded_flag) {
            return style;
        }
        loaded_flag = do_load_style(&style, path);
        return style;
    };

    switch (defaults_version) {
        case ::legacy_msc_version_v450: {
            static MStyle style_v450;
            static bool loaded_v450 = false;
            return loaded_style(style_v450, ::legacy_mss_v450_path, loaded_v450);
        } break;
        case ::legacy_msc_version_v302: {
            static MStyle style_v302;
            static bool loaded_v302 = false;
            return loaded_style(style_v302, ::legacy_mss_v302_path, loaded_v302);
        } break;
        case ::legacy_msc_version_v3: {
            static MStyle style_v3;
            static bool loaded_v3 = false;
            return loaded_style(style_v3, ::legacy_mss_v3_path, loaded_v3);
        } break;
        case ::legacy_msc_version_v2: {
            static MStyle style_v2;
            static bool loaded_v2 = false;
            return loaded_style(style_v2, ::legacy_mss_v2_path, loaded_v2);
        } break;
        case ::legacy_msc_version_v1: {
            static MStyle style_v1;
            static bool loaded_v1 = false;
            return loaded_style(style_v1, ::legacy_mss_v1_path, loaded_v1);
        } break;
        default: return base_style();
    }
}
