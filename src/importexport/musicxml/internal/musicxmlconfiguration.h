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

#include "../imusicxmlconfiguration.h"

namespace mu::iex::musicxml {
class MusicXmlConfiguration : public IMusicXmlConfiguration {
public:
    void init();

    bool import_breaks() const override;
    void set_import_breaks(bool value) override;

    bool import_layout() const override;
    void set_import_layout(bool value) override;

    bool export_layout() const override;
    void set_export_layout(bool value) override;

    bool export_mu3_compat() const override;
    void set_export_mu3_compat(bool value) override;

    MusicXmlExportBreaksType export_breaks_type() const override;
    void set_export_breaks_type(MusicXmlExportBreaksType breaks_type) override;

    bool export_invisible_elements() const override;
    void set_export_invisible_elements(bool value) override;

    bool need_use_default_font() const override;
    void set_need_use_default_font(bool value) override;
    void set_need_use_default_font_override(std::optional<bool> value) override;

    bool need_ask_about_applying_new_style() const override;
    void set_need_ask_about_applying_new_style(bool value) override;

    bool infer_text_type() const override;
    void set_infer_text_type(bool value) override;
    void set_infer_text_type_override(std::optional<bool> value) override;

private:
    std::optional<bool> need_use_default_font_override_;
    std::optional<bool> infer_text_type_override_;
};
} // namespace mu::iex::musicxml
