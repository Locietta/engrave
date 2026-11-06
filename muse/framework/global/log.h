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

#ifndef MUSE_GLOBAL_LOG_H
#define MUSE_GLOBAL_LOG_H

#include <cstdlib> // IWYU pragma: export
#include <cassert> // IWYU pragma: export

#include "profiler.h"                            // IWYU pragma: export
#include "thirdparty/kors_logger/src/log_base.h" // IWYU pragma: export

// The logger's generic macros collide with Ret::Code enumerators.
#undef NOT_IMPLEMENTED
#undef NOT_SUPPORTED
#undef NOT_IMPL_RETURN
#define MUSE_NOT_IMPLEMENTED LOGD() << "Not implemented!!"
#define MUSE_NOT_SUPPORTED LOGW() << "Not supported!!"
#define NOT_IMPL_RETURN MUSE_NOT_IMPLEMENTED return

#undef FALLTHROUGH

#if __has_cpp_attribute(fallthrough)
#define MU_FALLTHROUGH() [[fallthrough]]
#else
#define MU_FALLTHROUGH() (void) 0
#endif

#endif // MUSE_GLOBAL_LOG_H
