/* SPDX-License-Identifier: GPL-3.0-only */
#include "mmrestchange.h"

#include <stdexcept>

#include "dom/masterscore.h"
#include "dom/measure.h"
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
} // namespace mu::engraving
