// SPDX-License-Identifier: GPL-3.0-only
// Adapted from partitioncloud/webmscore web/importexport/positionjsonwriter.cpp.
// The pinned engine revision is the vendor/musescore submodule (docs/dev/fork.md).
#include "score_positions.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/page.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/system.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <functional>
#include <vector>

namespace music {
namespace {
using namespace mu::engraving;
using Json = nlohmann::json;

int checked_int(double value) {
    if (!std::isfinite(value) || value < std::numeric_limits<int>::min()
        || value > std::numeric_limits<int>::max())
        throw std::runtime_error("Position value exceeds supported integer range");
    return static_cast<int>(value);
}

System* layout_system(Measure* measure) {
    if (!measure || !measure->system() || !measure->system()->page())
        throw std::runtime_error("Score must be laid out before exporting positions");
    return measure->system();
}
}

std::pair<double, double> visit_score_positions(mu::engraving::Score* score, bool segments,
                                               const PositionVisitor& visitor, bool resolve_ids,
                                               std::size_t element_count) {
    if (!score || !score->masterScore() || score->pages().empty() || !score->pages().front())
        throw std::invalid_argument("A laid-out score with pages is required");

    // Cache once: repeated access can rebuild the list after upstream tie cleanup.
    // Explicit expansion leaves the master score's playback preference unchanged.
    const RepeatList& repeats = score->repeatList(true);
    struct IdEntry { const void* item; std::size_t id; };
    std::vector<IdEntry> ids;
    if (resolve_ids) ids.reserve(element_count);
    std::size_t next_id = 0;
    const auto add_element = [&](const void* item, auto x, auto y, double width,
                                 double height, Page* page) {
        const auto id = checked_int(static_cast<double>(next_id++));
        const auto page_index = score->pageIdx(page);
        if (page_index >= score->pages().size()) throw std::runtime_error("Layout page is not in score");
        visitor.element(id, x, y, width, height, static_cast<int>(page_index));
        if (resolve_ids) ids.push_back({item, static_cast<std::size_t>(id)});
    };
    // Scale 1.0 preserves webmscore's raw layout units (1200 DPI), independent of PNG DPI.
    if (segments) {
        auto* first = score->firstMeasureMM();
        while (first && !first->first(SegmentType::ChordRest)) first = first->nextMeasureMM();
        for (auto* segment = first ? first->first(SegmentType::ChordRest) : nullptr;
             segment; segment = segment->next1MM(SegmentType::ChordRest)) {
            auto* system = layout_system(segment->measure());
            double width = 0;
            for (size_t track = 0; track < score->nstaves() * VOICES; ++track) {
                auto* item = segment->element(static_cast<int>(track));
                // The pinned writer checks the obsolete item->bbox(), which is empty.
                if (item) {
                    const auto& box = item->ldata()->bbox(LD_ACCESS::MAYBE_NOTINITED);
                    if (box.isValid()) width = std::max(width, box.width());
                }
            }
            // Preserve upstream truncation specifically for segment x/y coordinates.
            add_element(segment, checked_int(segment->pagePos().x()), checked_int(segment->pagePos().y()),
                        width, system->height(), system->page());
        }
    } else {
        for (auto* measure = score->firstMeasureMM(); measure; measure = measure->nextMeasureMM()) {
            auto* system = layout_system(measure);
            add_element(measure, measure->pagePos().x(), system->pagePos().y(),
                        measure->ldata()->bbox().width(), system->height(), system->page());
        }
    }

    if (resolve_ids) {
        std::sort(ids.begin(), ids.end(), [](const IdEntry& a, const IdEntry& b) {
            return std::less<const void*>{}(a.item, b.item);
        });
    }
    const auto add_event = [&](const void* item, int tick) {
        int id = 0;
        if (resolve_ids) {
            const auto it = std::lower_bound(ids.begin(), ids.end(), item, [](const IdEntry& a, const void* b) {
                return std::less<const void*>{}(a.item, b);
            });
            if (it == ids.end() || it->item != item)
                throw std::runtime_error("Playback element has no layout position");
            id = static_cast<int>(it->id);
        }
        const auto time = checked_int(std::nearbyint(score->utick2utime(tick) * 1000.0));
        visitor.event(id, time);
    };
    for (const auto* repeat : repeats) {
        if (repeat->len() <= 0) continue;
        const int end_tick = repeat->tick + repeat->len();
        const int offset = repeat->utick - repeat->tick;
        for (auto* measure = score->tick2measureMM(Fraction::fromTicks(repeat->tick));
             measure; measure = measure->nextMeasureMM()) {
            if (segments) {
                for (auto* segment = measure->first(SegmentType::ChordRest); segment;
                     segment = segment->next(SegmentType::ChordRest))
                    add_event(segment, segment->tick().ticks() + offset);
            } else {
                add_event(measure, measure->tick().ticks() + offset);
            }
            if (measure->endTick().ticks() >= end_tick) break;
        }
    }
    const auto* page = score->pages().front();
    return {page->width(), page->height()};
}

nlohmann::json score_positions(mu::engraving::Score* score, bool segments) {
    Json elements = Json::array();
    Json events = Json::array();
    const PositionVisitor visitor{
        [&](int id, double x, double y, double width, double height, int page) {
            if (segments) {
                elements.push_back({{"id", id}, {"x", static_cast<int>(x)}, {"y", static_cast<int>(y)},
                                    {"sx", width}, {"sy", height}, {"page", page}});
            } else {
                elements.push_back({{"id", id}, {"x", x}, {"y", y}, {"sx", width}, {"sy", height}, {"page", page}});
            }
        },
        [&](int id, int time) { events.push_back({{"elid", id}, {"position", time}}); }
    };
    const auto [width, height] = visit_score_positions(score, segments, visitor, true);
    return {{"pageSize", {{"width", width}, {"height", height}}},
            {"elements", std::move(elements)}, {"events", std::move(events)}};
}
}
