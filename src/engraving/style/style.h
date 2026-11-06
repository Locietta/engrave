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

#include <array>
#include <cassert>

#include "../types/propertyvalue.h"
#include "../types/spatium.h"
#include "global/io/iodevice.h"
#include "styledef.h"

namespace mu::engraving::compat {
struct ReadChordListHook;
struct ReadStyleHook;
} // namespace mu::engraving::compat

namespace mu::engraving {
struct XmlReader;
struct XmlWriter;

struct MStyle {
    MStyle() = default;

    const PropertyValue &style_v(Sid idx) const { return value(idx); }

    Spatium style_s(Sid idx) const {
        assert(MStyle::value_type(idx) == PType::SPATIUM);
        return value(idx).value<Spatium>();
    }

    double style_absolute(Sid idx) const {
        assert(MStyle::value_type(idx) == PType::SPATIUM);
        return value_absolute(idx);
    }

    String style_st(Sid idx) const {
        assert(MStyle::value_type(idx) == PType::STRING);
        return value(idx).value<String>();
    }

    bool style_b(Sid idx) const {
        assert(MStyle::value_type(idx) == PType::BOOL);
        return value(idx).to_bool();
    }

    double style_d(Sid idx) const {
        assert(MStyle::value_type(idx) == PType::REAL);
        return value(idx).to_real();
    }

    int style_i(Sid idx) const { /* can be int or enum, so no assert */ return value(idx).to_int(); }

    const PropertyValue &value(Sid idx) const;
    double value_absolute(Sid idx) const;

    void set(Sid idx, const PropertyValue &v);

    double spatium() const { return style_d(Sid::SPATIUM); }

    void set_spatium(double v) { set(Sid::SPATIUM, v); }

    double default_spatium() const;

    bool is_default(Sid idx) const;
    void set_default_style_version(const int defaults_version);
    int default_style_version() const;

    bool read(muse::io::IODevice *device, bool ign = false);
    bool write(muse::io::IODevice *device);
    void save(XmlWriter &xml, bool optimize);
    static bool is_valid(muse::io::IODevice *device);

    void precompute_values();

    static PType value_type(const Sid);
    static const char *value_name(const Sid);
    static Sid style_idx(const String &name);

private:
    friend struct compat::ReadStyleHook;

    void read(XmlReader &e, compat::ReadChordListHook *read_chord_list_hook, int msc_version);

    bool read_properties(XmlReader &);
    bool read_style_val_compat(XmlReader &);
    bool read_text_style_val_compat(XmlReader &);

    void apply_compat_style_vals(int msc_version);

    std::array<PropertyValue, size_t(Sid::STYLES)> values_;
    std::array<double, size_t(Sid::STYLES)> precomputed_values_;
};
} // namespace mu::engraving
