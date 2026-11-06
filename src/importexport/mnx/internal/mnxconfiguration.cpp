/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include "mnxconfiguration.h"

#include <algorithm>

#include "settings.h"

using namespace mu;
using namespace mu::iex::mnxio;
using namespace muse;

static const std::string module_name("iex_mnx");

static const Settings::Key mnx_export_indent_key(module_name, "export/mnx/indentSpaces");
static const Settings::Key mnx_export_beams_key(module_name, "export/mnx/exportBeams");
static const Settings::Key mnx_export_rest_positions_key(module_name, "export/mnx/exportRestPositions");
static const Settings::Key mnx_import_exact_schema_validation_key(module_name, "import/mnx/requireExactSchemaValidation");

void MnxConfiguration::init() {
    settings()->set_default_value(mnx_export_indent_key, Val(4));
    settings()->set_default_value(mnx_export_beams_key, Val(true));
    settings()->set_default_value(mnx_export_rest_positions_key, Val(false));
    settings()->set_default_value(mnx_import_exact_schema_validation_key, Val(true));
}

int MnxConfiguration::mnx_indent_spaces() const {
    const int value = settings()->value(mnx_export_indent_key).to_int();
    return std::clamp(value, -1, 8);
}

void MnxConfiguration::set_mnx_indent_spaces(int value) {
    const int clamped = std::clamp(value, -1, 8);
    settings()->set_shared_value(mnx_export_indent_key, Val(clamped));
}

bool MnxConfiguration::mnx_export_beams() const { return settings()->value(mnx_export_beams_key).to_bool(); }

void MnxConfiguration::set_mnx_export_beams(bool value) { settings()->set_shared_value(mnx_export_beams_key, Val(value)); }

bool MnxConfiguration::mnx_export_rest_positions() const { return settings()->value(mnx_export_rest_positions_key).to_bool(); }

void MnxConfiguration::set_mnx_export_rest_positions(bool value) {
    settings()->set_shared_value(mnx_export_rest_positions_key, Val(value));
}

bool MnxConfiguration::mnx_require_exact_schema_validation() const {
    return settings()->value(mnx_import_exact_schema_validation_key).to_bool();
}

void MnxConfiguration::set_mnx_require_exact_schema_validation(bool value) {
    settings()->set_shared_value(mnx_import_exact_schema_validation_key, Val(value));
}
