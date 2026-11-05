// The rollback policy of every edit operation, one row each:
//
//   ENGRAVE_OP(<operation>, TRANSACTION | SNAPSHOT | UNEXPOSED)
//
// The rows follow the verdicts in engrave/tests/harness/op_evidence.json, and
// engrave_op_policy_test fails when they differ. An operation without evidence
// yet is SNAPSHOT. Each group of operations keeps to its own section.

// property (ops/property.cpp)
ENGRAVE_OP(SetProperty, TRANSACTION)
ENGRAVE_OP(ResetProperty, TRANSACTION)
ENGRAVE_OP(SetStyle, TRANSACTION)

// notes (ops/notes.cpp)
ENGRAVE_OP(EnterNotes, TRANSACTION)
ENGRAVE_OP(SetPitch, TRANSACTION)
ENGRAVE_OP(SetDuration, TRANSACTION)
ENGRAVE_OP(Remove, TRANSACTION)

// measures (ops/measures.cpp)
ENGRAVE_OP(InsertMeasures, SNAPSHOT)
ENGRAVE_OP(RemoveMeasures, SNAPSHOT)
ENGRAVE_OP(SetTimeSig, SNAPSHOT)
ENGRAVE_OP(SetKeySig, TRANSACTION)
ENGRAVE_OP(InsertClef, TRANSACTION)
ENGRAVE_OP(SetLayoutBreak, SNAPSHOT)

// parts (ops/parts.cpp)
ENGRAVE_OP(AddPart, TRANSACTION)
ENGRAVE_OP(RemovePart, TRANSACTION)
ENGRAVE_OP(AddStaff, SNAPSHOT)
ENGRAVE_OP(RemoveStaff, TRANSACTION)
// A snapshot, not a transaction: Excerpt::createExcerpt builds the excerpt's score as
// commands of the batch, and unwinding them frees the link lists and the elements twice, so
// the harness does not probe that path for this operation (tests/harness/harness.cpp). The
// undo of a committed batch leaves the excerpt's title frame detached from the score and
// registered, which C2 of the harness reports as `registered`.
ENGRAVE_OP(GenerateExcerpts, SNAPSHOT)

// spanners (ops/spanners.cpp)
ENGRAVE_OP(AddSpanner, TRANSACTION)
ENGRAVE_OP(AddTie, TRANSACTION)
ENGRAVE_OP(AddTuplet, TRANSACTION)

// text (ops/text.cpp)
ENGRAVE_OP(AddText, TRANSACTION)
ENGRAVE_OP(Add, TRANSACTION)
