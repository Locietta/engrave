/* SPDX-License-Identifier: GPL-3.0-only */
#include "rendering/score/layoutdatastore.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "dom/accidental.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/beam.h"
#include "dom/chord.h"
#include "dom/chordrest.h"
#include "dom/deadslapped.h"
#include "dom/factory.h"
#include "dom/guitarbend.h"
#include "dom/hammeronpulloff.h"
#include "dom/hook.h"
#include "dom/jianpudurationline.h"
#include "dom/jianpuoctavedot.h"
#include "dom/ledgerline.h"
#include "dom/lyrics.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurebase.h"
#include "dom/measurenumber.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/note.h"
#include "dom/notedot.h"
#include "dom/page.h"
#include "dom/parenthesis.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/sharedpart.h"
#include "dom/spacer.h"
#include "dom/spanner.h"
#include "dom/stafflines.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/system.h"
#include "dom/tabdurationsymbol.h"
#include "dom/tapping.h"
#include "dom/text.h"
#include "dom/textlinebase.h"
#include "dom/tie.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"
#include "engraving/dom/dummyparent.h"
#include "global/hashmap.h"
#include "global/slab_pool.h"
#include "rendering/score/layoutdatadispatch.h"
#include "rendering/score/scorerenderer.h"

namespace mu::engraving::rendering::score {
namespace {
struct BoolGuard {
    bool &flag;
    bool previous;

    BoolGuard(bool &target, bool next) : flag(target), previous(target) { flag = next; }

    ~BoolGuard() { flag = previous; }
};

bool contains_retired(std::span<const EngravingItem *const> targets, const EngravingItem *item) {
    if (!item || targets.empty()) return false;
    if (targets.size() <= 16) return std::find(targets.begin(), targets.end(), item) != targets.end();
    return std::binary_search(targets.begin(), targets.end(), item, std::less<>{});
}

void add_layout_payload_children(const EngravingItem &owner, const EngravingItem::LayoutData &data,
                                 muse::FnRef<void(const EngravingItem *)> visit) {
    if (owner.is_chord_rest()) {
        const auto *cr_data = static_cast<const ChordRest::LayoutData *>(&data);
        visit(cr_data->tab_dur);
        for (const auto *line : cr_data->jianpu_duration_lines) visit(line);
    }
    switch (owner.type()) {
        case ElementType::NOTE: {
            const auto *note_data = static_cast<const Note::LayoutData *>(&data);
            for (const auto *dot : note_data->dots) visit(dot);
            for (const auto *dot : note_data->jianpu_octave_dots) visit(dot);
            break;
        }
        case ElementType::REST:
        case ElementType::MMREST:
        case ElementType::MEASURE_REPEAT: {
            const auto *rest_data = static_cast<const Rest::LayoutData *>(&data);
            for (const auto *dot : rest_data->dots) visit(dot);
            break;
        }
        case ElementType::CHORD: {
            const auto *chord_data = static_cast<const Chord::LayoutData *>(&data);
            visit(chord_data->stem);
            visit(chord_data->hook);
            visit(chord_data->stem_slash);
            for (const auto &group : chord_data->parentheses) {
                visit(group.left);
                visit(group.right);
            }
            for (const auto *line : chord_data->ledger_lines) visit(line);
            break;
        }
        case ElementType::TUPLET: {
            const auto *tuplet_data = static_cast<const Tuplet::LayoutData *>(&data);
            visit(tuplet_data->number);
            break;
        }
        case ElementType::GUITAR_BEND: {
            const auto *bend_data = static_cast<const GuitarBend::LayoutData *>(&data);
            visit(bend_data->hold_line);
            for (const auto *seg : bend_data->segments) visit(seg);
            break;
        }
        case ElementType::TAPPING: {
            const auto *tapping_data = static_cast<const Tapping::LayoutData *>(&data);
            visit(tapping_data->half_slur_above);
            visit(tapping_data->half_slur_below);
            break;
        }
        case ElementType::SEGMENT: {
            const auto *seg_data = static_cast<const Segment::LayoutData *>(&data);
            for (const auto *copy : seg_data->display_copies) visit(copy);
            for (const auto *ann : seg_data->display_annotations) visit(ann);
            for (const auto *gen : seg_data->generated) visit(gen);
            break;
        }
        case ElementType::MEASURE: {
            const auto *meas_data = static_cast<const Measure::LayoutData *>(&data);
            visit(meas_data->mm_rest);
            for (const auto *seg : meas_data->segments) visit(seg);
            break;
        }
        default:
            if (owner.is_spanner() || owner.type() == ElementType::TEXTLINE_BASE) {
                const auto *span_data = static_cast<const Spanner::LayoutData *>(&data);
                for (const auto *seg : span_data->segments) visit(seg);
            }
            break;
    }
}

void collect_owned_subtree(const EngravingItem &item, std::vector<const EngravingItem *> &out, muse::HashSet<const EngravingItem *> &seen) {
    if (!seen.insert(&item).second) return;
    out.push_back(&item);
    const auto visit = [&](const EngravingItem *child) {
        if (child) collect_owned_subtree(*child, out, seen);
    };
    for (const auto *child : item.children()) {
        if (child && child->is_engraving_item()) visit(to_engraving_item(child));
    }
    visit(item.left_paren());
    visit(item.right_paren());
    if (item.has_layout_data()) add_layout_payload_children(item, *item.ldata(), visit);

    if (item.is_chord_rest()) {
        const auto *cr = to_chord_rest(&item);
        for (const auto *lyric : cr->lyrics()) visit(lyric);
        for (const auto *el : cr->el()) visit(el);
    }
    if (item.is_text_line_base_segment()) {
        const auto *seg = to_text_line_base_segment(&item);
        visit(seg->text());
        visit(seg->end_text());
    }
    if (item.is_measure_base()) {
        const auto *mb = to_measure_base(&item);
        for (const auto *el : mb->el()) visit(el);
    }

    switch (item.type()) {
        case ElementType::NOTE: {
            const auto *note = to_note(&item);
            visit(note->accidental());
            visit(note->tie_for());
            for (const auto *el : note->el()) visit(el);
            break;
        }
        case ElementType::CHORD: {
            const auto *chord = to_chord(&item);
            for (const auto *note : chord->notes()) visit(note);
            for (const auto *grace : chord->grace_notes()) visit(grace);
            for (const auto *art : chord->articulations()) visit(art);
            visit(chord->arpeggio());
            break;
        }
        case ElementType::REST:
        case ElementType::MMREST:
        case ElementType::MEASURE_REPEAT: visit(to_rest(&item)->dead_slapped()); break;
        case ElementType::TAPPING: visit(to_tapping(&item)->text()); break;
        case ElementType::GUITAR_BEND_SEGMENT: visit(to_guitar_bend_segment(&item)->bend_text()); break;
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
            for (const auto *txt : to_hammer_on_pull_off_segment(&item)->hopo_text()) visit(txt);
            break;
        case ElementType::SEGMENT: {
            const auto *seg = to_segment(&item);
            for (const auto *el : seg->elist()) visit(el);
            for (const auto *ann : seg->annotations()) visit(ann);
            break;
        }
        case ElementType::MEASURE: {
            const auto *m = to_measure(&item);
            for (const auto *row : m->mstaves()) {
                if (!row) continue;
                visit(row->measure_number());
                visit(row->mm_range_text());
                visit(row->lines());
                visit(row->vspacer_up());
                visit(row->vspacer_down());
            }
            for (const auto *s = m->first(); s; s = s->next()) visit(s);
            break;
        }
        default: break;
    }
}
} // namespace

struct LayoutDataStore::RetireEntry {
    LayoutDataStore *store = nullptr;
    std::vector<const EngravingItem *> retired;
};

namespace {
template <typename Binding>
using LayoutPool = muse::SlabPool<typename Binding::Data, EngravingItem, Binding::chunk_size>;

template <typename Dispatch>
struct LayoutDataPools;

template <typename... Bindings>
struct LayoutDataPools<LayoutDataDispatch<Bindings...>> : LayoutPool<Bindings>... {
    template <typename Data>
    auto &pool() {
        constexpr auto chunk_size = (std::size_t{0} + ... + (std::is_same_v<Data, typename Bindings::Data> ? Bindings::chunk_size : 0));
        return static_cast<muse::SlabPool<Data, EngravingItem, chunk_size> &>(*this);
    }

    void for_each_live(muse::FnRef<void(EngravingItem &, ItemLayoutData &)> visit) { (LayoutPool<Bindings>::for_each_live(visit), ...); }

    void for_each_live_data(muse::FnRef<void(ItemLayoutData &)> visit) { (LayoutPool<Bindings>::for_each_live_data(visit), ...); }

    template <typename Fn>
    void drain_owners(Fn &&visit) {
        (LayoutPool<Bindings>::drain_owners(visit), ...);
    }

    void release_all_chunks() noexcept { (LayoutPool<Bindings>::release_all_chunks(), ...); }

    template <typename Fn>
    void for_each_beam_data(Fn &&visit) {
        for_each_derived_data<BeamBase::LayoutData>(visit);
    }

    template <typename Fn>
    void for_each_chord_rest_data(Fn &&visit) {
        for_each_derived_data<ChordRest::LayoutData>(visit);
    }

    template <typename Fn>
    void for_each_spanner_data(Fn &&visit) {
        (
            [&] {
                if constexpr (std::is_base_of_v<Spanner::LayoutData, typename Bindings::Data>) {
                    LayoutPool<Bindings>::for_each_live(visit);
                }
            }(),
            ...);
    }

private:
    template <typename Base, typename Fn>
    void for_each_derived_data(Fn &visit) {
        (
            [&] {
                if constexpr (std::is_base_of_v<Base, typename Bindings::Data>) {
                    LayoutPool<Bindings>::for_each_live_data(visit);
                }
            }(),
            ...);
    }
};

template <typename Pool, typename Data>
struct SlotDestroyGuard {
    Pool &pool;
    std::uint32_t slot;
    Data *data;

    ~SlotDestroyGuard() noexcept { pool.finish_destroy(slot, data); }
};
} // namespace

struct LayoutDataStore::Records : LayoutDataPools<LayoutDataTypes> {};

LayoutDataStore::RetireScope::RetireScope(LayoutDataStore &store, std::vector<const EngravingItem *> retired) {
    if (retired.empty()) return;
    entry_ = std::make_unique<RetireEntry>(RetireEntry{&store, std::move(retired)});
    store.in_flight_retired_.push_back(entry_.get());
}

LayoutDataStore::RetireScope::RetireScope(RetireScope &&) noexcept = default;

LayoutDataStore::RetireScope &LayoutDataStore::RetireScope::operator=(RetireScope &&other) noexcept {
    if (this != &other) {
        reset();
        entry_ = std::move(other.entry_);
    }
    return *this;
}

LayoutDataStore::RetireScope::~RetireScope() { reset(); }

void LayoutDataStore::RetireScope::reset() {
    if (!entry_) return;
    if (entry_->store) std::erase(entry_->store->in_flight_retired_, entry_.get());
    entry_.reset();
}

LayoutDataStore::LayoutDataStore(Score &score) : records_(std::make_unique<Records>()), score_(&score) {}

LayoutDataStore::~LayoutDataStore() {
    for (auto *entry : in_flight_retired_) entry->store = nullptr;
    in_flight_retired_.clear();
    detach_score();
}

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
    muse::HashMap<ElementType, size_t> ordinals;
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

size_t LayoutDataStore::size() const { return total_size_; }

const ItemLayoutData *LayoutDataStore::find(const EngravingItem &item) const {
    return item.layout_store_ == this ? item.layout_data_ : nullptr;
}

ItemLayoutData *LayoutDataStore::mut_data(EngravingItem &item) {
    if (item.layout_data_) {
        if (item.layout_store_ != this) throw std::logic_error("layout payload belongs to a different store");
        return item.layout_data_;
    }
    return LayoutDataTypes::dispatch(item.layout_type_, [&]<typename Data>() -> ItemLayoutData * {
        const auto [slot, data] = records_->pool<Data>().emplace(&item);
        data->item_ = &item;
        item.layout_store_ = this;
        item.layout_data_ = data;
        item.layout_slot_ = slot;
        ++total_size_;
        return data;
    });
}

void LayoutDataStore::erase(EngravingItem &item) {
    if (item.layout_store_ != this) return;
    if (item.is_measure_base()) {
        const auto *data = static_cast<const MeasureBase::LayoutData *>(item.layout_data_);
        // The base destructor has already unlinked placement before erasing here.
        if (auto *system = data->system) system->remove_measure(to_measure_base(&item));
    }
    if (auto *shared = item.shared_item()) EngravingItem::disconnect_shared_item(shared, &item);
    EngravingItem::disconnect_all_origin_items(&item);

    LayoutDataTypes::dispatch(item.layout_type_, [&]<typename Data>() {
        auto &pool = records_->pool<Data>();
        const auto slot = item.layout_slot_;
        auto *data = pool.detach(slot);
        --total_size_;
        item.layout_data_ = nullptr;
        item.layout_store_ = nullptr;
        item.layout_slot_ = EngravingItem::invalid_layout_slot;

        // Keep the retirement scope active while the payload destroys owned children.
        RetireScope scope;
        const SlotDestroyGuard guard{pool, slot, data};
        data->disconnect_snapped_items();
        scope = retire_payload_children(item, *data);
    });
}

LayoutDataStore::RetireScope LayoutDataStore::retire_payload_children(const EngravingItem &item, const ItemLayoutData &data) {
    if (clearing_ || suppress_shape_prune_ || !has_borrower_containers()) return {};
    std::vector<const EngravingItem *> targets;
    muse::HashSet<const EngravingItem *> seen;
    if (!is_retired_in_flight(item)) {
        targets.push_back(&item);
        seen.insert(&item);
    }
    add_layout_payload_children(item, data, [&](const EngravingItem *child) {
        if (child && !is_retired_in_flight(*child)) collect_owned_subtree(*child, targets, seen);
    });
    std::erase_if(targets, [&](const EngravingItem *candidate) { return is_retired_in_flight(*candidate); });
    return begin_retire_scope(std::move(targets));
}

void LayoutDataStore::for_each_output(muse::FnRef<void(EngravingItem &, ItemLayoutData &)> complete) { records_->for_each_live(complete); }

void LayoutDataStore::drain_records() {
    BoolGuard guard(clearing_, true);
    // Erasing a payload can delete other owners, including owners in the same pool.
    while (total_size_ > 0) {
        records_->drain_owners([&](EngravingItem &owner) { erase(owner); });
    }
    records_->release_all_chunks();
}

void LayoutDataStore::detach_score() {
    discard_score_output();
    drain_records();
    if (score_ && score_->layout_data_store() == this) score_->bind_layout_data_store(nullptr);
    score_ = nullptr;
}

void LayoutDataStore::clear() {
    if (score_) {
        if (score_->master_score()->cmd_state().locked()) throw std::logic_error("cannot discard output during layout");
        score_->invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    }
    discard_score_output();
    drain_records();
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

bool LayoutDataStore::is_retired_in_flight(const EngravingItem &item) const {
    const auto *target = &item;
    for (const auto *entry : in_flight_retired_) {
        if (contains_retired(entry->retired, target)) return true;
    }
    return false;
}

bool LayoutDataStore::has_borrower_containers() const { return total_size_ != 0 || !output_.pages.empty() || !output_.systems.empty(); }

LayoutDataStore::RetireScope LayoutDataStore::begin_retire_scope(std::vector<const EngravingItem *> targets) {
    if (clearing_ || suppress_shape_prune_ || targets.empty()) return {};
    std::sort(targets.begin(), targets.end(), std::less<>{});
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    if (has_borrower_containers()) prune_shape_borrowers(targets, nullptr);
    return RetireScope(*this, std::move(targets));
}

LayoutDataStore::RetireScope LayoutDataStore::retire_for_disposal(const EngravingItem &item) {
    if (clearing_ || suppress_shape_prune_ || is_retired_in_flight(item)) return {};
    std::vector<const EngravingItem *> targets;
    muse::HashSet<const EngravingItem *> seen;
    collect_owned_subtree(item, targets, seen);
    std::erase_if(targets, [&](const EngravingItem *candidate) { return is_retired_in_flight(*candidate); });
    return begin_retire_scope(std::move(targets));
}

std::vector<LayoutDataStore::RetireScope> LayoutDataStore::retire_batch_for_disposal(std::span<EngravingItem *const> roots) {
    std::vector<const EngravingItem *> collected;
    muse::HashSet<const EngravingItem *> seen;
    for (const auto *root : roots) {
        if (root) collect_owned_subtree(*root, collected, seen);
    }
    muse::HashMap<LayoutDataStore *, std::vector<const EngravingItem *>> by_store;
    for (const auto *item : collected) {
        LayoutDataStore *score_store = (item->score() && Score::is_alive(item->score())) ? item->score()->layout_data_store() : nullptr;
        LayoutDataStore *bound_store = item->layout_store_;
        if (score_store && !score_store->is_retired_in_flight(*item)) by_store[score_store].push_back(item);
        if (bound_store && bound_store != score_store && !bound_store->is_retired_in_flight(*item)) by_store[bound_store].push_back(item);
    }
    std::vector<RetireScope> scopes;
    scopes.reserve(by_store.size());
    for (auto &[store, targets] : by_store) scopes.push_back(store->begin_retire_scope(std::move(targets)));
    return scopes;
}

void LayoutDataStore::prune_shape_borrowers(std::span<const EngravingItem *const> retired, const BeamBase *beam) {
    if (!retired.empty()) {
        for (auto *page : output_.pages) page->invalidate_bsp_tree_if_referenced(retired);
        const auto borrows_retired = [&](const ShapeElement &element) { return contains_retired(retired, element.item()); };
        records_->for_each_live_data([&](ItemLayoutData &data) {
            if (data.is_set_shape()) data.mut_shape().remove_if(borrows_retired);
            if (data.mask_.has_value()) data.mask_.mut_value().remove_if(borrows_retired);
        });
        const auto prune_skyline = [&](System &system) {
            for (auto *staff : system.staves()) {
                std::erase_if(staff->skyline().north().elements(), borrows_retired);
                std::erase_if(staff->skyline().south().elements(), borrows_retired);
            }
        };
        // Reusable tail systems live outside ScoreOutput during a range pass.
        records_->pool<System::LayoutData>().for_each_live(
            [&](EngravingItem &owner, System::LayoutData &) { prune_skyline(*to_system(&owner)); });
        for (auto *system : output_.systems) {
            if (!system->has_layout_data()) prune_skyline(*system);
        }
        records_->pool<Segment::LayoutData>().for_each_live([&](EngravingItem &owner, Segment::LayoutData &) {
            auto *segment = to_segment(&owner);
            const auto size = std::as_const(*segment).shapes().size();
            for (size_t i = 0; i < size; ++i) segment->staff_shape(i).remove_if(borrows_retired);
        });
        records_->pool<Chord::LayoutData>().for_each_live([&](EngravingItem &owner, Chord::LayoutData &) {
            to_chord(&owner)->remove_spanner_borrowers([&](const Spanner *span) { return contains_retired(retired, span); });
        });
    }
    if (beam) {
        records_->for_each_beam_data([&](BeamBase::LayoutData &data) {
            if (data.beam == beam) data.beam = nullptr;
            if (data.trem == beam) data.trem = nullptr;
        });
        records_->for_each_chord_rest_data([&](ChordRest::LayoutData &data) {
            if (data.rendered_beam == beam) data.rendered_beam = nullptr;
            if (data.beamlet && data.beamlet->parent_element == beam) data.beamlet = nullptr;
        });
    }
}

void LayoutDataStore::retire_beam_borrowers(const BeamBase &beam) {
    if (clearing_ || suppress_shape_prune_ || !has_borrower_containers()) return;
    std::vector<const EngravingItem *> targets;
    if (!is_retired_in_flight(beam)) {
        muse::HashSet<const EngravingItem *> seen;
        collect_owned_subtree(beam, targets, seen);
        std::erase_if(targets, [&](const EngravingItem *candidate) { return is_retired_in_flight(*candidate); });
        std::sort(targets.begin(), targets.end(), std::less<>{});
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    }
    prune_shape_borrowers(targets, &beam);
}

void LayoutDataStore::delete_layout_child(EngravingItem *item) {
    if (!item) return;
    std::vector<RetireScope> scopes;
    LayoutDataStore *score_store = (item->score() && Score::is_alive(item->score())) ? item->score()->layout_data_store() : nullptr;
    if (score_store) scopes.push_back(score_store->retire_for_disposal(*item));
    if (item->layout_store_ && item->layout_store_ != score_store) scopes.push_back(item->layout_store_->retire_for_disposal(*item));
    delete item;
}

void LayoutDataStore::delete_unborrowed_scratch(EngravingItem *item) {
    if (!item) return;
    assert(!item->layout_data_ || (!item->layout_data_->item_snapped_before() && !item->layout_data_->item_snapped_after()));
    assert(!item->shared_item());
    assert(item->origin_items().empty());
    assert(item->children().empty());

    auto *score_store = item->score() && Score::is_alive(item->score()) ? item->score()->layout_data_store() : nullptr;
    auto *bound_store = item->layout_store_;
    if (bound_store == score_store) bound_store = nullptr;
    bool unused_score = false;
    bool unused_bound = false;
    BoolGuard score_guard(score_store ? score_store->suppress_shape_prune_ : unused_score, true);
    BoolGuard bound_guard(bound_store ? bound_store->suppress_shape_prune_ : unused_bound, true);
    delete item;
}

void LayoutDataStore::retire_spanner_borrowers(const SpannerSegment &segment) { retire_shape_borrowers(segment); }

void LayoutDataStore::retire_shape_borrowers(const EngravingItem &retired) {
    if (clearing_ || suppress_shape_prune_ || !has_borrower_containers() || is_retired_in_flight(retired)) return;
    std::vector<const EngravingItem *> targets;
    muse::HashSet<const EngravingItem *> seen;
    collect_owned_subtree(retired, targets, seen);
    std::erase_if(targets, [&](const EngravingItem *candidate) { return is_retired_in_flight(*candidate); });
    if (targets.empty()) return;
    std::sort(targets.begin(), targets.end(), std::less<>{});
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    prune_shape_borrowers(targets, nullptr);
}

void LayoutDataStore::retire_item_borrowers(const EngravingItem &item) {
    if (clearing_ || suppress_shape_prune_ || is_retired_in_flight(item) || !has_borrower_containers()) return;
    const EngravingItem *target = &item;
    prune_shape_borrowers({&target, 1}, nullptr);
}

void LayoutDataStore::prune_spanner_segments() {
    std::vector<Spanner *> spanners;
    records_->for_each_spanner_data([&](EngravingItem &owner, Spanner::LayoutData &) {
        if (owner.is_spanner()) spanners.push_back(to_spanner(&owner));
    });
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
    records_->pool<Segment::LayoutData>().for_each_live([&](EngravingItem &owner, Segment::LayoutData &) {
        auto *segment = to_segment(&owner);
        if (segment->measure()->system() == system) segment->clear_shapes();
    });
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
        const muse::HashSet<Spanner *> retired(output_.shared_spanners.begin(), output_.shared_spanners.end());
        records_->pool<Segment::LayoutData>().for_each_live(
            [](EngravingItem &owner, Segment::LayoutData &) { to_segment(&owner)->clear_shapes(); });
        // A range pass can retain display chords after their shared spanners are retired.
        if (!retired.empty()) {
            records_->pool<Chord::LayoutData>().for_each_live([&](EngravingItem &owner, Chord::LayoutData &) {
                to_chord(&owner)->remove_spanner_borrowers([&](Spanner *span) { return retired.contains(span); });
            });
        }
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
    records_->for_each_live_data([](ItemLayoutData &data) {
        data.clear_shape();
        data.mask_.reset();
    });
    if (score_) {
        clear_output_anchors(*score_);
        // These legacy L5 caches can retain pointers to output-owned ledger and
        // Harmony parenthesis shells. Retire their views before deleting shells.
        for (auto *page : output_.pages) page->invalidate_bsp_tree();
        for (auto *system : output_.systems) {
            for (auto *staff : system->staves()) staff->skyline().clear();
        }
        // Staff shapes live on the segment even when its layout payload is absent.
        const auto clear_segments = [&](auto &&self, EngravingObject &owner) -> void {
            if (owner.is_segment()) {
                auto *segment = to_segment(&owner);
                if (!find(*segment)) segment->clear_shapes();
            }
            for (auto *child : owner.children()) {
                if (child->score() == score_) self(self, *child);
            }
        };
        clear_segments(clear_segments, *score_);
    }
    records_->pool<Segment::LayoutData>().for_each_live(
        [](EngravingItem &owner, Segment::LayoutData &) { to_segment(&owner)->clear_shapes(); });
    records_->for_each_beam_data([](auto &data) {
        if constexpr (std::is_same_v<std::remove_reference_t<decltype(data)>, Beam::LayoutData>) data.members.reset();
        data.beam = nullptr;
        data.trem = nullptr;
    });
    records_->for_each_chord_rest_data([](ChordRest::LayoutData &data) {
        data.beamlet = nullptr;
        data.rendered_beam.reset();
    });
    layout_copies_.clear();
    output_.stave_sharing_states.clear();
    output_.suspended_shared_parts.clear();
    output_.suppressed_shared_spanners.clear();
    {
        BoolGuard guard(suppress_shape_prune_, true);
        // Systems unlink their borrowers while pages and source owners are alive.
        muse::delete_all(output_.systems);
        output_.systems.clear();
        muse::delete_all(output_.beams);
        output_.beams.clear();
        retire_shared_auxiliary_output();
        muse::delete_all(output_.pages);
        output_.pages.clear();
    }
}

struct ScoreRenderer::ScratchLayouts {
    std::vector<std::unique_ptr<LayoutDataStore>> stores;
};

ScoreRenderer::ScoreRenderer() = default;
ScoreRenderer::~ScoreRenderer() = default;

LayoutDataStore &ScoreRenderer::layout_data_store(Score &score) {
    if (auto *master = score.master_score(); master && master->layout_boundaries && master->layout_boundaries->store)
        return master->layout_boundaries->store(score);
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
