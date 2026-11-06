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
#include <cstdint>

#include "engraving/dom/bracketitem.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stringtunings.h"
#include "gtp/gp7dombuilder.h"
#include "importgtp.h"
#include "log.h"
#include "serialization/zipreader.h"

using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::iex::guitarpro {
namespace {
constexpr uint64_t g_p7_max_entry_bytes = 64ull * 1024 * 1024;
constexpr uint64_t g_p7_max_total_bytes = 128ull * 1024 * 1024;
constexpr size_t g_p7_max_entries = 4096;
} // namespace

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro7::read(IODevice *io) {
    f = io;
    previous_tempo = -1;
    gpif_read_ = false;

    ZipReader zip(io);
    const auto entries = zip.file_info_list();
    if (zip.has_error() || entries.empty() || entries.size() > ::mu::iex::guitarpro::g_p7_max_entries) {
        LOGE() << "Invalid or oversized Guitar Pro 7 archive";
        return false;
    }

    uint64_t total = 0;
    for (const auto &entry : entries) {
        if (entry.size > ::mu::iex::guitarpro::g_p7_max_entry_bytes || total > ::mu::iex::guitarpro::g_p7_max_total_bytes - entry.size) {
            LOGE() << "Guitar Pro 7 archive declares an unsafe uncompressed size";
            return false;
        }
        total += entry.size;
    }

    if (!zip.file_exists("Content/score.gpif")) {
        LOGE() << "Guitar Pro 7 archive has no Content/score.gpif";
        return false;
    }

    ByteArray file_data = zip.file_data("Content/score.gpif");
    if (zip.has_error() || file_data.empty() || file_data.size() > ::mu::iex::guitarpro::g_p7_max_entry_bytes) {
        LOGE() << "Invalid Content/score.gpif in Guitar Pro 7 archive";
        return false;
    }

    ByteArray parts_data;
    if (zip.file_exists("Content/PartConfiguration")) {
        parts_data = zip.file_data("Content/PartConfiguration");
        if (parts_data.size() > ::mu::iex::guitarpro::g_p7_max_entry_bytes) {
            LOGE() << "Oversized Content/PartConfiguration in Guitar Pro 7 archive";
            return false;
        }
    }
    zip.close();

    properties_ = read_properties(&parts_data);

    read_gpif(&file_data);
    return gpif_read_;
}

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

GuitarPro::GPProperties GuitarPro7::read_properties(ByteArray *data) {
    GPProperties properties;
    size_t parts_info_size = data->size();
    const size_t num_instr_offset = 8;
    if (parts_info_size <= num_instr_offset) {
        LOGE() << "failed to read gp properties";
        return properties;
    }

    size_t number_of_instruments = static_cast<size_t>(data->at(num_instr_offset));
    if (parts_info_size <= num_instr_offset + number_of_instruments) {
        LOGE() << "failed to read gp properties";
        return properties;
    }

    std::vector<TabImportOption> parts_import_opts = properties.parts_import_options;

    for (size_t i = num_instr_offset + 1; i <= num_instr_offset + number_of_instruments; i++) {
        parts_import_opts.push_back(static_cast<TabImportOption>(data->at(static_cast<int>(i))));
    }

    return properties;
}

std::unique_ptr<IGPDomBuilder> GuitarPro7::create_gp_dom_builder() const { return std::make_unique<GP7DomBuilder>(); }
} // namespace mu::iex::guitarpro
