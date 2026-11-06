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
#ifndef MUSE_AUDIO_SAMPLERATECONVERTOR_H
#define MUSE_AUDIO_SAMPLERATECONVERTOR_H

#include <vector>
#include <deque>

namespace muse::audio::engine {
class SampleRateConvertor {
public:
    enum Method { SINC, FIR };

    explicit SampleRateConvertor(const std::vector<float> &data, unsigned int channels_count, unsigned int sample_rate_in,
                                 unsigned int sample_rate_out);

    //! offline convert full data set
    std::vector<float> convert();

    //! online convert
    unsigned int convert(float *buffer, unsigned int from, unsigned int count);

    void set_channel_count(unsigned int count);
    void set_sample_rate_in(unsigned int sample_rate);
    void set_sample_rate_out(unsigned int sample_rate);

private:

    //! return value of sinc function (LPF) with consideration of used sampleRates
    double sinc(double value) const;

    //! output sample value
    float y(unsigned int sample, unsigned int channel) const;
    float y_sinc(unsigned int sample, unsigned int channel) const;
    float y_fir(unsigned int sample, unsigned int channel) const;

    //! return true if there are samples in input buffer for conversion
    bool available_samples(unsigned int sample) const;

    //! calculate window function values
    void init_window();

    const static unsigned int use_samples = 16; //!< this value defines the quality and complexity of SRC.
    const std::vector<float> &data_;

    //! values of window function for USE_SAMPLES
    const static unsigned int fir_length = 33;
    unsigned int m_ = 1, l_ = 1;
    std::vector<float> fir_;
    mutable std::deque<float> y_;

    unsigned int channels_count_ = 0;
    unsigned int sample_rate_in_ = 0;
    unsigned int sample_rate_out_ = 0;
    Method method_ = FIR;
};
} // namespace muse::audio::engine

#endif // MUSE_AUDIO_SAMPLERATECONVERTOR_H
