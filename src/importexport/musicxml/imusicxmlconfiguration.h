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

#include <optional>

#include "io/path.h"

namespace mu::iex::musicxml {
class IMusicXmlConfiguration {
public:
    virtual ~IMusicXmlConfiguration() = default;

    virtual bool import_breaks() const = 0;
    virtual void set_import_breaks(bool value) = 0;

    virtual bool import_layout() const = 0;
    virtual void set_import_layout(bool value) = 0;

    virtual bool export_layout() const = 0;
    virtual void set_export_layout(bool value) = 0;

    virtual bool export_mu3_compat() const = 0;
    virtual void set_export_mu3_compat(bool value) = 0;

    enum class MusicXmlExportBreaksType { ALL, MANUAL, NO };

    virtual MusicXmlExportBreaksType export_breaks_type() const = 0;
    virtual void set_export_breaks_type(MusicXmlExportBreaksType breaks_type) = 0;

    virtual bool export_invisible_elements() const = 0;
    virtual void set_export_invisible_elements(bool value) = 0;

    virtual bool need_use_default_font() const = 0;
    virtual void set_need_use_default_font(bool value) = 0;
    virtual void set_need_use_default_font_override(std::optional<bool> value) = 0;

    virtual bool need_ask_about_applying_new_style() const = 0;
    virtual void set_need_ask_about_applying_new_style(bool value) = 0;

    virtual bool infer_text_type() const = 0;
    virtual void set_infer_text_type(bool value) = 0;
    virtual void set_infer_text_type_override(std::optional<bool> value) = 0;
};
} // namespace mu::iex::musicxml
