/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "automationrw.h"

#include <charconv>
#include <cmath>
#include <cstdint>

#include <glaze/json/write.hpp>

#include "engraving/automation/automationdata.h"
#include "global/log.h"
#include "global/serialization/json_fields.h"

using namespace mu::engraving;

namespace {

namespace json = muse::json;

constexpr std::pair<AutomationType, std::string_view> automation_types[]{
    {AutomationType::DYNAMICS, "Dynamics"},
    {AutomationType::TEMPO, "Tempo"},
    {AutomationType::VOLUME, "Volume"},
    {AutomationType::PAN, "Pan"},
};

constexpr std::string_view from_previous = "FromPrevious";

struct ControlPoint {
    glz::raw_json t;
    glz::raw_json value;

    struct glaze {
        static constexpr auto value = glz::object("t", &ControlPoint::t, "value", &ControlPoint::value);
    };
};

struct Point {
    std::optional<std::array<ControlPoint, 1>> ease;
    std::optional<bool> generated;
    std::optional<glz::raw_json> in_value;
    std::optional<std::string_view> in_value_kind;
    std::optional<std::string> item_id;
    glz::raw_json out_value;
    utick_t tick = 0;

    struct glaze {
        using T = Point;
        static constexpr auto value = glz::object("ease", &T::ease, "generated", &T::generated, "inValue", &T::in_value, "inValueKind",
                                                  &T::in_value_kind, "itemId", &T::item_id, "outValue", &T::out_value, "tick", &T::tick);
    };
};

struct Curve {
    std::optional<std::string> instrument_id;
    std::optional<std::string> part_id;
    std::vector<Point> points;
    std::optional<std::string> staff_id;
    std::string_view type;
    std::optional<int> voice_id;

    struct glaze {
        using T = Curve;
        static constexpr auto value = glz::object("instrumentId", &T::instrument_id, "partId", &T::part_id, "points", &T::points, "staffId",
                                                  &T::staff_id, "type", &T::type, "voiceId", &T::voice_id);
    };
};

muse::ID read_id(const glz::generic &field) {
    const auto &text = json::get_or_empty<std::string>(field);
    std::uint64_t value = 0;
    const auto end = text.data() + text.size();
    const auto [next, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && next == end ? muse::ID(value) : muse::ID{};
}

AutomationCurveKey read_key(const glz::generic &curve) {
    const auto &type_name = json::get_or_empty<std::string>(json::member(curve, "type"));
    AutomationType type = AutomationType::UNKNOWN;
    for (const auto &[candidate, name] : automation_types) {
        if (name == type_name) {
            type = candidate;
            break;
        }
    }

    if (const auto *part = json::find(curve, "partId")) {
        return AutomationCurveKey::instrument(
            type, {read_id(*part), muse::String::from_std_string(json::get_or_empty<std::string>(json::member(curve, "instrumentId")))});
    }
    if (const auto *staff = json::find(curve, "staffId")) {
        std::optional<size_t> voice_idx;
        if (const auto voice = json::rounded_int(json::member(curve, "voiceId")); voice && *voice >= 0) {
            voice_idx = static_cast<size_t>(*voice);
        }
        return AutomationCurveKey::staff(type, read_id(*staff), voice_idx);
    }
    return AutomationCurveKey::global(type);
}

double number(const glz::generic &object, std::string_view key) { return json::get_or_empty<double>(json::member(object, key)); }

AutomationPoint read_point(const glz::generic &source) {
    AutomationPoint point;
    point.value.out_value = number(source, "outValue");

    if (json::get_or_empty<std::string>(json::member(source, "inValueKind")) == from_previous) {
        point.value.in_value = AutomationPoint::ArrivalFromPrevious{};
    } else {
        AutomationPoint::Ease ease;
        const auto &controls = json::array(json::member(source, "ease"));
        if (!controls.empty()) {
            const auto &control = controls.front();
            ease.t = number(control, "t");
            ease.value = number(control, "value");
        }
        point.value.in_value = AutomationPoint::ExplicitArrival{number(source, "inValue"), ease};
    }

    if (const auto *item = json::find(source, "itemId")) {
        point.item_id = EID::from_std_string(json::get_or_empty<std::string>(*item));
    }
    if (const auto *generated = json::find(source, "generated")) {
        if (const auto *flag = generated->get_if<bool>())
            point.generated = *flag;
        else if (const auto *numeric = generated->get_if<double>())
            point.generated = *numeric != 0;
        else if (const auto *str = generated->get_if<std::string>())
            point.generated = !str->empty();
        else
            point.generated = !generated->is_null();
    }
    return point;
}

// Older MSCZ writers stored decimals with up to six places, without exponents.
glz::raw_json automation_number(double value) {
    if (!std::isfinite(value)) throw std::overflow_error("Non-finite automation point");
    if (std::abs(value) < 0x1p53 && std::trunc(value) == value) {
        return {std::to_string(static_cast<std::int64_t>(value))};
    }

    // A fixed decimal can need all 309 significant integer digits of a double.
    char buffer[320];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 6);
    if (error != std::errc{}) throw std::overflow_error("Cannot write automation point");
    const char *trimmed = end;
    while (trimmed[-1] == '0') --trimmed;
    if (trimmed[-1] == '.') --trimmed;
    return {std::string(buffer, static_cast<size_t>(trimmed - buffer))};
}

Point write_point(utick_t tick, const AutomationPoint &point) {
    Point result;
    result.tick = tick;
    result.out_value = automation_number(point.value.out_value.raw());
    if (const auto *arrival = std::get_if<AutomationPoint::ExplicitArrival>(&point.value.in_value)) {
        result.in_value = automation_number(arrival->value.raw());
        if (!arrival->ease.is_none()) {
            result.ease = std::array{ControlPoint{automation_number(arrival->ease.t.raw()), automation_number(arrival->ease.value.raw())}};
        }
    } else {
        result.in_value_kind = from_previous;
    }
    if (point.item_id) result.item_id = point.item_id->to_std_string();
    if (point.generated) result.generated = true;
    return result;
}

Curve write_key(const AutomationCurveKey &key) {
    Curve result;
    for (const auto &[type, name] : automation_types) {
        if (type == key.type) {
            result.type = name;
            break;
        }
    }
    if (const auto track = key.track_id()) {
        result.part_id = track->part_id.to_std_string();
        result.instrument_id = track->instrument_id.to_std_string();
    } else if (const auto staff = key.staff_id()) {
        result.staff_id = staff->to_std_string();
        if (const auto voice = key.voice_idx()) result.voice_id = static_cast<int>(*voice);
    }
    return result;
}

} // namespace

void AutomationRW::read(AutomationData &data, const muse::ByteArray &bytes) {
    if (bytes.empty()) return;

    const auto source = json::text(bytes);
    const auto parsed = glz::read_json<glz::generic>(source);
    if (!parsed) {
        LOGE() << "Failed to parse automation JSON: " << glz::format_error(parsed.error(), source);
        return;
    }
    if (!parsed->is_array()) {
        LOGE() << "Automation JSON must contain an array";
        return;
    }

    AutomationCurveMap curves;
    for (const auto &entry : parsed->get_array()) {
        const AutomationCurveKey key = read_key(entry);
        if (!key.is_valid()) continue;

        const auto &points = json::array(json::member(entry, "points"));
        for (const auto &point : points) {
            const auto tick = json::rounded_int(json::member(point, "tick"));
            if (!tick) continue;

            AutomationCurve &curve = curves[key];
            curve.try_emplace(curve.end(), *tick, read_point(point));
        }
    }

    data.set_curves(curves);
}

muse::ByteArray AutomationRW::write(const AutomationData &data, bool write_generated) {
    std::vector<Curve> curves;
    for (const auto &[key, curve] : data.curves()) {
        IF_ASSERT_FAILED(key.is_valid()) { continue; }

        Curve entry = write_key(key);
        for (const auto &[tick, point] : curve) {
            if (write_generated || !point.generated) entry.points.push_back(write_point(tick, point));
        }
        if (!entry.points.empty()) curves.push_back(std::move(entry));
    }

    auto text = glz::write<json::PrettyPrint{}>(curves);
    if (!text) {
        LOGE() << "Failed to write automation JSON: " << glz::format_error(text.error());
        return {};
    }
    return json::serialized_bytes(std::move(*text));
}
