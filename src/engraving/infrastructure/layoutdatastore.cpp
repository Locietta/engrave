/* SPDX-License-Identifier: GPL-3.0-only */
#include "rendering/score/layoutdatastore.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "dom/masterscore.h"
#include "dom/measurebase.h"
#include "dom/page.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/system.h"
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

void LayoutDataStore::discard_score_output() {
    if (score_) {
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
    // Systems unlink their borrowers while pages and source owners are alive.
    muse::delete_all(output_.systems);
    output_.systems.clear();
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
