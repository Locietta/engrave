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
#include "string.h"

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

#include <utf8.h>

#include "bytearray.h"

#include "log.h"

using namespace muse;

constexpr unsigned char u8_bom[] = {239, 187, 191};
constexpr unsigned char u16_le_bom[] = {255, 254};
constexpr unsigned char u16_be_bom[] = {254, 255};

// Helpers

static long int to_int_helper(const char *str, bool *ok, int base) {
    if (!str || std::strlen(str) == 0) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }
    const char *current_loc = setlocale(LC_NUMERIC, "C");
    char *end = nullptr;
    long int v = static_cast<int>(std::strtol(str, &end, base));
    setlocale(LC_NUMERIC, current_loc);
    bool my_ok = std::strlen(end) == 0;
    if (!my_ok) {
        v = 0;
    }

    if (ok) {
        *ok = my_ok;
    }
    return v;
}

static double to_double_helper(const char *str, bool *ok) {
    if (!str) {
        return 0.0;
    }
    const char *current_loc = setlocale(LC_NUMERIC, "C");
    char *end = nullptr;
    double v = std::strtod(str, &end);
    setlocale(LC_NUMERIC, current_loc);
    if (ok) {
        size_t sz = std::strlen(end);
        *ok = sz != std::strlen(str);
    }
    return v;
}

static void ltrim_helper(std::u16string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char16_t ch) { return !Char::is_space(ch); }));
}

static void rtrim_helper(std::u16string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](char16_t ch) { return !Char::is_space(ch); }).base(), s.end());
}

static void trim_helper(std::u16string &s) {
    ltrim_helper(s);
    rtrim_helper(s);
}

// ============================
// Char
// ============================
char Char::to_ascii(char16_t c, bool *ok) {
    if (is_ascii(c)) {
        if (ok) {
            *ok = true;
        }
        return static_cast<char>(c);
    } else {
        if (ok) {
            *ok = false;
        }
        return '?';
    }
}

bool Char::is_letter(char16_t c) {
    //! TODO
    return std::isalpha(static_cast<unsigned char>(c));
}

bool Char::is_space(char16_t c) {
    //! Mirrors the code points QChar::isSpace() accepts above ASCII; keeping the
    //! set here explicit avoids narrowing c to unsigned char, which would alias
    //! non-Latin1 code points onto ASCII ones (u'\u4f20' onto a space, for instance).
    static constexpr char16_t non_ascii_spaces[]{u'\u0085', u'\u00A0', u'\u1680', u'\u2000', u'\u2001', u'\u2002', u'\u2003',
                                                 u'\u2004', u'\u2005', u'\u2006', u'\u2007', u'\u2008', u'\u2009', u'\u200A',
                                                 u'\u2028', u'\u2029', u'\u202F', u'\u205F', u'\u3000'};

    if (c <= 0x7F) {
        return c == u' ' || (c >= u'\t' && c <= u'\r');
    }
    return std::binary_search(std::begin(non_ascii_spaces), std::end(non_ascii_spaces), c);
}

bool Char::is_digit(char16_t c) { return c >= u'0' && c <= u'9'; }

int Char::digit_value() const {
    if (!is_digit()) {
        return -1;
    }
    return ch_ - '0';
}

bool Char::is_punct(char16_t ch) { return std::ispunct(static_cast<unsigned char>(ch)); }

char16_t Char::to_lower(char16_t ch) {
    //! TODO
    return std::tolower(static_cast<unsigned char>(ch));
}

char16_t Char::to_upper(char16_t ch) {
    //! TODO
    return std::toupper(static_cast<unsigned char>(ch));
}

// ============================
// UtfCodec
// ============================
UtfCodec::Encoding UtfCodec::xml_encoding(const ByteArray &data) {
    if (data.size() < 3) {
        return Encoding::UNKNOWN;
    }

    // Check Bom
    if (std::memcmp(data.const_char(), ::u8_bom, 3) == 0) {
        return Encoding::UTF_8;
    }

    if (std::memcmp(data.const_char(), ::u16_le_bom, 2) == 0) {
        return Encoding::UTF_16LE;
    }

    if (std::memcmp(data.const_char(), ::u16_be_bom, 2) == 0) {
        return Encoding::UTF_16BE;
    }

    // Check content
    //! NOTE For XML we know that the content starts with an ascii character '<',
    //! it takes up 8 bits, so the remaining bits will be zero if the encoding is greater than UTF-8
    //! (for other content type this may not be true)
    const uint8_t *d = data.const_data();
    if (d[0] != 0 && d[1] != 0) {
        return Encoding::UTF_8;
    }

    if (d[0] != 0 && d[1] == 0) {
        return Encoding::UTF_16LE;
    }

    if (d[0] == 0 && d[1] != 0) {
        return Encoding::UTF_16BE;
    }

    return Encoding::UNKNOWN;
}

void UtfCodec::utf8to16(std::string_view src, std::u16string &dst) {
    try {
        utf8::utf8to16(src.begin(), src.end(), std::back_inserter(dst));
    } catch (const std::exception &e) {
        LOGE() << e.what();
    }
}

void UtfCodec::utf16to8(std::u16string_view src, std::string &dst) {
    try {
        utf8::utf16to8(src.begin(), src.end(), std::back_inserter(dst));
    } catch (const std::exception &e) {
        LOGE() << e.what();
    }
}

void UtfCodec::utf8to32(std::string_view src, std::u32string &dst) {
    try {
        utf8::utf8to32(src.begin(), src.end(), std::back_inserter(dst));
    } catch (const std::exception &e) {
        LOGE() << e.what();
    }
}

void UtfCodec::utf32to8(std::u32string_view src, std::string &dst) {
    try {
        utf8::utf32to8(src.begin(), src.end(), std::back_inserter(dst));
    } catch (const std::exception &e) {
        LOGE() << e.what();
    }
}

bool UtfCodec::is_valid_utf8(const std::string_view &src) { return utf8::is_valid(src.begin(), src.end()); }

void UtfCodec::replace_invalid(std::string_view src, std::string &dst) {
    try {
        utf8::replace_invalid(src.begin(), src.end(), std::back_inserter(dst));
    } catch (const std::exception &e) {
        LOGE() << e.what();
    }
}

// ============================
// String
// ============================

String::String() { data_ = std::make_shared<std::u16string>(); }

String::String(const char16_t *str) {
    data_ = std::make_shared<std::u16string>(str ? str : u"");
#ifdef MUSE_STRING_DEBUG_HACK
    updateDebugView();
#endif
}

String::String(const Char &ch) {
    data_ = std::make_shared<std::u16string>();
    *data_.get() += ch.unicode();
#ifdef MUSE_STRING_DEBUG_HACK
    updateDebugView();
#endif
}

String::String(const Char *unicode, size_t size) {
    if (!unicode) {
        data_ = std::make_shared<std::u16string>();
        return;
    }

    static_assert(sizeof(Char) == sizeof(char16_t));
    const char16_t *str = reinterpret_cast<const char16_t *>(unicode);
    if (size == muse::nidx) {
        data_ = std::make_shared<std::u16string>(str);
    } else {
        data_ = std::make_shared<std::u16string>(str, size);
    }

#ifdef MUSE_STRING_DEBUG_HACK
    updateDebugView();
#endif
}

#ifdef MUSE_STRING_DEBUG_HACK
void String::updateDebugView() {
    try {
        dview = to_std_string();
    } catch (const std::exception &e) {
        dview = "exception: " + std::string(e.what());
    }
}

#endif

const std::u16string &String::const_str() const { return *data_.get(); }

struct String::Mutator {
    std::u16string &s;
    String *self = nullptr;

    Mutator(std::u16string &s, String *self) : s(s), self(self) {}

    ~Mutator() {
#ifdef MUSE_STRING_DEBUG_HACK
        self->updateDebugView();
#endif
    }

    operator std::u16string &() { return s; }

    void reserve(size_t n) { s.reserve(n); }

    void resize(size_t n) { s.resize(n); }

    void clear() { s.clear(); }

    void push_back(char16_t c) { s.push_back(c); }

    void insert(size_t p, const std::u16string &v) { s.insert(p, v); }

    void erase(size_t p, size_t n) { s.erase(p, n); }

    std::u16string &operator=(const std::u16string &v) { return s.operator=(v); }

    std::u16string &operator=(const char16_t *v) { return s.operator=(v); }

    std::u16string &operator=(const char16_t v) { return s.operator=(v); }

    std::u16string &operator+=(const std::u16string &v) { return s.operator+=(v); }

    std::u16string &operator+=(const char16_t *v) { return s.operator+=(v); }

    std::u16string &operator+=(const char16_t v) { return s.operator+=(v); }

    char16_t &operator[](size_t i) { return s.operator[](i); }
};

String::Mutator String::mut_str(bool do_detach) {
    if (do_detach) {
        detach();
    }
    return Mutator(*data_.get(), this);
}

void String::reserve(size_t i) { mut_str().reserve(i); }

bool String::is_equal_ignore_case(const String &s) const {
    const std::u16string_view a{const_str()};
    const std::u16string_view b{s.const_str()};

    if (a.size() != b.size()) {
        return false;
    }

    return std::equal(a.begin(), a.end(), b.begin(),
                      [](const char16_t c1, const char16_t c2) { return Char::to_lower(c1) == Char::to_lower(c2); });
}

bool String::operator==(const AsciiStringView &s) const {
    if (size() != s.size()) {
        return false;
    }

    for (size_t i = 0; i < s.size(); ++i) {
        if (at(i).to_ascii() != s.at(i).ascii()) {
            return false;
        }
    }
    return true;
}

bool String::operator==(const char *s) const {
    if (!s) {
        return false;
    }

    std::string_view v(s);
    if (size() != v.size()) {
        return false;
    }

    for (size_t i = 0; i < v.size(); ++i) {
        IF_ASSERT_FAILED(Char::is_ascii(v.at(i))) {}
        if (at(i).to_ascii() != v.at(i)) {
            return false;
        }
    }
    return true;
}

void String::detach() {
    if (!data_) {
        return;
    }

    if (data_.use_count() == 1) {
        return;
    }

    data_ = std::make_shared<std::u16string>(*data_);
}

String &String::operator=(const char16_t *str) {
    mut_str() = str;
    return *this;
}

String &String::operator+=(const char16_t *s) {
    mut_str() += s;
    return *this;
}

char16_t String::operator[](size_t i) const { return const_str()[i]; }

char16_t &String::operator[](size_t i) { return mut_str()[i]; }

String &String::append(Char ch) {
    mut_str() += ch.unicode();
    return *this;
}

String &String::append(const String &s) {
    mut_str() += s.const_str();
    return *this;
}

String &String::prepend(Char ch) {
    mut_str() = ch.unicode() + const_str();
    return *this;
}

String &String::prepend(const String &s) {
    mut_str() = s.const_str() + const_str();
    return *this;
}

String String::from_utf16_le(const ByteArray &data) {
    //make sure len is divisible by 2
    size_t len = data.size();
    if (len % 2) {
        len--;
    }

    if (len < 2) {
        return String();
    }

    String u16;
    u16.reserve(len / 2);
    String::Mutator mut = u16.mut_str();

    const uint8_t *d = data.const_data();
    size_t start = 0;
    if (std::memcmp(d, ::u16_le_bom, 2) == 0) {
        start += 2;
    }

    for (size_t i = start; i < len;) {
        //little-endian
        int lo = d[i++] & 0xFF;
        int hi = d[i++] & 0xFF;
        mut.push_back(hi << 8 | lo);
    }

    return u16;
}

String String::from_utf16_be(const ByteArray &data) {
    //make sure len is divisible by 2
    size_t len = data.size();
    if (len % 2) {
        len--;
    }

    if (len < 2) {
        return String();
    }

    String u16;
    u16.reserve(len / 2);
    String::Mutator mut = u16.mut_str();

    const uint8_t *d = data.const_data();
    size_t start = 0;
    if (std::memcmp(d, ::u16_be_bom, 2) == 0) {
        start += 2;
    }

    for (size_t i = start; i < len;) {
        //big-endian
        int hi = d[i++] & 0xFF;
        int lo = d[i++] & 0xFF;
        mut.push_back(hi << 8 | lo);
    }

    return u16;
}

String String::from_utf8(const char *str) {
    if (!str) {
        return String();
    }
    return from_utf8(std::string_view(str));
}

String String::from_utf8(const std::string_view str) {
    String s;
    UtfCodec::utf8to16(str, s.mut_str());
    return s;
}

String String::from_utf8(const ByteArray &data) {
    if (data.empty()) {
        return String();
    }
    String s;
    UtfCodec::utf8to16(std::string_view(data.const_char(), data.size()), s.mut_str());
    return s;
}

ByteArray String::to_utf8() const {
    ByteArray ba;
    std::u16string_view v(const_str());
    try {
        utf8::utf16to8(v.begin(), v.end(), std::back_inserter(ba));
    } catch (const std::exception &e) {
        LOGE() << e.what();
    }
    return ba;
}

String String::from_ascii(const char *str, size_t size) {
    if (!str) {
        return String();
    }

    size = (size == muse::nidx) ? std::strlen(str) : size;
    String s;
    std::u16string &data = s.mut_str();
    data.resize(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = Char::from_ascii(str[i]).unicode();
    }
    return s;
}

ByteArray String::to_ascii(bool *ok) const {
    ByteArray ba;
    ba.resize(size());

    if (ok) {
        *ok = true;
    }

    for (size_t i = 0; i < size(); ++i) {
        bool cok = false;
        char ch = Char::to_ascii(const_str().at(i), &cok);
        if (!cok && ok) {
            *ok = false;
        }
        ba[i] = static_cast<uint8_t>(ch);
    }
    return ba;
}

String String::from_std_string(const std::string &str) {
    String s;
    UtfCodec::utf8to16(std::string_view(str), s.mut_str());
    return s;
}

std::string String::to_std_string() const {
    std::string s;
    UtfCodec::utf16to8(std::u16string_view(const_str()), s);
    return s;
}

std::u16string String::to_std_u16_string() const { return const_str(); }

String String::from_ucs4(const char32_t *str, size_t size) {
    std::u32string_view v32;
    if (size == muse::nidx) {
        v32 = std::u32string_view(str);
    } else {
        v32 = std::u32string_view(str, size);
    }

    std::string s8;
    UtfCodec::utf32to8(v32, s8);

    String s;
    UtfCodec::utf8to16(s8, s.mut_str());
    return s;
}

String String::from_ucs4(char32_t chr) { return from_ucs4(&chr, 1); }

std::u32string String::to_std_u32_string() const {
    std::string s;
    UtfCodec::utf16to8(const_str(), s);
    std::u32string s32;
    UtfCodec::utf8to32(s, s32);
    return s32;
}

std::wstring String::to_std_w_string() const {
    const std::u16string &u16 = const_str();
    std::wstring ws;
    ws.resize(u16.size());

    static_assert(sizeof(wchar_t) >= sizeof(char16_t));

    for (size_t i = 0; i < ws.size(); ++i) {
        ws[i] = static_cast<wchar_t>(u16.at(i));
    }

    return ws;
}

const String String::from_std_w_string(const std::wstring &str) {
    String s;
    s.mut_str().resize(str.size());

    static_assert(sizeof(wchar_t) >= sizeof(char16_t));

    for (size_t i = 0; i < str.size(); ++i) {
        s[i] = static_cast<char16_t>(str.at(i));
    }

    return s;
}

size_t String::size() const { return const_str().size(); }

bool String::empty() const { return const_str().empty(); }

void String::clear() { mut_str().clear(); }

Char String::at(size_t i) const {
    IF_ASSERT_FAILED(i < size()) { return Char(); }
    return Char(const_str().at(i));
}

bool String::contains(const Char &ch) const { return const_str().find(ch.unicode()) != std::u16string::npos; }

bool String::contains(const String &str, CaseSensitivity cs) const {
    if (cs == CaseSensitivity::CASE_SENSITIVE) {
        return const_str().find(str.const_str()) != std::u16string::npos;
    } else {
        std::u16string self = const_str();
        std::transform(self.begin(), self.end(), self.begin(), [](char16_t c) { return Char::to_lower(c); });
        std::u16string other = str.const_str();
        std::transform(other.begin(), other.end(), other.begin(), [](char16_t c) { return Char::to_lower(c); });
        return self.find(other) != std::u16string::npos;
    }
}

bool String::contains(const std::wregex &re) const {
    std::wstring ws = to_std_w_string();

    auto words_begin = std::wsregex_iterator(ws.begin(), ws.end(), re);
    if (words_begin != std::wsregex_iterator()) {
        return true;
    }
    return false;
}

int String::count(const Char &ch) const {
    int count = 0;
    for (size_t i = 0; i < const_str().size(); ++i) {
        if (const_str().at(i) == ch.unicode()) {
            ++count;
        }
    }
    return count;
}

int String::count(const String &str) const {
    int count = 0;
    std::string::size_type pos = 0;
    std::u16string other_str = str.const_str();
    while ((pos = const_str().find(other_str, pos)) != std::string::npos) {
        ++count;
        pos += str.size();
    }
    return count;
}

size_t String::index_of(const Char &ch, size_t from) const {
    for (size_t i = from; i < const_str().size(); ++i) {
        if (const_str().at(i) == ch.unicode()) {
            return i;
        }
    }
    return muse::nidx;
}

size_t String::index_of(const String &str, size_t from) const { return const_str().find(str.const_str(), from); }

size_t String::index_of(const char16_t *str, size_t from) const { return const_str().find(str, from); }

size_t String::last_index_of(const Char &ch, size_t from) const {
    from = std::min(from, const_str().size() - 1);

    for (int i = static_cast<int>(from); i >= 0; --i) {
        if (const_str().at(i) == ch.unicode()) {
            return i;
        }
    }
    return muse::nidx;
}

bool String::starts_with(const String &str, CaseSensitivity cs) const {
    if (str.size() > size()) {
        return false;
    }

    for (size_t i = 0; i < str.size(); ++i) {
        if (Char(const_str().at(i)) == str.at(i)) {
            continue;
        }

        if (cs == CASE_INSENSITIVE) {
            if (Char::to_lower(const_str().at(i)) == Char::to_lower(str.const_str().at(i))) {
                continue;
            }
        }

        return false;
    }

    return true;
}

bool String::starts_with(char16_t ch, CaseSensitivity cs) const {
    if (empty()) {
        return false;
    }

    if (const_str().front() == ch) {
        return true;
    }

    if (cs == CASE_INSENSITIVE) {
        if (Char::to_lower(const_str().front()) == Char::to_lower(ch)) {
            return true;
        }
    }

    return false;
}

bool String::ends_with(const String &str, CaseSensitivity cs) const {
    if (str.size() > size()) {
        return false;
    }

    size_t start = size() - str.size();
    for (size_t i = 0; i < str.size(); ++i) {
        if (const_str().at(start + i) == str.at(i)) {
            continue;
        }

        if (cs == CASE_INSENSITIVE) {
            if (Char::to_lower(const_str().at(start + i)) == Char::to_lower(str.const_str().at(i))) {
                continue;
            }
        }

        return false;
    }

    return true;
}

bool String::ends_with(char16_t ch, CaseSensitivity cs) const {
    if (empty()) {
        return false;
    }

    if (const_str().back() == ch) {
        return true;
    }

    if (cs == CASE_INSENSITIVE) {
        if (Char::to_lower(const_str().back()) == Char::to_lower(ch)) {
            return true;
        }
    }

    return false;
}

StringList String::split(const Char &ch, SplitBehavior behavior) const {
    StringList out;
    std::size_t current, previous = 0;
    current = const_str().find(ch.unicode());
    while (current != std::string::npos) {
        String sub = mid(previous, current - previous);
        if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && sub.empty()) {
            // skip
        } else {
            out.push_back(std::move(sub));
        }
        previous = current + 1;
        current = const_str().find(ch.unicode(), previous);
    }
    String sub = mid(previous, current - previous);
    if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && sub.empty()) {
        // skip
    } else {
        out.push_back(std::move(sub));
    }

    return out;
}

StringList String::split(const String &str, SplitBehavior behavior) const {
    StringList out;
    std::size_t current, previous = 0;
    current = const_str().find(str.const_str());
    while (current != std::string::npos) {
        String sub = mid(previous, current - previous);
        if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && sub.empty()) {
            // skip
        } else {
            out.push_back(std::move(sub));
        }
        previous = current + str.size();
        current = const_str().find(str.const_str(), previous);
    }
    String sub = mid(previous, current - previous);
    if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && sub.empty()) {
        // skip
    } else {
        out.push_back(std::move(sub));
    }

    return out;
}

StringList String::split(const std::regex &re, SplitBehavior behavior) const {
    std::string origin_u8;
    UtfCodec::utf16to8(std::u16string_view(const_str()), origin_u8);
    std::sregex_token_iterator iter(origin_u8.begin(), origin_u8.end(), re, -1);
    std::sregex_token_iterator end;
    std::vector<std::string> vec = {iter, end};

    StringList out;
    for (const std::string &s : vec) {
        if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && s.empty()) {
            // skip
            continue;
        }

        String sub;
        UtfCodec::utf8to16(s, sub.mut_str());
        out.push_back(std::move(sub));
    }

    return out;
}

StringList String::search(const std::regex &re, std::initializer_list<int> matches, SplitBehavior behavior) const {
    std::string origin_u8;
    UtfCodec::utf16to8(std::u16string_view(const_str()), origin_u8);
    std::sregex_token_iterator iter(origin_u8.begin(), origin_u8.end(), re, matches);
    std::sregex_token_iterator end;
    std::vector<std::string> vec = {iter, end};

    StringList out;
    for (const std::string &s : vec) {
        if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && s.empty()) {
            // skip
            continue;
        }
        String sub;
        UtfCodec::utf8to16(s, sub.mut_str());
        out.push_back(std::move(sub));
    }

    return out;
}

StringList String::search(const std::wregex &re, std::initializer_list<int> matches, SplitBehavior behavior) const {
    std::wstring ws = to_std_w_string();

    std::wsregex_token_iterator iter(ws.begin(), ws.end(), re, matches);
    std::wsregex_token_iterator end;
    std::vector<std::wstring> vec = {iter, end};

    StringList out;
    for (const std::wstring &s : vec) {
        if (behavior == SplitBehavior::SKIP_EMPTY_PARTS && s.empty()) {
            // skip
            continue;
        }
        String sub = String::from_std_w_string(s);
        out.push_back(std::move(sub));
    }

    return out;
}

String &String::replace(const String &before, const String &after) {
    if (before == after) {
        return *this;
    }
    Mutator h = mut_str();
    std::u16string &str = h.s;
    size_t start_pos = 0;
    while ((start_pos = str.find(before.const_str(), start_pos)) != std::string::npos) {
        str.replace(start_pos, before.size(), after.const_str());
        start_pos += after.size(); // Handles case where 'after' is a substring of 'before'
    }
    return *this;
}

String &String::replace(char16_t before, char16_t after) {
    Mutator h = mut_str();
    std::u16string &str = h.s;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str.at(i) == before) {
            str[i] = after;
        }
    }
    return *this;
}

String &String::replace(const std::regex &re, const String &after) {
    std::string after_u8;
    UtfCodec::utf16to8(std::u16string_view(after.const_str()), after_u8);

    std::string origin_u8;
    UtfCodec::utf16to8(std::u16string_view(const_str()), origin_u8);
    std::string replased_u8 = std::regex_replace(origin_u8, re, after_u8);

    Mutator h = mut_str();
    std::u16string &sefl = h.s;
    sefl.clear();
    UtfCodec::utf8to16(std::string_view(replased_u8), sefl);
    return *this;
}

String &String::replace(const std::wregex &re, const String &after) {
    std::wstring afterw;
    afterw = after.to_std_w_string();

    std::wstring originw;
    originw = to_std_w_string();
    std::wstring replacedw = std::regex_replace(originw, re, afterw);

    mut_str().clear();
    mut_str().resize(replacedw.size());

    for (size_t i = 0; i < replacedw.size(); ++i) {
        mut_str()[i] = static_cast<char16_t>(replacedw.at(i));
    }

    return *this;
}

String &String::insert(size_t position, const String &str) {
    mut_str().insert(position, str.const_str());
    return *this;
}

String &String::remove(const Char &ch) { return remove(ch.unicode()); }

String &String::remove(char16_t ch) {
    auto it = const_str().find(ch);
    if (it != std::u16string::npos) {
        mut_str().erase(it, 1);
    }
    return *this;
}

String &String::remove(size_t position, size_t n) {
    mut_str().erase(position, n);
    return *this;
}

void String::chop(size_t n) {
    if (n >= size()) {
        n = size();
    }

    remove(size() - n);
}

void String::truncate(size_t position) { mut_str().resize(position); }

static constexpr bool is1_to9(char16_t chr) { return u'1' <= chr && chr <= '9'; }

void String::do_args(std::u16string &out, const std::vector<std::u16string_view> &args) const {
    struct Part {
        std::u16string_view substr;
        size_t arg_idx_to_insert_after = muse::nidx;
    };

    const std::u16string &str = const_str();
    const std::u16string_view view(str);
    std::vector<Part> parts;

    {
        std::size_t current_percent_idx = view.find(u'%'), part_start_idx = 0;

        while (current_percent_idx != std::string::npos) {
            std::u16string_view sub = view.substr(part_start_idx, current_percent_idx - part_start_idx);
            std::size_t next_char_idx = current_percent_idx + 1;
            if (next_char_idx < view.size() && is1_to9(view.at(next_char_idx))) {
                size_t arg_idx = view.at(next_char_idx) - u'1';
                parts.push_back({std::move(sub), arg_idx});
                part_start_idx = next_char_idx + 1;
            }
            current_percent_idx = view.find(u'%', next_char_idx);
        }

        std::u16string_view sub = view.substr(part_start_idx);
        parts.push_back({std::move(sub)});
    }

    {
        for (const auto &[substr, arg_idx_to_insert_after] : parts) {
            if (!substr.empty()) {
                out += substr;
            }

            if (arg_idx_to_insert_after != muse::nidx) {
                if (arg_idx_to_insert_after < args.size()) {
                    out += args.at(arg_idx_to_insert_after);
                } else {
                    // When there are 5 args, %6 becomes %1
                    out.push_back(u'%');
                    out.push_back(u'1' + static_cast<char16_t>(arg_idx_to_insert_after - args.size()));
                }
            }
        }
    }
}

String String::arg(const String &val) const {
    String s;
    do_args(s.mut_str(), {std::u16string_view(val.const_str())});
    return s;
}

String String::arg(const String &val1, const String &val2) const {
    String s;
    do_args(s.mut_str(), {std::u16string_view(val1.const_str()), std::u16string_view(val2.const_str())});
    return s;
}

String String::arg(const String &val1, const String &val2, const String &val3) const {
    String s;
    do_args(s.mut_str(),
            {std::u16string_view(val1.const_str()), std::u16string_view(val2.const_str()), std::u16string_view(val3.const_str())});
    return s;
}

String String::arg(const String &val1, const String &val2, const String &val3, const String &val4) const {
    String s;
    do_args(s.mut_str(), {std::u16string_view(val1.const_str()), std::u16string_view(val2.const_str()),
                          std::u16string_view(val3.const_str()), std::u16string_view(val4.const_str())});
    return s;
}

String String::arg(const String &val1, const String &val2, const String &val3, const String &val4, const String &val5) const {
    String s;
    do_args(s.mut_str(),
            {std::u16string_view(val1.const_str()), std::u16string_view(val2.const_str()), std::u16string_view(val3.const_str()),
             std::u16string_view(val4.const_str()), std::u16string_view(val5.const_str())});
    return s;
}

String String::mid(size_t pos, size_t count) const {
    String s;
    if (pos > size()) {
        return s;
    }
    s.mut_str() = const_str().substr(pos, count);
    return s;
}

String String::left(size_t n) const { return mid(0, n); }

String String::right(size_t n) const { return mid(size() - n); }

String String::trimmed() const {
    String s = *this;
    trim_helper(s.mut_str());
    return s;
}

String String::simplified() const {
    //! TODO
    return *this;
}

String String::to_xml_escaped(char16_t c) {
    switch (c) {
        case u'<': return String(u"&lt;");
        case u'>': return String(u"&gt;");
        case u'&': return String(u"&amp;");
        case u'\"': return String(u"&quot;");
        default:
            // ignore invalid characters in xml 1.0
            if ((c < 0x0020 && c != 0x0009 && c != 0x000A && c != 0x000D)) {
                return String();
            }
            return String(Char(c));
    }
}

String String::to_xml_escaped(const String &s) {
    String escaped;
    escaped.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char16_t c = s.at(i).unicode();
        escaped += to_xml_escaped(c);
    }
    return escaped;
}

String String::to_xml_escaped() const { return to_xml_escaped(*this); }

String String::decode_xml_entities(const String &input_src) {
    std::string src = input_src.to_std_string();
    String ret = input_src;
    static const std::regex re("&#([0-9]+);");

    auto begin = std::sregex_iterator(src.begin(), src.end(), re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        std::string str0 = match[0];
        std::string str1 = match[1];
        ret.replace(String::from_std_string(str0), String(Char(std::stoi(str1))));
    }
    return ret;
}

String String::to_lower() const {
    //! TODO
    String s = *this;
    std::u16string &us = s.mut_str();
    std::transform(us.begin(), us.end(), us.begin(), [](char16_t c) { return Char::to_lower(c); });
    return s;
}

String String::to_upper() const {
    //! TODO
    String s = *this;
    std::u16string &us = s.mut_str();
    std::transform(us.begin(), us.end(), us.begin(), [](char16_t c) { return Char::to_upper(c); });
    return s;
}

int String::to_int(bool *ok, int base) const {
    ByteArray ba = to_utf8();
    return static_cast<int>(to_int_helper(ba.const_char(), ok, base));
}

unsigned int String::to_u_int(bool *ok, int base) const {
    ByteArray ba = to_utf8();
    return static_cast<unsigned int>(to_int_helper(ba.const_char(), ok, base));
}

String String::number(int n, int base) {
    std::stringstream stream;
    if (base == 16) {
        stream << std::hex;
    }
    stream << n;
    std::string s = stream.str();
    return from_ascii(s.c_str(), s.size());
}

String String::number(int64_t n) {
    std::string s = std::to_string(n);
    return from_ascii(s.c_str());
}

String String::number(size_t n) {
    std::string s = std::to_string(n);
    return from_ascii(s.c_str());
}

String String::number(double n, int prec) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(prec) << n;
    std::string s = stream.str();

    size_t corrected_idx = s.size() - 1;

    // remove extra '0'
    auto pos = s.find('.');
    if (pos != std::string::npos) {
        if (s.back() == '0') {
            for (int i = static_cast<int>(s.size() - 1); i > 0; --i) {
                if (s.at(i) != '0') {
                    corrected_idx = i;
                    break;
                }
            }
        }
    }

    if (s.at(corrected_idx) == '.') {
        --corrected_idx;
    }

    // remove extra '-'
    const char *cs = s.c_str();
    if (corrected_idx == 1 && std::strncmp(cs, "-0", 2) == 0) {
        ++cs;
        --corrected_idx;
    }

    return from_ascii(cs, corrected_idx + 1);
}

float String::to_float(bool *ok) const { return static_cast<float>(to_double(ok)); }

double String::to_double(bool *ok) const {
    ByteArray ba = to_utf8();
    return to_double_helper(ba.const_char(), ok);
}

// ============================
// StringList
// ============================
StringList &StringList::append(const StringList &l) {
    for (const String &s : l) {
        push_back(s);
    }
    return *this;
}

StringList StringList::filter(const String &str) const {
    StringList result;
    for (const String &s : *this) {
        if (s.contains(str)) {
            result << s;
        }
    }
    return result;
}

String StringList::join(const String &sep) const {
    String res;
    for (size_t i = 0; i < size(); ++i) {
        if (i) {
            res += sep;
        }
        res += at(i);
    }
    return res;
}

void StringList::insert(size_t idx, const String &str) { std::vector<String>::insert(begin() + idx, str); }

void StringList::insert(const_iterator it, const String &str) { std::vector<String>::insert(it, str); }

void StringList::replace(size_t idx, const String &str) { this->operator[](idx) = str; }

bool StringList::remove_all(const String &str) {
    size_t orig_size = size();
    erase(std::remove(begin(), end(), str), end());
    return orig_size != size();
}

void StringList::remove_at(size_t i) { erase(begin() + i); }

std::vector<std::string> StringList::to_std_string_list() const {
    std::vector<std::string> result;
    result.reserve(size());
    for (const String &s : *this) {
        result.push_back(s.to_std_string());
    }
    return result;
}

// ============================
// AsciiChar
// ============================

char AsciiChar::to_lower(char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); }

char AsciiChar::to_upper(char ch) { return static_cast<char>(std::toupper(static_cast<unsigned char>(ch))); }

// ============================
// AsciiStringView
// ============================
const char *AsciiStringView::ascii() const { return data_; }

size_t AsciiStringView::size() const { return size_; }

bool AsciiStringView::empty() const { return size_ == 0; }

AsciiChar AsciiStringView::at(size_t i) const {
    IF_ASSERT_FAILED(i < size()) { return AsciiChar(); }
    return AsciiChar(data_[i]);
}

bool AsciiStringView::contains(char ch) const { return index_of(ch) != muse::nidx; }

size_t AsciiStringView::index_of(char ch) const {
    for (size_t i = 0; i < size_; ++i) {
        if (data_[i] == ch) {
            return i;
        }
    }
    return muse::nidx;
}

int AsciiStringView::to_int(bool *ok, int base) const { return to_int_helper(data_, ok, base); }

double AsciiStringView::to_double(bool *ok) const { return to_double_helper(data_, ok); }
