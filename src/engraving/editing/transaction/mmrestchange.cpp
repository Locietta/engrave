/* SPDX-License-Identifier: GPL-3.0-only */
#include "mmrestchange.h"

#include <stdexcept>
#include <tuple>
#include <utility>

#include "dom/factory.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/system.h"
#include "rendering/score/layoutdatastore.h"
#include "transaction.h"

namespace mu::engraving {
MeasureBase *mm_rest_link(const Measure &target, MMRestLink link) {
    switch (link) {
        case MMRestLink::ASSOCIATION: return target.mm_rest();
        case MMRestLink::NEXT: return target.next();
        case MMRestLink::PREVIOUS: return target.prev();
    }
    throw std::logic_error("invalid MMR link");
}

void validate_mm_rest_link(const Measure &target, MeasureBase *value, MMRestLink link) {
    if (link != MMRestLink::ASSOCIATION && !target.is_mm_rest())
        throw std::invalid_argument("display measure links require an MMR overlay");
    if (value && value->score() != target.score()) throw std::invalid_argument("MMR links cannot cross scores");
    if (link == MMRestLink::ASSOCIATION && value && !value->is_measure())
        throw std::invalid_argument("an MMR association requires a measure");
}

void apply_mm_rest_link(Measure &target, MeasureBase *value, MMRestLink link) {
    switch (link) {
        case MMRestLink::ASSOCIATION: target.set_mm_rest(value ? to_measure(value) : nullptr); break;
        case MMRestLink::NEXT: target.set_next(value); break;
        case MMRestLink::PREVIOUS: target.set_prev(value); break;
    }
}

void restore_mm_rest_link(const MMRestLinkInverse &inverse) {
    auto &target = *to_measure(&inverse.target.object());
    MeasureBase *before = nullptr;
    if (inverse.before.is_valid()) {
        auto *object = inverse.target.document->eid_register()->item_from_handle(inverse.before);
        if (!object) throw std::logic_error("an MMR link lifetime ended before rollback");
        before = to_measure_base(object);
    }
    apply_mm_rest_link(target, before, inverse.link);
}

void record_mm_rest_association(Measure &source, Measure *overlay) {
    source.score()->transaction_manager()->current_or_dummy_transaction().change_mm_rest_link(source, overlay, MMRestLink::ASSOCIATION);
}

void record_mm_rest_next(Measure &overlay, MeasureBase *next) {
    overlay.score()->transaction_manager()->current_or_dummy_transaction().change_mm_rest_link(overlay, next, MMRestLink::NEXT);
}

void record_mm_rest_previous(Measure &overlay, MeasureBase *previous) {
    overlay.score()->transaction_manager()->current_or_dummy_transaction().change_mm_rest_link(overlay, previous, MMRestLink::PREVIOUS);
}

void dispose_created_mm_rest(CreatedMMRest &reference, bool committed) {
    if (!reference.owned) return;
    auto &registry = *reference.source.document->eid_register();
    auto *item = reference.owned.get();
    if (registry.item_from_handle(reference.object) != item) {
        std::ignore = reference.owned.release(); // An owning score already destroyed this lifetime.
        return;
    }
    auto *source_object = registry.item_from_handle(reference.source.handle);
    auto *source = source_object ? to_measure(source_object) : nullptr;
    if (source && source->mm_rest() == item) {
        auto *score = source->score();
        // A detached excerpt still owns its actual measure list. Publication
        // liveness alone would incorrectly delete that score's attached overlay.
        if (committed && score && Score::is_alive(score) && registry.owns(*score, *source)) {
            std::ignore = reference.owned.release();
            return;
        }
        source->set_mm_rest(nullptr);
    }
    if (auto *score = item->score(); score && Score::is_alive(score)) {
        if (!score->master_score()->cmd_state().locked())
            if (auto *store = score->layout_data_store()) store->clear();
        for (auto *system : score->systems()) system->remove_measure(item);
        score->layout_state.dirty = true;
    }
    // Detached excerpt topology may outlive its score; retire through the
    // retained document before destruction can encounter a dead score pointer.
    registry.on_item_destroyed(item);
    reference.owned.reset();
}

MMRestCreation::MMRestCreation() = default;

MMRestCreation::MMRestCreation(CreatedMMRest reference) : local_(std::move(reference)) {
    auto &source = *to_measure(&local_.source.object());
    source.set_mm_rest(local_.owned.get());
}

MMRestCreation::MMRestCreation(JournalTarget source, SourceHandle object) : local_{source, object, {}} {}

MMRestCreation::~MMRestCreation() { cancel(); }

MMRestCreation::MMRestCreation(MMRestCreation &&other) noexcept { *this = std::move(other); }

MMRestCreation &MMRestCreation::operator=(MMRestCreation &&other) noexcept {
    if (this == &other) return *this;
    cancel();
    local_ = std::move(other.local_);
    completed_ = std::exchange(other.completed_, true);
    other.local_.source = {};
    other.local_.object = {};
    return *this;
}

Measure *MMRestCreation::get() const {
    if (!local_.source.document) return nullptr;
    auto *object = local_.source.document->eid_register()->item_from_handle(local_.object);
    if (!object) throw std::logic_error("a newly created MMR lifetime ended during layout");
    return to_measure(object);
}

void MMRestCreation::complete() {
    auto *object = get();
    if (!object) return;
    auto &source = *to_measure(&local_.source.object());
    if (source.mm_rest() != object || !object->is_mm_rest() || !object->first() || !object->last())
        throw std::logic_error("cannot publish an incomplete MMR overlay");
    completed_ = true;
    std::ignore = local_.owned.release(); // In a batch, ownership is held by its separate arena.
}

void MMRestCreation::cancel() noexcept {
    if (!local_.source.document) return;
    if (!completed_) {
        auto &registry = *local_.source.document->eid_register();
        auto *source = registry.item_from_handle(local_.source.handle);
        auto *object = registry.item_from_handle(local_.object);
        if (source && object && to_measure(source)->mm_rest() == object) to_measure(source)->set_mm_rest(nullptr);
        dispose_created_mm_rest(local_, false);
    }
    local_.source = {};
    local_.object = {};
}

MMRestCreation begin_mm_rest_creation(Measure &source, Fraction length) {
    if (source.mm_rest()) throw std::logic_error("MMR creation requires an absent association");
    std::unique_ptr<Measure> overlay(Factory::create_measure(source.score()));
    overlay->set_ticks(length);
    overlay->set_tick(source.tick());
    return source.score()->transaction_manager()->current_or_dummy_transaction().create_mm_rest(source, std::move(overlay));
}
} // namespace mu::engraving
