// SPDX-License-Identifier: GPL-3.0-only
#include "document.h"

#include <tuple>

namespace mu::iex::mnxio::model {

std::expected<Root, ReadError> read_document(std::string_view text) {
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    // glaze reads a null-terminated buffer; std::string provides the terminator.
    const std::string buffer(text);
    Root root{};
    if (const auto error = glz::read<detail::read_opts>(root, buffer))
        return std::unexpected(ReadError{glz::format_error(error, buffer), error.count});
    return root;
}

std::string write_document(const Root &root) {
    std::string out;
    std::ignore = glz::write<glz::opts{.skip_null_members = true}>(root, out);
    return out;
}

} // namespace mu::iex::mnxio::model
