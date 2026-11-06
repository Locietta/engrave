#include "pitchwheelrenderer.h"

#include <algorithm>

#include "log.h"

using namespace mu::engraving;

PitchWheelRenderer::PitchWheelRenderer(PitchWheelSpecs wheel_spec) : wheel_spec_value(wheel_spec) {}

void PitchWheelRenderer::add_pitch_wheel_function(const PitchWheelFunction &function, uint32_t channel, staff_idx_t staff_idx,
                                                  MidiInstrumentEffect effect) {
    PitchWheelFunctions &functions = functions_value[channel];
    effect_by_channel[channel] = effect;
    staff_idx_by_channel[channel] = staff_idx;

    if (function.m_start_tick < functions.start_tick) {
        functions.start_tick = function.m_start_tick;
    }
    if (function.m_end_tick > functions.end_tick) {
        functions.end_tick = function.m_end_tick;
    }

    functions.functions.push_back(function);
}

EventsHolder PitchWheelRenderer::render_pitch_wheel() const noexcept {
    EventsHolder pitch_wheel_events;

    for (const auto &function : functions_value) {
        render_channel_pitch_wheel(pitch_wheel_events, function.second, function.first);
    }

    return pitch_wheel_events;
}

//! MARK: PRIVATE METHODS

void PitchWheelRenderer::render_channel_pitch_wheel(EventsHolder &pitch_wheel_events, const PitchWheelFunctions &functions,
                                                    uint32_t channel) const noexcept {
    if (functions.end_tick < functions.start_tick) {
        return;
    }

    MidiInstrumentEffect effect = MidiInstrumentEffect::NONE;
    if (effect_by_channel.find(channel) != effect_by_channel.end()) {
        effect = effect_by_channel.at(channel);
    }

    bool staff_info_valid = false;
    staff_idx_t staff_idx = 0;

    if (staff_idx_by_channel.find(channel) != staff_idx_by_channel.end()) {
        staff_info_valid = true;
        staff_idx = staff_idx_by_channel.at(channel);
    }

    std::map<int, int, std::greater<>> ranges;
    generate_ranges(functions.functions, ranges);

    for (auto rit = ranges.crbegin(); rit != ranges.crend(); ++rit) {
        int32_t start = rit->first;
        int32_t end = rit->second;
        int32_t tick = start;

        std::vector<PitchWheelFunction> functions_to_process;
        functions_to_process.reserve(functions.functions.size());
        for (const auto &func : functions.functions) {
            if (func.m_end_tick <= end) {
                functions_to_process.push_back(func);
            }
        }
        std::vector<int> pitches;
        pitches.reserve(functions_to_process.size());
        for (size_t i = 0; i < functions_to_process.size(); ++i) {
            pitches.push_back(0);
        }
        int prev_pitch = wheel_spec_value.m_limit;
        bool force_update = false;
        while (tick < end) {
            auto func_it = functions_to_process.begin();
            for (size_t i = 0; i < functions_to_process.size(); ++i) {
                if (tick >= func_it->m_end_tick) {
                    // Function exceeds its max range
                    // don't need its value anymore
                    pitches.at(i) = 0;
                    ++func_it;
                    force_update = true;
                    continue;
                }
                if (tick < func_it->m_start_tick) {
                    ++func_it;
                    continue;
                }
                int pitch = func_it->func(tick);
                pitches.at(i) = pitch;
                ++func_it;
            }
            int final_pitch = wheel_spec_value.m_limit;
            for (const auto &pitch : pitches) {
                final_pitch += pitch;
            }
            final_pitch = std::clamp(final_pitch, 0, 2 * wheel_spec_value.m_limit - 1);
            if (force_update || final_pitch != prev_pitch || tick == start) {
                NPlayEvent evb(ME_PITCHBEND, channel, final_pitch % 128, final_pitch / 128);
                evb.set_effect(effect);
                if (staff_info_valid) {
                    evb.set_originating_staff(staff_idx);
                }
                pitch_wheel_events[channel].emplace_hint(pitch_wheel_events[channel].end(), std::make_pair(tick, evb));
                force_update = false;
            }

            prev_pitch = final_pitch;
            tick += wheel_spec_value.m_step;
        }
    }
}

int32_t PitchWheelRenderer::find_next_start_tick(const std::vector<PitchWheelFunction> &functions) const noexcept {
    int32_t tick = std::numeric_limits<int32_t>::max();
    for (const auto &func : functions) {
        tick = std::min(tick, func.m_start_tick);
    }

    return tick;
}

int32_t PitchWheelRenderer::calculate_pitch_bend(const std::vector<PitchWheelFunction> &functions, int32_t tick) const noexcept {
    int pitch_value = wheel_spec_value.m_limit;

    for (const auto &func : functions) {
        if (tick < func.m_start_tick || tick >= func.m_end_tick) {
            continue;
        }

        pitch_value += func.func(tick);
    }

    return pitch_value;
}

// 1                |------|
// 2                   |-----------| handleStartTick();
// result           |--------------|
// 3           |--------|            handleEndTick();
// result      |-------------------|
void PitchWheelRenderer::generate_ranges(const std::vector<PitchWheelFunction> &functions, std::map<int, int, std::greater<>> &ranges) {
    // !NOTE ranges map is reversed. Use reverse iterators
    auto handle_end_tick = [&](const PitchWheelFunction &func) {
        auto lower_range = ranges.upper_bound(func.m_end_tick);
        if (lower_range == ranges.end()) {
            return false;
        }
        ranges.insert({func.m_start_tick, lower_range->second});
        ranges.erase(lower_range);
        return true;
    };

    auto handle_start_tick = [&](const PitchWheelFunction &func) {
        auto lower_range = ranges.upper_bound(func.m_start_tick);
        if (lower_range == ranges.end()) {
            // We are getting PW events sorted by startTick
            // So in ideal world handleEndTick always return false
            return handle_end_tick(func);
        }
        if (lower_range->second >= func.m_start_tick) {
            lower_range->second = std::max(lower_range->second, func.m_end_tick);
            return true;
        }
        return false;
    };

    for (const auto &func : functions) {
        if (auto key = ranges.find(func.m_start_tick); key != ranges.end()) {
            key->second = std::max(key->second, func.m_end_tick);
            continue;
        }
        if (!handle_start_tick(func)) {
            ranges.insert({func.m_start_tick, func.m_end_tick});
        }
    }
}
