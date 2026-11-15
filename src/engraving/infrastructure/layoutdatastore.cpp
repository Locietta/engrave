/* SPDX-License-Identifier: GPL-3.0-only */
#include "rendering/score/layoutdatastore.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "dom/beam.h"
#include "dom/factory.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurebase.h"
#include "dom/note.h"
#include "dom/page.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spanner.h"
#include "dom/system.h"
#include "dom/tuplet.h"
#include "rendering/score/layoutdatafactory.h"
#include "rendering/score/scorerenderer.h"

namespace mu::engraving::rendering::score {
struct LayoutDataStore::Records {
    struct Record {
        EngravingItem *item;
        std::unique_ptr<EngravingItem::LayoutData> data;
    };

    ankerl::unordered_dense::map<const EngravingItem *, Record> items;
};

LayoutDataStore::LayoutDataStore(Score &score) : records_(std::make_unique<Records>()), score_(&score) {}

LayoutDataStore::~LayoutDataStore() { detach_score(); }

void LayoutDataStore::register_layout_copy(const EngravingItem &source, EngravingItem &display) {
    if (&source == &display) return;
    display.set_layout_copy(true);
    for (auto &entry : layout_copies_)
        if (entry.second == &source) entry.second = &display;
    std::erase_if(layout_copies_, [&](const auto &entry) { return entry.first == &source; });
    layout_copies_.emplace_back(&source, &display);
    // Composite annotations (e.g. a fret diagram's harmony) keep their own IDs.
    // Reconnected ties can move relative to other types; order within a type is stable.
    const auto &source_children = source.children();
    const auto &display_children = display.children();
    std::map<ElementType, size_t> ordinals;
    for (const auto *child : source_children) {
        if (!child->is_engraving_item()) continue;
        size_t ordinal = ordinals[child->type()]++;
        const auto match = std::find_if(display_children.begin(), display_children.end(), [&](const auto *copy) {
            if (copy->type() != child->type()) return false;
            return ordinal-- == 0;
        });
        if (match != display_children.end()) register_layout_copy(*to_engraving_item(child), *to_engraving_item(*match));
    }
}

const EngravingItem *LayoutDataStore::layout_copy(const EngravingItem &source) const {
    const auto found = std::ranges::find_if(layout_copies_, [&](const auto &entry) { return entry.first == &source; });
    return found != layout_copies_.end() ? found->second : nullptr;
}

void LayoutDataStore::clear_layout_copies(const Measure &display) {
    std::erase_if(layout_copies_, [&](const auto &entry) { return entry.second->find_measure() == &display; });
}

void LayoutDataStore::retire_layout_copy(const EngravingItem &item) {
    std::erase_if(layout_copies_, [&](const auto &entry) { return entry.first == &item || entry.second == &item; });
}

size_t LayoutDataStore::size() const { return records_->items.size(); }

const EngravingItem::LayoutData *LayoutDataStore::find(const EngravingItem &item) const {
    const auto it = records_->items.find(&item);
    return it == records_->items.end() ? nullptr : it->second.data.get();
}

EngravingItem::LayoutData *LayoutDataStore::mut_data(EngravingItem &item) {
    if (item.layout_data_) {
        if (item.layout_store_ != this) throw std::logic_error("layout payload belongs to a different store");
        return item.layout_data_;
    }
    return adopt(item, make_layout_data(item.layout_type_));
}

EngravingItem::LayoutData *LayoutDataStore::adopt(EngravingItem &item, std::unique_ptr<EngravingItem::LayoutData> data) {
    if (item.layout_store_ || item.layout_data_) throw std::logic_error("layout payload already bound");
    data->item_ = &item;
    auto *result = data.get();
    records_->items.emplace(&item, Records::Record{&item, std::move(data)});
    item.layout_store_ = this;
    item.layout_data_ = result;
    return result;
}

void LayoutDataStore::erase(EngravingItem &item) {
    const auto it = records_->items.find(&item);
    if (it == records_->items.end()) return;
    if (item.is_measure_base()) {
        const auto *data = static_cast<const MeasureBase::LayoutData *>(it->second.data.get());
        // The base destructor has already unlinked placement before erasing here.
        if (auto *system = data->system) system->remove_measure(to_measure_base(&item));
    }
    if (auto *shared = item.shared_item()) EngravingItem::disconnect_shared_item(shared, &item);
    EngravingItem::disconnect_all_origin_items(&item);
    // Extract before running destructors: Chord payloads still own generated
    // ledger shells, whose destruction recursively retires other records (L3).
    auto data = std::move(it->second.data);
    records_->items.erase(it);
    data->disconnect_snapped_items();
    item.layout_data_ = nullptr;
    item.layout_store_ = nullptr;
}

void LayoutDataStore::for_each_output(const std::function<void(EngravingItem &, EngravingItem::LayoutData &)> &complete) {
    for (auto &record : records_->items.values()) complete(*record.second.item, *record.second.data);
}

void LayoutDataStore::detach_score() {
    discard_score_output();
    while (!records_->items.empty()) erase(*records_->items.begin()->second.item);
    if (score_ && score_->layout_data_store() == this) score_->bind_layout_data_store(nullptr);
    score_ = nullptr;
}

void LayoutDataStore::clear() {
    if (score_) {
        if (score_->master_score()->cmd_state().locked()) throw std::logic_error("cannot discard output during layout");
        score_->invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    }
    discard_score_output();
    while (!records_->items.empty()) erase(*records_->items.begin()->second.item);
}

static void clear_output_anchors(Score &score, const Measure *measure = nullptr) {
    const auto retired = [&](const EngravingItem *item) {
        return item && !source_object(*item) && (!measure || item->find_measure() == measure);
    };
    for (auto *spanner : score.spanner_list()) {
        if (retired(spanner->start_element())) spanner->set_start_element(nullptr);
        if (retired(spanner->end_element())) spanner->set_end_element(nullptr);
    }
}

Beam *LayoutDataStore::create_beam() {
    auto beam = std::unique_ptr<Beam>(Factory::create_beam(score_));
    beam->set_layout_copy(true);
    beam->set_generated(true);
    output_.beams.push_back(beam.get());
    return beam.release();
}

void LayoutDataStore::retire_beam_borrowers(const BeamBase &beam) {
    for (auto *page : output_.pages) page->invalidate_bsp_tree();
    const auto borrows_beam = [&](const ShapeElement &element) {
        for (auto *item = element.item(); item; item = item->ownership_parent_item())
            if (item == &beam) return true;
        return false;
    };
    // Reusable tail systems live outside ScoreOutput during a range pass.
    // Records include them and the completed systems whose collisions must stay.
    for (auto &record : records_->items.values()) {
        auto *item = record.second.item;
        auto &data = *record.second.data;
        if (data.is_set_shape()) data.mut_shape().remove_if(borrows_beam);
        if (data.mask_.has_value()) data.mask_.mut_value().remove_if(borrows_beam);
        if (item->is_beam() || item->type() == ElementType::TREMOLO_TWOCHORD) {
            auto *beam_data = static_cast<BeamBase::LayoutData *>(&data);
            if (beam_data->beam == &beam) beam_data->beam = nullptr;
        }
        if (item->is_chord_rest()) {
            auto *cr = to_chord_rest(item);
            auto *ld = static_cast<ChordRest::LayoutData *>(&data);
            if (ld->rendered_beam == &beam) ld->rendered_beam = nullptr;
            if (ld->beamlet && ld->beamlet->parent_element == &beam) cr->set_beamlet(nullptr);
        }
        if (item->is_system()) {
            for (auto *staff : to_system(item)->staves()) {
                std::erase_if(staff->skyline().north().elements(), borrows_beam);
                std::erase_if(staff->skyline().south().elements(), borrows_beam);
            }
        } else if (item->is_segment()) {
            auto *owner = to_segment(item);
            const auto size = static_cast<const Segment *>(owner)->shapes().size();
            for (size_t i = 0; i < size; ++i) owner->staff_shape(i).remove_if(borrows_beam);
        }
    }
}

void LayoutDataStore::delete_layout_child(EngravingItem *item) {
    if (!item) return;
    if (auto *store = item->score()->layout_data_store()) store->retire_shape_borrowers(*item);
    delete item;
}

void LayoutDataStore::retire_spanner_borrowers(const SpannerSegment &segment) { retire_shape_borrowers(segment); }

void LayoutDataStore::retire_shape_borrowers(const EngravingItem &retired) {
    for (auto *page : output_.pages) page->invalidate_bsp_tree();
    const auto borrows_item = [&](const ShapeElement &element) {
        for (auto *item = element.item(); item; item = item->ownership_parent_item())
            if (item == &retired) return true;
        return false;
    };
    // Reusable tail systems live outside ScoreOutput during a range pass.
    // Records include them and the completed systems whose collisions must stay.
    for (auto &record : records_->items.values()) {
        auto *item = record.second.item;
        auto &data = *record.second.data;
        if (data.is_set_shape()) data.mut_shape().remove_if(borrows_item);
        if (data.mask_.has_value()) data.mask_.mut_value().remove_if(borrows_item);
        if (item->is_system()) {
            for (auto *staff : to_system(item)->staves()) {
                std::erase_if(staff->skyline().north().elements(), borrows_item);
                std::erase_if(staff->skyline().south().elements(), borrows_item);
            }
        } else if (item->is_segment()) {
            auto *owner = to_segment(item);
            const auto size = static_cast<const Segment *>(owner)->shapes().size();
            for (size_t i = 0; i < size; ++i) owner->staff_shape(i).remove_if(borrows_item);
        }
    }
}

void LayoutDataStore::prune_spanner_segments() {
    std::vector<Spanner *> spanners;
    for (auto &record : records_->items.values())
        if (record.second.item->is_spanner()) spanners.push_back(to_spanner(record.second.item));
    for (auto *spanner : spanners) spanner->erase_detached_segments();
    std::erase_if(output_.beams, [](Beam *beam) {
        if (!beam->rendered_elements().empty()) return false;
        delete beam;
        return true;
    });
}

void LayoutDataStore::retire_measure_borrowers(Measure &measure) {
    if (!score_) return;
    clear_output_anchors(*score_, &measure);
    auto *system = measure.system();
    if (!system) return;
    if (auto *page = system->page()) page->invalidate_bsp_tree();
    for (auto *staff : system->staves()) staff->skyline().clear();
    for (auto &record : records_->items.values()) {
        auto *item = record.second.item;
        if (item->is_segment() && to_segment(item)->measure()->system() == system) to_segment(item)->clear_shapes();
    }
    if (score_->master_score()->cmd_state().locked()) return;
    retire_shared_auxiliary_output();
    // An edit can delete a source node together with its generated children
    // before the next pass. Rebuild every system whose borrowed views we cleared.
    if (auto *first = system->first_measure()) {
        auto *last = system->last_measure();
        score_->invalidate_layout(first->tick(), last ? last->end_tick() : first->end_tick());
    }
}

std::vector<Spanner *> LayoutDataStore::rendered_spanners(int first_tick, int last_tick) const {
    std::vector<Spanner *> result;
    for (auto interval : score_->spanner_map().find_overlapping(first_tick, last_tick)) {
        if (std::find(output_.suppressed_shared_spanners.begin(), output_.suppressed_shared_spanners.end(), interval.value) ==
            output_.suppressed_shared_spanners.end())
            result.push_back(interval.value);
    }
    for (auto *span : output_.shared_spanners) {
        if (!span->is_tie() && span->anchor() != Spanner::Anchor::NOTE && span->tick().ticks() <= last_tick &&
            span->tick2().ticks() >= first_tick)
            result.push_back(span);
    }
    return result;
}

void LayoutDataStore::retire_shared_auxiliary_output() {
    if (!output_.shared_spanners.empty() || !output_.shared_tuplets.empty()) {
        // Auxiliary output may span systems beyond the edited measure.
        for (auto *page : output_.pages) page->invalidate_bsp_tree();
        for (auto *system : output_.systems)
            for (auto *staff : system->staves()) staff->skyline().clear();
        for (auto &record : records_->items.values())
            if (record.second.item->is_segment()) to_segment(record.second.item)->clear_shapes();
    }
    // Note borrowers must be detached while the display notes are still alive.
    for (auto *spanner : output_.shared_spanners) {
        if (auto *parent = spanner->ownership_parent_item(); parent && parent->is_note()) parent->remove(spanner);
    }
    muse::delete_all(output_.shared_spanners);
    output_.shared_spanners.clear();
    muse::delete_all(output_.shared_tuplets);
    output_.shared_tuplets.clear();
}

void LayoutDataStore::discard_score_output() {
    // Payload destruction can retire spanners after notes and their dots. Clear
    // every copied collision before the first display child can be freed.
    for (auto &record : records_->items.values()) {
        record.second.data->clear_shape();
        record.second.data->mask_.reset();
    }
    if (score_) {
        clear_output_anchors(*score_);
        // These legacy L5 caches can retain pointers to output-owned ledger and
        // Harmony parenthesis shells. Retire their views before deleting shells.
        for (auto *page : output_.pages) page->invalidate_bsp_tree();
        for (auto *system : output_.systems) {
            for (auto *staff : system->staves()) staff->skyline().clear();
        }
        // Ownership traversal includes hidden and MMR display measures, unlike
        // drawing/source timeline scans. Retain each segment's staff slots.
        const auto clear_segments = [&](auto &&self, EngravingObject &owner) -> void {
            if (owner.is_segment()) to_segment(&owner)->clear_shapes();
            for (auto *child : owner.children()) {
                if (child->score() == score_) self(self, *child);
            }
        };
        clear_segments(clear_segments, *score_);
        // Detached scratch segments can also have prepared collision shapes.
        for (auto &record : records_->items.values()) {
            if (record.second.item->is_segment()) to_segment(record.second.item)->clear_shapes();
        }
    }
    for (auto &record : records_->items.values()) {
        if (record.second.item->is_beam()) static_cast<Beam::LayoutData *>(record.second.data.get())->members.reset();
        if (record.second.item->is_chord_rest()) {
            auto *data = static_cast<ChordRest::LayoutData *>(record.second.data.get());
            data->beamlet = nullptr;
            data->rendered_beam.reset();
        }
    }
    layout_copies_.clear();
    output_.shared_track_maps.clear();
    output_.suppressed_shared_spanners.clear();
    // Systems unlink their borrowers while pages and source owners are alive.
    muse::delete_all(output_.systems);
    output_.systems.clear();
    muse::delete_all(output_.beams);
    output_.beams.clear();
    retire_shared_auxiliary_output();
    muse::delete_all(output_.pages);
    output_.pages.clear();
}

struct ScoreRenderer::ScratchLayouts {
    std::vector<std::unique_ptr<LayoutDataStore>> stores;
};

ScoreRenderer::ScoreRenderer() = default;
ScoreRenderer::~ScoreRenderer() = default;

LayoutDataStore &ScoreRenderer::layout_data_store(Score &score) {
    if (auto *master = score.master_score(); master && master->layout_store_boundary) return master->layout_store_boundary(score);
    if (auto *store = score.layout_data_store()) return *store;
    return create_layout_data_store(score);
}

LayoutDataStore &ScoreRenderer::create_layout_data_store(Score &score) {
    if (!scratch_layouts_) scratch_layouts_ = std::make_unique<ScratchLayouts>();
    auto store = std::make_unique<LayoutDataStore>(score);
    auto *result = store.get();
    scratch_layouts_->stores.push_back(std::move(store));
    score.bind_layout_data_store(result);
    return *result;
}

std::unique_ptr<LayoutDataStore> ScoreRenderer::take_layout_data_store(Score &score) {
    if (!scratch_layouts_) return nullptr;
    auto &stores = scratch_layouts_->stores;
    const auto it = std::ranges::find_if(stores, [&](const auto &store) { return store->score() == &score; });
    if (it == stores.end()) return nullptr;
    auto result = std::move(*it);
    stores.erase(it);
    return result;
}

void ScoreRenderer::release_layout_data_store(Score &score) {
    auto scratch = take_layout_data_store(score);
    if (auto *store = score.layout_data_store()) store->detach_score();
}
} // namespace mu::engraving::rendering::score
