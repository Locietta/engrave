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

#pragma once

#include <cstddef>
#include <memory>
#include <utility>

#include <proxy/proxy.h>

#include "audio/common/audiotypes.h"
#include "global/types/ret.h"

namespace muse::audio::encode {
PRO_DEF_MEM_DISPATCH(EncoderBegin, begin);
PRO_DEF_MEM_DISPATCH(EncoderEncode, encode);
PRO_DEF_MEM_DISPATCH(EncoderEnd, end);

struct EncoderFacade : pro::facade_builder::add_convention<EncoderBegin, bool(samples_t)>::
                           add_convention<EncoderEncode, size_t(samples_t, const float *)>::add_convention<EncoderEnd, Ret()>::build {};

using AudioEncoder = pro::proxy<EncoderFacade>;

// What encoder factories return: sole ownership of one encoder. Only a
// std::unique_ptr converts, so raw and shared pointers are rejected at compile
// time, and a null owner yields an empty encoder rather than an engaged proxy
// around nullptr.
struct UniqueEncoder {
    UniqueEncoder() = default;

    template <class T>
    UniqueEncoder(std::unique_ptr<T> owner) { // NOLINT(google-explicit-constructor): factories return unique_ptr
        if (owner) proxy_ = std::move(owner);
    }

    AudioEncoder take() && noexcept { return std::move(proxy_); }

private:
    AudioEncoder proxy_;
};
} // namespace muse::audio::encode
