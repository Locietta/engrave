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
#include "json.h"

#include <cmath>
#include <cassert>

#define PICOJSON_USE_LOCALE 1
#define PICOJSON_ASSERT assert
#pragma push_macro("NULL")
#ifndef NULL
#define NULL 0
#endif
#include "picojson.h"
#pragma pop_macro("NULL")

#include "log.h"

using namespace muse;

struct muse::JsonData {
    picojson::value val;
};

static inline const picojson::value &val_const(const std::shared_ptr<JsonData> &d) { return d->val; }

static inline picojson::value &val_mut(std::shared_ptr<JsonData> &d) { return d->val; }

// =======================================
// JsonValue
// =======================================
JsonValue::JsonValue(bool v) : data_(std::make_shared<JsonData>()) { val_mut(data_).set<bool>(v); }

JsonValue::JsonValue(int v) : data_(std::make_shared<JsonData>()) {
    double d = static_cast<double>(v);
    val_mut(data_).set<double>(d);
}

JsonValue::JsonValue(double v) : data_(std::make_shared<JsonData>()) { val_mut(data_).set<double>(v); }

JsonValue::JsonValue(const String &v) : data_(std::make_shared<JsonData>()) { val_mut(data_).set<std::string>(v.to_std_string()); }

JsonValue::JsonValue(const std::string &v) : data_(std::make_shared<JsonData>()) { val_mut(data_).set<std::string>(v); }

JsonValue::JsonValue(const char *v) : data_(std::make_shared<JsonData>()) { val_mut(data_).set<std::string>(std::string(v)); }

JsonValue::JsonValue(const JsonArray &v) : data_(v.data_) {
    if (!data_) {
        data_ = std::make_shared<JsonData>();
    }
}

JsonValue::JsonValue(const JsonObject &v) : data_(v.data_) {
    if (!data_) {
        data_ = std::make_shared<JsonData>();
    }
}

JsonValue::JsonValue(std::shared_ptr<JsonData> d) : data_(d) {
    if (!data_) {
        data_ = std::make_shared<JsonData>();
    }
}

void JsonValue::detach() {
    if (!data_) {
        return;
    }

    if (data_.use_count() == 1) {
        return;
    }

    data_ = std::make_shared<JsonData>(*data_);
}

bool JsonValue::is_null() const { return val_const(data_).is<picojson::null>(); }

void JsonValue::set_null() {
    detach();
    val_mut(data_) = picojson::value();
}

bool JsonValue::is_bool() const { return val_const(data_).is<bool>(); }

bool JsonValue::to_bool() const { return val_const(data_).evaluate_as_boolean(); }

JsonValue &JsonValue::operator=(bool v) {
    detach();
    val_mut(data_).set<bool>(v);
    return *this;
}

bool JsonValue::is_number() const { return val_const(data_).is<double>(); }

int JsonValue::to_int() const {
    if (!is_number()) {
        return 0;
    }
    return static_cast<int>(std::round(val_const(data_).get<double>()));
}

double JsonValue::to_double() const {
    if (!is_number()) {
        return 0.0;
    }
    return val_const(data_).get<double>();
}

JsonValue &JsonValue::operator=(int v) {
    detach();
    double d = static_cast<double>(v);
    val_mut(data_).set<double>(d);
    return *this;
}

JsonValue &JsonValue::operator=(double v) {
    detach();
    val_mut(data_).set<double>(v);
    return *this;
}

bool JsonValue::is_string() const { return val_const(data_).is<std::string>(); }

String JsonValue::to_string() const {
    if (!is_string()) {
        static String dummy;
        return dummy;
    }
    return String::from_std_string(val_const(data_).get<std::string>());
}

const std::string &JsonValue::to_std_string() const {
    if (!is_string()) {
        static std::string dummy;
        return dummy;
    }
    return val_const(data_).get<std::string>();
}

JsonValue &JsonValue::operator=(const String &str) {
    detach();
    val_mut(data_).set<std::string>(str.to_std_string());
    return *this;
}

JsonValue &JsonValue::operator=(const std::string &str) {
    detach();
    val_mut(data_).set<std::string>(str);
    return *this;
}

JsonValue &JsonValue::operator=(const char *str) {
    detach();
    val_mut(data_).set<std::string>(std::string(str));
    return *this;
}

bool JsonValue::is_array() const { return val_const(data_).is<picojson::array>(); }

JsonArray JsonValue::to_array() const { return JsonArray(data_); }

JsonValue &JsonValue::operator=(const JsonArray &arr) {
    detach();
    val_mut(data_) = val_const(arr.data_);
    return *this;
}

bool JsonValue::is_object() const { return val_const(data_).is<picojson::object>(); }

JsonObject JsonValue::to_object() const { return JsonObject(data_); }

JsonValue &JsonValue::operator=(const JsonObject &obj) {
    detach();
    val_mut(data_) = val_const(obj.data_);
    return *this;
}

Val JsonValue::to_val() const {
    if (is_null()) {
        return Val();
    }
    if (is_bool()) {
        return Val(to_bool());
    }
    if (is_number()) {
        return Val(to_double());
    }
    if (is_string()) {
        return Val(to_std_string());
    }
    if (is_array()) {
        muse::ValList list;
        const JsonArray array = to_array();
        for (size_t i = 0; i < array.size(); ++i) {
            list.emplace_back(array[i].to_val());
        }
        return Val(list);
    }
    if (is_object()) {
        muse::ValMap map;
        const JsonObject object = to_object();
        for (const auto &key : object.keys()) {
            map[key] = object[key].to_val();
        }
        return muse::Val(map);
    }

    return muse::Val();
}

JsonValue JsonValue::from_val(const Val &val) {
    switch (val.type()) {
        case Val::Type::UNDEFINED: return JsonValue();
        case Val::Type::BOOL: return JsonValue(val.to_bool());
        case Val::Type::INT: return JsonValue(val.to_int());
        case Val::Type::INT64: return JsonValue(val.to_string()); // as string
        case Val::Type::DOUBLE: return JsonValue(val.to_double());
        case Val::Type::STRING: return JsonValue(val.to_string());
        case Val::Type::LIST: {
            JsonArray array;
            for (const auto &item : val.to_list()) {
                array.append(from_val(item));
            }
            return array;
        }
        case Val::Type::MAP: {
            JsonObject object;
            for (const auto &[key, value] : val.to_map()) {
                object[key] = from_val(value);
            }
            return object;
        }
        default: {
            MUSE_NOT_SUPPORTED << "data type: " << (int) val.type();
        }
    }

    return JsonValue();
}

// =======================================
// JsonValueRef
// =======================================

JsonValueRef::JsonValueRef(const std::string &key, JsonObject *o) : key_(key), object_(o) {}

JsonValueRef::JsonValueRef(size_t i, JsonArray *a) : idx_(i), array_(a) {}

JsonValueRef &JsonValueRef::operator=(bool v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(int v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(double v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(const String &v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(const std::string &v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(const char *v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(const JsonValue &v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(const JsonArray &v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

JsonValueRef &JsonValueRef::operator=(const JsonObject &v) {
    if (object_) {
        object_->set(key_, v);
    } else if (array_) {
        array_->set(idx_, v);
    }
    return *this;
}

// =======================================
// JsonArray
// =======================================

static inline const picojson::array &array_const(const std::shared_ptr<JsonData> &d) { return d->val.get<picojson::array>(); }

static inline picojson::array &array_mut(std::shared_ptr<JsonData> &d) { return d->val.get<picojson::array>(); }

JsonArray::JsonArray(std::shared_ptr<JsonData> d) : data_(d) {
    if (!data_) {
        data_ = std::make_shared<JsonData>();
        data_->val = picojson::value(picojson::array());
    }
}

JsonArray::JsonArray(std::initializer_list<JsonValue> args) {
    data_ = std::make_shared<JsonData>();
    data_->val = picojson::value(picojson::array());

    array_mut(data_).reserve(args.size());
    for (const JsonValue &v : args) {
        array_mut(data_).push_back(v.data_->val);
    }
}

void JsonArray::detach() {
    if (!data_) {
        return;
    }

    if (data_.use_count() == 1) {
        return;
    }

    data_ = std::make_shared<JsonData>(*data_);
}

size_t JsonArray::size() const { return array_const(data_).size(); }

void JsonArray::resize(size_t i) {
    detach();
    array_mut(data_).resize(i);
}

bool JsonArray::empty() const { return array_const(data_).size() == 0; }

JsonValue JsonArray::at(size_t i) const {
    std::shared_ptr<JsonData> d = std::make_shared<JsonData>();
    d->val = array_const(data_).at(i);
    return JsonValue(d);
}

JsonArray &JsonArray::set(size_t i, bool v) {
    detach();
    array_mut(data_)[i] = picojson::value(v);
    return *this;
}

JsonArray &JsonArray::set(size_t i, int v) {
    detach();
    array_mut(data_)[i] = picojson::value(static_cast<double>(v));
    return *this;
}

JsonArray &JsonArray::set(size_t i, double v) {
    detach();
    array_mut(data_)[i] = picojson::value(v);
    return *this;
}

JsonArray &JsonArray::set(size_t i, const String &str) {
    detach();
    array_mut(data_)[i] = picojson::value(str.to_std_string());
    return *this;
}

JsonArray &JsonArray::set(size_t i, const std::string &str) {
    detach();
    array_mut(data_)[i] = picojson::value(str);
    return *this;
}

JsonArray &JsonArray::set(size_t i, const char *str) {
    detach();
    array_mut(data_)[i] = picojson::value(std::string(str));
    return *this;
}

JsonArray &JsonArray::set(size_t i, const JsonValue &v) {
    detach();
    array_mut(data_)[i] = v.data_->val;
    return *this;
}

JsonArray &JsonArray::set(size_t i, const JsonArray &v) {
    detach();
    array_mut(data_)[i] = v.data_->val;
    return *this;
}

JsonArray &JsonArray::set(size_t i, const JsonObject &v) {
    detach();
    array_mut(data_)[i] = v.data_->val;
    return *this;
}

JsonArray &JsonArray::append(bool v) {
    detach();
    array_mut(data_).push_back(picojson::value(v));
    return *this;
}

JsonArray &JsonArray::append(int v) {
    detach();
    array_mut(data_).push_back(picojson::value(static_cast<double>(v)));
    return *this;
}

JsonArray &JsonArray::append(double v) {
    detach();
    array_mut(data_).push_back(picojson::value(v));
    return *this;
}

JsonArray &JsonArray::append(const String &str) {
    detach();
    array_mut(data_).push_back(picojson::value(str.to_std_string()));
    return *this;
}

JsonArray &JsonArray::append(const std::string &str) {
    detach();
    array_mut(data_).push_back(picojson::value(str));
    return *this;
}

JsonArray &JsonArray::append(const char *str) {
    detach();
    array_mut(data_).push_back(picojson::value(std::string(str)));
    return *this;
}

JsonArray &JsonArray::append(const JsonValue &v) {
    detach();
    array_mut(data_).push_back(val_const(v.data_));
    return *this;
}

JsonArray &JsonArray::append(const JsonArray &v) {
    detach();
    array_mut(data_).push_back(val_const(v.data_));
    return *this;
}

JsonArray &JsonArray::append(const JsonObject &v) {
    detach();
    array_mut(data_).push_back(val_const(v.data_));
    return *this;
}

JsonValue JsonArray::operator[](size_t i) const { return at(i); }

JsonValueRef JsonArray::operator[](size_t i) { return JsonValueRef(i, this); }

// =======================================
// JsonObject
// =======================================
static const picojson::object &object_const(const std::shared_ptr<JsonData> &d) { return d->val.get<picojson::object>(); }

static picojson::object &object_mut(std::shared_ptr<JsonData> &d) { return d->val.get<picojson::object>(); }

JsonObject::JsonObject(std::shared_ptr<JsonData> d) : data_(d) {
    if (!data_) {
        data_ = std::make_shared<JsonData>();
        data_->val = picojson::value(picojson::object());
    }
}

void JsonObject::detach() {
    if (!data_) {
        return;
    }

    if (data_.use_count() == 1) {
        return;
    }

    data_ = std::make_shared<JsonData>(*data_);
}

bool JsonObject::is_valid() const { return val_const(data_).is<picojson::object>(); }

bool JsonObject::empty() const {
    if (!is_valid()) {
        return true;
    }

    return object_const(data_).size() == 0;
}

size_t JsonObject::size() const {
    if (!is_valid()) {
        return 0;
    }

    return object_const(data_).size();
}

bool JsonObject::contains(const std::string &key) const {
    if (!is_valid()) {
        return false;
    }

    const picojson::object &o = object_const(data_);
    return o.find(key) != o.cend();
}

JsonValue JsonObject::value(const std::string &key, JsonValue def) const {
    if (!is_valid()) {
        return def;
    }

    const picojson::object &o = object_const(data_);
    auto it = o.find(key);
    if (it != o.cend()) {
        std::shared_ptr<JsonData> d = std::make_shared<JsonData>();
        d->val = it->second;
        return JsonValue(d);
    }
    return def;
}

JsonObject &JsonObject::set(const std::string &key, bool v) {
    detach();
    object_mut(data_).insert_or_assign(key, picojson::value(v));
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, int v) {
    detach();
    object_mut(data_).insert_or_assign(key, picojson::value(static_cast<double>(v)));
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, double v) {
    detach();
    object_mut(data_).insert_or_assign(key, picojson::value(v));
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, const std::string &v) {
    detach();
    object_mut(data_).insert_or_assign(key, picojson::value(v));
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, const char *str) {
    detach();
    object_mut(data_).insert_or_assign(key, picojson::value(std::string(str)));
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, const String &str) {
    detach();
    object_mut(data_).insert_or_assign(key, picojson::value(str.to_std_string()));
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, const JsonValue &v) {
    detach();
    object_mut(data_).insert_or_assign(key, v.data_->val);
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, const JsonArray &v) {
    detach();
    object_mut(data_).insert_or_assign(key, v.data_->val);
    return *this;
}

JsonObject &JsonObject::set(const std::string &key, const JsonObject &v) {
    detach();
    object_mut(data_).insert_or_assign(key, v.data_->val);
    return *this;
}

JsonValue JsonObject::operator[](const std::string &key) const { return value(key); }

JsonValueRef JsonObject::operator[](const std::string &key) { return JsonValueRef(key, this); }

std::vector<std::string> JsonObject::keys() const {
    std::vector<std::string> result;
    const picojson::object &o = object_const(data_);
    for (auto &&p : o) {
        result.push_back(p.first);
    }
    return result;
}

// =======================================
// JsonDocument
// =======================================
JsonDocument::JsonDocument(std::shared_ptr<JsonData> d) : data_(d) {}

JsonDocument::JsonDocument(JsonObject o) : data_(o.data_) {}

JsonDocument::JsonDocument(JsonArray a) : data_(a.data_) {}

ByteArray JsonDocument::to_json(Format format) const {
    std::string json = data_->val.serialize(format == Format::INDENTED);
    return ByteArray(json.c_str(), json.size());
}

JsonDocument JsonDocument::from_json(const ByteArray &ba, std::string *err) {
    std::shared_ptr<JsonData> d = std::make_shared<JsonData>();

    std::string_view json(ba.const_char(), ba.size());

    const char *current_loc = setlocale(LC_NUMERIC, "C");
    picojson::parse(d->val, json.begin(), json.end(), err);
    setlocale(LC_NUMERIC, current_loc);

    JsonDocument doc(d);
    return doc;
}

bool JsonDocument::is_object() const { return data_->val.is<picojson::object>(); }

bool JsonDocument::is_array() const { return data_->val.is<picojson::array>(); }

JsonObject JsonDocument::root_object() const { return JsonObject(data_); }

JsonArray JsonDocument::root_array() const { return JsonArray(data_); }
