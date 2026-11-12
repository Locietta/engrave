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
#ifndef MUSE_GLOBAL_STRING_H
#define MUSE_GLOBAL_STRING_H

#include <cstring>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "bytearray.h"
#include "containers.h"
#include "global/logstream.h"

namespace muse {
enum CaseSensitivity { CASE_INSENSITIVE = 0, CASE_SENSITIVE = 1 };

enum SplitBehavior {
    KEEP_EMPTY_PARTS = 0,
    SKIP_EMPTY_PARTS = 1,
};

// ============================
// AsciiChar (ASCII)
// ============================
struct AsciiChar {
public:
    constexpr AsciiChar() = default;

    constexpr explicit AsciiChar(char c) : ch_(c) {}

    constexpr char ascii() const noexcept { return ch_; }

    constexpr char16_t unicode() const noexcept { return char16_t(ch_); }

    inline char to_lower() const { return to_lower(ch_); }

    inline char to_upper() const { return to_upper(ch_); }

    static char to_lower(char ch);
    static char to_upper(char ch);

private:
    char ch_ = 0;
};

// ============================
// Char (UTF-16)
// ============================
struct Char {
    enum SpecialCharacter {
        NUL = 0x0000,
        TABULATION = 0x0009,
        LINE_FEED = 0x000a,
        FORM_FEED = 0x000c,
        CARRIAGE_RETURN = 0x000d,
        SPACE = 0x0020,
        NBSP = 0x00a0,
        SOFT_HYPHEN = 0x00ad,
        REPLACEMENT_CHARACTER = 0xfffd,
        OBJECT_REPLACEMENT_CHARACTER = 0xfffc,
        BYTE_ORDER_MARK = 0xfeff,
        BYTE_ORDER_SWAPPED = 0xfffe,
        PARAGRAPH_SEPARATOR = 0x2029,
        LINE_SEPARATOR = 0x2028,
        LAST_VALID_CODE_POINT = 0x10ffff
    };

    constexpr Char() = default;

    constexpr Char(char16_t c) : ch_(c) {}

    Char(AsciiChar c) : ch_(c.unicode()) {}

    constexpr bool operator==(Char c) const { return ch_ == c.ch_; }

    constexpr bool operator!=(Char c) const { return !operator==(c); }

    constexpr bool operator==(char16_t c) const { return ch_ == c; }

    constexpr bool operator!=(char16_t c) const { return !operator==(c); }

    constexpr bool operator==(AsciiChar c) const { return ch_ == c.unicode(); }

    constexpr bool operator!=(AsciiChar c) const { return !operator==(c); }

    constexpr bool operator>(Char c) const { return ch_ > c.ch_; }

    constexpr bool operator>(char16_t c) const { return ch_ > c; }

    constexpr bool operator<(Char c) const { return ch_ < c.ch_; }

    constexpr bool operator<(char16_t c) const { return ch_ < c; }

    constexpr bool operator>=(Char c) const { return ch_ >= c.ch_; }

    constexpr bool operator>=(char16_t c) const { return ch_ >= c; }

    constexpr bool operator<=(Char c) const { return ch_ <= c.ch_; }

    constexpr bool operator<=(char16_t c) const { return ch_ <= c; }

    constexpr char16_t unicode() const { return ch_; }

    inline bool is_null() const { return ch_ == 0; }

    constexpr bool is_ascii() const { return is_ascii(ch_); }

    static constexpr bool is_ascii(char16_t c) { return c <= 0x7f; }

    inline char to_ascii(bool *ok = nullptr) const { return to_ascii(ch_, ok); }

    static char to_ascii(char16_t c, bool *ok = nullptr);

    static inline Char from_ascii(char c) { return static_cast<char16_t>(c); }

    inline bool is_letter() const { return is_letter(ch_); }

    static bool is_letter(char16_t c);

    inline bool is_space() const { return is_space(ch_); }

    static bool is_space(char16_t c);

    inline bool is_digit() const { return is_digit(ch_); }

    static bool is_digit(char16_t c);
    int digit_value() const;

    inline bool is_punct() const { return is_punct(ch_); }

    static bool is_punct(char16_t c);

    inline bool is_lower() const { return to_lower() == ch_; }

    inline Char to_lower() const { return to_lower(ch_); }

    static char16_t to_lower(char16_t ch);

    inline bool is_upper() const { return to_upper() == ch_; }

    inline Char to_upper() const { return to_upper(ch_); }

    static char16_t to_upper(char16_t ch);

    inline bool is_high_surrogate() const { return is_high_surrogate(ch_); }

    inline bool is_low_surrogate() const { return is_low_surrogate(ch_); }

    inline bool is_surrogate() const { return is_surrogate(ch_); }

    static constexpr char32_t surrogate_to_ucs4(char16_t high, char16_t low) { return (char32_t(high) << 10) + low - 0x35fdc00; }

    static constexpr char32_t surrogate_to_ucs4(Char high, Char low) { return surrogate_to_ucs4(high.ch_, low.ch_); }

    static constexpr bool is_high_surrogate(char32_t ucs4) { return (ucs4 & 0xfffffc00) == 0xd800; }

    static constexpr bool is_low_surrogate(char32_t ucs4) { return (ucs4 & 0xfffffc00) == 0xdc00; }

    static constexpr bool is_surrogate(char32_t ucs4) { return ucs4 - 0xd800u < 2048u; }

    static constexpr char16_t high_surrogate(char32_t ucs4) { return char16_t((ucs4 >> 10) + 0xd7c0); }

    static constexpr char16_t low_surrogate(char32_t ucs4) { return char16_t(ucs4 % 0x400 + 0xdc00); }

    static constexpr bool requires_surrogates(char32_t ucs4) { return ucs4 >= 0x10000; }

private:
    char16_t ch_ = 0;
};

// ============================
// UtfCodec
// ============================
struct UtfCodec {
    enum class Encoding {
        UNKNOWN,
        UTF_8,
        UTF_16LE,
        UTF_16BE,
    };

    static Encoding xml_encoding(std::string_view data);

    static Encoding xml_encoding(const ByteArray &data) { return xml_encoding(std::string_view(data.const_char(), data.size())); }

    static void utf8to16(std::string_view src, std::u16string &dst);
    static void utf16to8(std::u16string_view src, std::string &dst);
    static void utf8to32(std::string_view src, std::u32string &dst);
    static void utf32to8(std::u32string_view src, std::string &dst);
    static void replace_invalid(std::string_view src, std::string &dst);
    static bool is_valid_utf8(const std::string_view &src);
};

// ============================
// String (UTF-16)
// ============================
struct StringList;
struct AsciiStringView;

struct String {

    String();
    String(const char16_t *str);
    String(const Char &ch);
    String(const Char *unicode, size_t size = muse::nidx);

    String &operator=(const char16_t *str);
    void reserve(size_t i);

    inline bool operator==(const String &s) const { return const_str() == s.const_str(); }

    inline bool operator!=(const String &s) const { return !operator==(s); }

    bool is_equal_ignore_case(const String &s) const;

    bool operator==(const AsciiStringView &s) const;

    inline bool operator!=(const AsciiStringView &s) const { return !operator==(s); }

    inline bool operator==(const char16_t *s) const { return const_str() == s; }

    inline bool operator!=(const char16_t *s) const { return !operator==(s); }

    bool operator==(const char *s) const;

    inline bool operator!=(const char *s) const { return !operator==(s); }

    inline bool operator<(const String &s) const { return const_str() < s.const_str(); }

    inline bool operator>(const String &s) const { return const_str() > s.const_str(); }

    inline bool operator<=(const String &s) const { return const_str() <= s.const_str(); }

    inline bool operator>=(const String &s) const { return const_str() >= s.const_str(); }

    inline String &operator+=(const String &s) { return append(s); }

    String &operator+=(const char16_t *s);

    inline String &operator+=(char16_t s) { return append(s); }

    inline String operator+(const muse::String &s) const {
        String t(*this);
        t += s;
        return t;
    }

    inline String operator+(const char16_t *s) const {
        String t(*this);
        t += s;
        return t;
    }

    inline String operator+(char16_t s) const {
        String t(*this);
        t += s;
        return t;
    }

    char16_t operator[](size_t i) const;
    char16_t &operator[](size_t i);

    String &append(Char ch);
    String &append(const String &s);
    String &prepend(Char ch);
    String &prepend(const String &s);

    static String from_utf16_le(const ByteArray &data);
    static String from_utf16_be(const ByteArray &data);

    static String from_utf8(const char *str);
    static String from_utf8(std::string_view);
    static String from_utf8(const ByteArray &data);
    ByteArray to_utf8() const;

    static String from_ascii(const char *str, size_t size = muse::nidx);
    ByteArray to_ascii(bool *ok = nullptr) const;

    static String from_std_string(const std::string &str);
    std::string to_std_string() const;
    std::u16string to_std_u16_string() const;

    static String from_ucs4(const char32_t *str, size_t size = muse::nidx);
    static String from_ucs4(char32_t chr);
    std::u32string to_std_u32_string() const;

    std::wstring to_std_w_string() const;
    static const String from_std_w_string(const std::wstring &str);

    size_t size() const;
    bool empty() const;

    inline bool is_empty() const { return empty(); }

    void clear();
    Char at(size_t i) const;

    Char front() const { return at(0); }

    Char back() const { return at(size() - 1); }

    bool contains(const Char &ch) const;
    bool contains(const String &str, CaseSensitivity cs = CASE_SENSITIVE) const;
    bool contains(const std::wregex &re) const;
    int count(const Char &ch) const;
    int count(const String &str) const;
    size_t index_of(const Char &ch, size_t from = 0) const;
    size_t index_of(const String &str, size_t from = 0) const;
    size_t index_of(const char16_t *str, size_t from = 0) const;
    size_t last_index_of(const Char &ch, size_t from = muse::nidx) const;

    //! NOTE Now implemented only compare with ASCII
    bool starts_with(const String &str, CaseSensitivity cs = CASE_SENSITIVE) const;
    bool starts_with(char16_t ch, CaseSensitivity cs = CASE_SENSITIVE) const;
    bool ends_with(const String &str, CaseSensitivity cs = CASE_SENSITIVE) const;
    bool ends_with(char16_t ch, CaseSensitivity cs = CASE_SENSITIVE) const;

    StringList split(const Char &ch, SplitBehavior behavior = KEEP_EMPTY_PARTS) const;
    StringList split(const String &str, SplitBehavior behavior = KEEP_EMPTY_PARTS) const;
    StringList split(const std::regex &re, SplitBehavior behavior = KEEP_EMPTY_PARTS) const;
    StringList search(const std::regex &re, std::initializer_list<int> matches, SplitBehavior behavior = KEEP_EMPTY_PARTS) const;
    StringList search(const std::wregex &re, std::initializer_list<int> matches, SplitBehavior behavior = KEEP_EMPTY_PARTS) const;
    String &replace(const String &before, const String &after);
    String &replace(char16_t before, char16_t after);
    String &replace(const std::regex &re, const String &after);
    String &replace(const std::wregex &re, const String &after);
    String &insert(size_t position, const String &str);

    String &remove(const String &str) { return replace(str, String()); }

    String &remove(const std::regex &rx) { return replace(rx, String()); }

    String &remove(const std::wregex &rx) { return replace(rx, String()); }

    String &remove(const Char &ch);
    String &remove(char16_t ch);
    String &remove(size_t position, size_t n = muse::nidx);
    void chop(size_t n);
    void truncate(size_t position);

    String arg(const String &val) const;
    String arg(const String &val1, const String &val2) const;
    String arg(const String &val1, const String &val2, const String &val3) const;
    String arg(const String &val1, const String &val2, const String &val3, const String &val4) const;
    String arg(const String &val1, const String &val2, const String &val3, const String &val4, const String &val5) const;

    String arg(int val) const { return arg(number(val)); }

    String arg(int val1, int val2) const { return arg(number(val1), number(val2)); }

    String arg(int val1, int val2, int val3) const { return arg(number(val1), number(val2), number(val3)); }

    String arg(int64_t val) const { return arg(number(val)); }

    String arg(int64_t val1, int64_t val2) const { return arg(number(val1), number(val2)); }

    String arg(int64_t val1, int64_t val2, int64_t val3) const { return arg(number(val1), number(val2), number(val3)); }

    String arg(size_t val) const { return arg(number(val)); }

    String arg(size_t val1, size_t val2) const { return arg(number(val1), number(val2)); }

    String arg(size_t val1, size_t val2, size_t val3) const { return arg(number(val1), number(val2), number(val3)); }

    String arg(double val) const { return arg(number(val)); }

    String arg(double val1, double val2) const { return arg(number(val1), number(val2)); }

    String arg(double val1, double val2, double val3) const { return arg(number(val1), number(val2), number(val3)); }

    String mid(size_t pos, size_t count = muse::nidx) const;
    String left(size_t n) const;
    String right(size_t n) const;

    String trimmed() const;
    String simplified() const;
    String to_xml_escaped() const;
    static String to_xml_escaped(const String &str);
    static String to_xml_escaped(char16_t c);
    static String decode_xml_entities(const String &src);

    String to_lower() const;
    String to_upper() const;

    int to_int(bool *ok = nullptr, int base = 10) const;
    unsigned int to_u_int(bool *ok = nullptr, int base = 10) const;
    float to_float(bool *ok = nullptr) const;
    double to_double(bool *ok = nullptr) const;

    static String number(int n, int base = 10);
    static String number(int64_t n);
    static String number(size_t n);
    static String number(double n, int prec = 6);

    inline size_t hash() const { return std::hash<std::u16string>{}(const_str()); }

private:
    const std::u16string &const_str() const { return data_; }

    void do_args(std::u16string &out, const std::vector<std::u16string_view> &args) const;

    std::u16string data_;
};

struct StringList : std::vector<String> {
    StringList() = default;

    StringList(std::initializer_list<String> l) : std::vector<String>(l) {}

    StringList &operator<<(const String &s) { return append(s); }

    StringList &append(const String &s) {
        push_back(s);
        return *this;
    }

    StringList &operator<<(const StringList &l) { return append(l); }

    StringList &append(const StringList &l);

    size_t index_of(const String &s) const { return muse::index_of(*this, s); }

    bool contains(const String &s) const { return muse::contains(*this, s); }

    StringList filter(const String &str) const;
    String join(const String &sep) const;

    void insert(size_t idx, const String &str);
    void insert(const_iterator it, const String &str);
    void replace(size_t idx, const String &str);
    bool remove_all(const String &str);
    void remove_at(size_t i);

    std::vector<std::string> to_std_string_list() const;
};

// ============================
// AsciiStringView (ASCII)
// Be carefully!!, this class just hold pointer to string (no copy), so source string should be present, while use view
// ============================
struct AsciiStringView {

    constexpr AsciiStringView() = default;

    constexpr AsciiStringView(const char *str) : size_(str ? std::char_traits<char>::length(str) : 0), data_(str) {}

    constexpr AsciiStringView(const char *str, size_t size) : size_(size), data_(str) {}

    AsciiStringView(const std::string &str) : size_(str.size()), data_(str.c_str()) {}

    operator std::string_view() const {
        if (!data_) {
            return std::string_view{};
        }
        return std::string_view(data_, size_);
    }

    inline bool operator==(const AsciiStringView &s) const { return size_ == s.size_ && std::memcmp(data_, s.data_, size_) == 0; }

    inline bool operator!=(const AsciiStringView &s) const { return !this->operator==(s); }

    inline bool operator==(std::string_view s) const {
        return size_ == s.size() && (size_ == 0 || std::memcmp(data_, s.data(), size_) == 0);
    }

    inline bool operator==(const char *s) const {
        size_t sz = (s ? std::char_traits<char>::length(s) : 0);
        return size_ == sz && (s ? std::memcmp(data_, s, size_) == 0 : true);
    }

    inline bool operator!=(const char *s) const { return !this->operator==(s); }

    inline bool operator<(const AsciiStringView &s) const {
        if (size_ != s.size_) {
            return size_ < s.size_;
        }
        return std::memcmp(data_, s.data_, size_) < 0;
    }

    const char *ascii() const;
    size_t size() const;
    bool empty() const;
    AsciiChar at(size_t i) const;
    bool contains(char ch) const;
    size_t index_of(char ch) const;

    int to_int(bool *ok = nullptr, int base = 10) const;
    double to_double(bool *ok = nullptr) const;

private:
    size_t size_ = 0;
    const char *data_ = nullptr;
};

inline String operator+(char16_t s1, const String &s2) {
    String t(s1);
    t += s2;
    return t;
}

inline String operator+(const char16_t *s1, const String &s2) {
    String t(s1);
    t += s2;
    return t;
}
} // namespace muse

// ============================
// Char (UTF-16)
// ============================
inline bool operator==(const char16_t c1, const muse::Char c2) { return c2 == c1; }

inline bool operator!=(const char16_t c1, const muse::Char c2) { return c2 != c1; }

// ============================
// String (UTF-16)
// ============================
inline bool operator==(const char16_t *s1, const muse::String &s2) { return s2 == s1; }

inline bool operator!=(const char16_t *s1, const muse::String &s2) { return s2 != s1; }

template <>
struct std::hash<muse::String> {
    std::size_t operator()(const muse::String &s) const noexcept { return s.hash(); }
};

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::String &str) {
    s << str.to_utf8().const_char();
    return s;
}

// ============================
// AsciiStringView (ASCII)
// ============================
inline bool operator==(const char *s1, const muse::AsciiStringView &s2) { return s2 == s1; }

inline bool operator!=(const char *s1, const muse::AsciiStringView &s2) { return s2 != s1; }

inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::AsciiStringView &str) {
    s << str.ascii();
    return s;
}

#ifndef muPrintable
#define muPrintable(string) string.to_utf8().const_char()
#endif

#endif // MUSE_GLOBAL_STRING_H
