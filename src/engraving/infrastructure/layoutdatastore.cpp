/* SPDX-License-Identifier: GPL-3.0-only */
#include "rendering/score/layoutdatastore.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "dom/masterscore.h"
#include "dom/score.h"
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
    while (!records_->items.empty()) erase(*records_->items.begin()->second.item);
    if (score_ && score_->layout_data_store() == this) score_->bind_layout_data_store(nullptr);
    score_ = nullptr;
}

void LayoutDataStore::transfer(EngravingItem &item, LayoutDataStore &destination) {
    if (&destination == this) return;
    const auto it = records_->items.find(&item);
    if (it == records_->items.end()) throw std::logic_error("layout payload binding is missing");
    // All destination allocation precedes changing the source owner. Heap
    // payloads and the item's typed borrowed pointer retain their addresses.
    auto [target, inserted] = destination.records_->items.emplace(&item, Records::Record{&item, nullptr});
    if (!inserted) throw std::logic_error("destination already has this layout identity");
    target->second.data = std::move(it->second.data);
    records_->items.erase(it);
    item.layout_store_ = &destination;
}

struct ScoreRenderer::ScratchLayouts {
    std::vector<std::unique_ptr<LayoutDataStore>> stores;
};

ScoreRenderer::ScoreRenderer() = default;
ScoreRenderer::~ScoreRenderer() = default;

LayoutDataStore &ScoreRenderer::layout_data_store(Score &score) {
    if (auto *master = score.master_score(); master && master->layout_store_boundary) return master->layout_store_boundary(score);
    if (auto *store = score.layout_data_store()) return *store;
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
