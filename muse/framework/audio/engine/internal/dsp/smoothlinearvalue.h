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
    SmoothLinearValue(const ValueT &startValue = ValueT(0)) {
        static_assert(std::is_floating_point<StepT>::value, "Internal step type needs to be floating point.");
        targetValue_ = currentValue_ = startValue;
        currentSteps_ = (StepT) _initialSteps;
        targetSteps_ = 0;
        deltaValue_ = 0;
    }

    void setSteps(int steps) { currentSteps_ = (StepT) std::max(1, steps); }

    /** Advance one tick by just adding the delta. Can lead to error accumulation.
     * @return true if target was reached
     */
    bool tick() {
        if (targetSteps_ <= 0) {
            currentValue_ = targetValue_; // get rid of error accumulation
            return true;
        }

        targetSteps_--;
        currentValue_ += deltaValue_;
        return false;
    }

    /** Advance one tick and use multiplication to avoid error accumulation
     * @return true if target was reached
     */
    bool preciseTick() {
        if (targetSteps_ <= 0) {
            return true;
        }

        targetSteps_--;
        currentValue_ = targetValue_ - targetSteps_ * deltaValue_; // if n reaches 0, _value will be exactly targetvalue
        return false;
    }

    /** Advance one tick without checking whether target was reached
     * can lead to error accumulation for large n and small delta
     * warning! will overshoot target if called too many times
     */
    void tickUnchecked() {
        targetSteps_--;
        currentValue_ += deltaValue_;
    }

    /** Advance one tick without checking whether target was reached
     * uses an extra multiplication to avoid error accumulation
     * warning! will overshoot target if called too many times
     */
    void preciseTickUnchecked() {
        targetSteps_--;
        currentValue_ = targetValue_ - targetSteps_ * deltaValue_;
    }

    /** Advance multiple ticks
     *  @param n num ticks to advance
     */
    void advanceTicks(int n) {
        targetSteps_ -= (double) n;
        if (targetSteps_ < 0) {
            targetSteps_ = 0;
        }
        currentValue_ = targetValue_ - targetSteps_ * deltaValue_;
    }

    /// Set the value instantly (for initialization)
    void initWithValue(const ValueT &val) {
        targetValue_ = currentValue_ = val;
        targetSteps_ = 0;
    }

    /// Set value and target instantly
    void initWithValueAndTarget(const ValueT &val, const ValueT &target) {
        initWithValue(val);
        setTargetValue(target);
    }

    /** Sets the target value which is reached after "steps" calls to smoothTick */
    void setTargetValue(const ValueT &val) {
        if (targetValue_ != val) {
            targetValue_ = val;
            deltaValue_ = ValueT((1.0 / currentSteps_) * (targetValue_ - currentValue_));
            targetSteps_ = currentSteps_;
        }
    }

    /** @returns the value the smoothed value will reach eventually */
    const ValueT &getTargetValue() const { return targetValue_; }

    /** @return the current (smoothed) value */
    inline const ValueT &getValue() const { return currentValue_; }

    /** Go to target immediately.
     * @returns value
     */
    inline const ValueT &setToTarget() {
        currentValue_ = targetValue_;
        targetSteps_ = 0.f;
        return currentValue_;
    }

    /// check if the smoothed value isn't moving and has a specific (target)value
    bool isStaticAtValue(const ValueT &static_value) const { return isAtTargetValue() && (getTargetValue() == static_value); }

    inline bool isAtTargetValue() const { return targetSteps_ <= (StepT) 0; }

    int getRemainingSteps() const { return (int) targetSteps_; }

    int smoothingSteps() const { return (int) currentSteps_; }

protected:
    ValueT currentValue_;
    ValueT targetValue_;
    ValueT deltaValue_;
    StepT targetSteps_;
    StepT currentSteps_;
};
} // namespace muse::audio::dsp

#endif // MUSE_AUDIO_SMOOTHLINEARVALUE_H
