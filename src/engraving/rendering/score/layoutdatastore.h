/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "engraving/types/stavesharing.h"
#include "global/flatmap.h"
#include "global/fn.h"
#include "global/hashmap.h"
#include "global/unique_fn.h"

namespace mu::engraving {
struct Beam;
struct BeamBase;
struct EngravingItem;
struct ItemLayoutData;
struct Measure;
struct Page;
struct Score;
struct SharedPart;
struct Spanner;
struct SpannerSegment;
struct System;
struct Tuplet;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct HeaderFooterLayout;

struct LayoutDataStore {
    explicit LayoutDataStore(Score &score);
    ~LayoutDataStore();
    LayoutDataStore(const LayoutDataStore &) = delete;
    LayoutDataStore &operator=(const LayoutDataStore &) = delete;

    struct ScoreOutput {
        // LayoutState temporarily borrows a raw tail during range reuse.
        std::vector<Beam *> beams;
        std::vector<Page *> pages;
        std::vector<System *> systems;
        std::vector<Spanner *> shared_spanners;
        std::vector<const Spanner *> suppressed_shared_spanners;
        std::vector<Tuplet *> shared_tuplets;
        muse::FlatMap<const SharedPart *, RenderedStaveSharingStates> stave_sharing_states;
        std::vector<const SharedPart *> suspended_shared_parts;
    };

    Score *score() const { return score_; }

    const ScoreOutput &score_output() const { return output_; }

    ScoreOutput &score_output() { return output_; }

    void register_layout_copy(const EngravingItem &source, EngravingItem &display);
    const EngravingItem *layout_copy(const EngravingItem &source) const;
    void clear_layout_copies(const Measure &display);
    void retire_layout_copy(const EngravingItem &item);
    void discard_score_output();
    void retire_shared_auxiliary_output();
    std::vector<Spanner *> rendered_spanners(int first_tick, int last_tick) const;
    void retire_measure_borrowers(Measure &measure);
    Beam *create_beam();
    void retire_beam_borrowers(const BeamBase &beam);
    void retire_spanner_borrowers(const SpannerSegment &segment);
    void register_shape_borrower(EngravingItem &owner);
    void retire_shape_borrowers(const EngravingItem &item);
    void retire_item_borrowers(const EngravingItem &item);
    static void delete_layout_child(EngravingItem *item);
    void prune_spanner_segments();

    struct RetireEntry;

    struct RetireScope {
        RetireScope() = default;
        RetireScope(LayoutDataStore &store, std::vector<const EngravingItem *> retired);
        RetireScope(RetireScope &&other) noexcept;
        RetireScope &operator=(RetireScope &&other) noexcept;
        RetireScope(const RetireScope &) = delete;
        RetireScope &operator=(const RetireScope &) = delete;
        ~RetireScope();

    private:
        void reset();

        std::unique_ptr<RetireEntry> entry_;
    };

    [[nodiscard]] static std::vector<RetireScope> retire_batch_for_disposal(std::span<EngravingItem *const> roots);
    [[nodiscard]] RetireScope retire_for_disposal(const EngravingItem &item);

    size_t size() const;
    const ItemLayoutData *find(const EngravingItem &item) const;
    ItemLayoutData *mut_data(EngravingItem &item);
    void erase(EngravingItem &item);
    // Invalidate geometry before retiring payloads and their generated children.
    // Call only after active layout borrowers have unwound.
    void clear();
    void detach_score();
    // Explicit producer pass. The callback may mutate payloads, not membership.
    void for_each_output(muse::FnRef<void(EngravingItem &, ItemLayoutData &)> complete);

private:
    friend struct HeaderFooterLayout;

    // The item and its payload must never have been published to layout borrowers.
    static void delete_unborrowed_scratch(EngravingItem *item);

    void drain_records();
    bool is_retired_in_flight(const EngravingItem &item) const;
    bool has_borrower_containers() const;
    RetireScope retire_payload_children(const EngravingItem &item, const ItemLayoutData &data);
    RetireScope begin_retire_scope(std::vector<const EngravingItem *> targets);
    void prune_shape_borrowers(std::span<const EngravingItem *const> retired, const BeamBase *beam);

    struct Records;
    std::unique_ptr<Records> records_;
    size_t total_size_ = 0;
    Score *score_;
    ScoreOutput output_;
    muse::HashMap<const EngravingItem *, EngravingItem *> layout_copies_;
    muse::HashMap<const EngravingItem *, std::vector<const EngravingItem *>> layout_copy_sources_;
    muse::HashSet<EngravingItem *> shape_borrowers_;
    std::vector<RetireEntry *> in_flight_retired_;
    bool clearing_ = false;
    bool suppress_shape_prune_ = false;
};
} // namespace mu::engraving::rendering::score

namespace mu::engraving {
struct LayoutBoundaries {
    muse::UniqueFn<void(Score &)> layout;
    muse::UniqueFn<rendering::score::LayoutDataStore &(Score &)> store;
};
} // namespace mu::engraving
