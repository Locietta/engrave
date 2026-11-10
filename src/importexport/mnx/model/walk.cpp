// SPDX-License-Identifier: GPL-3.0-only
#include "walk.h"

#include <type_traits>
#include <variant>

#include "music.h"

namespace mu::iex::mnxio::model {

namespace {

bool walk_content(const std::vector<SequenceItem> &content, const WalkHooks &hooks, WalkContext &context);

// The events of a grace note or tremolo, reported through on_event only.
bool walk_events(const std::vector<Event> &events, const WalkHooks &hooks, WalkContext &context) {
    for (const auto &event : events) {
        const Rational start = context.elapsed;
        const Rational duration = duration_of(event.duration) * context.ratio;
        if (hooks.on_event && !hooks.on_event(event, start, duration, context)) return false;
        context.elapsed += duration;
    }
    return true;
}

bool walk_item(const SequenceItem &item, const WalkHooks &hooks, WalkContext &context) {
    bool children = true;
    if (hooks.on_item) {
        const WalkControl control = hooks.on_item(item, context);
        if (control == WalkControl::STOP) return false;
        children = control != WalkControl::SKIP_CHILDREN;
    }
    const bool walked = std::visit(
        [&]<class T>(const T &value) -> bool {
            if constexpr (std::is_same_v<T, Event>) {
                const Rational start = context.elapsed;
                const Rational duration = duration_of(value.duration) * context.ratio;
                if (hooks.on_event && !hooks.on_event(value, start, duration, context)) return false;
                context.elapsed += duration;
            } else if constexpr (std::is_same_v<T, Grace>) {
                if (children) {
                    WalkContext child = context;
                    child.ratio = Rational(0);
                    child.in_grace = true;
                    if (!walk_events(value.content, hooks, child)) return false;
                    // Grace notes take no time.
                }
            } else if constexpr (std::is_same_v<T, Tuplet>) {
                if (children) {
                    WalkContext child = context;
                    child.ratio = context.ratio * tuplet_ratio(value);
                    if (!walk_content(value.content, hooks, child)) return false;
                    context.elapsed = child.elapsed;
                } else {
                    context.elapsed += duration_of(value.outer) * context.ratio;
                }
            } else if constexpr (std::is_same_v<T, MultiNoteTremolo>) {
                const Rational span = duration_of(value.outer) * context.ratio;
                if (children) {
                    const Rational start = context.elapsed;
                    WalkContext child = context;
                    child.ratio = context.ratio / Rational(value.outer.multiple);
                    if (!walk_events(value.content, hooks, child)) return false;
                    // The tremolo spans its outer value whatever its events add up to.
                    context.elapsed = start + span;
                } else {
                    context.elapsed += span;
                }
            } else if constexpr (std::is_same_v<T, Space>) {
                context.elapsed += Rational(value.duration) * context.ratio;
            }
            return true;
        },
        item);
    if (!walked) return false;
    if (hooks.on_after_item) hooks.on_after_item(item, context);
    return true;
}

bool walk_content(const std::vector<SequenceItem> &content, const WalkHooks &hooks, WalkContext &context) {
    for (const auto &item : content)
        if (!walk_item(item, hooks, context)) return false;
    return true;
}

} // namespace

bool walk_sequence(const Sequence &sequence, const Rational &full_measure_length, const WalkHooks &hooks, WalkContext *context) {
    WalkContext local;
    WalkContext &c = context ? *context : local;
    if (sequence.full_measure) {
        const Rational start = c.elapsed;
        if (hooks.on_full_measure && !hooks.on_full_measure(*sequence.full_measure, start, full_measure_length, c)) return false;
        c.elapsed += full_measure_length;
    }
    return walk_content(sequence.content, hooks, c);
}

} // namespace mu::iex::mnxio::model
