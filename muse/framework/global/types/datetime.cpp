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
#include "datetime.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "string.h"

using namespace muse;

// ==================================
// Format
// ==================================
static Date date_from_tm(const std::tm &tm) { return Date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday); }

static void to_tm(std::tm &tm, const Date &d) {
    tm.tm_year = std::max(d.year() - 1900, 0);
    tm.tm_mon = std::max(d.month() - 1, 0);
    tm.tm_mday = std::max(d.day(), 1);
}

static Time time_from_tm(const std::tm &tm) { return Time(tm.tm_hour, tm.tm_min, tm.tm_sec); }

static void to_tm(std::tm &tm, const Time &t) {
    tm.tm_hour = std::max(t.hour(), 0);
    tm.tm_min = std::max(t.minute(), 0);
    tm.tm_sec = std::max(t.second(), 0);
}

static DateTime datetime_from_tm(const std::tm &tm) { return DateTime(date_from_tm(tm), time_from_tm(tm)); }

static void to_tm(std::tm &tm, const DateTime &dt) {
    to_tm(tm, dt.date());
    to_tm(tm, dt.time());
}

static String format_time(const Time &t, const char *fmt) {
    std::tm tm;
    to_tm(tm, t);

    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), fmt, &tm);

    return String::from_utf8(mbstr);
}

static String format_date(const Date &d, const char *fmt) {
    std::tm tm;
    to_tm(tm, d);

    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), fmt, &tm);

    return String::from_utf8(mbstr);
}

static String format_date_time(const DateTime &dt, const char *fmt) {
    std::tm tm;
    to_tm(tm, dt);

    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), fmt, &tm);

    return String::from_utf8(mbstr);
}

// ==================================
// Date
// ==================================
Date::Date(int y, int m, int d) : year_(y), month_(m), day_(d) {}

bool Date::is_null() const { return year_ == 0 && month_ == 0 && day_ == 0; }

int Date::year() const { return year_; }

int Date::month() const { return month_; }

int Date::day() const { return day_; }

int64_t Date::days_to(const Date &d) const {
    if (is_null() || d.is_null()) {
        return 0;
    }

    std::tm me_tm = {};
    to_tm(me_tm, *this);

    std::tm other_tm = {};
    to_tm(other_tm, d);

    std::time_t me_time = std::mktime(&me_tm);
    std::time_t other_time = std::mktime(&other_tm);
    if (me_time != (std::time_t) (-1) && other_time != (std::time_t) (-1)) {
        double difference = std::difftime(other_time, me_time) / (60 * 60 * 24);
        return difference;
    }
    return 0;
}

Date Date::current_date() {
    using namespace std::chrono;
    milliseconds ms_d = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    std::time_t sec = static_cast<std::time_t>(ms_d.count() / 1000);
    std::tm tm;
#ifdef WIN32
    bool err = localtime_s(&tm, &sec) != 0;
#else
    bool err = localtime_r(&sec, &tm) == nullptr;
#endif
    assert(!err);
    if (err) {
        return Date();
    }
    return date_from_tm(tm);
}

String Date::to_string(DateFormat format) const {
    switch (format) {
        case DateFormat::ISO_DATE: return format_date(*this, "%Y-%m-%d");
        case DateFormat::LOCALE_SHORT_FORMAT: return format_date(*this, "%x");
    }
    return String();
}

Date Date::from_string_iso_format(const String &str) {
    std::tm tm = {};
    std::stringstream ss(str.to_std_string());
    ss >> std::get_time(&tm, "%Y-%m-%d");
    return date_from_tm(tm);
}

// ==================================
// Time
// ==================================

Time::Time(int h, int m, int s) : hour_(h), minute_(m), second_(s) {}

int Time::hour() const { return hour_; }

int Time::minute() const { return minute_; }

int Time::second() const { return second_; }

Time Time::current_time() {
    using namespace std::chrono;
    milliseconds ms_d = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    std::time_t sec = static_cast<std::time_t>(ms_d.count() / 1000);
    std::tm tm;
#ifdef WIN32
    bool err = localtime_s(&tm, &sec) != 0;
#else
    bool err = localtime_r(&sec, &tm) == nullptr;
#endif
    assert(!err);
    if (err) {
        return Time();
    }
    return time_from_tm(tm);
}

String Time::to_string(DateFormat format) const {
    switch (format) {
        case DateFormat::ISO_DATE: return format_time(*this, "%H:%M:%S");
        case DateFormat::LOCALE_SHORT_FORMAT: return format_time(*this, "%R");
    }
    return String();
}

Time Time::from_string_iso_format(const String &str) {
    std::tm tm = {};
    std::stringstream ss(str.to_std_string());
    ss >> std::get_time(&tm, "%H:%M:%S");
    return time_from_tm(tm);
}

// ==================================
// DateTime
// ==================================
DateTime::DateTime(const Date &d, const Time &t) : date_(d), time_(t) {}

const Date &DateTime::date() const { return date_; }

const Time &DateTime::time() const { return time_; }

DateTime DateTime::current_date_time() {
    using namespace std::chrono;
    milliseconds ms_d = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    std::time_t sec = static_cast<std::time_t>(ms_d.count() / 1000);
    std::tm tm;
#ifdef WIN32
    bool err = localtime_s(&tm, &sec) != 0;
#else
    bool err = localtime_r(&sec, &tm) == nullptr;
#endif
    assert(!err);
    if (err) {
        return DateTime();
    }

    return datetime_from_tm(tm);
}

String DateTime::to_string(DateFormat format) const {
    switch (format) {
        case DateFormat::ISO_DATE: return format_date_time(*this, "%Y-%m-%dT%H:%M:%S");
        case DateFormat::LOCALE_SHORT_FORMAT: return format_date_time(*this, "%x %R");
    }
    return String();
}

DateTime DateTime::from_string_iso_format(const String &str) {
    std::tm tm = {};
    std::stringstream ss(str.to_std_string());
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return datetime_from_tm(tm);
}
