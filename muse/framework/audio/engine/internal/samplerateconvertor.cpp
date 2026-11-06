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
#include "samplerateconvertor.h"

#include <cmath>

#include "log.h"

using namespace muse::audio::engine;

SampleRateConvertor::SampleRateConvertor(const std::vector<float> &data, unsigned int channels_count, unsigned int sample_rate_in,
                                         unsigned int sample_rate_out)
    : data_(data), fir_(), channels_count_(channels_count), sample_rate_in_(sample_rate_in), sample_rate_out_(sample_rate_out) {
    fir_.resize(::muse::audio::engine::SampleRateConvertor::fir_length, 0);
    y_.resize(::muse::audio::engine::SampleRateConvertor::fir_length, 0);
    init_window();
}

std::vector<float> SampleRateConvertor::convert() {
    std::vector<float> out;
    auto result_samples = data_.size() * sample_rate_out_ / (channels_count_ * sample_rate_in_);

    out.resize(result_samples * channels_count_);
    for (unsigned sample = 0; sample < result_samples; ++sample) {
        for (unsigned int channel = 0; channel < channels_count_; ++channel) {
            out[sample * channels_count_ + channel] = y(sample, channel);
        }
    }

    return out;
}

unsigned int SampleRateConvertor::convert(float *buffer, unsigned int from, unsigned int count) {
    unsigned int sample = from;
    unsigned int converted = 0;

    for (converted = 0; converted < count; ++converted, ++sample) {
        for (unsigned int channel = 0; channel < channels_count_; ++channel) {
            if (!available_samples(sample)) {
                return converted;
            }
            buffer[converted * channels_count_ + channel] = y(sample, channel);
        }
    }
    return converted;
}

float SampleRateConvertor::y(unsigned int sample, unsigned int channel) const {
    switch (method_) {
        case SINC: return y_sinc(sample, channel);
        case FIR: return y_fir(sample, channel);
    }
    return 0.f;
}

void SampleRateConvertor::set_channel_count(unsigned int count) { channels_count_ = count; }

void SampleRateConvertor::set_sample_rate_in(unsigned int sample_rate) {
    if (sample_rate_in_ != sample_rate) {
        sample_rate_in_ = sample_rate;
        init_window();
    }
}

void SampleRateConvertor::set_sample_rate_out(unsigned int sample_rate) {
    if (sample_rate_out_ != sample_rate) {
        sample_rate_out_ = sample_rate;
        init_window();
    }
}

float SampleRateConvertor::y_sinc(unsigned int sample, unsigned int channel) const {
    double current_output_sample_time = sample / static_cast<double>(sample_rate_out_);
    auto zero_input_sample = std::floor(current_output_sample_time * sample_rate_in_);

    float out = 0.f;
    for (unsigned int i = 0; i < ::muse::audio::engine::SampleRateConvertor::use_samples; ++i) {
        int current_sample = zero_input_sample + i - ::muse::audio::engine::SampleRateConvertor::use_samples / 2;
        double current_input_sample_time = current_sample / static_cast<double>(sample_rate_in_);

        if (current_sample < 0 || current_sample * channels_count_ + channel >= data_.size()) {
            continue;
        }
        out += data_.at(current_sample * channels_count_ + channel) * sinc(current_output_sample_time - current_input_sample_time);
    }
    return out;
}

float SampleRateConvertor::y_fir(unsigned int sample, unsigned int channel) const {
    auto x = [this, &channel](int m) -> float {
        int pos = m * m_ / l_ * channels_count_ + channel;
        if (pos >= 0 && pos < static_cast<int>(data_.size())) {
            return data_.at(pos);
        }
        return 0.f;
    };

    float y = 0.f;
    for (unsigned int i = 0; i < ::muse::audio::engine::SampleRateConvertor::fir_length; ++i) {
        int t = sample - static_cast<int>(i);
        y += x(t) * fir_[i];
    }
    return y;
}

bool SampleRateConvertor::available_samples(unsigned int sample) const {
    float current_output_sample_time = sample / static_cast<float>(sample_rate_out_);
    auto first_input_sample =
        std::floor(current_output_sample_time * sample_rate_in_) - ::muse::audio::engine::SampleRateConvertor::use_samples / 2;

    // first sample for conversion points out of the input buffer
    if (first_input_sample * channels_count_ >= data_.size()) {
        return false;
    }
    return true;
}

inline double zero_bessel(double x) {
    //return std::cyl_bessel_i(0, x);

    double s = 1, y = 1;
    int m = 1;

    while (y > std::numeric_limits<float>::min()) {
        m += 2;
        y *= x * x / (4 * m * m);
        s += s * y;
    }

    return s;
}

void SampleRateConvertor::init_window() {
    int max = std::max(sample_rate_in_, sample_rate_out_), min = std::min(sample_rate_in_, sample_rate_out_);
    int max_common_divider = 1;
    while (max % min != 0) {
        max_common_divider = max % min;
        max = min;
        min = max_common_divider;
    }
    m_ = sample_rate_in_ / max_common_divider;
    l_ = sample_rate_out_ / max_common_divider;
    double f_stop = std::min(sample_rate_in_, sample_rate_out_) / 2, f_intermediate_sample_rate = sample_rate_in_ * m_,
           attenuation = 96 /*dB*/;
    int M = ::muse::audio::engine::SampleRateConvertor::fir_length;
    int np = (M - 1) / 2;

    double alpha = 0.1102 * (attenuation - 8.7);
    double A[::muse::audio::engine::SampleRateConvertor::fir_length];

    A[0] = 2 * f_stop / f_intermediate_sample_rate;
    for (int j = 1; j <= np; j++) {
        A[j] = std::sin(2 * j * M_PI * f_stop / f_intermediate_sample_rate) / j * M_PI;
    }

    for (int j = 0; j <= np; j++) {
        fir_[np + j] = A[j];
        fir_[np + j] *= zero_bessel(alpha * std::sqrt(1 - (j * j / (np * np))));
        fir_[np + j] /= zero_bessel(alpha);
    }

    for (int j = 0; j < np; j++) {
        fir_[j] = fir_[M - 1 - j];
    }
}

double SampleRateConvertor::sinc(double value) const {
    if (value == 0) {
        return 1.f;
    }
    double sinc_arg = M_PI * value * sample_rate_in_;
    double gain = std::min(1.0, sample_rate_out_ / static_cast<double>(sample_rate_in_));
    return gain * std::sin(sinc_arg) / sinc_arg;
}
