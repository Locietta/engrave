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

#include "engraving/automation/automationdata.h"
#include "global/containers.h"
#include "global/log.h"
#include "global/serialization/json.h"

using namespace mu::engraving;

static const std::unordered_map<AutomationType, muse::String> automation_type_to_string{
    {AutomationType::DYNAMICS, u"Dynamics"},
    {AutomationType::TEMPO, u"Tempo"},
    {AutomationType::VOLUME, u"Volume"},
    {AutomationType::PAN, u"Pan"},
};

static constexpr const char *type_key = "type";
static constexpr const char *part_id_key = "partId";
static constexpr const char *instrument_id_key = "instrumentId";
static constexpr const char *staff_id_key = "staffId";
static constexpr const char *voice_id_key = "voiceId";
static constexpr const char *out_value_key = "outValue";
static constexpr const char *in_value_key = "inValue";
static constexpr const char *in_value_kind_key = "inValueKind";
static constexpr const char *ease_key = "ease";
static constexpr const char *control_point_t_key = "t";
static constexpr const char *control_point_value_key = "value";
static constexpr const char *item_id_key = "itemId";
static constexpr const char *generated_key = "generated";
static constexpr const char *tick_key = "tick";
static constexpr const char *points_key = "points";

static const std::string in_value_kind_from_previous = "FromPrevious";

static AutomationCurveKey read_key(const muse::JsonObject &obj) {
    const AutomationType type = muse::key(::automation_type_to_string, obj.value(::type_key).to_string(), AutomationType::UNKNOWN);

    if (obj.contains(::part_id_key)) {
        InstrumentTrackId track_id;
        track_id.part_id = muse::ID(obj.value(::part_id_key).to_string().to_std_string());
        track_id.instrument_id = obj.value(::instrument_id_key).to_string();
        return AutomationCurveKey::instrument(type, track_id);
    }

    if (obj.contains(::staff_id_key)) {
        const muse::ID staff_id(obj.value(::staff_id_key).to_string().to_std_string());

        std::optional<size_t> voice_idx;
        if (obj.contains(::voice_id_key)) {
            voice_idx = static_cast<size_t>(obj.value(::voice_id_key).to_int());
        }

        return AutomationCurveKey::staff(type, staff_id, voice_idx);
    }

    return AutomationCurveKey::global(type);
}

static AutomationPoint read_point(const muse::JsonObject &obj) {
    AutomationPoint point;
    point.value.out_value = obj.value(::out_value_key).to_double();

    const std::string in_value_kind = obj.contains(::in_value_kind_key) ? obj.value(::in_value_kind_key).to_std_string() : std::string();
    if (in_value_kind == ::in_value_kind_from_previous) {
        point.value.in_value = AutomationPoint::ArrivalFromPrevious{};
    } else {
        AutomationPoint::Ease ease;
        if (obj.contains(::ease_key)) {
            // Array for forward-compat; only element 0 is used, since Ease only holds one point
            const muse::JsonArray ease_array = obj.value(::ease_key).to_array();
            if (!ease_array.empty()) {
                const muse::JsonObject control_point_obj = ease_array.at(0).to_object();
                ease.t = control_point_obj.value(::control_point_t_key).to_double();
                ease.value = control_point_obj.value(::control_point_value_key).to_double();
            }
        }

        const muse::real_t value = muse::real_t(obj.value(::in_value_key).to_double());
        point.value.in_value = AutomationPoint::ExplicitArrival{value, ease};
    }

    if (obj.contains(::item_id_key)) {
        point.item_id = EID::from_std_string(obj.value(::item_id_key).to_string().to_std_string());
    }
    if (obj.contains(::generated_key)) {
        point.generated = obj.value(::generated_key).to_bool();
    }

    return point;
}

static void write_key(const AutomationCurveKey &key, muse::JsonObject &obj) {
    obj[::type_key] = muse::value(::automation_type_to_string, key.type);

    if (const std::optional<InstrumentTrackId> track_id = key.track_id()) {
        obj[::part_id_key] = track_id->part_id.to_std_string();
        obj[::instrument_id_key] = track_id->instrument_id;
        return;
    }

    if (const std::optional<muse::ID> staff_id = key.staff_id()) {
        obj[::staff_id_key] = staff_id->to_std_string();

        if (const std::optional<size_t> voice_idx = key.voice_idx()) {
            obj[::voice_id_key] = static_cast<int>(*voice_idx);
        }
    }
}

static void write_point(const AutomationPoint &point, muse::JsonObject &obj) {
    if (std::holds_alternative<AutomationPoint::ArrivalFromPrevious>(point.value.in_value)) {
        obj[::in_value_kind_key] = ::in_value_kind_from_previous;
    } else {
        const AutomationPoint::ExplicitArrival &explicit_arrival = std::get<AutomationPoint::ExplicitArrival>(point.value.in_value);
        obj[::in_value_key] = explicit_arrival.value;
        if (!explicit_arrival.ease.is_none()) {
            muse::JsonObject control_point_obj;
            control_point_obj[::control_point_t_key] = explicit_arrival.ease.t.raw();
            control_point_obj[::control_point_value_key] = explicit_arrival.ease.value.raw();

            muse::JsonArray ease_array;
            ease_array << control_point_obj;
            obj[::ease_key] = ease_array;
        }
    }

    obj[::out_value_key] = point.value.out_value;

    if (point.item_id.has_value()) {
        obj[::item_id_key] = point.item_id->to_std_string();
    }
    if (point.generated) {
        obj[::generated_key] = point.generated;
    }
}

void AutomationRW::read(AutomationData &data, const muse::ByteArray &json) {

    if (json.empty()) {
        return;
    }

    std::string err;
    const muse::JsonDocument doc = muse::JsonDocument::from_json(json, &err);
    if (!err.empty() || !doc.is_array()) {
        LOGE() << "Failed to parse automation json: " << err;
        return;
    }

    AutomationCurveMap curves;

    const muse::JsonArray root_array = doc.root_array();
    for (size_t i = 0; i < root_array.size(); ++i) {
        const muse::JsonObject curve_obj = root_array.at(i).to_object();
        const AutomationCurveKey key = read_key(curve_obj);
        IF_ASSERT_FAILED(key.is_valid()) { continue; }

        const muse::JsonArray point_array = curve_obj.value(::points_key).to_array();
        if (point_array.empty()) {
            continue;
        }

        AutomationCurve &curve = curves[key];
        for (size_t j = 0; j < point_array.size(); ++j) {
            const muse::JsonObject point_obj = point_array.at(j).to_object();
            const AutomationPoint point = read_point(point_obj);
            const utick_t tick = point_obj.value(::tick_key).to_int();
            // Points are written in ascending tick order, so end() is always the right hint
            curve.try_emplace(curve.end(), tick, point);
        }
    }

    data.set_curves(curves);
}

muse::ByteArray AutomationRW::write(const AutomationData &data, bool write_generated) {

    muse::JsonArray root_array;
    for (const auto &[key, curve] : data.curves()) {
        IF_ASSERT_FAILED(key.is_valid()) { continue; }

        muse::JsonArray point_array;
        for (const auto &[tick, point] : curve) {
            if (!write_generated && point.generated) {
                continue;
            }

            muse::JsonObject point_obj;
            point_obj[::tick_key] = tick;
            write_point(point, point_obj);
            point_array << point_obj;
        }

        if (!point_array.empty()) {
            muse::JsonObject curve_obj;
            write_key(key, curve_obj);
            curve_obj[::points_key] = point_array;
            root_array << curve_obj;
        }
    }

    return muse::JsonDocument(root_array).to_json();
}
