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

#include <memory>

#include "audio/common/audiotypes.h"
#include "ifxprocessor.h"

namespace muse::audio::fx {
struct IFxResolver {
    virtual ~IFxResolver() = default;

    struct IResolver {
        virtual ~IResolver() = default;

        virtual std::vector<IFxProcessorPtr> resolve_fx_list(const audio::TrackId track_id, const AudioFxChain &fx_chain,
                                                             const OutputSpec &output_spec) = 0;
        virtual std::vector<IFxProcessorPtr> resolve_master_fx_list(const AudioFxChain &fx_chain, const OutputSpec &output_spec) = 0;
        virtual AudioResourceMetaList resolve_resources() const = 0;

        virtual void refresh() = 0;
        virtual void clear_all_fx() = 0;
    };

    using IResolverPtr = std::shared_ptr<IResolver>;

    virtual std::vector<IFxProcessorPtr> resolve_master_fx_list(const AudioFxChain &fx_chain, const OutputSpec &output_spec) = 0;
    virtual std::vector<IFxProcessorPtr> resolve_fx_list(const TrackId track_id, const AudioFxChain &fx_chain,
                                                         const OutputSpec &output_spec) = 0;
    virtual AudioResourceMetaList resolve_available_resources() const = 0;

    virtual void register_resolver(const AudioFxType type, IResolverPtr resolver) = 0;
    virtual void clear_all_fx() = 0;
};

using IFxResolverPtr = std::shared_ptr<IFxResolver>;
} // namespace muse::audio::fx
