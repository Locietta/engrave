/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include "dom/engravingitem.h"
#include "dom/sharedpart.h"

namespace mu::engraving {
struct BeamBase;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
// Item addresses are lifetime-qualified by explicit erase before destruction/address reuse.
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
        std::map<const SharedPart *, std::map<Fraction, SharedTrackMap>> shared_track_maps;
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
    const EngravingItem::LayoutData *find(const EngravingItem &item) const;
    EngravingItem::LayoutData *mut_data(EngravingItem &item);
    EngravingItem::LayoutData *adopt(EngravingItem &item, std::unique_ptr<EngravingItem::LayoutData> data);
    void erase(EngravingItem &item);
    // Invalidate geometry before retiring payloads and their generated children.
    // Call only after active layout borrowers have unwound.
    void clear();
    void detach_score();
    // Explicit producer pass. The callback may mutate payloads, not membership.
    void for_each_output(const std::function<void(EngravingItem &, EngravingItem::LayoutData &)> &complete);

private:
    bool is_retired_in_flight(const EngravingItem &item) const;
    bool has_borrower_containers() const;
    RetireScope begin_retire_scope(std::vector<const EngravingItem *> targets);
    void prune_shape_borrowers(std::span<const EngravingItem *const> retired, const BeamBase *beam);

    struct Records;
    std::unique_ptr<Records> records_;
    Score *score_;
    ScoreOutput output_;
    std::vector<std::pair<const EngravingItem *, EngravingItem *>> layout_copies_;
    std::vector<RetireEntry *> in_flight_retired_;
    bool clearing_ = false;
    bool suppress_shape_prune_ = false;
};
} // namespace mu::engraving::rendering::score
