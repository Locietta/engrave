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
#ifndef MUSE_GLOBAL_DATETIME_H
#define MUSE_GLOBAL_DATETIME_H

#include "string.h"

namespace muse {
enum class DateFormat {
    ISODate, // ISO 8601
    LocaleShortFormat
};

class Date {
public:
    Date() = default;
    Date(int y, int m, int d);

    bool isNull() const;

    int year() const;
    int month() const;
    int day() const;

    inline bool operator==(const Date &d) const { return year_ == d.year_ && month_ == d.month_ && day_ == d.day_; }

    inline bool operator!=(const Date &d) const { return !this->operator==(d); }

    int64_t daysTo(const Date &d) const;

    static Date currentDate();

    String toString(DateFormat format = DateFormat::ISODate) const;
    static Date fromStringISOFormat(const String &str);


private:
    int year_ = 0;
    int month_ = 0;
    int day_ = 0;
};

class Time {
public:

    Time() = default;
    Time(int h, int m, int s);

    int hour() const;
    int minute() const;
    int second() const;

    inline bool operator==(const Time &t) const { return hour_ == t.hour_ && minute_ == t.minute_ && second_ == t.second_; }

    inline bool operator!=(const Time &t) const { return !this->operator==(t); }

    static Time currentTime();

    String toString(DateFormat format = DateFormat::ISODate) const;
    static Time fromStringISOFormat(const String &str);


private:
    int hour_ = 0;
    int minute_ = 0;
    int second_ = 0;
};

class DateTime {
public:
    DateTime() = default;
    DateTime(const Date &d, const Time &t);

    const Date &date() const;
    const Time &time() const;

    inline bool operator==(const DateTime &dt) const { return dt.date() == date() && dt.time() == time(); }

    inline bool operator!=(const DateTime &dt) const { return !this->operator==(dt); }

    static DateTime currentDateTime();

    String toString(DateFormat format = DateFormat::ISODate) const;
    static DateTime fromStringISOFormat(const String &str);


private:

    Date date_;
    Time time_;
};
} // namespace muse

#endif // MUSE_GLOBAL_DATETIME_H
