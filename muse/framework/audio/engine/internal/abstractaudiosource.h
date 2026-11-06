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
#ifndef MUSE_AUDIO_ABSTRACTAUDIOSOURCE_H
#define MUSE_AUDIO_ABSTRACTAUDIOSOURCE_H

#include "../iaudiosource.h"

namespace muse::audio::engine {
class AbstractAudioSource : public IAudioSource {
public:
    virtual ~AbstractAudioSource() = default;

    virtual void set_mode(const ProcessMode mode) override;
    ProcessMode mode() const override;

    virtual void set_output_spec(const OutputSpec &spec) override;


protected:
    ProcessMode mode_ = ProcessMode::UNDEFINED;
    OutputSpec output_spec_;
};
} // namespace muse::audio::engine

#endif // MUSE_AUDIO_ABSTRACTAUDIOSOURCE_H
