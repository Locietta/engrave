/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "../ifxresolver.h"

namespace muse::audio::engine {
struct AudioFactory {
private:
    OutputSpec spec_;
    fx::IFxResolver &fx_resolver_;

    fx::IFxResolver *fx_resolver() const { return &fx_resolver_; }

public:
    explicit AudioFactory(fx::IFxResolver &fx_resolver) : fx_resolver_(fx_resolver) {}

    void set_output_spec(const OutputSpec &spec) { spec_ = spec; }

    void clear_all_fx();
};
} // namespace muse::audio::engine
