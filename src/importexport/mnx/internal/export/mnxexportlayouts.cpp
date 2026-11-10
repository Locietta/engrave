/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include <algorithm>
#include <optional>
#include <vector>

#include "engraving/dom/barline.h"
#include "engraving/dom/bracketitem.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/staff.h"
#include "internal/shared/mnxtypesconv.h"
#include "log.h"
#include "mnxexporter.h"
#include "model/layout.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
namespace {
struct LayoutGroupSpan {
    size_t start{};
    size_t end{};
    size_t column{};
    BracketType type{BracketType::NO_BRACKET};
};

struct LayoutGroupNode {
    LayoutGroupSpan span;
    std::vector<size_t> children;
};

struct LayoutBuildContext {
    MnxExporter *exporter{};
    const std::vector<Staff *> *staves{};
    size_t staff_count{};
    const std::vector<LayoutGroupNode> *nodes{};
};
} // namespace

//---------------------------------------------------------
//   containsSpan
//---------------------------------------------------------

static bool contains_span(const LayoutGroupSpan &parent, const LayoutGroupSpan &child) {
    return parent.start <= child.start && parent.end >= child.end;
}

//---------------------------------------------------------
//   sortChildIndices
//---------------------------------------------------------

static void sort_child_indices(std::vector<size_t> &child_idxs, const std::vector<LayoutGroupNode> &nodes) {
    std::sort(child_idxs.begin(), child_idxs.end(), [&](size_t lhs, size_t rhs) {
        const LayoutGroupSpan &a = nodes[lhs].span;
        const LayoutGroupSpan &b = nodes[rhs].span;
        if (a.start != b.start) {
            return a.start < b.start;
        }
        if (a.end != b.end) {
            return a.end > b.end;
        }
        return a.column < b.column;
    });
}

//---------------------------------------------------------
//   buildGroupSpans
//---------------------------------------------------------

static std::vector<LayoutGroupSpan> build_group_spans(const std::vector<Staff *> &staves, size_t staff_count) {
    std::vector<LayoutGroupSpan> groups;
    groups.reserve(staff_count);

    for (size_t staff_idx = 0; staff_idx < staff_count; ++staff_idx) {
        const Staff *staff = staves[staff_idx];
        if (!staff) {
            continue;
        }
        for (const BracketItem *bracket : staff->score()->brackets(staff->idx())) {
            if (!bracket) {
                continue;
            }
            const size_t span = bracket->bracket_span();
            if (span == 0) {
                continue;
            }
            size_t end = staff_idx + span - 1;
            if (end >= staff_count) {
                LOGW() << "Bracket span exceeds staff count; clamping span. Start=" << staff_idx << " span=" << span
                       << " staves=" << staff_count;
                end = staff_count - 1;
                if (end < staff_idx) {
                    continue;
                }
            }
            if (bracket->bracket_type() == BracketType::NO_BRACKET) {
                if (span <= 1) {
                    continue;
                }
                bool has_barline_grouping = false;
                for (size_t idx = staff_idx; idx < end; ++idx) {
                    const Staff *grouped_staff = staves[idx];
                    if (grouped_staff && grouped_staff->bar_line_span()) {
                        has_barline_grouping = true;
                        break;
                    }
                }
                if (!has_barline_grouping) {
                    continue;
                }
            }
            groups.push_back({staff_idx, end, bracket->column(), bracket->bracket_type()});
        }
    }

    std::sort(groups.begin(), groups.end(), [](const LayoutGroupSpan &a, const LayoutGroupSpan &b) {
        if (a.start != b.start) {
            return a.start < b.start;
        }
        if (a.end != b.end) {
            return a.end > b.end;
        }
        return a.column < b.column;
    });

    return groups;
}

//---------------------------------------------------------
//   buildGroupNodes
//---------------------------------------------------------

static void build_group_nodes(const std::vector<LayoutGroupSpan> &groups, std::vector<LayoutGroupNode> &nodes,
                              std::vector<size_t> &root_children) {
    nodes.clear();
    root_children.clear();
    nodes.reserve(groups.size());

    for (const LayoutGroupSpan &group : groups) {
        const size_t node_idx = nodes.size();
        nodes.push_back({group, {}});

        std::optional<size_t> parent_idx;
        for (size_t candidate = 0; candidate < node_idx; ++candidate) {
            const LayoutGroupSpan &parent_span = nodes[candidate].span;
            if (!contains_span(parent_span, group) || parent_span.column >= group.column) {
                continue;
            }
            if (!parent_idx) {
                parent_idx = candidate;
                continue;
            }
            const LayoutGroupSpan &best_span = nodes[*parent_idx].span;
            if (parent_span.column > best_span.column) {
                parent_idx = candidate;
                continue;
            }
            const size_t parent_span_size = parent_span.end - parent_span.start;
            const size_t best_span_size = best_span.end - best_span.start;
            if (parent_span.column == best_span.column && parent_span_size < best_span_size) {
                parent_idx = candidate;
            }
        }

        if (parent_idx) {
            nodes[*parent_idx].children.push_back(node_idx);
        } else {
            root_children.push_back(node_idx);
        }
    }

    for (auto &node : nodes) {
        sort_child_indices(node.children, nodes);
    }
    sort_child_indices(root_children, nodes);
}

//---------------------------------------------------------
//   appendLayoutStaff
//---------------------------------------------------------

static void append_layout_staff(LayoutBuildContext &ctx, std::vector<model::SystemLayoutItem> &content, size_t staff_idx) {
    Staff *staff = ctx.staves->at(staff_idx);
    if (!staff) {
        LOGW() << "Skipping layout staff " << staff_idx << " with null staff.";
        return;
    }
    const staff_idx_t score_staff_idx = staff->idx();
    const auto &staff_to_part_staff = ctx.exporter->staff_to_part_staff();
    const auto it = staff_to_part_staff.find(score_staff_idx);
    if (it == staff_to_part_staff.end()) {
        LOGW() << "Skipping layout staff " << staff_idx << " with no part/staff mapping.";
        return;
    }
    const auto [part_index, staff_num] = it->second;
    std::string part_id;
    const model::Root &document = ctx.exporter->document();
    if (part_index < document.parts.size()) {
        if (const auto &id = document.parts[part_index].id) {
            part_id = *id;
        }
    }
    if (part_id.empty()) {
        Part *part = staff ? staff->part() : nullptr;
        if (part) {
            part_id = ctx.exporter->get_or_assign_eid(part).to_std_string();
        }
    }
    if (part_id.empty()) {
        LOGW() << "Skipping layout staff " << staff_idx << " with no part id.";
        return;
    }
    model::Staff mnx_staff;
    mnx_staff.sources.push_back(model::StaffSource{.part = model::Id(part_id), .staff = staff_num});
    content.emplace_back(std::move(mnx_staff));
}

//---------------------------------------------------------
//   mensurStricheSpanFrom
//---------------------------------------------------------

static int mensur_striche_span_from(const Staff *staff) {
    IF_ASSERT_FAILED(staff) { return 0; }
    const int lines = staff->lines(Fraction(0, 1)) - 1;
    return lines <= 0 ? ::mu::engraving::barline_span_1_linestaff_to : 2 * lines;
}

//---------------------------------------------------------
//   isSinglePartGroup
//---------------------------------------------------------

static bool is_single_part_group(const LayoutBuildContext &ctx, const LayoutGroupSpan &span) {
    if (span.start >= ctx.staff_count || span.end >= ctx.staff_count) {
        return false;
    }

    const Staff *first_staff = ctx.staves->at(span.start);
    const Part *first_part = first_staff ? first_staff->part() : nullptr;
    if (!first_part) {
        return false;
    }

    for (size_t idx = span.start + 1; idx <= span.end; ++idx) {
        const Staff *staff = ctx.staves->at(idx);
        if (!staff || staff->part() != first_part) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   calcGroupBarlineOverride
//---------------------------------------------------------

static model::BarlineOverride calc_group_barline_override(const LayoutBuildContext &ctx, const LayoutGroupSpan &span) {
    if (span.end <= span.start || span.end >= ctx.staff_count) {
        return model::BarlineOverride::NONE;
    }

    bool all_connected = true;
    bool all_mensurstrich = true;

    for (size_t idx = span.start; idx < span.end; ++idx) {
        const Staff *staff = ctx.staves->at(idx);
        if (!staff || !staff->bar_line_span()) {
            all_connected = false;
            break;
        }

        const bool has_mensur_span = (staff->bar_line_to() == 0) && (staff->bar_line_from() == mensur_striche_span_from(staff));
        all_mensurstrich = all_mensurstrich && has_mensur_span;
    }

    if (!all_connected) {
        return model::BarlineOverride::NONE;
    }
    if (all_mensurstrich) {
        return model::BarlineOverride::MENSURSTRICH;
    }
    return model::BarlineOverride::UNIFIED;
}

//---------------------------------------------------------
//   buildContent
//---------------------------------------------------------

static void build_content(LayoutBuildContext &ctx, std::vector<model::SystemLayoutItem> &content, size_t start, size_t end,
                          const std::vector<size_t> &children) {
    size_t staff_idx = start;
    size_t child_pos = 0;
    const auto &nodes = *ctx.nodes;
    while (staff_idx <= end && staff_idx < static_cast<size_t>(ctx.staff_count)) {
        while (child_pos < children.size() && nodes[children[child_pos]].span.end < staff_idx) {
            ++child_pos;
        }
        if (child_pos < children.size() && nodes[children[child_pos]].span.start == staff_idx) {
            const LayoutGroupNode &node = nodes[children[child_pos]];
            model::StaffGroup mnx_group;
            model::StaffGroupBarlineStyle style = model::StaffGroupBarlineStyle::INDIVIDUAL;
            switch (calc_group_barline_override(ctx, node.span)) {
                case model::BarlineOverride::NONE: style = model::StaffGroupBarlineStyle::INDIVIDUAL; break;
                case model::BarlineOverride::UNIFIED:
                    style = is_single_part_group(ctx, node.span) ? model::StaffGroupBarlineStyle::INSTRUMENT :
                                                                   model::StaffGroupBarlineStyle::UNIFIED;
                    break;
                case model::BarlineOverride::MENSURSTRICH: style = model::StaffGroupBarlineStyle::MENSURSTRICH; break;
            }
            set_unless_default(mnx_group.barline_style, style, model::StaffGroupBarlineStyle::INSTRUMENT);
            const model::StaffSymbol symbol = to_mnx_layout_symbol(node.span.type);
            if (symbol != model::StaffSymbol::NO_SYMBOL) {
                mnx_group.symbol = symbol;
            }
            build_content(ctx, mnx_group.content, node.span.start, node.span.end, node.children);
            content.emplace_back(std::move(mnx_group));
            staff_idx = node.span.end + 1;
            ++child_pos;
        } else {
            append_layout_staff(ctx, content, staff_idx);
            ++staff_idx;
        }
    }
}

//---------------------------------------------------------
//   createLayout
//---------------------------------------------------------

void MnxExporter::create_layout(const std::vector<Staff *> &staves, const std::string &layout_id) {
    auto &mnx_layout = ensure(root_.layouts).emplace_back();
    if (!layout_id.empty()) {
        mnx_layout.id = model::Id(layout_id);
    }

    const size_t staff_count = staves.size();
    if (staff_count == 0) {
        return;
    }

    std::vector<LayoutGroupSpan> groups = build_group_spans(staves, staff_count);

    std::vector<LayoutGroupNode> nodes;
    std::vector<size_t> root_children;
    build_group_nodes(groups, nodes, root_children);

    LayoutBuildContext ctx;
    ctx.exporter = this;
    ctx.staves = &staves;
    ctx.staff_count = staff_count;
    ctx.nodes = &nodes;

    build_content(ctx, mnx_layout.content, 0, staff_count - 1, root_children);
}
} // namespace mu::iex::mnxio
