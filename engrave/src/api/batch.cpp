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
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/score.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/editing/cmd.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/engravingproject.h"

namespace engrave {
Result<EditResult> Document::edit(const Batch &batch, std::optional<std::uint64_t> expected_revision) {
    impl_->engine->check_thread();
    return api::BatchGuard(*this).edit(batch, expected_revision);
}

namespace api {
namespace {
using mu::engraving::EngravingObject;
using mu::engraving::MasterScore;
using mu::engraving::MScore;
using mu::engraving::MsError;

std::unexpected<Error> fail(ErrorCode code, std::string message, std::optional<std::size_t> op = {}) {
    Error error{code, std::move(message), op};
    error.poisoned = code == ErrorCode::POISONED;
    return std::unexpected(std::move(error));
}

bool engine_failed() { return MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning; }

bool engine_warned() { return MScore::error != MsError::MS_NO_ERROR && MScore::error_is_warning; }
} // namespace

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

Result<EditResult> BatchGuard::edit(const Batch &batch, std::optional<std::uint64_t> expected_revision) {
    if (auto admitted = admit(expected_revision); !admitted) return std::unexpected(admitted.error());
    context_.created_.assign(batch.ops.size(), {});
    std::vector<Step> steps;
    steps.reserve(batch.ops.size());
    for (std::size_t i = 0; i < batch.ops.size(); ++i) {
        context_.op_ = i;
        auto step = std::visit([this](const auto &value) { return prepare(context_, value); }, batch.ops[i]);
        if (!step) {
            auto error = std::move(step.error());
            error.op_index = i;
            return std::unexpected(std::move(error));
        }
        steps.push_back(std::move(*step));
    }
    return execute(std::move(steps));
}

Result<EditResult> BatchGuard::run(std::vector<Step> steps) {
    if (auto admitted = admit({}); !admitted) return std::unexpected(admitted.error());
    context_.created_.assign(steps.size(), {});
    return execute(std::move(steps));
}

Result<EditResult> BatchGuard::execute(std::vector<Step> steps) {
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
                if (auto applied = steps[i](context_); !applied) {
                    failure = std::move(applied.error());
                    failure->op_index = i;
                }
            } catch (const std::exception &error) {
                failure = Error{ErrorCode::EDIT_REJECTED, std::format("the engine failed: {}", error.what()), i};
            }
            if (!failure) check_engine(i);
            if (failure) {
                if (master.cmd_state().mutation_count == marking) {
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
            auto changes = manager->prepare_commit();
            if (check_engine({})) {
                assign_missing_ids(master);
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
                manager->commit_prepared(changes);
                if (changed) ++document.revision;
                result.applied = applied_ops.size();
                result.ops.resize(result.applied);
                result.revision = document.revision;
                return result;
            }
        }
    } catch (const std::exception &error) {
        close_failed_batch();
        Error rejected{ErrorCode::EDIT_REJECTED, std::format("the engine failed: {}", error.what()),
                       context_.op_ < steps.size() ? std::optional(context_.op_) : std::nullopt};
        rejected.poisoned = document.poisoned;
        rejected.applied_ops = std::move(applied_ops);
        return std::unexpected(std::move(rejected));
    } catch (...) {
        close_failed_batch();
        throw;
    }
    close_failed_batch();
    failure->poisoned = document.poisoned;
    failure->applied_ops = std::move(applied_ops);
    return std::unexpected(std::move(*failure));
}
} // namespace api
} // namespace engrave
