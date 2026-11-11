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
struct MusicXmlConfiguration {
    enum class MusicXmlExportBreaksType { ALL, MANUAL, NO };

    void init();

    bool import_breaks() const;
    void set_import_breaks(bool value);

    bool import_layout() const;
    void set_import_layout(bool value);

    bool export_layout() const;
    void set_export_layout(bool value);

    bool export_mu3_compat() const;
    void set_export_mu3_compat(bool value);

    MusicXmlExportBreaksType export_breaks_type() const;
    void set_export_breaks_type(MusicXmlExportBreaksType breaks_type);

    bool export_invisible_elements() const;
    void set_export_invisible_elements(bool value);

    bool need_use_default_font() const;
    void set_need_use_default_font(bool value);
    void set_need_use_default_font_override(std::optional<bool> value);

    bool need_ask_about_applying_new_style() const;
    void set_need_ask_about_applying_new_style(bool value);

    bool infer_text_type() const;
    void set_infer_text_type(bool value);
    void set_infer_text_type_override(std::optional<bool> value);

private:
    std::optional<bool> need_use_default_font_override_;
    std::optional<bool> infer_text_type_override_;
};
} // namespace mu::iex::musicxml
