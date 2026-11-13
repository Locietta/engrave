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
#include "engrave/src/api/pid_support.h"
#include "engrave/src/api/state.h"
#include "engrave/src/runtime/score_io.h"
#include "engrave/src/score/score_semantic.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/score.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/editing/transaction/mutationjournal.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transaction/undoablecommand.h"
#include "engraving/engravingproject.h"

namespace engrave {

Result<EditResult> Document::edit(const Batch &batch, std::optional<std::uint64_t> expected_revision) {
    impl_->engine->check_thread();
    return api::BatchGuard(*this).edit(batch, expected_revision);
}

namespace api {
namespace {

using mu::engraving::CommandType;
using mu::engraving::EngravingObject;
using mu::engraving::MasterScore;
using mu::engraving::MScore;
using mu::engraving::MsError;
using mu::engraving::UndoableCommand;
using mu::engraving::MutationJournal;

std::unexpected<Error> fail(ErrorCode code, std::string message, std::optional<std::size_t> op = {}) {
    return std::unexpected(Error{code, std::move(message), op});
}

// The ids of the document, as the harness's fingerprint writes them: the writer's
// order (the spanner map, the excerpt list) is not the order an undo puts them back
// in, so a rollback compares them sorted.
std::vector<std::string> id_list(MasterScore &master) {
    std::vector<std::string> ids;
    for (const LiveId &entry : live_ids(master)) {
        ids.push_back(std::format("{} {} {}/{}/{}", to_string(entry.id), entry.object->type_name(), entry.placement.score,
                                  entry.placement.tick, entry.placement.track));
    }
    std::ranges::sort(ids);
    return ids;
}

bool engine_failed() { return MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning; }

bool engine_warned() { return MScore::error != MsError::MS_NO_ERROR && MScore::error_is_warning; }

Policy recovery_policy(MasterScore &master, const Op &op) {
    const Policy policy = policy_of(op);
    const std::string *property = nullptr;
    if (const auto *set = std::get_if<SetProperty>(&op)) property = &set->property;
    if (const auto *reset = std::get_if<ResetProperty>(&op)) property = &reset->property;
    if (property) {
        // Relinking copies a complete master group, beyond the flag inverse.
        // Keep native source recovery until that whole mutation has an inverse.
        switch (pid_of(*property)) {
            case mu::engraving::Pid::APPEARANCE_LINKED_TO_MASTER:
            case mu::engraving::Pid::POSITION_LINKED_TO_MASTER:
            case mu::engraving::Pid::TEXT_LINKED_TO_MASTER: return Policy::SNAPSHOT;
            default: break;
        }
    }
    const auto *remove = std::get_if<Remove>(&op);
    if (!remove) return policy;
    const auto *id = std::get_if<ElementId>(&remove->target);
    // Addresses cannot name these source objects. The only current OpRef
    // producer in these families is InsertMeasures, already a snapshot step.
    if (!id) return policy;
    const auto found = resolve(master, *id);
    if (!found) return policy; // prepare() reports invalid references.
    switch (found->object->type()) {
        // Generic deletion enters the same timing/rewrite/break routines as
        // their dedicated snapshot operations. Other removals stay journal-only.
        case mu::engraving::ElementType::MEASURE:
        case mu::engraving::ElementType::TIMESIG:
        case mu::engraving::ElementType::LAYOUT_BREAK: return Policy::SNAPSHOT;
        default: return policy;
    }
}

} // namespace

std::string_view to_string(Policy policy) noexcept {
    switch (policy) {
        case Policy::TRANSACTION: return "Transaction";
        case Policy::SNAPSHOT: return "Snapshot";
        case Policy::UNEXPOSED: return "Unexposed";
    }
    return "Unknown";
}

std::string_view op_name(const Op &op) {
    return std::visit([](const auto &value) { return OpTraits<std::decay_t<decltype(value)>>::name; }, op);
}

Policy policy_of(const Op &op) {
    return std::visit([](const auto &value) { return OpTraits<std::decay_t<decltype(value)>>::policy; }, op);
}

// --- EditContext ------------------------------------------------------------

Result<Target> EditContext::target(const Ref &ref) const {
    if (const auto *made = std::get_if<OpRef>(&ref)) {
        if (made->op >= op_) {
            return fail(ErrorCode::INVALID_ARGUMENT,
                        std::format("an OpRef names operation {}, which does not run before this one", made->op));
        }
        return Target{ref, nullptr, epoch_};
    }
    const Result<Resolved> found =
        std::holds_alternative<ElementId>(ref) ? resolve(master_, std::get<ElementId>(ref)) : resolve(master_, std::get<Address>(ref));
    if (!found) return std::unexpected(found.error());
    if (!found->writable) return fail(ErrorCode::INVALID_ARGUMENT, "layout builds this element; it cannot be edited");
    return Target{ref, found->object, epoch_, master_.eid_register()->handle_of(found->object)};
}

Result<EngravingObject *> EditContext::object(const Target &target) const {
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
    const Result<Target> again = this->target(target.ref);
    if (!again) return std::unexpected(again.error());
    return again->object;
}

Result<mu::engraving::Score *> EditContext::score(const ScoreSel &selection) const {
    if (!selection.excerpt) return &master_;
    const Result<Resolved> found = resolve(master_, *selection.excerpt);
    if (!found) return std::unexpected(found.error());
    if (!found->object->is_score()) {
        return fail(ErrorCode::INVALID_ARGUMENT, std::format("{} is not a score", to_string(*selection.excerpt)));
    }
    return mu::engraving::to_score(found->object);
}

void EditContext::created(EngravingObject *object) {
    created_[op_].push_back(object ? master_.eid_register()->enroll(*object) : mu::engraving::SourceHandle{});
}

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
    // including changes that have no UndoableCommand. Re-resolve earlier targets.
    ++epoch_;
    assign_missing_ids(master_);
}

// --- BatchGuard -------------------------------------------------------------

// The score as an in-memory MSCZ, with the version stamps the writer replaces, per
// score: the master first, then the excerpts. The semantic dump and the ids are what
// a rollback compares against, since the writer's order is not the undo's.
struct BatchGuard::Snapshot {
    struct Versions {
        muse::String version;
        int revision = 0;
        int msc_version = 0;
    };

    std::string mscz;
    std::string semantic;
    std::vector<std::string> ids;
    std::vector<Versions> versions;
    mu::engraving::RuntimeTempoState runtime_tempo;
    uint64_t source_revision = 0;
    uint64_t saved_revision = 0;
};

BatchGuard::BatchGuard(Document &document) : document_(document), context_(*DocumentAccess::impl(document).project->master_score()) {}

Result<void> BatchGuard::admit(std::optional<std::uint64_t> expected_revision) const {
    const auto &document = DocumentAccess::impl(document_);
    if (document.poisoned) return fail(ErrorCode::POISONED, "an earlier edit left the document unusable");
    if (expected_revision && *expected_revision != document.revision) {
        return fail(ErrorCode::REVISION_CONFLICT,
                    std::format("the document is at revision {}, not {}", document.revision, *expected_revision));
    }
    const MasterScore &master = context_.master();
    const MutationJournal *stack = master.mutation_journal();
    if (master.read_only() || stack->is_locked() || stack->has_active_transaction())
        return fail(ErrorCode::EDIT_REJECTED, "the score cannot be edited now");
    return {};
}

Result<EditResult> BatchGuard::edit(const Batch &batch, std::optional<std::uint64_t> expected_revision) {
    if (const auto admitted = admit(expected_revision); !admitted) return std::unexpected(admitted.error());
    context_.created_.assign(batch.ops.size(), {});
    Policy policy = Policy::TRANSACTION;
    std::vector<Step> steps;
    std::vector<bool> snapshot_step;
    steps.reserve(batch.ops.size());
    snapshot_step.reserve(batch.ops.size());
    for (std::size_t i = 0; i < batch.ops.size(); ++i) {
        const Op &op = batch.ops[i];
        context_.op_ = i;
        const Policy own = recovery_policy(context_.master(), op);
        if (own == Policy::UNEXPOSED) return fail(ErrorCode::UNSUPPORTED, std::format("{} is not available", op_name(op)), i);
        if (own == Policy::SNAPSHOT) policy = Policy::SNAPSHOT;
        snapshot_step.push_back(own == Policy::SNAPSHOT);
        auto step = std::visit([this](const auto &value) { return prepare(context_, value); }, op);
        if (!step) {
            Error error = std::move(step.error());
            error.op_index = i;
            return std::unexpected(std::move(error));
        }
        // Operations resolve source addresses and may read lines, anchors or
        // repeat state. Unknown operation dependencies conservatively use the
        // full semantic barrier; a clean document does no normalization work.
        steps.push_back([apply = std::move(*step)](EditContext &context) {
            context.prepare_semantics();
            return apply(context);
        });
    }
    return execute(std::move(steps), policy, std::move(snapshot_step));
}

Result<EditResult> BatchGuard::run(std::vector<Step> steps, Policy policy, std::optional<std::uint64_t> expected_revision) {
    if (const auto admitted = admit(expected_revision); !admitted) return std::unexpected(admitted.error());
    context_.created_.assign(steps.size(), {});
    return execute(std::move(steps), policy, {});
}

Result<EditResult> BatchGuard::execute(std::vector<Step> steps, Policy policy, std::vector<bool> snapshot_step) {
    auto &document = DocumentAccess::impl(document_);
    EditResult result{.revision = document.revision};
    if (steps.empty()) return result;
    const EditHooks &hooks = document.edit_hooks;
    if (hooks.policy) {
        policy = *hooks.policy;
        // A forced policy applies to every step, and the guard cannot tell which of
        // them the unwind is safe for.
        std::ranges::fill(snapshot_step, policy == Policy::SNAPSHOT);
    }
    // Without per-step policies (run(), whose caller brings its own steps), a snapshot
    // policy says nothing about which step is safe to unwind.
    bool snapshot_ran = snapshot_step.empty() && policy == Policy::SNAPSHOT;

    MasterScore &master = context_.master();
    MutationJournal *stack = master.mutation_journal();
    std::optional<Snapshot> snapshot;
    if (policy == Policy::SNAPSHOT) {
        snapshot = Snapshot{music::export_native_score(*document.engine->context, document.project, "mscz",
                                                       music::ExportOptions{.transaction_snapshot = true}, {}),
                            music::dump_json(music::semantic_dump(&master), 1),
                            id_list(master),
                            {},
                            master.runtime_tempo_state(),
                            master.source_revision(),
                            master.saved_revision()};
        for (const mu::engraving::Score *score : master.score_list())
            snapshot->versions.push_back({score->mscore_version(), score->mscore_revision(), score->msc_version()});
    }

    struct SavedLayout {
        mu::engraving::Score *score;
        mu::engraving::Score::LayoutState state;
        mu::engraving::NormalizeState normalized;
    };

    std::vector<SavedLayout> saved_layouts;
    for (auto *score : master.score_list()) saved_layouts.push_back({score, score->layout_state, score->normalize_state});
    const auto *original_project = document.project.get();
    const auto rollback = [&](bool unsafe) {
        roll_back(snapshot ? &*snapshot : nullptr, unsafe);
        // A snapshot reload owns new, fully dirty caches and instance-local counters;
        // saved pointers and freshness belong only to an in-place unwind.
        if (document.project.get() != original_project) return;
        const auto scores = document.project->master_score()->score_list();
        for (const auto &saved : saved_layouts) {
            if (std::ranges::find(scores, saved.score) == scores.end()) continue;
            // Counters measure executed work, including unsuccessful transactions.
            auto &state = saved.score->layout_state;
            // Beam/system cleanup can detach display topology during an edit.
            // Source replay cannot restore those caches. Rebuild only when a
            // geometry consumer next asks; never advertise the old view as clean.
            state.dirty = true;
            state.start = mu::engraving::Fraction(0, 1);
            state.end = mu::engraving::Fraction(-1, 1);
            state.font_generation = saved.state.font_generation;
            state.revision = saved.state.revision;
            saved.score->normalize_state = saved.normalized;
        }
    };
    const std::vector<LiveId> before = live_ids(master);

    // An engine error that a hook or a step leaves behind rejects the batch at that point.
    std::optional<Error> failure;
    bool engine_threw = false;
    const auto check_engine = [&failure, &result, &steps](std::size_t step) {
        if (engine_failed()) {
            failure = Error{ErrorCode::EDIT_REJECTED, std::format("the engine refused it: {}", MScore::error_to_string(MScore::error)),
                            step < steps.size() ? std::optional(step) : std::nullopt};
        } else if (engine_warned()) {
            result.diagnostics.push_back({Severity::WARNING, MScore::error_to_string(MScore::error)});
        }
        MScore::set_error(MsError::MS_NO_ERROR);
        return !failure;
    };
    try {
        MScore::set_error(MsError::MS_NO_ERROR);
        master.start_cmd(muse::TranslatableString::untranslatable("engrave edit"));
        if (!stack->has_active_transaction()) return fail(ErrorCode::EDIT_REJECTED, "the engine did not start a transaction");
        for (std::size_t i = 0; i <= steps.size(); ++i) {
            context_.op_ = i;
            if (hooks.before_step) hooks.before_step(i);
            if (!check_engine(i) || i == steps.size()) break;
            if (i < snapshot_step.size() && snapshot_step[i]) snapshot_ran = true;
            if (hooks.needs_normalized_state && hooks.needs_normalized_state(i)) context_.prepare_semantics();
            const std::size_t mark = stack->active_transaction()->entry_count();
            if (auto applied = steps[i](context_); !applied) {
                failure = std::move(applied.error());
                failure->op_index = i;
                break;
            }
            if (!check_engine(i)) break;
            if (stack->active_transaction()->structural_since(mark)) ++context_.epoch_;
        }
        if (!failure) {
            auto *manager = master.transaction_manager();
            auto changes = manager->prepare_commit();
            if (check_engine(steps.size())) {
                assign_missing_ids(master);
                IdDiff diff = diff_ids(before, live_ids(master));
                const bool changed = !stack->active_transaction()->empty();
                if (changed && document.revision == std::numeric_limits<uint64_t>::max())
                    throw std::overflow_error("document revision exhausted");
                const auto revision = document.revision + (changed ? 1 : 0);
                const auto scores = master.score_list();
                result.revision = revision;
                result.created = std::move(diff.created);
                result.removed = std::move(diff.removed);
                result.replaced = std::move(diff.replaced);
                result.ops.resize(steps.size());
                for (std::size_t i = 0; i < steps.size(); ++i) {
                    result.ops[i].created.reserve(context_.created_[i].size());
                    for (auto handle : context_.created_[i]) {
                        const auto *object = master.eid_register()->live_item(handle);
                        const bool named = object && takes_id(*object) && object->eid().is_valid();
                        result.ops[i].created.push_back(named ? std::optional(from_engine(object->eid())) : std::nullopt);
                    }
                }
                if (hooks.before_cleanup) hooks.before_cleanup();
                if (check_engine(steps.size())) {
                    manager->commit_prepared(changes);
                    document.revision = revision;
                    for (auto *score : scores)
                        if (!score->layout_state.dirty) score->layout_state.revision = revision;
                }
            }
        }
    } catch (const std::bad_alloc &) {
        rollback(true);
        throw;
    } catch (const std::exception &error) {
        const std::size_t op = context_.op_;
        engine_threw = true;
        failure = Error{ErrorCode::EDIT_REJECTED, std::format("the engine failed: {}", error.what()),
                        op < steps.size() ? std::optional(op) : std::nullopt};
    } catch (...) {
        rollback(true);
        throw;
    }
    if (failure) {
        rollback(engine_threw || snapshot_ran);
        return std::unexpected(std::move(*failure));
    }

    return result;
}

// Brings the score back to where it was before the batch. A failure here leaves the
// document poisoned and is thrown.
void BatchGuard::roll_back(const Snapshot *snapshot, bool unwind_unsafe) {
    auto &document = DocumentAccess::impl(document_);
    MasterScore &master = context_.master();
    MutationJournal *stack = master.mutation_journal();
    try {
        if (snapshot) {
            // Prefer an unwind when the commands are safe to replay. It preserves
            // source identity and compatibility state that a native snapshot reload
            // can change. Geometry is invalidated after either rollback path.
            const bool active = stack->has_active_transaction();
            if (active && !unwind_unsafe) {
                // Replay normalizes without layout. Fall back to replacement if
                // the resulting musical content or source identities differ.
                master.end_cmd(true);
                if (!same_score(*snapshot)) restore(*snapshot);
            } else {
                // The project is replaced rather than unwound: an operation whose rollback
                // policy is Snapshot cannot be undone (the excerpt build frees its links
                // twice), and after a failure in the middle of a command the undo is not
                // to be trusted either.
                if (active) stack->abandon();
                restore(*snapshot);
            }
        } else if (stack->has_active_transaction()) {
            master.end_cmd(true);
        }
    } catch (...) {
        document.poisoned = true;
        throw;
    }
    MScore::set_error(MsError::MS_NO_ERROR);
}

void BatchGuard::restore(const Snapshot &snapshot) {
    auto &document = DocumentAccess::impl(document_);
    if (document.edit_hooks.fail_restore) throw std::runtime_error("the snapshot restore failed (test hook)");
    music::ScoreProject project = music::load_native_score(*document.engine->context, snapshot.mscz, "mscz");
    // Restore compatibility versions before normalization. Replacement geometry
    // stays fully dirty until an explicit geometry consumer uses the boundary.
    mu::engraving::ScoreLoad load;
    auto versions = snapshot.versions.begin();
    for (mu::engraving::Score *score : project->master_score()->score_list()) {
        if (versions == snapshot.versions.end()) break;
        score->set_mscore_version(versions->version);
        score->set_mscore_revision(versions->revision);
        score->set_msc_version(versions->msc_version);
        mu::engraving::invalidate_normalization(*score);
        ++versions;
    }
    mu::engraving::normalize_document(*project->master_score(), {.mode = mu::engraving::NormalizeMode::LOAD});
    project->master_score()->invalidate_repeat_list();
    project->master_score()->init_automation();
    project->master_score()->restore_runtime_tempo_state(snapshot.runtime_tempo);
    project->master_score()->restore_source_revisions(snapshot.source_revision, snapshot.saved_revision);
    assign_missing_ids(*project->master_score());
    // A replacement has no geometry to retain. Its full pending layout also
    // covers every old pending range; counters belong to this new Score instance.
    document.project = std::move(project);
    document.bind_layout_boundary();
    for (auto *score : document.project->master_score()->score_list()) {
        score->layout_state.revision = document.revision;
        score->layout_state.font_generation = document.engine->font_generation;
    }
}

// Whether the undo left the document where the batch found it: the same musical
// content and the same ids. A serialized comparison would be stricter than the undo
// can promise (the writer orders spanners and excerpts its own way), and a restore is
// what a difference costs, so the harness's own notion of the score's content is used.
bool BatchGuard::same_score(const Snapshot &snapshot) {
    MasterScore &master = context_.master();
    return music::dump_json(music::semantic_dump(&master), 1) == snapshot.semantic && id_list(master) == snapshot.ids;
}

} // namespace api
} // namespace engrave
