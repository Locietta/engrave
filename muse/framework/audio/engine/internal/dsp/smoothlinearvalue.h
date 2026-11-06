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

#ifndef MUSE_AUDIO_SMOOTHLINEARVALUE_H
#define MUSE_AUDIO_SMOOTHLINEARVALUE_H

#include <algorithm>
#include <type_traits>

namespace muse::audio::dsp {
template <typename ValueT, int _initialSteps = 1024, typename StepT = double>
class SmoothLinearValue {
public:
    SmoothLinearValue(const ValueT &start_value = ValueT(0)) {
        static_assert(std::is_floating_point<StepT>::value, "Internal step type needs to be floating point.");
        target_value_ = current_value_ = start_value;
        current_steps_ = (StepT) _initialSteps;
        target_steps_ = 0;
        delta_value_ = 0;
    }

    void set_steps(int steps) { current_steps_ = (StepT) std::max(1, steps); }

    /** Advance one tick by just adding the delta. Can lead to error accumulation.
     * @return true if target was reached
     */
    bool tick() {
        if (target_steps_ <= 0) {
            current_value_ = target_value_; // get rid of error accumulation
            return true;
        }

        target_steps_--;
        current_value_ += delta_value_;
        return false;
    }

    /** Advance one tick and use multiplication to avoid error accumulation
     * @return true if target was reached
     */
    bool precise_tick() {
        if (target_steps_ <= 0) {
            return true;
        }

        target_steps_--;
        current_value_ = target_value_ - target_steps_ * delta_value_; // if n reaches 0, _value will be exactly targetvalue
        return false;
    }

    /** Advance one tick without checking whether target was reached
     * can lead to error accumulation for large n and small delta
     * warning! will overshoot target if called too many times
     */
    void tick_unchecked() {
        target_steps_--;
        current_value_ += delta_value_;
    }

    /** Advance one tick without checking whether target was reached
     * uses an extra multiplication to avoid error accumulation
     * warning! will overshoot target if called too many times
     */
    void precise_tick_unchecked() {
        target_steps_--;
        current_value_ = target_value_ - target_steps_ * delta_value_;
    }

    /** Advance multiple ticks
     *  @param n num ticks to advance
     */
    void advance_ticks(int n) {
        target_steps_ -= (double) n;
        if (target_steps_ < 0) {
            target_steps_ = 0;
        }
        current_value_ = target_value_ - target_steps_ * delta_value_;
    }

    /// Set the value instantly (for initialization)
    void init_with_value(const ValueT &val) {
        target_value_ = current_value_ = val;
        target_steps_ = 0;
    }

    /// Set value and target instantly
    void init_with_value_and_target(const ValueT &val, const ValueT &target) {
        init_with_value(val);
        set_target_value(target);
    }

    /** Sets the target value which is reached after "steps" calls to smoothTick */
    void set_target_value(const ValueT &val) {
        if (target_value_ != val) {
            target_value_ = val;
            delta_value_ = ValueT((1.0 / current_steps_) * (target_value_ - current_value_));
            target_steps_ = current_steps_;
        }
    }

    /** @returns the value the smoothed value will reach eventually */
    const ValueT &get_target_value() const { return target_value_; }

    /** @return the current (smoothed) value */
    inline const ValueT &get_value() const { return current_value_; }

    /** Go to target immediately.
     * @returns value
     */
    inline const ValueT &set_to_target() {
        current_value_ = target_value_;
        target_steps_ = 0.f;
        return current_value_;
    }

    /// check if the smoothed value isn't moving and has a specific (target)value
    bool is_static_at_value(const ValueT &static_value) const { return is_at_target_value() && (get_target_value() == static_value); }

    inline bool is_at_target_value() const { return target_steps_ <= (StepT) 0; }

    int get_remaining_steps() const { return (int) target_steps_; }

    int smoothing_steps() const { return (int) current_steps_; }

protected:
    ValueT current_value_;
    ValueT target_value_;
    ValueT delta_value_;
    StepT target_steps_;
    StepT current_steps_;
};
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_SMOOTHLINEARVALUE_H
