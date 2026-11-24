#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "engraving/infrastructure/sourcehandle.h"
#include "global/unique_fn.h"
#include <engrave/document.h>
#include <engrave/edit.h>
#include <engrave/error.h>

namespace mu::engraving {
struct EngravingObject;
struct MasterScore;
struct Measure;
struct Score;
struct Segment;
} // namespace mu::engraving

namespace engrave::api {

enum struct Selector { NONE, MEASURE, STAFF, POSITION };

// Only EIDs can supply a preflight object. The other references depend on
// what preceding operations leave in the score.
struct Target {
    std::variant<ElementId, OpRef, SegmentAddress, MeasureIndex, StaffIndex, RelativePosition> ref;
    std::string_view field;
    mu::engraving::EngravingObject *object = nullptr;
    std::size_t epoch = 0;
    mu::engraving::SourceHandle handle;
};

struct Position {
    SegmentAddress address;
    mu::engraving::Measure *measure;
};

struct BatchGuard;

struct EditContext {
    explicit EditContext(mu::engraving::MasterScore &master) : master_(master) {}

    mu::engraving::MasterScore &master() const { return master_; }

    // The operation being prepared or run.
    std::size_t op() const { return op_; }

    Result<Target> target(const Ref &ref, std::string_view field, Selector selector = Selector::NONE) const;
    Result<Target> position_target(const EditAddress &at, bool chord_rest_only) const;
    Result<Target> position_target(const EntryAddress &at) const;
    Result<Position> position(const Target &target) const;
    Result<mu::engraving::Segment *> segment(const Position &position, const Target &target) const;
    // While running: the live element. A target is resolved again when an earlier
    // operation changed more than properties, since it may have removed it.
    Result<mu::engraving::EngravingObject *> object(const Target &target) const;
    Result<ScoreSel> style_selection(const std::optional<ScoreSel> &selection) const;
    Result<mu::engraving::Score *> score(const ScoreSel &selection) const;

    // Records an element the running operation created, for OpRef and OpResult.
    void created(mu::engraving::EngravingObject *object);
    // Records a note whose pitch the running operation changed; its ties are
    // checked when the batch commits.
    void repitched(mu::engraving::EngravingObject &note);
    // Explicit semantic dependency barrier. Invalidates cached targets.
    void prepare_semantics();

private:
    friend struct BatchGuard;

    Result<mu::engraving::Measure *> measure_at(MeasureIndex index) const;
    Result<void> check_scope(const mu::engraving::EngravingObject &object) const;

    mu::engraving::MasterScore &master_;
    std::optional<ScoreSel> scope_;
    mu::engraving::Score *scoped_score_ = nullptr;
    std::size_t op_ = 0;
    std::size_t epoch_ = 0; // advanced by every operation that changed more than properties
    std::vector<std::vector<mu::engraving::SourceHandle>> created_;
    std::vector<mu::engraving::SourceHandle> repitched_;
};

// Applies an operation after all independent inputs have been prepared.
struct Step {
    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, Step> && std::invocable<F &, EditContext &>)
    Step(F &&fn) : apply_(std::forward<F>(fn)) {}

    Result<void> operator()(EditContext &context) { return apply_(context); }

private:
    muse::UniqueFn<Result<void>(EditContext &)> apply_;
};

Result<Step> prepare(EditContext &context, const SetProperty &op);
Result<Step> prepare(EditContext &context, const ResetProperty &op);
Result<Step> prepare(EditContext &context, const SetStyle &op);
Result<Step> prepare(EditContext &context, const EnterNotes &op);
Result<Step> prepare(EditContext &context, const AddGraceChord &op);
Result<Step> prepare(EditContext &context, const EnterDrums &op);
Result<Step> prepare(EditContext &context, const SetPitch &op);
Result<Step> prepare(EditContext &context, const SetDuration &op);
Result<Step> prepare(EditContext &context, const Remove &op);
Result<Step> prepare(EditContext &context, const InsertMeasures &op);
Result<Step> prepare(EditContext &context, const RemoveMeasures &op);
Result<Step> prepare(EditContext &context, const SetTimeSig &op);
Result<Step> prepare(EditContext &context, const SetKeySig &op);
Result<Step> prepare(EditContext &context, const InsertClef &op);
Result<Step> prepare(EditContext &context, const SetLayoutBreak &op);
Result<Step> prepare(EditContext &context, const AddPart &op);
Result<Step> prepare(EditContext &context, const RemovePart &op);
Result<Step> prepare(EditContext &context, const AddStaff &op);
Result<Step> prepare(EditContext &context, const RemoveStaff &op);
Result<Step> prepare(EditContext &context, const GenerateExcerpts &op);
Result<Step> prepare(EditContext &context, const AddSpanner &op);
Result<Step> prepare(EditContext &context, const AddSpannerToBarEnd &op);
Result<Step> prepare(EditContext &context, const AddVolta &op);
Result<Step> prepare(EditContext &context, const SetRepeatBarlines &op);
Result<Step> prepare(EditContext &context, const AddTie &op);
Result<Step> prepare(EditContext &context, const AddTuplet &op);
Result<Step> prepare(EditContext &context, const AddText &op);
Result<Step> prepare(EditContext &context, const Add &op);

// Owns references to the current score throughout a single edit transaction.
struct BatchGuard {
    explicit BatchGuard(Document &document);

    Result<EditResult> run(std::vector<Step> steps);

private:
    friend struct ::engrave::Document;

    Result<void> admit(std::optional<std::uint64_t> expected_revision) const;
    Result<std::vector<Step>> prepare_batch(const Batch &batch);
    Result<EditResult> execute(std::vector<Step> steps, EditMode mode);

    Document &document_;
    EditContext context_;
};

} // namespace engrave::api
