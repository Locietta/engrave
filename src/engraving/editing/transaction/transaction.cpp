#include "transaction.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "dom/beam.h"
#include "dom/bracketitem.h"
#include "dom/chord.h"
#include "dom/chordline.h"
#include "dom/excerpt.h"
#include "dom/factory.h"
#include "dom/fret.h"
#include "dom/harmony.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/sharedpart.h"
#include "dom/staff.h"
#include "dom/stringtunings.h"
#include "dom/textbase.h"
#include "dom/textline.h"
#include "dom/tremolotwochord.h"
#include "editing/addremoveelement.h"
#include "editing/editmeasures.h"
#include "editing/editstaffbrackets.h"
#include "editing/inserttime.h"
#include "rendering/score/layoutdatastore.h"

namespace mu::engraving {

std::vector<EngravingObject *> compound_objects(EngravingObject *object) {
    std::vector<EngravingObject *> objects;

    if (object->is_chord()) {
        const Chord *chord = to_chord(object);
        for (const Note *note : chord->notes()) {
            for (Note *compound_note : note->compound_notes()) {
                objects.push_back(compound_note);
            }
        }
    } else if (object->is_note()) {
        const Note *note = to_note(object);
        for (Note *compound_note : note->compound_notes()) {
            objects.push_back(compound_note);
        }
    } else if (object->is_fret_diagram()) {
        const FretDiagram *fret = to_fret_diagram(object);
        if (Harmony *harmony = fret->harmony()) {
            objects.push_back(harmony);
        }
    }

    objects.push_back(object);

    return objects;
}

void apply_property(EngravingObject &object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path) {

    if (reset_path && id == Pid::BEAM_POS && (object.is_beam() || object.type() == ElementType::TREMOLO_TWOCHORD)) {
        auto &owner = static_cast<EngravingItem &>(object);
        const auto direction =
            object.is_beam() ? static_cast<Beam &>(object).direction_idx() : static_cast<TremoloTwoChord &>(object).direction_idx();
        uint32_t ordinal = 0;
        for (const auto &entry : authored_override_table(&owner)->entries)
            if (entry.child.role == AuthoredChildRole::BEAM_FRAGMENT && entry.child.direction == direction && entry.pid == id)
                ordinal = std::max(ordinal, entry.child.ordinal);
        reset_child_override(owner, {AuthoredChildRole::BEAM_FRAGMENT, ordinal, uint8_t(direction)}, id);
    } else if (reset_path && id == Pid::PATH &&
               (object.is_chord_line() &&
                (std::ranges::find(ChordLine::appearance_properties, id) != ChordLine::appearance_properties.end() ||
                 id == Pid::APPEARANCE_LINKED_TO_MASTER)))
        object.reset_property(id);
    else
        object.set_property(id, value);
    object.set_property_flags(id, flags);
}

void apply_text_line_property(EngravingObject &object, const PropertyValue &value) {
    apply_property(object, Pid::SYSTEM_FLAG, value, PropertyFlags::NOSTYLE, false);
    if (object.is_text_line()) to_text_line(&object)->init_style();
}

void apply_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags) {
    text.set_property_flags(id, flags);
    text.set_property(id, value);
}

void apply_part_membership(Part &part, size_t index, bool adding) {
    if (adding)
        part.score()->insert_part(&part, index);
    else
        part.score()->remove_part(&part);
}

void apply_part_label(Part &part, Fraction tick, const String &value, bool long_name) {
    if (long_name)
        part.set_long_name(value, tick);
    else
        part.set_short_name(value, tick);
    part.score()->set_layout_all();
}

void apply_staff_membership(Staff &staff, staff_idx_t index, bool adding) {
    if (adding)
        staff.score()->insert_staff(&staff, index);
    else
        staff.score()->remove_staff(&staff);
}

void apply_staff_brackets(Staff &staff, size_t level, BracketType type, size_t span, bool adding) {
    EditStaffBrackets::set_bracket_type(staff.score(), staff.idx(), level, adding ? type : BracketType::NO_BRACKET);
    if (adding) EditStaffBrackets::set_bracket_span(staff.score(), staff.idx(), level, span);
    staff.trigger_layout();
}

void apply_staff_tracks(Measure &measure, staff_idx_t first, staff_idx_t last, bool adding) {
    if (adding)
        measure.insert_staves(first, last);
    else
        measure.remove_staves(first, last);
}

void Transaction::change_property(EngravingObject *object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path) {
    if (note_change(object, CommandType::CHANGE_PROPERTY)) object->master_score()->cmd_state().changes.changed_property_id_set.insert(id);

    apply_property(*object, id, value, flags, reset_path);
}

void Transaction::change_style_values(Score &score, std::span<const StyleValue> values) {
    bool changed = false;
    for (const auto &entry : values) changed |= score.style().style_v(entry.id) != entry.value;
    if (!changed) return;
    if (note_change(&score, CommandType::CHANGE_STYLE_VALUES))
        for (const auto &entry : values)
            if (score.style().style_v(entry.id) != entry.value)
                score.master_score()->cmd_state().changes.changed_style_id_set.insert(entry.id);

    apply_style_values(score, values);
}

void Transaction::replace_style(Score &score, const MStyle &style, bool overlap) {
    if (note_change(&score, CommandType::CHANGE_STYLE))
        for (int i = 0; i < int(Sid::STYLES); ++i) {
            const auto id = static_cast<Sid>(i);
            if (score.style().style_v(id) != style.style_v(id)) score.master_score()->cmd_state().changes.changed_style_id_set.insert(id);
        }

    apply_style_replacement(score, style, overlap);
}

void Transaction::insert_time(Score &score, Fraction tick, Fraction length, bool unmanaged) {
    note_change(&score, unmanaged ? CommandType::INSERT_TIME_UNMANAGED_SPANNER : CommandType::INSERT_TIME);

    apply_time_insertion(score, tick, length, unmanaged);
}

void Transaction::change_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes) {
    note_change(first, adding ? CommandType::INSERT_MEASURES : CommandType::REMOVE_MEASURES);
    note_change(last, adding ? CommandType::INSERT_MEASURES : CommandType::REMOVE_MEASURES);
    if (!adding) {
        for (auto *item = first; item; item = item->next()) {
            defer_root(*item, item->master_score()->eid_register()->enroll(*item));
            if (item == last) break;
        }
    }

    if (adding)
        insert_measure_chain(first, last, move_staff_type_changes);
    else
        remove_measure_chain(first, last, move_staff_type_changes);
}

void Transaction::change_measure_repeat_count(Measure *measure, int count, staff_idx_t staff) {
    note_change(measure, CommandType::CHANGE_MEASURE_REPEAT_COUNT);

    apply_measure_repeat_count(measure, count, staff);
}

void Transaction::change_mstaff(Measure *measure, MStaff *row, staff_idx_t index, bool adding) {
    note_change(measure, adding ? CommandType::INSERT_M_STAFF : CommandType::REMOVE_M_STAFF);
    if (adding)
        measure->insert_m_staff(row, index);
    else {
        defer_root(*measure, row);
        measure->remove_m_staff(row, index);
    }
}

void Transaction::add_mstaff(Measure *measure, std::unique_ptr<MStaff> row, staff_idx_t index) {
    note_change(measure, CommandType::INSERT_M_STAFF);
    try {
        measure->insert_m_staff(row.get(), index);
    } catch (...) {
        if (muse::contains(measure->mstaves(), row.get())) row.release();
        throw;
    }
    row.release();
}

void Transaction::change_staff_tracks(Measure *measure, staff_idx_t first, staff_idx_t last, bool adding) {
    note_change(measure, adding ? CommandType::INSERT_STAVES : CommandType::REMOVE_STAVES);

    apply_staff_tracks(*measure, first, last, adding);
}

void Transaction::change_staff_brackets(Staff *staff, size_t level, BracketType type, size_t span, bool adding) {
    note_change(staff, CommandType::UNKNOWN);

    apply_staff_brackets(*staff, level, type, span, adding);
}

void Transaction::change_staff_membership(Staff *staff, staff_idx_t index, bool adding) {
    note_change(staff, adding ? CommandType::INSERT_STAFF : CommandType::REMOVE_STAFF);
    if (!adding) defer_root(*staff, staff->master_score()->eid_register()->enroll(*staff));

    apply_staff_membership(*staff, index, adding);
}

void Transaction::change_part_membership(Part *part, size_t index, bool adding) {
    note_change(part, adding ? CommandType::INSERT_PART : CommandType::REMOVE_PART);
    if (!adding) defer_root(*part, part->master_score()->eid_register()->enroll(*part));

    apply_part_membership(*part, index, adding);
}

void Transaction::change_part_label(Fraction tick, Part *part, const String &value, bool long_name) {
    note_change(part, long_name ? CommandType::CHANGE_INSTRUMENT_LONG : CommandType::CHANGE_INSTRUMENT_SHORT);

    apply_part_label(*part, tick, value, long_name);
}

void Transaction::change_shared_origin(SharedPart *shared, Part *origin, bool connecting) {
    note_change(shared, connecting ? CommandType::CONNECT_SHARED_PART : CommandType::DISCONNECT_SHARED_PART);

    if (connecting)
        shared->add_origin_part(origin);
    else
        shared->remove_origin_part(origin);
}

void Transaction::change_stave_sharing_state(SharedPart &part, const Fraction &tick, const AuthoredStaveSharingState *state) {
    const auto *old = part.exact_authored_state(tick);
    if (state ? old && *old == *state : !old) return;

    note_change(&part, CommandType::CHANGE_STAVE_SHARING_STATE);
    if (state) {
        part.set_authored_state(tick, *state);
    } else {
        part.remove_authored_state(tick);
    }

    const auto &states = part.authored_states();
    const auto next =
        std::upper_bound(states.begin(), states.end(), tick, [](const Fraction &value, const auto &entry) { return value < entry.first; });
    const auto end = next == states.end() ? part.score()->end_tick() : next->first;
    const auto range = part.track_range();
    if (range.start_track < range.end_track) {
        part.score()->set_layout(tick, end, track2staff(range.start_track), track2staff(range.end_track - 1));
    }
}

void Transaction::change_excerpt_membership(Excerpt *excerpt, bool adding) {
    auto &master = *excerpt->master_score();
    note_change(&master, adding ? CommandType::ADD_EXCERPT : CommandType::REMOVE_EXCERPT);
    if (adding)
        master.add_excerpt(excerpt);
    else {
        defer_root(master, excerpt);
        master.remove_excerpt(excerpt);
    }
}

void Transaction::add_element(EngravingItem *item) {
    validate_element_add(*item);
    note_change(item, CommandType::ADD_ELEMENT);

    apply_element_add(*item);
}

void Transaction::remove_element(EngravingItem *item) {
    note_change(item, CommandType::REMOVE_ELEMENT);
    defer_root(*item, item->master_score()->eid_register()->enroll(*item));
    prepare_element_removal(*item);

    apply_element_remove(*item);
}

void Transaction::replace_element(EngravingItem *before, EngravingItem *after) {
    note_change(before, CommandType::CHANGE_ELEMENT);
    note_change(after, CommandType::CHANGE_ELEMENT);
    defer_root(*before, before->master_score()->eid_register()->enroll(*before));

    apply_element_replace(*before, *after);
}

void Transaction::reparent_element(EngravingItem *item, EngravingItem *parent, staff_idx_t staff) {
    note_change(item, CommandType::CHANGE_PARENT);
    note_change(parent, CommandType::CHANGE_PARENT);

    apply_element_reparent(*item, *parent, staff * voices + item->voice());
}

void Transaction::reparent_segment(Segment *item, Measure *parent, Fraction tick) {
    note_change(item, CommandType::CHANGE_PARENT);

    apply_segment_reparent(*item, *parent, tick);
}

void Transaction::link_objects(EngravingObject *item, EngravingObject *peer) {
    note_change(item, CommandType::LINK);

    item->link_to(peer);
}

void Transaction::unlink_object(EngravingObject *item) {
    note_change(item, CommandType::UNLINK);

    item->unlink();
}

void Transaction::change_bracket_property(BracketItem &item, Pid id, const PropertyValue &value, PropertyFlags flags) {
    if (note_change(item.score(), CommandType::CHANGE_PROPERTY))
        item.master_score()->cmd_state().changes.changed_property_id_set.insert(id);

    // Immediate edits still avoid writing flags through a deleted bracket.
    item.set_property_flags(id, flags);
    item.set_property(id, value);
}

void Transaction::change_text_line_property(EngravingObject &object, const PropertyValue &value) {
    if (note_change(&object, CommandType::CHANGE_PROPERTY))
        object.master_score()->cmd_state().changes.changed_property_id_set.insert(Pid::SYSTEM_FLAG);

    apply_text_line_property(object, value);
}

void Transaction::change_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags) {
    note_change(&text, CommandType::TEXT_EDIT);

    apply_text_format(text, id, value, flags);
}

void Score::apply_element_add(EngravingItem *item) { transaction_manager()->current_or_dummy_transaction().add_element(item); }

void Score::apply_element_remove(EngravingItem *item) { transaction_manager()->current_or_dummy_transaction().remove_element(item); }

void Score::apply_element_replace(EngravingItem *before, EngravingItem *after) {
    transaction_manager()->current_or_dummy_transaction().replace_element(before, after);
}

void Score::apply_parent_change(EngravingItem *item, EngravingItem *parent, staff_idx_t staff) {
    transaction_manager()->current_or_dummy_transaction().reparent_element(item, parent, staff);
}

void Score::apply_segment_parent_change(Segment *item, Measure *parent, Fraction tick) {
    transaction_manager()->current_or_dummy_transaction().reparent_segment(item, parent, tick);
}

void Score::apply_link(EngravingObject *item, EngravingObject *peer) {
    transaction_manager()->current_or_dummy_transaction().link_objects(item, peer);
}

void Score::apply_unlink(EngravingObject *item) { transaction_manager()->current_or_dummy_transaction().unlink_object(item); }

void Score::apply_mstaff(Measure *measure, MStaff *row, staff_idx_t index, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_mstaff(measure, row, index, adding);
}

void Score::apply_mstaff_add(Measure *measure, std::unique_ptr<MStaff> row, staff_idx_t index) {
    transaction_manager()->current_or_dummy_transaction().add_mstaff(measure, std::move(row), index);
}

void Score::apply_staff_tracks(Measure *measure, staff_idx_t first, staff_idx_t last, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_staff_tracks(measure, first, last, adding);
}

void Score::apply_part_membership(Part *part, size_t index, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_part_membership(part, index, adding);
}

void Score::apply_excerpt(Excerpt *excerpt, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_excerpt_membership(excerpt, adding);
}

void Score::apply_bracket_add(Staff *staff, size_t level, BracketType type, size_t span) {
    transaction_manager()->current_or_dummy_transaction().change_staff_brackets(staff, level, type, span, true);
}

void Score::apply_bracket_remove(Staff *staff, size_t level) {
    transaction_manager()->current_or_dummy_transaction().change_staff_brackets(staff, level, BracketType::NO_BRACKET, 0, false);
}

void Score::apply_staff_membership(Staff *staff, staff_idx_t index, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_staff_membership(staff, index, adding);
}

void Score::apply_time_insertion(Score *target, Fraction tick, Fraction length, bool unmanaged) {
    transaction_manager()->current_or_dummy_transaction().insert_time(*target, tick, length, unmanaged);
}

void Score::apply_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes) {
    transaction_manager()->current_or_dummy_transaction().change_measures(first, last, adding, move_staff_type_changes);
}

void Score::apply_measure_repeat_count(Measure *measure, int count, staff_idx_t staff) {
    transaction_manager()->current_or_dummy_transaction().change_measure_repeat_count(measure, count, staff);
}

bool note_change(EngravingObject *object, CommandType type) {
    if (!object) return false;
    auto *master = object->master_score();
    if (!master || !master->transaction_manager()->current_transaction() || master->cmd_state().locked()) return false;
    auto &state = master->cmd_state();
    master->eid_register()->touch(*object);
    ++state.mutation_count;
    for (auto *changed : compound_objects(object)) {
        state.changes.changed_types.insert(changed->type());
        state.changes.changed_objects[changed].insert(type);
    }
    return true;
}

void defer_root(EngravingObject &owner, decltype(DetachedRoot::object) object) {
    auto *master = owner.master_score();
    auto &roots = master->transaction_manager()->detached_roots;
    auto handle = master->eid_register()->enroll(owner);
    if (std::ranges::none_of(roots, [&](const auto &root) { return root.owner == handle && root.object == object; }))
        roots.push_back({handle, object});
}

TransactionManager::~TransactionManager() { cleanup_detached(); }

void TransactionManager::cleanup_detached() {
    auto roots = std::move(detached_roots);
    detached_roots.clear();
    auto &registry = *master_score_->eid_register();
    // Non-DOM records borrow children of their source owner until disposal.
    for (auto &root : roots) {
        auto *owner = registry.item_from_handle(root.owner);
        if (auto *row = std::get_if<MStaff *>(&root.object)) {
            if (!owner || !muse::contains(to_measure(owner)->mstaves(), *row)) delete *row;
        } else if (auto *group = std::get_if<NoteParenthesisInfo *>(&root.object)) {
            if (!owner || !muse::contains(to_chord(owner)->note_parentheses(), *group)) delete *group;
        } else if (auto *lock = std::get_if<const RangeLock *>(&root.object)) {
            if (!owner || (to_score(owner)->system_locks()->lock_starting_at((*lock)->start_mb()) != *lock &&
                           to_score(owner)->page_locks()->lock_starting_at((*lock)->start_mb()) != *lock))
                delete *lock;
        }
    }
    std::vector<EngravingItem *> detached_items;
    detached_items.reserve(roots.size());
    for (auto &root : roots) {
        if (auto *handle = std::get_if<SourceHandle>(&root.object)) {
            if (auto *object = registry.item_from_handle(*handle); object && object->is_engraving_item() && !source_attached(*object))
                detached_items.push_back(to_engraving_item(object));
        }
    }
    const auto retire_scopes = rendering::score::LayoutDataStore::retire_batch_for_disposal(detached_items);
    for (auto &root : roots) {
        if (auto *handle = std::get_if<SourceHandle>(&root.object)) {
            auto *object = registry.item_from_handle(*handle);
            if (object && !source_attached(*object)) delete object;
        }
    }
    for (auto &root : roots)
        if (auto *excerpt = std::get_if<Excerpt *>(&root.object))
            if (!muse::contains(master_score_->excerpts(), *excerpt)) delete *excerpt;
}

void TransactionManager::transaction(const muse::TranslatableString &description, std::function<void(Transaction &)> func) {
    if (active_) {
        func(transaction_);
        return;
    }
    begin_transaction(description);
    try {
        func(transaction_);
        end_transaction();
    } catch (...) {
        end_transaction(true);
        throw;
    }
}

void TransactionManager::begin_transaction([[maybe_unused]] const muse::TranslatableString &description) {
    if (active_) return;
    cleanup_detached();
    master_score_->cmd_state().reset();
    MScore::set_error(MsError::MS_NO_ERROR);
    active_ = true;
}

ScoreChanges TransactionManager::prepare_commit() {
    if (!active_) throw std::logic_error("no active mutation batch");
    if (master_score_->read_only() || (MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning))
        throw std::runtime_error("the engine rejected the mutation batch");
    if (master_score_->source_revision() == std::numeric_limits<uint64_t>::max()) throw std::overflow_error("source revision exhausted");
    master_score_->update(false, NormalizeMode::TRANSACTION);
    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(true);
    auto &state = master_score_->cmd_state();
    auto &changes = state.changes;
    changes.tick_from = state.start_tick().ticks();
    changes.tick_to = state.end_tick().ticks();
    changes.staff_idx_from = state.start_staff();
    changes.staff_idx_to = state.end_staff();
    for (const auto &[object, types] : changes.changed_objects) {
        if (!object->is_engraving_item()) continue;
        const int tick = to_engraving_item(object)->tick().ticks();
        if (changes.tick_from == -1 || changes.tick_from > tick) changes.tick_from = tick;
        if (changes.tick_to == -1 || changes.tick_to < tick) changes.tick_to = tick;
    }
    master_score_->update_automation(changes);
    return changes;
}

void TransactionManager::commit_prepared(const ScoreChanges &changes) {
    const bool changed = master_score_->cmd_state().mutation_count != 0;
    if (changed) {
        master_score_->notify_changes(changes);
        if (MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning)
            throw std::runtime_error("the engine rejected batch finalization");
        master_score_->advance_source_revision();
    }
    active_ = false;
    cleanup_detached();
    master_score_->cmd_state().reset();
}

void TransactionManager::end_transaction(bool failed) {
    if (!active_) {
        cleanup_detached();
        return;
    }
    if (!failed) {
        auto changes = prepare_commit();
        commit_prepared(changes);
        return;
    }
    if (master_score_->cmd_state().mutation_count != 0) master_score_->advance_source_revision();
    active_ = false;
    cleanup_detached();
    master_score_->cmd_state().reset();
}
} // namespace mu::engraving
