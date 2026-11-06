/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_ALLPASSDISPERSION_H
#define MUSE_AUDIO_ALLPASSDISPERSION_H

#include "iirbiquadfilter.h"

namespace muse::audio::fx {
struct AllPassDispersion {
    AllPassDispersion() {
        // these values have been manually tuned to produce a 50-sample IR that has a (low) peak of 0.453731
        processor_[0].set_target_coefficients(IirBiquadFilter::create_allpass2_p(0.2492, 2.168, 1.0));
        processor_[1].set_target_coefficients(IirBiquadFilter::create_allpass2_p(0.1119, 2.033, 1.0));
        reset();
    }

    void reset() {
        for (auto &p : processor_) {
            p.reset();
        }
    }

    void process(float **signal_ptr, int num_ch, int n) {
        for (auto &p : processor_) {
            p.process(signal_ptr, num_ch, n);
        }
    }

    void process_mono_sample(float &s) {
        for (auto &p : processor_) {
            p.process_mono_sample(s);
        }
    }

    void process_stereo_sample(float &l, float &r) {
        for (auto &p : processor_) {
            p.process_stereo_sample(l, r);
        }
    }

private:
    std::array<IirBiquadFilter::DF1Processor<double>, 2> processor_;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_ALLPASSDISPERSION_H
