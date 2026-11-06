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

#ifndef MUSE_AUDIO_REVERBFILTERS_H
#define MUSE_AUDIO_REVERBFILTERS_H

namespace muse::audio::fx::reverbfilters {
template <typename T>
struct OneZeroCoeffs {
    T b0 = T(1), b1 = T(0);
};

/// define the gain at nyquist. <1 = steep lowpass  >2 6dB/oct highfshelf
template <typename T>
auto one_zero_coeffs_from_nyquist_gain(T gain_fact_nyq) {
    OneZeroCoeffs<T> cf;
    auto r = gain_fact_nyq;
    auto alpha = (T(1) - r) / (T(1) + r);
    cf.b0 = T(1) / (T(1) + alpha);
    cf.b1 = alpha * cf.b0;
    return cf;
}

template <typename T>
struct OneZeroFilter {
    T process_sample(T x0) {
        auto res = cf.b0 * x0 + cf.b1 * x1;
        x1 = x0;
        return res;
    }

    void reset() { x1 = T(0); }

    OneZeroCoeffs<T> cf;

private:
    T x1 = T(0);
};

template <typename T>
struct OnePoleCoeffs {
    T b0 = T(1), b1 = T(0), a1 = T(0);
};

/// coeffs for a one-pole one zero lowpass filter using the bilinear transform.
/// @param freq the -3dB point of the filter
/// this leads to gain 0 at nyquist.
template <typename T>
auto one_pole_coeffs_lowpass_blt(T freq, double sample_rate) {
    auto alpha = std::tan(3.14159265358979323846 * freq / sample_rate); // prewarp
    OnePoleCoeffs<T> cf;
    auto n = 1 / (1 + alpha);
    cf.b0 = alpha * n;
    cf.b1 = cf.b0;
    cf.a1 = (alpha - 1) * n;
    return cf;
}

/// coeffs for a one-pole one zero lowpass filter using the bilinear transform.
/// gain 1 at DC, gain 0 at nyquist, one defined point with gain < 1
template <typename T>
auto one_pole_coeffs_lowpass1_point(T freq, T gain_fact, double sample_rate) {
    gain_fact = std::min(T(0.9999), gain_fact);

    auto cw = std::cos(6.28318530717958647692 * freq / sample_rate);
    T g2 = gain_fact * gain_fact;
    T root = (T) std::sqrt((1 - cw * cw) * (1 - g2));

    OnePoleCoeffs<T> cf;
    cf.b0 = -T((gain_fact * root - (1 - cw) * g2) / (2 * g2 - 1 - cw));
    cf.b1 = cf.b0;
    cf.a1 = 2 * cf.b0 - 1;
    return cf;
}

template <typename T>
struct OnePoleFilter {
    T process_sample(T x0) {
        auto res = cf.b0 * x0 + cf.b1 * x1 - cf.a1 * y1_;
        x1 = x0;
        y1_ = res;
        return res;
    }

    void reset() {
        y1_ = T(0);
        x1 = T(0);
    }

    OnePoleCoeffs<T> cf;

private:
    T y1_ = T(0), x1 = T(0);
};
} // namespace muse::audio::fx::reverbfilters

#endif // MUSE_AUDIO_REVERBFILTERS_H
