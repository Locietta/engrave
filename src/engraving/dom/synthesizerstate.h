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

#ifndef MU_ENGRAVING_SYNTHESIZERSTATE_H
#define MU_ENGRAVING_SYNTHESIZERSTATE_H

#include <list>

#include "global/allocator.h"
#include "types/string.h"

namespace mu::engraving {
class SynthesizerState;

class XmlWriter;
class XmlReader;

//---------------------------------------------------------
//   IdValue
//---------------------------------------------------------

struct IdValue {
    int id = 0;
    muse::String data;

    IdValue() {}

    IdValue(int input_id, const muse::String &input_data) : id(input_id), data(input_data) {}
};

//---------------------------------------------------------
//   SynthesizerGroup
//---------------------------------------------------------

class SynthesizerGroup : public std::list<IdValue> {
    OBJECT_ALLOCATOR(engraving, SynthesizerGroup)

public:
    const muse::String &name() const { return name_; }

    void set_name(const muse::String &s) { name_ = s; }

    SynthesizerGroup() : std::list<IdValue>() {}

    SynthesizerGroup(const char *n, std::list<IdValue> l) : std::list<IdValue>(l), name_(muse::String::from_utf8(n)) {}

private:
    muse::String name_;
};

//---------------------------------------------------------
//   SynthesizerState
//---------------------------------------------------------

class SynthesizerState : public std::list<SynthesizerGroup> {
    OBJECT_ALLOCATOR(engraving, SynthesizerState)

public:
    SynthesizerState(std::initializer_list<SynthesizerGroup> l) { insert(end(), l.begin(), l.end()); }

    SynthesizerState() : std::list<SynthesizerGroup>() {}

    void write(XmlWriter &, bool force = false) const;
    void read(XmlReader &);
    SynthesizerGroup group(const muse::String &name) const;
    bool is_default_synth_soundfont();
    int cc_to_use() const;
    int method() const;

    bool is_default() const { return is_default_; }

    void set_is_default(bool val) { is_default_ = val; }

private:

    bool is_default_ = true;
};
} // namespace mu::engraving
#endif
