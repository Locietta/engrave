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
#include "musicxmlconfiguration.h"

#include "settings.h"
#include "translation.h"

using namespace mu;
using namespace muse;
using namespace mu::iex::musicxml;

static const std::string module_name("iex_musicxml");

static const Settings::Key musicxml_import_breaks_key(module_name, "import/musicXml/importBreaks");
static const Settings::Key musicxml_import_layout_key(module_name, "import/musicXml/importLayout");
static const Settings::Key musicxml_export_layout_key(module_name, "export/musicXml/exportLayout");
static const Settings::Key musicxml_export_m_u3_compat_key(module_name, "export/musicXml/exportMu3Compat");
static const Settings::Key musicxml_export_breaks_type_key(module_name, "export/musicXml/exportBreaks");
static const Settings::Key musicxml_export_invisible_elements_key(module_name, "export/musicXml/exportInvisibleElements");
static const Settings::Key migration_apply_edwin_for_xml(module_name, "import/compatibility/apply_edwin_for_xml");
static const Settings::Key migration_not_ask_again_key(module_name, "import/compatibility/do_not_ask_me_again");
static const Settings::Key musicxml_import_infer_text_type(module_name, "import/musicXml/importInferTextType");

void MusicXmlConfiguration::init() {
    settings()->set_default_value(::musicxml_import_breaks_key, Val(true));

    settings()->set_default_value(::musicxml_import_layout_key, Val(true));

    settings()->set_default_value(::musicxml_export_layout_key, Val(true));
    settings()->set_default_value(::musicxml_export_m_u3_compat_key, Val(false));
    settings()->set_description(::musicxml_export_m_u3_compat_key,
                                //: Means that less information will be included in exported MusicXML files,
                                //: to prevent errors when importing them into MuseScore 3
                                muse::trc("iex_musicxml", "Limit MusicXML export for compatibility with MuseScore 3"));
    settings()->set_can_be_manually_edited(::musicxml_export_m_u3_compat_key, true);
    settings()->set_default_value(::musicxml_export_breaks_type_key, Val(MusicXmlExportBreaksType::ALL));
    settings()->set_default_value(::musicxml_export_invisible_elements_key, Val(false));
    settings()->set_description(::musicxml_export_invisible_elements_key,
                                muse::trc("iex_musicxml", "Export invisible elements to MusicXML"));
    settings()->set_can_be_manually_edited(::musicxml_export_invisible_elements_key, true);

    settings()->set_default_value(::migration_apply_edwin_for_xml, Val(false));

    settings()->set_default_value(::migration_not_ask_again_key, Val(false));

    settings()->set_default_value(::musicxml_import_infer_text_type, Val(false));
}

bool MusicXmlConfiguration::import_breaks() const { return settings()->value(::musicxml_import_breaks_key).to_bool(); }

void MusicXmlConfiguration::set_import_breaks(bool value) { settings()->set_shared_value(::musicxml_import_breaks_key, Val(value)); }

bool MusicXmlConfiguration::import_layout() const { return settings()->value(::musicxml_import_layout_key).to_bool(); }

void MusicXmlConfiguration::set_import_layout(bool value) { settings()->set_shared_value(::musicxml_import_layout_key, Val(value)); }

bool MusicXmlConfiguration::export_layout() const { return settings()->value(::musicxml_export_layout_key).to_bool(); }

void MusicXmlConfiguration::set_export_layout(bool value) { settings()->set_shared_value(::musicxml_export_layout_key, Val(value)); }

bool MusicXmlConfiguration::export_mu3_compat() const { return settings()->value(::musicxml_export_m_u3_compat_key).to_bool(); }

void MusicXmlConfiguration::set_export_mu3_compat(bool value) {
    settings()->set_shared_value(::musicxml_export_m_u3_compat_key, Val(value));
}

MusicXmlConfiguration::MusicXmlExportBreaksType MusicXmlConfiguration::export_breaks_type() const {
    return settings()->value(::musicxml_export_breaks_type_key).to_enum<MusicXmlExportBreaksType>();
}

void MusicXmlConfiguration::set_export_breaks_type(MusicXmlExportBreaksType breaks_type) {
    settings()->set_shared_value(::musicxml_export_breaks_type_key, Val(breaks_type));
}

bool MusicXmlConfiguration::export_invisible_elements() const {
    return settings()->value(::musicxml_export_invisible_elements_key).to_bool();
}

void MusicXmlConfiguration::set_export_invisible_elements(bool value) {
    settings()->set_shared_value(::musicxml_export_invisible_elements_key, Val(value));
}

bool MusicXmlConfiguration::need_use_default_font() const {
    if (need_use_default_font_override_.has_value()) {
        return need_use_default_font_override_.value();
    }

    return settings()->value(::migration_apply_edwin_for_xml).to_bool();
}

void MusicXmlConfiguration::set_need_use_default_font(bool value) {
    settings()->set_shared_value(::migration_apply_edwin_for_xml, Val(value));
}

void MusicXmlConfiguration::set_need_use_default_font_override(std::optional<bool> value) { need_use_default_font_override_ = value; }

bool MusicXmlConfiguration::need_ask_about_applying_new_style() const {
    return !settings()->value(::migration_not_ask_again_key).to_bool();
}

void MusicXmlConfiguration::set_need_ask_about_applying_new_style(bool value) {
    settings()->set_shared_value(::migration_not_ask_again_key, Val(!value));
}

bool MusicXmlConfiguration::infer_text_type() const {
    if (infer_text_type_override_.has_value()) {
        return infer_text_type_override_.value();
    }

    return settings()->value(::musicxml_import_infer_text_type).to_bool();
}

void MusicXmlConfiguration::set_infer_text_type(bool value) { settings()->set_shared_value(::musicxml_import_infer_text_type, Val(value)); }

void MusicXmlConfiguration::set_infer_text_type_override(std::optional<bool> value) { infer_text_type_override_ = value; }
