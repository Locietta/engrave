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

#include "scorerw.h"

#include "engraving/compat/mscxcompat.h"
#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/factory.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include "engraving/rw/rwregister.h"
#include "engraving/rw/write/twrite.h"
#include "global/io/buffer.h"
#include "global/io/file.h"
#include "log.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

String ScoreRW::root_path_;
const EngravingServices *ScoreRW::services_ = nullptr;

void ScoreRW::set_root_path(const String &path) { root_path_ = path; }

String ScoreRW::root_path() { return root_path_; }

void ScoreRW::set_services(const EngravingServices *services) { services_ = services; }

const EngravingServices &ScoreRW::services() { return *services_; }

MasterScore *ScoreRW::read_score(const String &name, bool is_absolute_path, ImportFunc import_func) {
    muse::io::PathT path = is_absolute_path ? name : (root_path() + u"/" + name);
    MasterScore *score = compat::ScoreAccess::create_master_score_with_base_style(services());
    score->set_file_info_provider(std::make_shared<LocalFileInfoProvider>(path));
    std::string suffix = muse::io::suffix(path);

    ScoreLoad sl;
    Err rv;
    if (suffix == "mscz" || suffix == "mscx") {
        rv = static_cast<Err>(compat::load_mscz_or_mscx(score, path.to_string(), false).code());
    } else if (import_func) {
        rv = import_func(score, path);
    } else {
        rv = Err::FILE_UNKNOWN_TYPE;
    }

    if (rv != Err::NO_ERROR) {
        LOGE() << "can't load score, path: " << path;
        delete score;
        return nullptr;
    }

    for (Score *s : score->score_list()) {
        s->do_layout();
    }

    return score;
}

bool ScoreRW::save_score(Score *score, const String &name) {
    File::remove(name);

    auto output = Buffer::opened(IODevice::WRITE_ONLY);
    if (!rw::RWRegister::writer()->write_score(score, &output)) {
        return false;
    }
    output.close();

    return File::write_file(name, output.data());
}

bool ScoreRW::save_score(Score *score, const String &name, ExportFunc export_func) {
    File::remove(name);

    muse::io::PathT path = name;
    Err rv = export_func(score, path);

    if (rv != Err::NO_ERROR) {
        LOGE() << "can't load score, path: " << path;
        return false;
    } else {
        return true;
    }
}

muse::ByteArray ScoreRW::write_element(const EngravingItem *element) {
    auto buffer = Buffer::opened(IODevice::WRITE_ONLY);
    XmlWriter xml(&buffer);
    xml.start_document();
    rw::RWRegister::writer()->write_item(element, xml);
    xml.flush();
    buffer.close();
    return buffer.data();
}

EngravingItem *ScoreRW::write_read_element(EngravingItem *element) {
    XmlReader e(write_element(element));
    e.read_next_start_element();
    element = Factory::create_item_by_name(e.name(), element->score()->dummy());
    rw::RWRegister::reader()->read_item(element, e);
    return element;
}

bool ScoreRW::save_mime_data(muse::ByteArray mime_data, const String &save_name) { return File::write_file(save_name, mime_data); }
