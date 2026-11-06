/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#ifndef MUSE_GLOBAL_RET_H
#define MUSE_GLOBAL_RET_H

#include <string>
#include <map>
#include <any>

#include "global/asserts.h"

#undef NOT_SUPPORTED
#undef NOT_IMPLEMENTED

namespace muse {
class Ret {
public:

    enum class Code {
        UNDEFINED = -1,
        OK = 0,
        UNKNOWN_ERROR = 1,

        // not error, just codes
        CANCEL = 3, // abort by user

        NOT_SUPPORTED = 4,
        NOT_IMPLEMENTED = 5,
        NOT_ALLOWED = 9,

        BAD_DATA = 6,
        BAD_ARGS = 7,
        BUSY = 8,

        // Global errors
        GLOBAL_FIRST = 20,
        INTERNAL_ERROR = 21,
        GLOBAL_LAST = 99,

        UI_FIRST = 100,
        UI_LAST = 199,

        EXTENSIONS_FIRST = 200,
        EXTENSIONS_LAST = 299,

        AUDIO_FIRST = 300,
        AUDIO_LAST = 399,

        SYSTEM_FIRST = 400,
        SYSTEM_LAST = 499,

        NETWORK_FIRST = 500,
        NETWORK_LAST = 599,

        MIDI_FIRST = 600,
        MIDI_LAST = 699,

        LANGUAGES_FIRST = 700,
        LANGUAGES_LAST = 799,

        NOTATION_FIRST = 1000,
        NOTATION_LAST = 1299,

        CONVERTER_FIRST = 1300,
        CONVERTER_LAST = 1399,

        VST_FIRST = 1400,
        VST_LAST = 1499,

        WORKSPACE_FIRST = 1500,
        WORKSPACE_LAST = 1599,

        LEARN_FIRST = 1600,
        LEARN_LAST = 1699,

        UPDATE_FIRST = 1700,
        UPDATE_LAST = 1799,

        CLOUD_FIRST = 1800,
        CLOUD_LAST = 1899,

        ENGRAVING_FIRST = 2000,
        ENGRAVING_LAST = 2999,

        PROJECT_FIRST = 3000,
        PROJECT_LAST = 3999,

        DIAGNOSTICS_FIRST = 4000,
        DIAGNOSTICS_LAST = 4999,

        MUSE_SOUNDS_FIRST = 5000,
        MUSE_SOUNDS_LAST = 5999
    };

    Ret() = default;
    Ret(bool arg);
    explicit Ret(int c);
    explicit Ret(Code c);
    explicit Ret(const int &c, const std::string &text);

    void set_code(int c);
    int code() const;
    bool valid() const;
    bool success() const;
    void set_text(const std::string &s);
    const std::string &text() const;
    void set_data(const std::string &key, const std::any &val);

    template <typename DataType>
    DataType data(const std::string &key, const DataType &default_value) const {
        static_assert(!std::is_reference_v<DataType>, "DataType must not be a reference");
        static_assert(!std::is_pointer_v<DataType>, "DataType must not be a pointer");

        const auto it = data_.find(key);
        if (it == data_.end()) {
            return default_value;
        }

        if (it->second.type() == typeid(DataType)) {
            try {
                return std::any_cast<DataType>(it->second);
            } catch (const std::bad_any_cast &e) {
                IF_ASSERT_FAILED_X(false, "Ret::data<" + std::string(typeid(DataType).name()) + ">: bad_any_cast for key '" + key +
                                              "': " + e.what()) {
                    return default_value;
                }
            }
        }
        IF_ASSERT_FAILED_X(false, "Ret::data<" + std::string(typeid(DataType).name()) + ">: type mismatch for key '" + key +
                                      "', stored type is " + it->second.type().name()) {
            return default_value;
        }
    }

    inline Ret &operator=(int c) {
        code_ = c;
        return *this;
    }

    inline Ret &operator=(bool arg) {
        code_ = arg ? int(Code::OK) : int(Code::UNKNOWN_ERROR);
        return *this;
    }

    inline operator bool() const { return success(); }

    inline bool operator!() const { return !success(); }

    std::string to_string() const;

private:
    int code_ = int(Code::UNDEFINED);
    std::string text_;
    std::map<std::string, std::any> data_;
};

inline muse::Ret make_ok() { return Ret(static_cast<int>(Ret::Code::OK)); }

inline muse::Ret make_ret(Ret::Code e) { return Ret(static_cast<int>(e)); }

inline muse::Ret make_ret(Ret::Code e, const std::string &text) { return Ret(static_cast<int>(e), text); }

inline muse::Ret make_ret(Ret::Code e, const char *text) { return Ret(static_cast<int>(e), std::string(text)); }

inline muse::Ret make_ret(int e, const std::string &text) { return Ret(e, text); }

inline bool check_ret(const Ret &r, Ret::Code c) { return r.code() == int(c); }
} // namespace muse

#endif // MUSE_GLOBAL_RET_H
