// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

namespace mu::engraving {
struct Score;
} // namespace mu::engraving

namespace engrave::write470 {

struct ConnectorIssue {
    pugi::xml_node node;
    std::string detail;
};

bool is_legacy_connector(std::string_view type, std::string_view receiver);
std::vector<ConnectorIssue> reconstruct_connectors(pugi::xml_node root, std::span<mu::engraving::Score *const> scores, void (*poll)());

} // namespace engrave::write470
