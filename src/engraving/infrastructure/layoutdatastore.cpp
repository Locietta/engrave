/* SPDX-License-Identifier: GPL-3.0-only */
#include "rendering/score/layoutdatastore.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <ankerl/unordered_dense.h>

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
#include "rendering/score/layoutdatafactory.h"
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
                                 const std::function<void(const EngravingItem *)> &visit) {
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

void collect_owned_subtree(const EngravingItem &item, std::vector<const EngravingItem *> &out,
                           ankerl::unordered_dense::set<const EngravingItem *> &seen) {
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

struct LayoutDataStore::Records {
    struct Record {
        EngravingItem *item;
        std::unique_ptr<EngravingItem::LayoutData> data;
    };

    ankerl::unordered_dense::map<const EngravingItem *, Record> items;
};

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
    RetireScope scope;
    auto data = std::move(it->second.data);
    records_->items.erase(it);
    data->disconnect_snapped_items();
    item.layout_data_ = nullptr;
    item.layout_store_ = nullptr;
    if (!clearing_ && !suppress_shape_prune_ && has_borrower_containers()) {
        std::vector<const EngravingItem *> targets;
        ankerl::unordered_dense::set<const EngravingItem *> seen;
        if (!is_retired_in_flight(item)) {
            targets.push_back(&item);
            seen.insert(&item);
        }
        add_layout_payload_children(item, *data, [&](const EngravingItem *child) {
            if (child && !is_retired_in_flight(*child)) collect_owned_subtree(*child, targets, seen);
        });
        std::erase_if(targets, [&](const EngravingItem *candidate) { return is_retired_in_flight(*candidate); });
        scope = begin_retire_scope(std::move(targets));
    }
    // Destroy data while RetireScope guards nested teardown against redundant rescans.
    data.reset();
}

void LayoutDataStore::for_each_output(const std::function<void(EngravingItem &, EngravingItem::LayoutData &)> &complete) {
    for (auto &record : records_->items.values()) complete(*record.second.item, *record.second.data);
}

void LayoutDataStore::detach_score() {
    discard_score_output();
    BoolGuard guard(clearing_, true);
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
    BoolGuard guard(clearing_, true);
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

bool LayoutDataStore::is_retired_in_flight(const EngravingItem &item) const {
    const auto *target = &item;
    for (const auto *entry : in_flight_retired_) {
        if (contains_retired(entry->retired, target)) return true;
    }
    return false;
}

bool LayoutDataStore::has_borrower_containers() const {
    return !records_->items.empty() || !output_.pages.empty() || !output_.systems.empty();
}

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
    ankerl::unordered_dense::set<const EngravingItem *> seen;
    collect_owned_subtree(item, targets, seen);
    std::erase_if(targets, [&](const EngravingItem *candidate) { return is_retired_in_flight(*candidate); });
    return begin_retire_scope(std::move(targets));
}

std::vector<LayoutDataStore::RetireScope> LayoutDataStore::retire_batch_for_disposal(std::span<EngravingItem *const> roots) {
    std::vector<const EngravingItem *> collected;
    ankerl::unordered_dense::set<const EngravingItem *> seen;
    for (const auto *root : roots) {
        if (root) collect_owned_subtree(*root, collected, seen);
    }
    ankerl::unordered_dense::map<LayoutDataStore *, std::vector<const EngravingItem *>> by_store;
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
    if (retired.empty() && !beam) return;
    if (!retired.empty()) {
        for (auto *page : output_.pages) page->invalidate_bsp_tree_if_referenced(retired);
    }
    const auto borrows_retired = [&](const ShapeElement &element) { return contains_retired(retired, element.item()); };
    // Reusable tail systems live outside ScoreOutput during a range pass.
    // Records include them and the completed systems whose collisions must stay.
    for (auto &record : records_->items.values()) {
        auto *item = record.second.item;
        auto &data = *record.second.data;
        if (!retired.empty()) {
            if (data.is_set_shape()) data.mut_shape().remove_if(borrows_retired);
            if (data.mask_.has_value()) data.mask_.mut_value().remove_if(borrows_retired);
            if (item->is_system()) {
                for (auto *staff : to_system(item)->staves()) {
                    std::erase_if(staff->skyline().north().elements(), borrows_retired);
                    std::erase_if(staff->skyline().south().elements(), borrows_retired);
                }
            } else if (item->is_segment()) {
                auto *owner = to_segment(item);
                const auto size = static_cast<const Segment *>(owner)->shapes().size();
                for (size_t i = 0; i < size; ++i) owner->staff_shape(i).remove_if(borrows_retired);
            }
        }
        if (beam) {
            if (item->is_beam() || item->type() == ElementType::TREMOLO_TWOCHORD) {
                auto *beam_data = static_cast<BeamBase::LayoutData *>(&data);
                if (beam_data->beam == beam) beam_data->beam = nullptr;
                if (beam_data->trem == beam) beam_data->trem = nullptr;
            }
            if (item->is_chord_rest()) {
                auto *cr = to_chord_rest(item);
                auto *ld = static_cast<ChordRest::LayoutData *>(&data);
                if (ld->rendered_beam == beam) ld->rendered_beam = nullptr;
                if (ld->beamlet && ld->beamlet->parent_element == beam) cr->set_beamlet(nullptr);
            }
        }
    }
    if (!retired.empty()) {
        for (auto *system : output_.systems) {
            if (system->has_layout_data()) continue;
            for (auto *staff : system->staves()) {
                std::erase_if(staff->skyline().north().elements(), borrows_retired);
                std::erase_if(staff->skyline().south().elements(), borrows_retired);
            }
        }
    }
}

void LayoutDataStore::retire_beam_borrowers(const BeamBase &beam) {
    if (clearing_ || suppress_shape_prune_ || !has_borrower_containers()) return;
    std::vector<const EngravingItem *> targets;
    if (!is_retired_in_flight(beam)) {
        ankerl::unordered_dense::set<const EngravingItem *> seen;
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

void LayoutDataStore::retire_spanner_borrowers(const SpannerSegment &segment) { retire_shape_borrowers(segment); }

void LayoutDataStore::retire_shape_borrowers(const EngravingItem &retired) {
    if (clearing_ || suppress_shape_prune_ || !has_borrower_containers() || is_retired_in_flight(retired)) return;
    std::vector<const EngravingItem *> targets;
    ankerl::unordered_dense::set<const EngravingItem *> seen;
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
        if (record.second.item->is_beam() || record.second.item->type() == ElementType::TREMOLO_TWOCHORD) {
            auto *beam_data = static_cast<BeamBase::LayoutData *>(record.second.data.get());
            beam_data->beam = nullptr;
            beam_data->trem = nullptr;
        }
        if (record.second.item->is_chord_rest()) {
            auto *data = static_cast<ChordRest::LayoutData *>(record.second.data.get());
            data->beamlet = nullptr;
            data->rendered_beam.reset();
        }
    }
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
