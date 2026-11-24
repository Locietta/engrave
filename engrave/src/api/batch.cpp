#include "engrave/src/api/batch.h"

#include <algorithm>
#include <format>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/runtime/score_io.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/dom/tie.h"
#include "engraving/editing/cmd.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/engravingproject.h"

namespace engrave {
Result<EditResult> Document::edit(const Batch &batch, std::optional<std::uint64_t> expected_revision) {
    return edit(batch, EditOptions{.expected_revision = expected_revision, .mode = EditMode::Prefix});
}

Result<EditResult> Document::edit(const Batch &batch, const EditOptions &options) {
    impl_->engine->check_thread();
    std::optional<Impl::Snapshot> rollback;
    const auto attempt = [&]() -> Result<EditResult> {
        api::BatchGuard guard(*this);
        if (auto admitted = guard.admit(options.expected_revision); !admitted) return std::unexpected(admitted.error());
        auto steps = guard.prepare_batch(batch);
        if (!steps) return std::unexpected(steps.error());
        if (options.mode == EditMode::Atomic && !steps->empty()) {
            auto captured = impl_->capture_snapshot();
            if (!captured) return std::unexpected(captured.error());
            rollback.emplace(std::move(*captured));
        }
        return guard.execute(std::move(*steps), options.mode);
    };
    if (options.mode == EditMode::Prefix) return attempt();

    Result<EditResult> result;
    try {
        result = attempt();
    } catch (const std::exception &exception) {
        result = std::unexpected(Error{ErrorCode::EDIT_REJECTED, exception.what()});
    } catch (...) {
        result = std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine threw an unknown exception"});
    }
    if (result || !rollback) return result;

    // The guard, its closures and the registry diff must be gone before the
    // restore replaces the DOM they reference.
    auto error = std::move(result.error());
    if (auto restored = impl_->restore_snapshot(*rollback, false); !restored) {
        impl_->poisoned = true;
        error.poisoned = true;
        error.message += "; rollback failed: " + restored.error().message;
        return std::unexpected(std::move(error));
    }
    error.poisoned = false;
    error.applied_ops.clear();
    if (!error.op_index) return std::unexpected(std::move(error));
    return EditResult{.rejection = BatchRejection{*error.op_index, std::move(error)}, .revision = impl_->revision};
}

namespace api {
namespace {
using mu::engraving::EngravingObject;
using mu::engraving::MasterScore;
using mu::engraving::MScore;
using mu::engraving::MsError;
using mu::engraving::Note;
using mu::engraving::Tie;

std::unexpected<Error> fail(ErrorCode code, std::string message, std::optional<std::size_t> op = {}) {
    Error error{code, std::move(message), op};
    error.poisoned = code == ErrorCode::POISONED;
    return std::unexpected(std::move(error));
}

bool engine_failed() { return MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning; }

bool engine_warned() { return MScore::error != MsError::MS_NO_ERROR && MScore::error_is_warning; }

std::string id_text(const EngravingObject &object) { return object.eid().is_valid() ? to_string(from_engine(object.eid())) : "(no id)"; }

// SetPitch moves only the note it names, so pairing a tie's ends is left to the
// rest of the batch. A tie still joining two pitches when the batch ends stays
// in the score, but MusicXML and 4.70 exports cannot keep it.
void warn_mismatched_ties(MasterScore &master, const std::vector<mu::engraving::SourceHandle> &notes, std::vector<Diagnostic> &out) {
    std::vector<const Tie *> reported;
    for (const auto handle : notes) {
        const EngravingObject *object = master.eid_register()->live_item(handle);
        if (!object || !object->is_note()) continue;
        const Note *note = mu::engraving::to_note(object);
        for (const Tie *tie : {note->tie_back(), note->tie_for()}) {
            if (!tie || !tie->start_note() || !tie->end_note() || tie->start_note()->ppitch() == tie->end_note()->ppitch()) continue;
            const bool duplicate = std::ranges::any_of(
                reported, [&](const Tie *prev) { return prev == tie || (prev->links() && prev->links() == tie->links()); });
            if (duplicate) continue;
            reported.push_back(tie);
            out.push_back({Severity::WARNING,
                           std::format("tie {} now joins notes {} and {}, which have different pitches; MusicXML and 4.70 exports cannot "
                                       "keep it",
                                       id_text(*tie), id_text(*tie->start_note()), id_text(*tie->end_note()))});
        }
    }
}
template <class Op>
constexpr bool requires_master_score =
    std::same_as<Op, InsertMeasures> || std::same_as<Op, RemoveMeasures> || std::same_as<Op, AddPart> || std::same_as<Op, RemovePart> ||
    std::same_as<Op, GenerateExcerpts> || std::same_as<Op, SetTimeSig> || std::same_as<Op, SetRepeatBarlines>;

template <class T>
Result<T> in_field(Result<T> result, std::string_view field) {
    if (result) return result;
    auto &error = result.error();
    error.message = std::format("{}: {}", field, error.message);
    if (error.position && error.position->field.empty()) error.position->field = field;
    return result;
}

std::unexpected<Error> past_end(PositionError::Kind kind, std::int32_t index, std::size_t count, ErrorCode code) {
    std::string_view noun;
    switch (kind) {
        case PositionError::Kind::MEASURE: noun = "measure"; break;
        case PositionError::Kind::STAFF: noun = "staff"; break;
        case PositionError::Kind::VOICE: noun = "voice"; break;
    }
    Error error{code, std::format("the score has no {} at index {}", noun, index)};
    error.position = PositionError{kind, {}, index, count};
    return std::unexpected(std::move(error));
}
} // namespace

Result<Target> EditContext::target(const Ref &ref, std::string_view field, Selector selector) const {
    const auto prepare = [&]() -> Result<Target> {
        if (const auto *made = std::get_if<OpRef>(&ref)) {
            if (made->op >= op_) {
                return fail(ErrorCode::INVALID_ARGUMENT,
                            std::format("an OpRef names operation {}, which does not run before this one", made->op));
            }
            return Target{*made, field};
        }
        if (const auto *index = std::get_if<MeasureIndex>(&ref)) {
            if (selector != Selector::MEASURE) return fail(ErrorCode::INVALID_ARGUMENT, "a measure index is not accepted here");
            if (index->index < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the measure index must be nonnegative");
            return Target{*index, field};
        }
        if (const auto *index = std::get_if<StaffIndex>(&ref)) {
            if (selector != Selector::STAFF) return fail(ErrorCode::INVALID_ARGUMENT, "a staff index is not accepted here");
            if (index->index < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the staff index must be nonnegative");
            return Target{*index, field};
        }
        if (const auto *position = std::get_if<RelativePosition>(&ref)) {
            if (selector != Selector::POSITION) return fail(ErrorCode::INVALID_ARGUMENT, "a relative position is not accepted here");
            if (position->measure.index < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the measure index must be nonnegative");
            if (position->staff < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the staff index must be nonnegative");
            if (position->voice < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the voice index must be nonnegative");
            if (position->offset.num < 0 || !to_reduced_engine_fraction(position->offset))
                return fail(ErrorCode::INVALID_ARGUMENT, "offset must be a nonnegative fraction representable by the engine");
            return Target{*position, field};
        }
        if (const auto *address = std::get_if<Address>(&ref)) {
            if (selector == Selector::MEASURE || selector == Selector::STAFF)
                return fail(ErrorCode::INVALID_ARGUMENT, "an address cannot name a measure or staff");
            const auto *segment = std::get_if<SegmentAddress>(address);
            if (!segment) {
                const auto found = resolve(master_, *address);
                if (!found) return std::unexpected(found.error());
                return fail(ErrorCode::INVALID_ARGUMENT, "layout builds this element; it cannot be edited");
            }
            if (auto valid = validate_segment_address(*segment); !valid) return std::unexpected(valid.error());
            if (segment->staff < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the staff index must be nonnegative");
            if (segment->voice < 0) return fail(ErrorCode::INVALID_ARGUMENT, "the voice index must be nonnegative");
            if (static_cast<std::size_t>(segment->voice) >= mu::engraving::voices)
                return past_end(PositionError::Kind::VOICE, segment->voice, mu::engraving::voices, ErrorCode::INVALID_ARGUMENT);
            const auto measure = segment_measure(master_, *segment);
            if (!measure) return std::unexpected(measure.error());
            if (auto allowed = check_scope(**measure); !allowed) return std::unexpected(allowed.error());
            return Target{*segment, field};
        }
        const auto id = std::get<ElementId>(ref);
        const auto found = resolve(master_, id);
        if (!found) return std::unexpected(found.error());
        if (auto allowed = check_scope(*found->object); !allowed) return std::unexpected(allowed.error());
        return Target{id, field, found->object, epoch_, master_.eid_register()->handle_of(found->object)};
    };
    return in_field(prepare(), field);
}

Result<Target> EditContext::position_target(const EditAddress &at, bool chord_rest_only) const {
    if (const auto *relative = std::get_if<RelativePosition>(&at)) return target(*relative, "at", Selector::POSITION);
    const auto &address = std::get<Address>(at);
    const auto *segment = std::get_if<SegmentAddress>(&address);
    if (!segment || (chord_rest_only && segment->kind != "ChordRest"))
        return fail(ErrorCode::INVALID_ARGUMENT,
                    chord_rest_only ? "at: a ChordRest segment address is required" : "at: a segment address is required");
    return target(address, "at");
}

Result<Target> EditContext::position_target(const EntryAddress &at) const {
    return std::visit([this](const auto &value) { return position_target(EditAddress{value}, true); }, at);
}

Result<void> EditContext::check_scope(const EngravingObject &object) const {
    if (scoped_score_ && object.score() != scoped_score_)
        return fail(ErrorCode::INVALID_ARGUMENT, "the operand belongs to a different score than the batch scope");
    return {};
}

Result<mu::engraving::Measure *> EditContext::measure_at(MeasureIndex index) const {
    auto &score = scoped_score_ ? *scoped_score_ : master_;
    auto *measure = score.first_measure();
    for (std::int32_t i = 0; measure && i < index.index; ++i) measure = measure->next_measure();
    if (measure) return measure;
    return past_end(PositionError::Kind::MEASURE, index.index, score.nmeasures(), ErrorCode::EDIT_REJECTED);
}

Result<Position> EditContext::position(const Target &target) const {
    const auto locate = [&]() -> Result<Position> {
        if (const auto *address = std::get_if<SegmentAddress>(&target.ref)) {
            const auto measure = segment_measure(master_, *address);
            if (!measure) return std::unexpected(measure.error());
            if (auto allowed = check_scope(**measure); !allowed) return std::unexpected(allowed.error());
            if ((*measure)->score()->read_only() || id_class(**measure) == IdClass::LAYOUT)
                return fail(ErrorCode::INVALID_ARGUMENT, "the measure cannot be edited");
            if (const auto staves = (*measure)->score()->nstaves(); static_cast<std::size_t>(address->staff) >= staves)
                return past_end(PositionError::Kind::STAFF, address->staff, staves, ErrorCode::INVALID_ARGUMENT);
            return Position{*address, *measure};
        }

        const auto &relative = std::get<RelativePosition>(target.ref);
        const auto measure = measure_at(relative.measure);
        if (!measure) return std::unexpected(measure.error());
        if (const auto staves = (*measure)->score()->nstaves(); static_cast<std::size_t>(relative.staff) >= staves)
            return past_end(PositionError::Kind::STAFF, relative.staff, staves, ErrorCode::EDIT_REJECTED);
        if (static_cast<std::size_t>(relative.voice) >= mu::engraving::voices)
            return past_end(PositionError::Kind::VOICE, relative.voice, mu::engraving::voices, ErrorCode::EDIT_REJECTED);
        const auto length = (*measure)->ticks();
        if (relative.offset >= Fraction{length.numerator64(), length.denominator64()})
            return fail(ErrorCode::EDIT_REJECTED, "offset must fall within the measure's duration");
        const auto start = (*measure)->tick();
        const auto onset = to_reduced_engine_fraction({start.numerator64(), start.denominator64()});
        if (!onset) return fail(ErrorCode::EDIT_REJECTED, "the measure's tick cannot be represented by the engine");
        // Two reduced int-sized fractions can be added with the engine's 64-bit intermediates.
        const auto sum = *onset + *to_reduced_engine_fraction(relative.offset);
        const auto tick = to_reduced_engine_fraction({sum.numerator64(), sum.denominator64()});
        if (!tick) return fail(ErrorCode::EDIT_REJECTED, "offset produces an unrepresentable score tick");
        return Position{
            {from_engine((*measure)->eid()), relative.staff, relative.voice, {tick->numerator64(), tick->denominator64()}, "ChordRest"},
            *measure};
    };
    return in_field(locate(), target.field);
}

Result<mu::engraving::Segment *> EditContext::segment(const Position &position, const Target &target, bool allow_missing) const {
    const auto locate = [&]() -> Result<mu::engraving::Segment *> {
        const bool relative = std::holds_alternative<RelativePosition>(target.ref);
        bool ambiguous = false;
        auto *segment = find_segment(*position.measure, position.address, &ambiguous);
        if (ambiguous)
            return fail(ErrorCode::EDIT_REJECTED, std::format("more than one {} segment matches this onset", position.address.kind));
        if (!segment) {
            if (allow_missing) return nullptr;
            auto error = missing_segment(position.address);
            if (relative) error.code = ErrorCode::EDIT_REJECTED;
            return std::unexpected(std::move(error));
        }
        if (id_class(*segment) == IdClass::LAYOUT)
            return fail(ErrorCode::INVALID_ARGUMENT, "layout builds this element; it cannot be edited");
        return segment;
    };
    return in_field(locate(), target.field);
}

Result<SegmentTrack> EditContext::segment_track(const Target &target) const {
    const auto at = position(target);
    if (!at) return std::unexpected(at.error());
    const auto found = segment(*at, target);
    if (!found) return std::unexpected(found.error());
    return SegmentTrack{*found, mu::engraving::staff2track(at->address.staff, at->address.voice)};
}

Result<mu::engraving::ChordRest *> EditContext::chord_rest_at(const Target &target) const {
    EngravingObject *item = nullptr;
    if (target.is_position()) {
        const auto at = segment_track(target);
        if (!at) return std::unexpected(at.error());
        item = at->segment->element(at->track);
    } else {
        const auto found = object(target);
        if (!found) return std::unexpected(found.error());
        item = *found;
        if (item->is_note()) item = mu::engraving::to_note(item)->chord();
    }
    if (item && item->is_chord_rest()) return mu::engraving::to_chord_rest(item);
    const auto code = target.is_position() ? ErrorCode::EDIT_REJECTED : ErrorCode::INVALID_ARGUMENT;
    return fail(code, std::format("{}: a chord or rest is required at the requested position", target.field));
}

Result<EngravingObject *> EditContext::object(const Target &target) const {
    if (target.is_position()) {
        const auto found = segment_track(target);
        if (!found) return std::unexpected(found.error());
        return found->segment;
    }
    const auto locate = [&]() -> Result<EngravingObject *> {
        if (const auto *measure = std::get_if<MeasureIndex>(&target.ref)) {
            const auto found = measure_at(*measure);
            if (!found) return std::unexpected(found.error());
            return *found;
        }
        if (const auto *staff = std::get_if<StaffIndex>(&target.ref)) {
            auto &score = scoped_score_ ? *scoped_score_ : master_;
            if (static_cast<std::size_t>(staff->index) >= score.nstaves())
                return past_end(PositionError::Kind::STAFF, staff->index, score.nstaves(), ErrorCode::EDIT_REJECTED);
            return score.staff(staff->index);
        }
        if (const auto *made = std::get_if<OpRef>(&target.ref)) {
            const auto &list = created_[made->op];
            if (made->item >= list.size()) {
                return fail(ErrorCode::INVALID_ARGUMENT,
                            std::format("operation {} created {} element(s), not {}", made->op, list.size(), made->item + 1));
            }
            EngravingObject *object = master_.eid_register()->live_item(list[made->item]);
            if (!object) {
                return fail(ErrorCode::STALE_HANDLE,
                            std::format("element {} of operation {} is no longer part of the score", made->item, made->op));
            }
            return object;
        }
        if (target.epoch == epoch_) {
            if (auto *object = master_.eid_register()->live_item(target.handle)) return object;
            return fail(ErrorCode::STALE_HANDLE, "the prepared source element is no longer live");
        }
        const auto again = resolve(master_, std::get<ElementId>(target.ref));
        if (!again) return std::unexpected(again.error());
        return again->object;
    };
    auto found = locate();
    if (found) {
        if (auto allowed = check_scope(**found); !allowed) found = std::unexpected(allowed.error());
    }
    return in_field(std::move(found), target.field);
}

Result<ScoreSel> EditContext::style_selection(const std::optional<ScoreSel> &selection) const {
    if (scope_ && selection && selection->excerpt != scope_->excerpt)
        return fail(ErrorCode::INVALID_ARGUMENT, "score: the style selection conflicts with the batch scope");
    return selection ? *selection : scope_.value_or(ScoreSel{});
}

Result<mu::engraving::Score *> EditContext::score(const ScoreSel &selection) const {
    auto selected = select_score(master_, selection);
    if (!selected) return selected;
    if (selection.excerpt && (*selected)->is_master()) return fail(ErrorCode::INVALID_ARGUMENT, "the selection must identify an excerpt");
    if ((*selected)->read_only()) return fail(ErrorCode::INVALID_ARGUMENT, "the selected score is read-only");
    return selected;
}

void EditContext::created(EngravingObject *object) {
    created_[op_].push_back(object ? master_.eid_register()->enroll(*object) : mu::engraving::SourceHandle{});
}

void EditContext::repitched(EngravingObject &note) { repitched_.push_back(master_.eid_register()->enroll(note)); }

void EditContext::prepare_semantics() {
    bool pending = false;
    for (auto *score : master_.score_list()) {
        const auto &state = score->normalize_state;
        pending |=
            state.dirty || state.rebuild_midi_mapping || !state.includes_hidden_staves || score->need_update_ticks_and_time_sig_map();
    }
    if (!pending) return;
    mu::engraving::normalize_document(master_, {.include_hidden_staves = true});
    // Repairs can add/remove semantic children or change direct source anchors,
    // so earlier targets must be resolved again.
    ++epoch_;
    assign_missing_ids(master_);
}

BatchGuard::BatchGuard(Document &document) : document_(document), context_(*DocumentAccess::impl(document).project->master_score()) {}

Result<void> BatchGuard::admit(std::optional<std::uint64_t> expected_revision) const {
    const auto &document = DocumentAccess::impl(document_);
    if (document.poisoned) return fail(ErrorCode::POISONED, "restore_checkpoint or reopen the document before editing");
    if (expected_revision && *expected_revision != document.revision)
        return fail(ErrorCode::REVISION_CONFLICT,
                    std::format("the document is at revision {}, not {}", document.revision, *expected_revision));
    const auto &master = context_.master();
    if (master.read_only() || master.cmd_state().locked() || master.transaction_manager()->current_transaction())
        return fail(ErrorCode::EDIT_REJECTED, "the score cannot be edited now");
    return {};
}

Result<std::vector<Step>> BatchGuard::prepare_batch(const Batch &batch) {
    context_.scope_ = batch.scope;
    if (batch.scope) {
        auto selected = in_field(context_.score(*batch.scope), "scope");
        if (!selected) return std::unexpected(selected.error());
        context_.scoped_score_ = *selected;
    }
    context_.created_.assign(batch.ops.size(), {});
    std::vector<Step> steps;
    steps.reserve(batch.ops.size());
    for (std::size_t i = 0; i < batch.ops.size(); ++i) {
        context_.op_ = i;
        auto step = std::visit(
            [this](const auto &op) -> Result<Step> {
                using Operation = std::remove_cvref_t<decltype(op)>;
                if constexpr (requires_master_score<Operation>) {
                    if (context_.scope_ && context_.scope_->excerpt)
                        return fail(ErrorCode::INVALID_ARGUMENT, "scope: this operation requires the master score");
                }
                return prepare(context_, op);
            },
            batch.ops[i]);
        if (!step) {
            auto error = std::move(step.error());
            error.op_index = i;
            return std::unexpected(std::move(error));
        }
        steps.push_back(std::move(*step));
    }
    return steps;
}

Result<EditResult> BatchGuard::run(std::vector<Step> steps) {
    if (auto admitted = admit({}); !admitted) return std::unexpected(admitted.error());
    context_.created_.assign(steps.size(), {});
    return execute(std::move(steps), EditMode::Prefix);
}

Result<EditResult> BatchGuard::execute(std::vector<Step> steps, EditMode mode) {
    auto &document = DocumentAccess::impl(document_);
    EditResult result{.revision = document.revision};
    if (steps.empty()) return result;
    if (document.revision == std::numeric_limits<std::uint64_t>::max())
        return fail(ErrorCode::RESOURCE_LIMIT, "document revision exhausted");
    auto &master = context_.master();
    auto *manager = master.transaction_manager();
    auto &registry = *master.eid_register();
    registry.begin_id_diff();

    struct DiffScope {
        mu::engraving::EIDRegister &registry;

        ~DiffScope() { registry.end_id_diff(); }
    } diff_scope{registry};

    std::vector<std::size_t> applied_ops;
    applied_ops.reserve(steps.size());
    result.ops.resize(steps.size());
    std::optional<Error> failure;
    const auto close_failed_batch = [&] {
        const bool changed = master.cmd_state().mutation_count != 0;
        document.poisoned = changed;
        if (changed) {
            ++document.revision;
            master.invalidate_repeat_list();
        }
        manager->end_transaction(true);
        MScore::set_error(MsError::MS_NO_ERROR);
    };
    const auto report_failure = [&](Error error) -> Result<EditResult> {
        error.poisoned = document.poisoned;
        error.applied_ops = std::move(applied_ops);
        return std::unexpected(std::move(error));
    };
    const auto check_engine = [&](std::optional<std::size_t> op) {
        if (engine_failed())
            failure = Error{ErrorCode::EDIT_REJECTED, std::format("the engine refused it: {}", MScore::error_to_string(MScore::error)), op};
        else if (engine_warned())
            result.diagnostics.push_back({Severity::WARNING, MScore::error_to_string(MScore::error)});
        MScore::set_error(MsError::MS_NO_ERROR);
        return !failure;
    };
    try {
        master.start_cmd(muse::TranslatableString::untranslatable("engrave edit"));
        for (std::size_t i = 0; i < steps.size(); ++i) {
            context_.op_ = i;
            context_.prepare_semantics();
            const auto marking = master.cmd_state().mutation_count;
            try {
                if (context_.scope_) {
                    auto selected = in_field(context_.score(*context_.scope_), "scope");
                    if (!selected)
                        failure = std::move(selected.error());
                    else
                        context_.scoped_score_ = *selected;
                }
                if (!failure) {
                    if (auto applied = steps[i](context_); !applied) failure = std::move(applied.error());
                }
                if (failure) {
                    failure->op_index = i;
                }
            } catch (const std::exception &error) {
                failure = Error{ErrorCode::EDIT_REJECTED, std::format("the engine failed: {}", error.what()), i};
            } catch (...) {
                if (mode == EditMode::Prefix) throw;
                failure = Error{ErrorCode::EDIT_REJECTED, "the engine threw an unknown exception", i};
            }
            if (!failure) check_engine(i);
            if (failure) {
                if (mode == EditMode::Prefix && master.cmd_state().mutation_count == marking) {
                    result.rejection = BatchRejection{i, std::move(*failure)};
                    failure.reset();
                    MScore::set_error(MsError::MS_NO_ERROR);
                }
                break;
            }
            applied_ops.push_back(i);
            ++context_.epoch_;
        }
        if (!failure) {
            context_.op_ = steps.size();
            manager->prepare_commit();
            if (check_engine({})) {
                assign_missing_ids(master);
                warn_mismatched_ties(master, context_.repitched_, result.diagnostics);
                auto diff = batch_id_diff(master);
                result.created = std::move(diff.created);
                result.removed = std::move(diff.removed);
                result.replaced = std::move(diff.replaced);
                for (std::size_t i = 0; i < steps.size(); ++i)
                    for (auto handle : context_.created_[i]) {
                        const auto *object = master.eid_register()->live_item(handle);
                        result.ops[i].created.push_back(object && takes_id(*object) && object->eid().is_valid() ?
                                                            std::optional(from_engine(object->eid())) :
                                                            std::nullopt);
                    }
                const bool changed = master.cmd_state().mutation_count != 0;
                manager->commit_prepared();
                if (changed) ++document.revision;
                result.applied = applied_ops.size();
                result.ops.resize(result.applied);
                result.revision = document.revision;
                return result;
            }
        }
    } catch (const std::exception &error) {
        close_failed_batch();
        return report_failure({ErrorCode::EDIT_REJECTED, std::format("the engine failed: {}", error.what()),
                               context_.op_ < steps.size() ? std::optional(context_.op_) : std::nullopt});
    } catch (...) {
        close_failed_batch();
        if (mode == EditMode::Atomic)
            return report_failure({ErrorCode::EDIT_REJECTED, "the engine threw an unknown exception",
                                   context_.op_ < steps.size() ? std::optional(context_.op_) : std::nullopt});
        throw;
    }
    close_failed_batch();
    return report_failure(std::move(*failure));
}
} // namespace api
} // namespace engrave
