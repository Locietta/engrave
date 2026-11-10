// SPDX-License-Identifier: GPL-3.0-only
#include "document.h"

#include <format>
#include <tuple>

namespace mu::iex::mnxio::model {

std::expected<Root, ReadError> read_document(std::string_view text) {
    std::size_t skipped = 0;
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.remove_prefix(3);
        skipped = 3;
    }
    // glaze reads a null-terminated buffer; std::string provides the terminator.
    const std::string buffer(text);
    Root root{};
    if (const auto error = glz::read<detail::read_opts>(root, buffer))
        return std::unexpected(ReadError{glz::format_error(error, buffer), skipped + error.count});
    return root;
}

std::string write_document(const Root &root) {
    std::string out;
    std::ignore = glz::write<glz::opts{.skip_null_members = true}>(root, out);
    return out;
}

std::expected<Document, LoadError> load_document(std::string_view text, const IndexOptions &options) {
    auto root = read_document(text);
    if (!root) return std::unexpected(LoadError{std::move(root.error().message)});
    auto indexed = build_index(*root, options);
    if (!indexed) {
        constexpr std::size_t shown = 20;
        std::string message;
        std::size_t errors = 0;
        for (const auto &diagnostic : indexed.error()) {
            if (diagnostic.severity != Severity::ERROR) continue;
            if (errors++ < shown) message += std::format("{}{}: {}", message.empty() ? "" : "\n", diagnostic.pointer, diagnostic.message);
        }
        if (errors > shown) message += std::format("\n(and {} more errors)", errors - shown);
        return std::unexpected(LoadError{std::move(message)});
    }
    // The index points into the vectors of `root`, whose buffers survive these moves.
    return Document{std::move(*root), std::move(indexed->index), std::move(indexed->warnings)};
}

} // namespace mu::iex::mnxio::model
