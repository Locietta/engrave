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

#include "synthesizerstate.h"

#include "rw/xmlwriter.h"
#include "rw/xmlreader.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   write
//---------------------------------------------------------

void SynthesizerState::write(XmlWriter &xml, bool force /* = false */) const {
    if (is_default() && !force) {
        return;
    }

    xml.start_element("Synthesizer");
    for (const SynthesizerGroup &g : *this) {
        if (!g.name().is_empty()) {
            muse::ByteArray ba = g.name().to_ascii();
            xml.start_element(ba.const_char());
            for (const IdValue &v : g) {
                xml.tag("val", {{"id", v.id}}, v.data);
            }
            xml.end_element();
        }
    }
    xml.end_element();
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void SynthesizerState::read(XmlReader &e) {
    std::list<SynthesizerGroup> temp_groups;
    while (e.read_next_start_element()) {
        SynthesizerGroup group;
        group.set_name(String::from_ascii(e.name().ascii()));

        while (e.read_next_start_element()) {
            if (e.name() == "val") {
                int id = e.int_attribute("id");
                group.push_back(IdValue(id, e.read_text()));
            } else {
                e.unknown();
            }
        }
        temp_groups.push_back(group);
    }

    if (!temp_groups.empty()) {
        // Replace any previously set state if we have read a new state
        swap(temp_groups);
        set_is_default(false);
    }
}

//---------------------------------------------------------
//   group
///  Get SynthesizerGroup by name
//---------------------------------------------------------

SynthesizerGroup SynthesizerState::group(const String &name) const {
    for (const SynthesizerGroup &g : *this) {
        if (g.name() == name) {
            return g;
        }
    }
    SynthesizerGroup sg;
    return sg;
}

//---------------------------------------------------------
//   isDefaultSynthSoundfont
///  check if synthesizer state uses default synth and
///  default font only
//---------------------------------------------------------

bool SynthesizerState::is_default_synth_soundfont() {
    SynthesizerGroup fluid = group(u"Fluid");
    if (fluid.size() == 1) {
        if (fluid.front().data == u"MS Basic.sf3") {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   ccToUse
//---------------------------------------------------------

int SynthesizerState::cc_to_use() const {
    SynthesizerGroup g = group(u"master");

    int method = 1;
    int cc = -1;

    for (const IdValue &id_val : g) {
        if (id_val.id == 4) {
            method = id_val.data.to_int();
        } else if (id_val.id == 5) {
            switch (id_val.data.to_int()) {
                case 0: cc = 1; break;
                case 1: cc = 2; break;
                case 2: cc = 4; break;
                case 3: cc = 11; break;
                default: LOGW("Unrecognised CCToUse index from synthesizer: %d", id_val.data.to_int());
            }
        }
    }

    if (method == 0) { // velocity only
        return -1;
    }

    return cc;
}

//---------------------------------------------------------
//   method
//---------------------------------------------------------

int SynthesizerState::method() const {
    SynthesizerGroup g = group(u"master");

    int method = -1;

    for (const IdValue &id_val : g) {
        if (id_val.id == 4) {
            method = id_val.data.to_int();
            break;
        }
    }

    return method;
}
} // namespace mu::engraving
