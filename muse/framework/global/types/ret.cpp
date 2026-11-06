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

#include "ret.h"

using namespace muse;

Ret::Ret(bool arg) : code_(arg ? int(Code::OK) : int(Code::UNKNOWN_ERROR)) {}

Ret::Ret(int c) : code_(c) {}

Ret::Ret(Code c) : code_(static_cast<int>(c)) {}

Ret::Ret(const int &c, const std::string &text) : code_(c), text_(text) {}

bool Ret::valid() const { return code_ > int(Code::UNDEFINED); }

bool Ret::success() const { return code_ == int(Code::OK); }

void Ret::set_code(int c) { code_ = c; }

int Ret::code() const { return code_; }

void Ret::set_text(const std::string &s) { text_ = s; }

const std::string &Ret::text() const { return text_; }

void Ret::set_data(const std::string &key, const std::any &val) { data_[key] = val; }

std::string Ret::to_string() const { return "[" + std::to_string(code_) + "] " + text_; }
