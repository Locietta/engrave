// SPDX-License-Identifier: MIT
// The JSON document model shared by the vendored json-schema-validator and
// mnxdom: glaze's generic DOM, plus the RFC 6901 JSON pointer, navigation,
// parsing and serialization semantics both libraries were written against
// (nlohmann::json 3.12).
#pragma once

#include <compare>
#include <cstddef>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/json/generic.hpp>

namespace json_schema {

// Lexicographically ordered objects and distinct unsigned, signed and floating
// numbers: the observable model of nlohmann::json.
using json = glz::generic_sorted_u64;

struct parse_error : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

struct out_of_range : std::out_of_range {
    using std::out_of_range::out_of_range;
};

struct type_error : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

// An RFC 6901 JSON pointer: the reference tokens, unescaped.
class json_pointer {
public:
    json_pointer() = default;
    // Parses "" or "/token/token..." (~0 and ~1 escapes); throws parse_error.
    explicit json_pointer(std::string_view text);

    [[nodiscard]] std::string to_string() const;
    friend std::ostream &operator<<(std::ostream &stream, const json_pointer &pointer) { return stream << pointer.to_string(); }

    json_pointer &operator/=(const json_pointer &suffix);
    json_pointer &operator/=(std::string token);
    json_pointer &operator/=(std::size_t index);
    friend json_pointer operator/(json_pointer pointer, const json_pointer &suffix) { return pointer /= suffix; }
    friend json_pointer operator/(json_pointer pointer, std::string token) { return pointer /= std::move(token); }
    friend json_pointer operator/(json_pointer pointer, const char *token) { return pointer /= std::string(token); }
    friend json_pointer operator/(json_pointer pointer, std::size_t index) { return pointer /= index; }

    // The root's parent is the root.
    [[nodiscard]] json_pointer parent_pointer() const;
    // These throw out_of_range on the root pointer.
    void pop_back();
    [[nodiscard]] const std::string &back() const;
    void push_back(std::string token) { tokens_.push_back(std::move(token)); }

    [[nodiscard]] bool empty() const noexcept { return tokens_.empty(); }
    [[nodiscard]] const std::vector<std::string> &tokens() const noexcept { return tokens_; }

    friend bool operator==(const json_pointer &, const json_pointer &) = default;
    friend std::strong_ordering operator<=>(const json_pointer &, const json_pointer &) = default;

private:
    std::vector<std::string> tokens_;
};

// The node a pointer refers to, or nullptr when it does not resolve (a missing
// key, an index that is malformed, "-" or out of range, or a token below a
// primitive value).
[[nodiscard]] const json *find(const json &root, const json_pointer &pointer) noexcept;
[[nodiscard]] json *find(json &root, const json_pointer &pointer) noexcept;
[[nodiscard]] inline bool contains(const json &root, const json_pointer &pointer) noexcept { return find(root, pointer) != nullptr; }

// Resolves a pointer that must exist; throws out_of_range or parse_error with
// the reason it does not.
[[nodiscard]] const json &at(const json &root, const json_pointer &pointer);
[[nodiscard]] json &at(json &root, const json_pointer &pointer);

// Resolves a pointer for writing (json::operator[](json_pointer)): missing
// object members are created, a null node becomes an array when the token is
// numeric or "-" and an object otherwise, arrays grow with nulls up to the
// index and "-" appends.  Throws out_of_range below a primitive value.
json &get_or_create(json &root, const json_pointer &pointer);

// Parses one JSON text; a leading UTF-8 byte order mark is skipped and
// trailing non-whitespace content is an error.  Throws parse_error.
[[nodiscard]] json parse(std::string_view text);

// Serializes compactly (indent < 0) or with indent spaces per level, byte for
// byte as nlohmann::json::dump(indent) does (floating numbers keep a ".0" or
// an exponent; control characters are escaped).
[[nodiscard]] std::string dump(const json &value, int indent = -1);

// "null", "boolean", "number", "string", "array" or "object".
[[nodiscard]] std::string_view type_name(const json &value) noexcept;

// nlohmann::json's operator==: numbers compare by value across their unsigned,
// signed and floating representations (an unsigned value is cast to the signed
// type against a signed one, as nlohmann does).
[[nodiscard]] bool equal(const json &left, const json &right) noexcept;

// nlohmann::json's operator< order, consistent with equal(): values of
// different types order by type (null < boolean < number < object < array <
// string), numbers by value, strings, arrays and objects lexicographically.
// Unordered only where a NaN is involved.
[[nodiscard]] std::partial_ordering compare(const json &left, const json &right) noexcept;

} // namespace json_schema
