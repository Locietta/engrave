// SPDX-License-Identifier: GPL-3.0-only
// Bounded, transactional DOM pitch editing for the live engraving score.
#include "score_edit.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "engraving/dom/chord.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/interval.h"
#include "engraving/dom/key.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/spannermap.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/editing/editnote.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/editing/transpose.h"
#include "engraving/infrastructure/eid.h"
#include "engraving/types/fraction.h"

namespace music {
using namespace mu::engraving;

namespace {

constexpr size_t max_pitch_changes = 256;

bool is_base64_char(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/'; }

EID parse_note_eid(const std::string &text) {
    if (text.empty() || text.size() > EID::max_str_size) throw std::invalid_argument("Malformed note id length");

    size_t separators = 0;
    for (const char c : text) {
        if (c == '_') {
            ++separators;
            continue;
        }
        if (!is_base64_char(c)) throw std::invalid_argument("Malformed note id characters");
    }
    if (separators != 1) throw std::invalid_argument("Malformed note id separator");
    const size_t separator = text.find('_');
    if (separator == std::string::npos || separator == 0 || separator + 1 == text.size())
        throw std::invalid_argument("Malformed note id fields");

    const EID eid = EID::from_std_string(text);
    // fromStdString accepts non-canonical base64 such as leading zero digits;
    // require an exact canonical round trip before it reaches the registry.
    if (!eid.is_valid() || eid.to_std_string() != text) throw std::invalid_argument("Malformed note id encoding");
    return eid;
}

bool find_part_index(const Score *score, const Part *part, size_t &index) {
    const std::vector<Part *> &parts = score->parts();
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] == part) {
            index = i;
            return true;
        }
    }
    return false;
}

std::vector<Measure *> standard_measures(Score *score) {
    std::vector<Measure *> measures;
    for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        measures.push_back(measure);
    }
    return measures;
}

bool chord_has_spanner_anchor(Chord *chord) {
    Score *score = chord ? chord->score() : nullptr;
    if (!score) return false;
    for (const auto &entry : score->spanner_map().map()) {
        Spanner *spanner = entry.second;
        if (!spanner) continue;
        const auto anchored = [&](EngravingItem *item) {
            if (!item) return false;
            if (item == chord) return true;
            return item->is_note() && to_note(item)->chord() == chord;
        };
        if (anchored(spanner->start_element()) || anchored(spanner->end_element())) return true;
    }
    return false;
}

void validate_editable_note(Note *note) {
    if (!note) throw std::runtime_error("Pitch edit target note is null");
    Chord *chord = note->chord();
    Staff *staff = note->staff();
    if (!chord) throw std::runtime_error("Pitch edit target note has no chord");
    if (!staff) throw std::runtime_error("Pitch edit target note has no staff");

    if (chord->is_grace() || note->is_grace()) throw std::runtime_error("Grace-note pitch edits are not supported");
    if (chord->staff_move() != 0) throw std::runtime_error("Cross-staff note pitch edits are not supported");
    if (note->tie_for() || note->tie_back()) throw std::runtime_error("Tied note pitch edits are not supported");
    if (!note->spanner_for().empty() || !note->spanner_back().empty())
        throw std::runtime_error("Spanned note pitch edits are not supported");
    if (chord_has_spanner_anchor(chord)) throw std::runtime_error("Chord with an attached spanner cannot be pitch-edited");

    const Fraction tick = chord->tick();
    if (staff->is_tab_staff(tick)) throw std::runtime_error("Tablature note pitch edits are not supported");
    if (staff->is_drum_staff(tick) || !staff->is_pitched_staff(tick))
        throw std::runtime_error("Drum or unpitched-staff note pitch edits are not supported");
}

void compute_spelling(Note *note, int pitch, int &tpc1, int &tpc2) {
    Chord *chord = note->chord();
    Staff *staff = note->staff();
    if (!chord || !staff) throw std::runtime_error("Cannot spell a note without chord and staff");

    const Fraction tick = chord->tick();
    const Key concert_key = staff->concert_key(tick);
    tpc1 = pitch2tpc(pitch, concert_key, Prefer::NEAREST);

    // Match Note::setTpcFromPitch: keep the written spelling by transposing the
    // concert tpc through the inverse instrument interval.
    Interval interval = staff->transpose(tick);
    tpc2 = tpc1;
    if (!interval.is_zero()) {
        interval.flip();
        tpc2 = Transpose::transpose_tpc(tpc1, interval, true);
    }
    if (!tpc_is_valid(tpc1) || !tpc_is_valid(tpc2)) throw std::runtime_error("Pitch edit produced an invalid tonal pitch class");
}

struct ValidatedChange {
    Note *note = nullptr;
    int pitch = 0;
    int tpc1 = 0;
    int tpc2 = 0;
    int old_pitch = 0;
    size_t part_index_value = 0;
    size_t measure_index = 0;
};

// Only $v depends on dirty(); upstream $r prints an empty revision while dirty,
// so a relayout cannot stale it.
bool uses_dirty_version_macro(const Score *score) {
    static constexpr Sid version_styles[] = {
        Sid::EVEN_HEADER_L, Sid::EVEN_HEADER_C, Sid::EVEN_HEADER_R, Sid::ODD_HEADER_L, Sid::ODD_HEADER_C, Sid::ODD_HEADER_R,
        Sid::EVEN_FOOTER_L, Sid::EVEN_FOOTER_C, Sid::EVEN_FOOTER_R, Sid::ODD_FOOTER_L, Sid::ODD_FOOTER_C, Sid::ODD_FOOTER_R,
    };
    for (const Sid sid : version_styles) {
        if (score->style().style_st(sid).contains(u"$v")) return true;
    }
    return false;
}

bool has_dirty_version_chrome(const MasterScore *score) {
    if (uses_dirty_version_macro(score)) return true;
    for (const Excerpt *excerpt : score->excerpts()) {
        const Score *part = excerpt->excerpt_score();
        if (part && uses_dirty_version_macro(part)) return true;
    }
    return false;
}

struct ScoreCommandGuard {
    explicit ScoreCommandGuard(MasterScore *score) : score_(score) {}

    ~ScoreCommandGuard() {
        if (active_) {
            try {
                score_->end_cmd(true, true);
            } catch (...) {}
        }
    }

    void begin() {
        MScore::set_error(MsError::MS_NO_ERROR);
        score_->start_cmd(TranslatableString::untranslatable("MCP pitch edit"));
        active_ = score_->undo_stack()->has_active_transaction();
        if (!active_) throw std::runtime_error("Failed to begin pitch edit");
    }

    void finish(bool rollback) {
        const bool was_dirty = score_->dirty();
        score_->end_cmd(rollback, true); // Headless excerpts are not marked open.
        active_ = false;
        // endCmd lays out the edited range before the undo macro marks the score
        // dirty, so $v page chrome would stay stale until the next layout.
        // Relayout once, on the clean -> dirty edge, and only when a header or
        // footer actually prints $v. Excerpts already on screen keep their pages
        // current too; a never-laid-out excerpt has no stale chrome to fix.
        if (!rollback && !was_dirty && score_->dirty() && has_dirty_version_chrome(score_)) {
            score_->do_layout();
            for (Excerpt *excerpt : score_->excerpts()) {
                Score *part = excerpt->excerpt_score();
                if (part && part->npages() > 0) part->do_layout();
            }
        }
    }
private:
    MasterScore *score_;
    bool active_ = false;
};

std::string engine_error_message() {
    std::string message = "Pitch edit was rejected by the engraving engine";
    if (MScore::error != MsError::MS_NO_ERROR) message += ": " + MScore::error_to_string(MScore::error);
    return message;
}

struct PreparedPitchEdit {
    std::vector<ValidatedChange> changes;
    NativePitchResult result;
};

PreparedPitchEdit prepare_score_pitch_edit(mu::engraving::MasterScore *score, const NativePitchEdit &edit) {
    if (!score) throw std::invalid_argument("A master score is required");
    if (edit.changes.empty()) throw std::invalid_argument("Pitch edit must contain at least one change");
    if (edit.changes.size() > ::music::max_pitch_changes) throw std::invalid_argument("Pitch edit exceeds the 256-change bound");
    if (score->read_only()) throw std::runtime_error("Score is read-only");
    UndoStack *stack = score->undo_stack();
    if (!stack) throw std::runtime_error("Score has no undo stack");
    if (stack->is_locked()) throw std::runtime_error("Score undo stack is locked");
    if (stack->has_active_transaction()) throw std::runtime_error("Score already has an active undo command");

    const std::vector<Measure *> measures = standard_measures(score);
    std::unordered_map<const Measure *, size_t> measure_indices;
    measure_indices.reserve(measures.size());
    for (size_t i = 0; i < measures.size(); ++i) measure_indices.emplace(measures[i], i);

    std::unordered_set<std::string> seen_ids;
    std::vector<EID> requested_eids;
    requested_eids.reserve(edit.changes.size());
    for (const PitchChange &request : edit.changes) {
        if (request.pitch < ::mu::engraving::min_pitch || request.pitch > ::mu::engraving::max_pitch)
            throw std::invalid_argument("Pitch must be in 0..127");
        if (!seen_ids.insert(request.note_id).second) throw std::invalid_argument("Pitch edit contains a duplicate note id");
        requested_eids.push_back(parse_note_eid(request.note_id));
    }

    // Resolve IDs by walking the live master-score DOM rather than dereferencing
    // the EID registry directly: the registry does not remove deleted items
    // automatically, so a stale entry must never become an edit target.
    const std::unordered_set<EID> wanted(requested_eids.begin(), requested_eids.end());
    std::unordered_map<EID, Note *> live_notes;
    for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
            for (track_idx_t track = 0; track < score->ntracks(); ++track) {
                EngravingItem *item = segment->element(track);
                if (!item || !item->is_chord()) continue;
                for (Note *note : to_chord(item)->notes()) {
                    if (!note) continue;
                    const EID eid = note->eid();
                    if (eid.is_valid() && wanted.count(eid)) live_notes.emplace(eid, note);
                }
            }
        }
    }

    std::vector<ValidatedChange> changes;
    changes.reserve(edit.changes.size());
    std::unordered_map<Note *, int> linked_targets;

    for (size_t i = 0; i < edit.changes.size(); ++i) {
        const PitchChange &request = edit.changes[i];
        const auto resolved = live_notes.find(requested_eids[i]);
        if (resolved == live_notes.end()) throw std::invalid_argument("Note id does not identify a live master-score note");
        Note *note = resolved->second;

        validate_editable_note(note);

        Chord *chord = note->chord();
        Part *part = note->staff()->part();
        size_t part_index = 0;
        if (!part || !find_part_index(score, part, part_index)) throw std::invalid_argument("Note does not belong to a master-score part");
        const auto measure_pos = measure_indices.find(chord->measure());
        if (measure_pos == measure_indices.end()) throw std::invalid_argument("Note measure is not in the master score");

        int tpc1 = 0;
        int tpc2 = 0;
        if (note->pitch() == request.pitch && tpc_is_valid(note->tpc1()) && tpc_is_valid(note->tpc2())) {
            // A same-pitch request is a true no-op, including the spelling.
            tpc1 = note->tpc1();
            tpc2 = note->tpc2();
        } else {
            compute_spelling(note, request.pitch, tpc1, tpc2);
        }

        // Linked notes are edited by Score::undoChangePitch as well. Validate
        // them now so an unsupported linked staff cannot fail mid-transaction.
        for (EngravingObject *linked : note->link_list()) {
            if (!linked || !linked->is_note()) throw std::runtime_error("Note link list contains a non-note object");
            Note *linked_note = to_note(linked);
            validate_editable_note(linked_note);
            if (linked_note->master_score() != score) throw std::runtime_error("Linked notes in another master score cannot be edited");
            const auto [target, inserted] = linked_targets.emplace(linked_note, request.pitch);
            if (!inserted && target->second != request.pitch) throw std::invalid_argument("Linked notes were assigned conflicting pitches");
            int linked_tpc1 = 0, linked_tpc2 = 0;
            compute_spelling(linked_note, request.pitch, linked_tpc1, linked_tpc2);
            if (note->pitch() != request.pitch && (linked_tpc1 != tpc1 || linked_tpc2 != tpc2))
                throw std::runtime_error("Linked notes have incompatible key or transposition state");
        }

        changes.push_back({note, request.pitch, tpc1, tpc2, note->pitch(), part_index, measure_pos->second});
    }

    // Footprints are full measures. Linked notes in the same master-score DOM
    // contribute their own measures; notes in another master region (an
    // excerpt) cannot be expressed in master measure coordinates and therefore
    // expand the footprint to the whole requested part.
    std::map<size_t, std::vector<std::pair<size_t, size_t>>> intervals;
    std::set<size_t> whole_parts;
    for (const ValidatedChange &change : changes) {
        intervals[change.part_index_value].push_back({change.measure_index, change.measure_index});
        Note *note = change.note;
        for (EngravingObject *linked : note->link_list()) {
            if (!linked || !linked->is_note()) {
                whole_parts.insert(change.part_index_value);
                continue;
            }
            Note *linked_note = to_note(linked);
            if (!linked_note->master_score() || linked_note->master_score() != score) {
                whole_parts.insert(change.part_index_value);
                continue;
            }
            if (linked_note->score() != score) {
                whole_parts.insert(change.part_index_value);
                continue;
            }
            Part *linked_part = linked_note->staff() ? linked_note->staff()->part() : nullptr;
            size_t linked_part_index = 0;
            if (!linked_part || !find_part_index(score, linked_part, linked_part_index)) {
                whole_parts.insert(change.part_index_value);
                continue;
            }
            const Chord *linked_chord = linked_note->chord();
            const auto linked_measure = linked_chord ? measure_indices.find(linked_chord->measure()) : measure_indices.end();
            if (linked_measure == measure_indices.end()) {
                whole_parts.insert(change.part_index_value);
                continue;
            }
            intervals[linked_part_index].push_back({linked_measure->second, linked_measure->second});
        }
    }

    NativePitchResult result;
    result.changed_notes = 0;
    result.requires_stable_structure = true;
    for (const auto &[part_index, ranges] : intervals) {
        if (whole_parts.count(part_index)) continue;
        std::vector<std::pair<size_t, size_t>> merged;
        std::vector<std::pair<size_t, size_t>> sorted = ranges;
        std::sort(sorted.begin(), sorted.end());
        for (const auto &range : sorted) {
            if (merged.empty() || range.first > merged.back().second + 1) {
                merged.push_back(range);
            } else if (range.second > merged.back().second) {
                merged.back().second = range.second;
            }
        }
        for (const auto &range : merged) {
            result.write_scopes.push_back({part_index, range.first, range.second - range.first + 1, false});
        }
    }
    for (const size_t part_index : whole_parts) {
        result.write_scopes.push_back({part_index, 0, measures.size(), true});
    }
    std::sort(result.write_scopes.begin(), result.write_scopes.end(), [](const ScoreEditScope &left, const ScoreEditScope &right) {
        if (left.part_index != right.part_index) return left.part_index < right.part_index;
        if (left.whole_part != right.whole_part) return left.whole_part < right.whole_part;
        return left.first_measure < right.first_measure;
    });
    result.read_scopes = result.write_scopes;

    for (const ValidatedChange &change : changes) {
        if (change.old_pitch != change.pitch) ++result.changed_notes;
    }

    return {std::move(changes), std::move(result)};
}
} // namespace

NativePitchResult inspect_score_pitch_edit(mu::engraving::MasterScore *score, const NativePitchEdit &edit) {
    return prepare_score_pitch_edit(score, edit).result;
}

namespace {
NativeEditTestFault test_fault = NativeEditTestFault::NONE;
} // namespace

void set_native_edit_test_fault(NativeEditTestFault fault) { test_fault = fault; }

NativePitchResult apply_score_pitch_edit(mu::engraving::MasterScore *score, const NativePitchEdit &edit) {
    auto prepared = prepare_score_pitch_edit(score, edit);
    auto &changes = prepared.changes;
    auto &result = prepared.result;
    if (result.changed_notes == 0) return result;
    auto *stack = score->undo_stack();
    const size_t before_index = stack->current_index();
    ScoreCommandGuard command(score);
    try {
        command.begin();
        size_t applied = 0;
        for (const auto &change : changes) {
            if (change.old_pitch == change.pitch) continue;
            EditNote::undo_change_pitch(score, change.note, change.pitch, change.tpc1, change.tpc2);
            ++applied;
            if (applied == 1 && test_fault != NativeEditTestFault::NONE) {
                const auto fault = test_fault;
                test_fault = NativeEditTestFault::NONE;
                if (fault == NativeEditTestFault::EXCEPTION_AFTER_FIRST) throw std::runtime_error("Injected pitch transaction exception");
                MScore::set_error(MsError::NO_NOTE_SELECTED);
            }
        }
        const bool engine_failed = MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning;
        const auto message = engine_failed ? engine_error_message() : std::string{};
        command.finish(engine_failed);
        if (engine_failed) {
            bool restored = !stack->has_active_transaction() && stack->current_index() == before_index;
            for (const auto &change : changes) restored = restored && change.note->pitch() == change.old_pitch;
            throw NativeEditFailure(message, !restored);
        }
        bool committed = !stack->has_active_transaction() && stack->current_index() == before_index + 1 &&
                         (MScore::error == MsError::MS_NO_ERROR || MScore::error_is_warning);
        for (const auto &change : changes) committed = committed && change.note->pitch() == change.pitch;
        if (!committed) throw NativeEditFailure("Engraving transaction did not commit consistently", true);
    } catch (const NativeEditFailure &) {
        throw;
    } catch (const std::exception &e) {
        // The guard attempts to close/unwind the macro. A partial C++ layout
        // failure is still poisoned: successful unwinding cannot prove safety.
        throw NativeEditFailure(e.what(), true);
    } catch (...) {
        throw NativeEditFailure("Unknown engraving transaction failure", true);
    }
    return result;
}

bool cap_native_edit_history(mu::engraving::MasterScore *score, size_t maximum) noexcept {
    try {
        if (!score || !score->undo_stack()) return false;
        auto *stack = score->undo_stack();
        return stack->size() <= maximum || stack->clear_history();
    } catch (...) {
        return false;
    }
}
} // namespace music
