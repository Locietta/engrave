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

#include <dyn/dyn.h>

#include "audio/common/audiotypes.h"
#include "global/types/ret.h"

namespace muse::audio::encode {
DYN_METHOD(EncoderBegin, begin);
DYN_METHOD(EncoderEncode, encode);
DYN_METHOD(EncoderEnd, end);

using EncoderTrait = dyn::Trait<EncoderBegin<bool(samples_t)>, EncoderEncode<size_t(samples_t, const float *)>, EncoderEnd<Ret()>>;
using AudioEncoder = dyn::Box<EncoderTrait>;

// What encoder factories return: sole ownership of one encoder. Only a
// std::unique_ptr converts, so raw and shared pointers are rejected at compile
// time, and a null owner yields an empty encoder rather than an engaged box
// around nullptr.
struct UniqueEncoder {
    UniqueEncoder() = default;

    template <class T>
        requires dyn::Impl<T, EncoderTrait>
    UniqueEncoder(std::unique_ptr<T> owner) noexcept // NOLINT(google-explicit-constructor): factories return unique_ptr
        : encoder_(std::move(owner)) {}

    AudioEncoder take() && noexcept { return std::move(encoder_); }

private:
    AudioEncoder encoder_;
};
} // namespace muse::audio::encode
