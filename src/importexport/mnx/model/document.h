// SPDX-License-Identifier: GPL-3.0-only
// Reading and writing MNX documents (see docs/mnx.md).
#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "mnx_model.h"

namespace mu::iex::mnxio::model {

// Why a text is not an MNX document: invalid JSON or a schema violation.
struct ReadError {
    std::string message;    // glaze's description, with line, column and the offending text
    std::size_t offset = 0; // byte offset of the error in the input
};

// Reads an MNX document. A leading UTF-8 byte order mark is skipped. The read enforces the
// MNX schema (docs/mnx.md lists the deliberate departures); it does not check the semantic
// rules, which build_index does.
std::expected<Root, ReadError> read_document(std::string_view text);

// Writes a document as compact JSON, omitting absent optional members.
std::string write_document(const Root &root);

} // namespace mu::iex::mnxio::model
