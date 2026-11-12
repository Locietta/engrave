/* SPDX-License-Identifier: GPL-3.0-only */
#include "partchange.h"

#include <algorithm>
#include <stdexcept>

#include "dom/masterscore.h"
#include "dom/sharedpart.h"
#include "dom/stringtunings.h"
#include "transaction.h"

namespace mu::engraving {
Part &journal_part(const JournalTarget &target) {
    auto &object = target.object();
    if (!object.is_part() && !object.is_shared_part()) throw std::logic_error("journal target is not a part");
    return static_cast<Part &>(object);
}

void apply_part_membership(Part &part, size_t index, bool adding) {
    if (adding)
        part.score()->insert_part(&part, index);
    else
        part.score()->remove_part(&part);
}

void dispose_part_membership(const PartMembershipInverse &inverse, bool committed) {
    if (inverse.adding == committed) return;
    auto *registry = inverse.target.document->eid_register();
    auto *object = registry->item_from_handle(inverse.target.handle);
    if (!object) return;
    auto &part = journal_part(inverse.target);
    // An inactive excerpt still owns its reattached parts until it is disposed.
    if (muse::contains(part.score()->parts(), &part)) return;
    delete &part;
}

void apply_part_label(Part &part, Fraction tick, const String &value, bool long_name) {
    if (long_name)
        part.set_long_name(value, tick);
    else
        part.set_short_name(value, tick);
    part.score()->set_layout_all();
}

void exchange_part_instrument(Part &part, std::unique_ptr<Instrument> &held) {
    auto *before = part.instrument();
    part.set_instrument(held.get());
    held.release();
    held.reset(before);
}

void refresh_part_instrument(Part &part) {
    part.update_harmony_channels(false);
    auto *score = part.score();
    score->master_score()->rebuild_midi_mapping();
    score->set_instruments_changed(true);
    score->set_layout_all();
}

void restore_part_instrument(PartInstrumentInverse &inverse) {
    if (!inverse.applied) return;
    auto &part = journal_part(inverse.target);
    exchange_part_instrument(part, inverse.displaced);
    inverse.applied = false;
    refresh_part_instrument(part);
}

void restore_shared_origin(const SharedOriginInverse &inverse) {
    auto &shared = *to_shared_part(&inverse.target.object());
    auto *origin = to_part(&inverse.origin.object());
    const auto &members = shared.origin_parts();
    auto found = std::find(members.begin(), members.end(), origin);
    if (inverse.present) {
        if (found != members.end() && size_t(found - members.begin()) == inverse.position) return;
        if (found != members.end()) shared.remove_origin_part(origin);
        shared.add_origin_part(origin, inverse.position);
    } else if (found != members.end())
        shared.remove_origin_part(origin);
}

void Score::record_part_membership(Part *part, size_t index, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_part_membership(part, index, adding);
}

void Score::record_part_label(Fraction tick, Part *part, const String &value, bool long_name) {
    transaction_manager()->current_or_dummy_transaction().change_part_label(tick, part, value, long_name);
}

void Score::record_part_instrument(Part *part, std::unique_ptr<Instrument> value) {
    transaction_manager()->current_or_dummy_transaction().replace_part_instrument(part, std::move(value));
}

void Score::record_string_data(StringTunings *item, const StringData &value) {
    transaction_manager()->current_or_dummy_transaction().change_string_data(item, value);
}

void Score::record_excerpt(Excerpt *excerpt, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_excerpt_membership(excerpt, adding);
}
} // namespace mu::engraving
