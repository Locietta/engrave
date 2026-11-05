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

#include "id.h"

using namespace muse;

ID::ID() : id_(0) {}

ID::ID(const ID &id) : id_(id.id_) {}

ID::ID(const std::string &id) : id_(!id.empty() ? std::stoul(id) : 0) {}

bool ID::isValid() const { return id_ != 0; }

ID &ID::operator=(const ID &id) {
    id_ = id.id_;
    return *this;
}

ID &ID::operator+=(const ID &id) {
    id_ += id.id_;
    return *this;
}

ID ID::operator+(const ID &id) const { return ID(id_ + id.id_); }

ID ID::operator^(const ID &id) const { return ID(id_ ^ id.id_); }

bool ID::operator==(const ID &id) const { return id_ == id.id_; }

bool ID::operator==(uint64_t id) const { return id_ == id; }

bool ID::operator!=(const ID &id) const { return id_ != id.id_; }

bool ID::operator<(const ID &id) const { return id_ < id.id_; }

bool ID::operator>(const ID &id) const { return id_ > id.id_; }

uint64_t ID::toUint64() const { return id_; }

std::string ID::toStdString() const { return std::to_string(id_); }
