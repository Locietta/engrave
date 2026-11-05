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

LocalFileInfoProvider::LocalFileInfoProvider(const muse::io::path_t &path) : path_(path) {}

bool LocalFileInfoProvider::saved() const { return false; }

muse::io::path_t LocalFileInfoProvider::path() const { return io::absolutePath(path_); }

muse::io::path_t LocalFileInfoProvider::fileName(bool includingExtension) const { return io::filename(path_, includingExtension); }

muse::io::path_t LocalFileInfoProvider::absoluteDirPath() const { return io::absoluteDirpath(path_); }

String LocalFileInfoProvider::displayName() const { return String::fromUtf8(fileName(/*includingExtension*/ false).c_str()); }

DateTime LocalFileInfoProvider::birthTime() const { return muse::io::fileSystem()->birthTime(path_); }

DateTime LocalFileInfoProvider::lastModified() const { return muse::io::fileSystem()->lastModified(path_); }

bool LocalFileInfoProvider::isNewlyCreated() const { return true; }
