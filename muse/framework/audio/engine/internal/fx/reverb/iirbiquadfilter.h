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

#ifndef MUSE_AUDIO_IIRBIQUADFILTER_H
#define MUSE_AUDIO_IIRBIQUADFILTER_H

#include <array>
#include <cassert>
#include <cmath>

/**
 * Biquad filter routines with per-sample processing
 * and not containing any allocation/deallocation
 */

namespace muse::audio::fx { namespace IirBiquadFilter {
namespace { // private methods

template <typename T>
T inline sq(T x) {
    return x * x;
}

/// all 2p filter types have similar poles. Calculate a1 and a2 from the analog poles using matched-Z transform
template <typename coeffT>
inline void matched_zpole_coeffs(coeffT normf, coeffT Q, coeffT &a1, coeffT &a2) {
    auto q = 1 / (2 * Q);
    auto w0 = normf * 6.28318530717958647692;

    auto ex = std::exp(-q * w0);
    a1 = -2 * ex;
    if (q <= 1.0) {
        a1 *= std::cos(std::sqrt(1 - sq(q)) * w0);
    } else {
        a1 *= std::cosh(std::sqrt(sq(q) - 1) * w0);
    }
    a2 = sq(ex);
}

/// calculate the phi angles for vicanek's matching form
inline std::array<double, 3> phi_factors_for_freq(double normf) {
    auto cw = 0.5 * cos(normf * 6.28318530717958647692);
    std::array<double, 3> phi;
    phi[0] = 0.5 + cw;
    phi[1] = 0.5 - cw;
    phi[2] = 4 * phi[0] * phi[1]; // very precision-sensitive at low freq.
    return phi;
}

/// convert biquad coefficients to phi matching form coefficients
inline std::array<double, 3> phi_coeffs_for_biquad_coeffs(double c0, double c1, double c2) {
    return {sq(c0 + c1 + c2), sq(c0 - c1 + c2), -4 * c0 * c2};
}

/// convert phi matching form coeffs to biquad coeffs
inline void biquad_coeffs_for_phi_coeffs(const std::array<double, 3> &C, double &c0, double &c1, double &c2) {
    auto W = 0.5 * (std::sqrt(C[0]) + std::sqrt(C[1]));
    c0 = 0.5 * (W + std::sqrt(sq(W) + C[2]));
    c1 = 0.5 * (std::sqrt(C[0]) - std::sqrt(C[1]));
    c2 = -0.25 * C[2] / c0;
}
} // namespace

template <typename T>
struct Coeffs {
    T a1 = T(0), a2 = T(0);            // poles
    T b0 = T(1), b1 = T(0), b2 = T(0); // zeros

    // operator support for step-wise smoothing
    bool operator!=(const Coeffs &rhs) const { return a1 != rhs.a1 || a2 != rhs.a2 || b0 != rhs.b0 || b1 != rhs.b1 || b2 != rhs.b2; }

    Coeffs &operator+=(const Coeffs &rhs) {
        a1 += rhs.a1;
        a2 += rhs.a2;
        b0 += rhs.b0;
        b1 += rhs.b1;
        b2 += rhs.b2;
        return *this;
    }
};

template <typename T>
inline Coeffs<T> operator-(const Coeffs<T> &a, const Coeffs<T> &b) {
    return {a.a1 - b.a1, a.a2 - b.a2, a.b0 - b.b0, a.b1 - b.b1, a.b2 - b.b2};
}

template <typename T>
inline Coeffs<T> operator*(T f, const Coeffs<T> &a) {
    return {f * a.a1, f * a.a2, f * a.b0, f * a.b1, f * a.b2};
}

template <typename T>
struct DF1State {
    T x1 = {0}, y1 = {0};
    T x2 = {0}, y2 = {0};
};

template <typename T>
inline void reset_state(DF1State<T> &state) {
    state = {0};
}

/// Calculate one sample of direct-form-I filtered audio
template <typename SampleT, typename CoeffT, typename StateT>
inline SampleT process_sample_d_f1(SampleT x0, const Coeffs<CoeffT> &cf, DF1State<StateT> &st) {
    auto y0 = cf.b0 * x0 + cf.b1 * st.x1 + cf.b2 * st.x2 - cf.a1 * st.y1 - cf.a2 * st.y2;
    st.x2 = st.x1;
    st.y2 = st.y1;
    st.x1 = x0;
    st.y1 = y0;
    return (SampleT) y0;
}

template <typename SampleT, typename CoeffT, typename StateT>
inline void process_block_d_f1(SampleT *smp, int num_samples, const Coeffs<CoeffT> &coeffs, DF1State<StateT> &state) {
    const auto cf = coeffs;
    auto st = state;
    for (int i = 0; i < num_samples; ++i) {
        smp[i] = process_sample_d_f1(smp[i], cf, st);
    }

    state = st;
}

template <typename SampleT, typename CoeffT, typename StateT>
inline void process_block_delta_d_f1(SampleT *smp, int num_samples, const Coeffs<CoeffT> &coeffs, const Coeffs<CoeffT> &coeffs_dt,
                                     DF1State<StateT> &state) {
    auto cf = coeffs;
    const auto cf_dt = coeffs_dt;
    auto st = state;
    for (int i = 0; i < num_samples; ++i) {
        smp[i] = process_sample_d_f1(smp[i], cf, st);
        cf += cf_dt;
    }
    state = st;
}

template <typename T>
struct DF2State {
    T w1 = {0}, w2 = {0};
};

template <typename T>
inline void reset_state(DF2State<T> &state) {
    state = {0};
}

template <typename SampleT, typename CoeffT, typename StateT>
inline SampleT process_sample_d_f2(SampleT x0, const Coeffs<CoeffT> &cf, DF2State<StateT> &st) {
    auto y0 = x0 * cf.b0 + st.w1;
    st.w1 = x0 * cf.b1 - y0 * cf.a1 + st.w2;
    st.w2 = x0 * cf.b2 - y0 * cf.a2;
    return (SampleT) y0;
}

//! Utility class to process a DF1 biquad with coefficient smoothing.
template <typename coeffT>
struct DF1Processor {
    DF1Processor() {
        set_smoothing_steps(32);
        reset();
    }

    void process(float **signal, int num_channels, int n) {
        assert(num_channels <= 2);
        int smooth_samples = std::min(smooth_n, n);
        if (smooth_samples > 0) {
            for (int ch = 0; ch < num_channels; ++ch) {
                process_block_delta_d_f1(signal[ch], smooth_samples, coeffs_value, coeffs_delta, state_value[ch]);
            }

            coeffs_value += coeffT(smooth_samples) * coeffs_delta;
            smooth_n -= smooth_samples;
            if (smooth_samples < n) {
                for (int ch = 0; ch < num_channels; ++ch) {
                    process_block_d_f1(signal[ch] + smooth_samples, n - smooth_samples, coeffs_target, state_value[ch]);
                }
            }
        } else {
            for (int ch = 0; ch < num_channels; ++ch) {
                process_block_d_f1(signal[ch], n, coeffs_target, state_value[ch]);
            }
        }
    }

    void process_mono_sample(float &s) {
        if (smooth_n <= 0) {
            s = process_sample_d_f1(s, coeffs_target, state_value[0]);
        } else {
            s = process_sample_d_f1(s, coeffs_value, state_value[0]);
            coeffs_value += coeffs_delta;
            smooth_n--;
        }
    }

    void process_stereo_sample(float &l, float &r) {
        if (smooth_n <= 0) {
            l = process_sample_d_f1(l, coeffs_target, state_value[0]);
            r = process_sample_d_f1(r, coeffs_target, state_value[1]);
        } else {
            l = process_sample_d_f1(l, coeffs_value, state_value[0]);
            r = process_sample_d_f1(r, coeffs_value, state_value[1]);
            coeffs_value += coeffs_delta;
            smooth_n--;
        }
    }

    void reset() {
        coeffs_value = coeffs_target;
        smooth_n = 0;
        for (int ch = 0; ch < 2; ++ch) {
            reset_state(state_value[ch]);
        }
    }

    void set_smoothing_steps(int n) { smoothing_steps = n; }

    void set_target_coefficients(const Coeffs<coeffT> &cf) {
        if (cf != coeffs_target) {
            coeffs_target = cf;
            coeffs_delta = (coeffT(1) / coeffT(smoothing_steps)) * (coeffs_target - coeffs_value);
            smooth_n = smoothing_steps;
        }
    }

private:
    DF1State<coeffT> state_value[2]; // max 2 channels for now
    Coeffs<coeffT> coeffs_target, coeffs_value, coeffs_delta;
    int smooth_n = 0;
    int smoothing_steps = 32;
};

#if 0
#pragma mark -
#pragma mark IIR Biquad standard type coefficient calculation
#endif

/**
  Calculate biquad coeffs that just apply a gain factor
 */
template <typename coeffT>
inline Coeffs<coeffT> create_gain_filter(coeffT gain_fact) {
    return {0.0, 0.0, gain_fact, 0.0, 0.0};
}

/**
 * Calculate biquad coeffs for a band shelfing filter
 *
 * @param cutOffFrequency Cutoff freq
 * @param Q Q-Value (sqrt(0.5) = Butterworth response)
 * @param sampleRate Sampling Rate
 * @param gainFact gainFactor at cutoff freq
 *
 * @return The coefficients of the biquad filter
 */
inline Coeffs<double> create_band_shelf_matched(double cut_off_frequency, double Q, double gain_fact, double sample_rate) {
    // From Vicanek: Matched Second Order Digital Filters.
    auto normf = cut_off_frequency / sample_rate;
    // this method cannot handle freqs above nyquist, matched-z poles would alias
    normf = std::min(normf, 0.5);

    Coeffs<double> cf;
    matched_zpole_coeffs(normf, std::sqrt(gain_fact) * Q, cf.a1, cf.a2);
    auto phi = phi_factors_for_freq(normf);
    auto A = phi_coeffs_for_biquad_coeffs(1.0, cf.a1, cf.a2);

    // zero coeffs calculated to match analog response
    auto r1 = (A[0] * phi[0] + A[1] * phi[1] + A[2] * phi[2]) * sq(gain_fact);
    auto r2 = (-A[0] + A[1] + 4 * (phi[0] - phi[1]) * A[2]) * sq(gain_fact);
    std::array<double, 3> B;
    B[0] = A[0];
    B[2] = 0.25 * (r1 - r2 * phi[1] - B[0]) / sq(phi[1]);
    B[1] = r2 + B[0] + 4 * (phi[1] - phi[0]) * B[2];

    biquad_coeffs_for_phi_coeffs(B, cf.b0, cf.b1, cf.b2);
    return cf;
}

/**
 * Calculate biquad coeffs for a 6db/oct low pass filter
 *
 * @param cutOffFrequency Cutoff freq
 * @param sampleRate Sampling Rate
 *
 * @return The coefficients of the biquad filter
 */
template <typename coeffT>
inline Coeffs<coeffT> create_low_pass1_p_matched(coeffT cut_off_frequency, coeffT sample_rate) {
    // adrian's method similar to simple Vicanek, but for 1 pole.
    auto normf = cut_off_frequency / sample_rate;

    Coeffs<coeffT> cf;
    coeffT w0 = normf * 6.28318530717958647692;
    cf.a1 = -std::exp(-w0);
    cf.a2 = 0;

    coeffT normf_sq = sq(normf);
    coeffT gn = std::sqrt(normf_sq / (coeffT(0.25) + normf_sq));
    // 1. gain 1 at DC
    // 2. match gain at nyquist
    cf.b0 = coeffT(0.5) * (gn * (1 - cf.a1) + 1 + cf.a1);
    cf.b1 = coeffT(1) + cf.a1 - cf.b0;
    cf.b2 = coeffT(0);
    return cf;
}

/**
 * Calculate biquad coeffs for a 6db/oct high pass filter
 *
 * @param cutOffFrequency Cutoff freq
 * @param sampleRate Sampling Rate
 *
 * @return The coefficients of the biquad filter
 */
template <typename coeffT>
inline Coeffs<coeffT> create_high_pass1_p_matched(coeffT cut_off_frequency, coeffT sample_rate) {
    // adrian's method similar to simple Vicanek, but for 1 pole.
    auto normf = cut_off_frequency / sample_rate;

    Coeffs<coeffT> cf;
    coeffT w0 = normf * 6.28318530717958647692;
    cf.a1 = -std::exp(-w0);
    cf.a2 = 0;

    coeffT gn = std::sqrt(0.25 / (0.25 + sq(normf))); // analog gain at nyquist
    // 1. gain 0 at DC
    // 2. match gain at nyquist
    cf.b0 = coeffT(0.5) * (gn * (1 - cf.a1));
    cf.b1 = -cf.b0;
    cf.b2 = 0.0;
    return cf;
}

/**
 * Calculate biquad coeffs for an allpass filter using BLT
 *
 * @param cutoffFrequency Cutoff freq
 * @param Q filter q
 * @param sampleRate Sampling Rate
 *
 * @return The coefficients of the biquad filter
 */
template <typename T>
inline Coeffs<T> create_allpass2_p(T cutoff_frequency, T Q, T sample_rate) {
    T w0 = 6.28318530717958647692 * cutoff_frequency / sample_rate;
    auto alpha = 0.5 * std::sin(w0) / Q;

    auto b0 = 1 - alpha;
    auto b1 = -2 * std::cos(w0);
    auto b2 = 1 + alpha;
    auto a0 = b2;
    auto a1 = b1;
    auto a2 = b0;
    auto ia0 = 1 / a0;

    Coeffs<T> cf;
    cf.b0 = b0 * ia0;
    cf.b1 = b1 * ia0;
    cf.b2 = b2 * ia0;
    cf.a1 = a1 * ia0;
    cf.a2 = a2 * ia0;
    return cf;
}

template <typename T>
inline Coeffs<T> create_high_shelf2_p(T freq, T gain_fact, T sample_rate) {
    auto w = (6.28318530717958647692 * freq) / sample_rate;
    auto alpha = std::sin(w) / sqrt(2.0);
    auto cosw = std::cos(w);
    auto A = std::sqrt(gain_fact);
    auto B = A + 1;
    auto C = 2 * std::sqrt(A) * alpha;
    auto D = (A - 1) * cosw;

    auto b0 = A * (B + D + C);
    auto b1 = -2 * A * ((A - 1) + B * cosw);
    auto b2 = A * (B + D - C);

    auto a0 = B - D + C;
    auto a1 = 2 * ((A - 1) - B * cosw);
    auto a2 = B - D - C;

    auto ia0 = 1 / a0;
    Coeffs<T> cf;
    cf.b0 = T(b0 * ia0);
    cf.b1 = T(b1 * ia0);
    cf.b2 = T(b2 * ia0);
    cf.a1 = T(a1 * ia0);
    cf.a2 = T(a2 * ia0);
    return cf;
}

/**
 * Calculate a three band tone control filter with variable crossover frequencies using two 2P shelfing filters
 * @param xoverFreqLM crossover freq between low and mid
 * @param xoverFreqMH crossover freq between mid and high
 * @param gainFactL gain factor for low band
 * @param gainFactM gain factor for middle band
 * @param gainFactH gain factor for high band
 */
template <typename T>
inline void create3_band_tone_control4_p(T xover_freq_lm, T xover_freq_mh, T gain_fact_l, T gain_fact_m, T gain_fact_h, T sample_rate,
                                         Coeffs<T> &cf1, Coeffs<T> &cf2) {
    cf1 = create_high_shelf2_p(xover_freq_lm, gain_fact_m / gain_fact_l, sample_rate);
    cf2 = create_high_shelf2_p(xover_freq_mh, gain_fact_h / gain_fact_m, sample_rate);

    // adjust gain to match at DC
    float gain_l = std::sqrt(gain_fact_l);
    cf1.b0 *= gain_l;
    cf1.b1 *= gain_l;
    cf1.b2 *= gain_l;
    cf2.b0 *= gain_l;
    cf2.b1 *= gain_l;
    cf2.b2 *= gain_l;
}
}} // namespace muse::audio::fx::IirBiquadFilter

#endif // MUSE_AUDIO_IIRBIQUADFILTER_H
