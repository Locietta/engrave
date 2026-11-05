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

SampleRateConvertor::SampleRateConvertor(const std::vector<float> &data, unsigned int channelsCount, unsigned int sampleRateIn,
                                         unsigned int sampleRateOut)
    : data_(data), fir_(), channelsCount_(channelsCount), sampleRateIn_(sampleRateIn), sampleRateOut_(sampleRateOut) {
    fir_.resize(FIR_LENGTH, 0);
    y_.resize(FIR_LENGTH, 0);
    initWindow();
}

std::vector<float> SampleRateConvertor::convert() {
    std::vector<float> out;
    auto resultSamples = data_.size() * sampleRateOut_ / (channelsCount_ * sampleRateIn_);

    out.resize(resultSamples * channelsCount_);
    for (unsigned sample = 0; sample < resultSamples; ++sample) {
        for (unsigned int channel = 0; channel < channelsCount_; ++channel) {
            out[sample * channelsCount_ + channel] = y(sample, channel);
        }
    }

    return out;
}

unsigned int SampleRateConvertor::convert(float *buffer, unsigned int from, unsigned int count) {
    unsigned int sample = from;
    unsigned int converted = 0;

    for (converted = 0; converted < count; ++converted, ++sample) {
        for (unsigned int channel = 0; channel < channelsCount_; ++channel) {
            if (!availableSamples(sample)) {
                return converted;
            }
            buffer[converted * channelsCount_ + channel] = y(sample, channel);
        }
    }
    return converted;
}

float SampleRateConvertor::y(unsigned int sample, unsigned int channel) const {
    switch (method_) {
        case SINC: return ySinc(sample, channel);
        case FIR: return yFIR(sample, channel);
    }
    return 0.f;
}

void SampleRateConvertor::setChannelCount(unsigned int count) { channelsCount_ = count; }

void SampleRateConvertor::setSampleRateIn(unsigned int sampleRate) {
    if (sampleRateIn_ != sampleRate) {
        sampleRateIn_ = sampleRate;
        initWindow();
    }
}

void SampleRateConvertor::setSampleRateOut(unsigned int sampleRate) {
    if (sampleRateOut_ != sampleRate) {
        sampleRateOut_ = sampleRate;
        initWindow();
    }
}

float SampleRateConvertor::ySinc(unsigned int sample, unsigned int channel) const {
    double currentOutputSampleTime = sample / static_cast<double>(sampleRateOut_);
    auto zeroInputSample = std::floor(currentOutputSampleTime * sampleRateIn_);

    float out = 0.f;
    for (unsigned int i = 0; i < USE_SAMPLES; ++i) {
        int currentSample = zeroInputSample + i - USE_SAMPLES / 2;
        double currentInputSampleTime = currentSample / static_cast<double>(sampleRateIn_);

        if (currentSample < 0 || currentSample * channelsCount_ + channel >= data_.size()) {
            continue;
        }
        out += data_.at(currentSample * channelsCount_ + channel) * sinc(currentOutputSampleTime - currentInputSampleTime);
    }
    return out;
}

float SampleRateConvertor::yFIR(unsigned int sample, unsigned int channel) const {
    auto x = [this, &channel](int m) -> float {
        int pos = m * M_ / L_ * channelsCount_ + channel;
        if (pos >= 0 && pos < static_cast<int>(data_.size())) {
            return data_.at(pos);
        }
        return 0.f;
    };

    float y = 0.f;
    for (unsigned int i = 0; i < FIR_LENGTH; ++i) {
        int t = sample - static_cast<int>(i);
        y += x(t) * fir_[i];
    }
    return y;
}

bool SampleRateConvertor::availableSamples(unsigned int sample) const {
    float currentOutputSampleTime = sample / static_cast<float>(sampleRateOut_);
    auto firstInputSample = std::floor(currentOutputSampleTime * sampleRateIn_) - USE_SAMPLES / 2;

    // first sample for conversion points out of the input buffer
    if (firstInputSample * channelsCount_ >= data_.size()) {
        return false;
    }
    return true;
}

inline double zeroBessel(double x) {
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

void SampleRateConvertor::initWindow() {
    int max = std::max(sampleRateIn_, sampleRateOut_), min = std::min(sampleRateIn_, sampleRateOut_);
    int maxCommonDivider = 1;
    while (max % min != 0) {
        maxCommonDivider = max % min;
        max = min;
        min = maxCommonDivider;
    }
    M_ = sampleRateIn_ / maxCommonDivider;
    L_ = sampleRateOut_ / maxCommonDivider;
    double fStop = std::min(sampleRateIn_, sampleRateOut_) / 2, fIntermediateSampleRate = sampleRateIn_ * M_, attenuation = 96 /*dB*/;
    int M = FIR_LENGTH;
    int Np = (M - 1) / 2;

    double alpha = 0.1102 * (attenuation - 8.7);
    double A[FIR_LENGTH];

    A[0] = 2 * fStop / fIntermediateSampleRate;
    for (int j = 1; j <= Np; j++) {
        A[j] = std::sin(2 * j * M_PI * fStop / fIntermediateSampleRate) / j * M_PI;
    }

    for (int j = 0; j <= Np; j++) {
        fir_[Np + j] = A[j];
        fir_[Np + j] *= zeroBessel(alpha * std::sqrt(1 - (j * j / (Np * Np))));
        fir_[Np + j] /= zeroBessel(alpha);
    }

    for (int j = 0; j < Np; j++) {
        fir_[j] = fir_[M - 1 - j];
    }
}

double SampleRateConvertor::sinc(double value) const {
    if (value == 0) {
        return 1.f;
    }
    double sincArg = M_PI * value * sampleRateIn_;
    double gain = std::min(1.0, sampleRateOut_ / static_cast<double>(sampleRateIn_));
    return gain * std::sin(sincArg) / sincArg;
}
