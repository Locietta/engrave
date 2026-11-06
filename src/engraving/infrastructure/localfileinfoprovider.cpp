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

#include "localfileinfoprovider.h"

using namespace muse;
using namespace mu;
using namespace mu::engraving;

LocalFileInfoProvider::LocalFileInfoProvider(const muse::io::PathT &path) : path_(path) {}

bool LocalFileInfoProvider::saved() const { return false; }

muse::io::PathT LocalFileInfoProvider::path() const { return io::absolute_path(path_); }

muse::io::PathT LocalFileInfoProvider::file_name(bool including_extension) const { return io::filename(path_, including_extension); }

muse::io::PathT LocalFileInfoProvider::absolute_dir_path() const { return io::absolute_dirpath(path_); }

String LocalFileInfoProvider::display_name() const { return String::from_utf8(file_name(/*includingExtension*/ false).c_str()); }

DateTime LocalFileInfoProvider::birth_time() const { return muse::io::file_system()->birth_time(path_); }

DateTime LocalFileInfoProvider::last_modified() const { return muse::io::file_system()->last_modified(path_); }

bool LocalFileInfoProvider::is_newly_created() const { return true; }
