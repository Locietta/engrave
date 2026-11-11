/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "scoreautomationcontroller.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "engraving/automation/automationdata.h"
#include "engraving/automation/dynamicvalues.h"
#include "engraving/automation/tempovalues.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/gradualtempochange.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/part.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/volta.h"
#include "engraving/types/constants.h"
#include "global/containers.h"
#include "global/log.h"
#include "global/realfn.h"

using namespace mu::engraving;

static constexpr real_t dynamic_step = real_t::make(0.05);

// Normalized [0.0, 1.0] dynamic level used when no dynamic precedes a hairpin start ("natural").
static constexpr real_t natural_normalized_dynamic = real_t::make(0.5);

// Dynamics, measure repeats, fermatas, and tempo texts can only appear on these segment types
static constexpr SegmentType relevant_segment_types = SegmentType::CHORD_REST | SegmentType::TIME_TICK;

static BeatsPerSecond round_tempo(const BeatsPerSecond &bps) { return muse::real_round(bps.val, Constants::tempo_precision); }

static AutomationPoint make_tempo_point(real_t normalized_bps, std::optional<EID> item_id) {
    AutomationPoint point;
    point.value.out_value = std::clamp(normalized_bps, ::mu::engraving::min_normalized_tempo, ::mu::engraving::max_normalized_tempo);
    point.item_id = item_id;
    point.generated = true;
    return point;
}

//! TODO: map EXPONENTIAL/EASE_IN/EASE_OUT/EASE_IN_OUT to a real ease shape; every method is linear for now
static AutomationPoint::Ease change_method_to_ease(ChangeMethod) { return AutomationPoint::Ease::none(); }

//! NOTE: segments are ordered by utick; returns the index of the last one starting at or before utick
static std::optional<size_t> find_segment_index(const std::vector<RepeatSegmentInfo> &segments, utick_t utick) {
    const auto it =
        std::upper_bound(segments.begin(), segments.end(), utick, [](utick_t u, const RepeatSegmentInfo &seg) { return u < seg.utick; });
    if (it == segments.begin()) {
        return std::nullopt;
    }

    return size_t(std::prev(it) - segments.begin());
}

ScoreAutomationController::StaffRange::StaffRange(const Score *score, staff_idx_t staff_idx_from, staff_idx_t staff_idx_to) {
    const staff_idx_t last_staff_idx = score->nstaves() - 1;

    if (staff_idx_from == muse::nidx) {
        from = 0;
        to = last_staff_idx;
    } else {
        from = staff_idx_from;
        to = std::min(staff_idx_to, last_staff_idx);
    }

    is_full = (from == 0 && to == last_staff_idx);

    if (!is_full) {
        for (staff_idx_t staff_idx = from; staff_idx <= to; ++staff_idx) {
            if (const Staff *staff = score->staff(staff_idx)) {
                staff_ids.insert(staff->id());
            }
        }
    }
}

bool ScoreAutomationController::StaffRange::contains(staff_idx_t staff_idx) const {
    return is_full || (staff_idx >= from && staff_idx <= to);
}

bool ScoreAutomationController::StaffRange::contains(const muse::ID &staff_id) const {
    return is_full || muse::contains(staff_ids, staff_id);
}

static std::optional<real_t> dynamic_value(DynamicType type, bool start_value) {
    if (auto it = ::mu::engraving::ordinary_dynamic_values.find(type); it != ::mu::engraving::ordinary_dynamic_values.end()) {
        return it->second;
    }

    if (auto it = ::mu::engraving::single_note_dynamic_values.find(type); it != ::mu::engraving::single_note_dynamic_values.end()) {
        return it->second;
    }

    if (auto it = ::mu::engraving::compound_dynamic_values.find(type); it != ::mu::engraving::compound_dynamic_values.end()) {
        return start_value ? it->second.first : it->second.second;
    }

    return std::nullopt;
}

static std::optional<real_t> start_hairpin_value(const Hairpin *hairpin) {
    const DynamicType type = hairpin->dynamic_type_from();
    if (type == DynamicType::OTHER) {
        return std::nullopt;
    }

    return dynamic_value(type, false);
}

static DynamicType find_end_dynamic_type(const Hairpin *hairpin) {
    const DynamicType text_type = hairpin->dynamic_type_to();
    if (text_type != DynamicType::OTHER) {
        return text_type;
    }

    if (hairpin->spanner_segments().empty()) {
        const Segment *end_segment = hairpin->end_segment();
        if (!end_segment) {
            return DynamicType::OTHER;
        }

        const track_idx_t track_idx = hairpin->track();
        const std::vector<EngravingItem *> dynamics = end_segment->find_annotations(ElementType::DYNAMIC, track_idx, track_idx);
        for (const EngravingItem *item : dynamics) {
            if (item && item->is_dynamic() && to_dynamic(item)->play_dynamic()) {
                return to_dynamic(item)->dynamic_type();
            }
        }

        return DynamicType::OTHER;
    }

    const LineSegment *seg = hairpin->back_segment();
    if (!seg) {
        return DynamicType::OTHER;
    }

    // Optimization: first check if there is a cached dynamic
    const EngravingItem *snapped_item = seg->ldata()->item_snapped_after();
    if (!snapped_item || !snapped_item->is_dynamic() || !to_dynamic(snapped_item)->play_dynamic()) {
        snapped_item = to_hairpin_segment(seg)->find_element_to_snap_after(false /*ignoreInvisible*/, true /*requirePlayable*/);
        if (!snapped_item || !snapped_item->is_dynamic() || !to_dynamic(snapped_item)->play_dynamic()) {
            return DynamicType::OTHER;
        }
    }

    return to_dynamic(snapped_item)->dynamic_type();
}

static std::optional<real_t> end_hairpin_value(const Hairpin *hairpin) {
    const DynamicType type = find_end_dynamic_type(hairpin);
    if (type == DynamicType::OTHER) {
        return std::nullopt;
    }

    return dynamic_value(type, true);
}

static int dynamic_priority(const EngravingItem *item) {
    return static_cast<int>(item->get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>());
}

static const AutomationPoint *active_point(const AutomationCurveMap &curves, const AutomationCurveKey &key, utick_t tick) {
    static const AutomationCurve no_curve;

    const auto key_curve_it = curves.find(key);
    const AutomationCurve &key_curve = key_curve_it != curves.end() ? key_curve_it->second : no_curve;
    const auto key_it = muse::find_less_or_equal(key_curve, tick);

    if (key.voice_idx().has_value()) {
        const AutomationCurveKey shared_key = key.without_voice();
        const auto shared_curve_it = curves.find(shared_key);
        const AutomationCurve &shared_curve = shared_curve_it != curves.end() ? shared_curve_it->second : no_curve;
        const auto shared_it = muse::find_less_or_equal(shared_curve, tick);

        if (shared_it != shared_curve.cend()) {
            if (key_it == key_curve.cend() || shared_it->first > key_it->first) {
                return &shared_it->second;
            }
        }
    }

    return key_it != key_curve.cend() ? &key_it->second : nullptr;
}

static real_t current_tempo_at(const AutomationCurve &tempo_curve, utick_t tick) {
    const auto it = muse::find_less_or_equal(tempo_curve, tick);
    return it != tempo_curve.cend() ? it->second.value.out_value : ::mu::engraving::default_normalized_tempo;
}

struct ChangeRelevance {
    bool dynamics = false;
    bool tempo = false;
};

static ChangeRelevance classify_changes(const ScoreChanges &changes) {
    ChangeRelevance result;

    for (const ElementType type : changes.changed_types) {
        switch (type) {
            case ElementType::DYNAMIC:
            case ElementType::HAIRPIN:
            case ElementType::HAIRPIN_SEGMENT:
            case ElementType::MEASURE_REPEAT: result.dynamics = true; break;
            case ElementType::BREATH:
            case ElementType::LAYOUT_BREAK:
            case ElementType::FERMATA:
            case ElementType::GRADUAL_TEMPO_CHANGE:
            case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
            case ElementType::TEMPO_TEXT: result.tempo = true; break;
            case ElementType::VOLTA:
            case ElementType::VOLTA_SEGMENT:
            case ElementType::MARKER:
            case ElementType::JUMP: return {true, true};
            default: break;
        }

        if (result.dynamics && result.tempo) {
            break;
        }
    }

    // VoiceAssignment change shifts which staves a dynamic/hairpin covers,
    // so the old points on staves outside the new assignment must also be cleared
    result.dynamics |= muse::contains(changes.changed_property_id_set, Pid::VOICE_ASSIGNMENT);

    static const std::unordered_set<Pid> repeat_properties{
        Pid::REPEAT_START,
        Pid::REPEAT_END,
        Pid::REPEAT_JUMP,
        Pid::REPEAT_COUNT,
    };

    bool repeat_property_changed = false;
    for (const Pid pid : repeat_properties) {
        if (muse::contains(changes.changed_property_id_set, pid)) {
            repeat_property_changed = true;
            break;
        }
    }

    if (repeat_property_changed) {
        result.dynamics = true;
        result.tempo = true;
    }

    return result;
}

const TempoTimeline &ScoreAutomationController::tempo_timeline(bool expand_repeats) const {
    if (tempo_timeline_override_) {
        return *tempo_timeline_override_;
    }

    return expand_repeats ? tempo_timeline_ : flattened_tempo_timeline_;
}

void ScoreAutomationController::set_tempo_multiplier(const BeatsPerSecond &bps) {
    tempo_timeline_.set_tempo_multiplier(bps);
    flattened_tempo_timeline_.set_tempo_multiplier(bps);
}

void ScoreAutomationController::set_tempo_timeline_override(std::optional<TempoTimeline> timeline) {
    tempo_timeline_override_ = std::move(timeline);
}

void ScoreAutomationController::init(Score *score) {
    score_ = score;

    if (!automation_data_) {
        automation_data_ = std::make_shared<AutomationData>();
    }

    AutomationCurveMap curves = automation_data_->curves();
    full_rebuild(curves);
}

void ScoreAutomationController::full_rebuild(AutomationCurveMap &curves, bool include_tempo, bool include_dynamics) {
    const UpdateRequest request{0, muse::nidx, muse::nidx, include_tempo, include_dynamics};
    update(request, curves);
}

void ScoreAutomationController::ensure_initialized(Score *score) {
    if (score_) {
        return;
    }
    init(score);
}

void ScoreAutomationController::insert_time(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &old_segments) {

    const int diff = len.ticks();
    if (!score_ || !automation_data_ || automation_data_->is_empty() || diff == 0) {
        return;
    }

    const int raw_insert_tick = tick.ticks();
    const std::vector<RepeatSegmentInfo> new_segments = score_->expanded_repeat_list().segment_info_list();

    AutomationCurveMap curves = automation_data_->curves();

    for (auto &entry : curves) {
        AutomationCurve &curve = entry.second;
        AutomationCurve shifted;

        for (const auto &[utick, point] : curve) {
            // Generated points are regenerated by the full update below - only authored points need relocating
            if (point.generated) {
                continue;
            }

            // Re-express the point in raw-tick coordinates local to its repeat pass,
            // so the shift below is correct even for a pass other than the first
            const std::optional<size_t> old_idx = find_segment_index(old_segments, utick);
            if (!old_idx || *old_idx >= new_segments.size()) {
                continue; // no corresponding pass anymore (edit changed the repeat structure itself)
            }

            const RepeatSegmentInfo &old_seg = old_segments[*old_idx];
            const int old_raw_tick = utick - (old_seg.utick - old_seg.tick);

            if (diff < 0 && old_raw_tick >= raw_insert_tick && old_raw_tick < raw_insert_tick - diff) {
                continue; // point sat inside the removed range
            }

            const int new_raw_tick = old_raw_tick >= raw_insert_tick ? old_raw_tick + diff : old_raw_tick;

            // Re-derive the utick for the *same* repeat pass under the new (post-edit) RepeatList
            const RepeatSegmentInfo &new_seg = new_segments[*old_idx];
            const utick_t new_utick = utick_t(new_raw_tick + (new_seg.utick - new_seg.tick));

            shifted.emplace_hint(shifted.end(), new_utick, point);
        }

        curve = std::move(shifted);
    }

    // Kept as empty entries, not erased: replaceCurves() only clears a key when given it with an
    // empty curve - a key absent from the map below would leave stale data untouched instead
    full_rebuild(curves);
}

void ScoreAutomationController::update(const ScoreChanges &changes) {
    if (changes.is_text_editing) {
        return;
    }

    const ChangeRelevance relevance = classify_changes(changes);
    if (!relevance.dynamics && !relevance.tempo) {
        return;
    }

    UpdateRequest request;
    request.include_tempo = relevance.tempo;
    request.include_dynamics = relevance.dynamics;

    // When tempo is relevant, the whole score must be reparsed in one full pass,
    // so tickFrom/staffIdxFrom/staffIdxTo are left at their defaults (0/nidx/nidx)
    if (!relevance.tempo) {
        request.tick_from = std::max(changes.tick_from, 0);

        // VoiceAssignment change shifts which staves a dynamic/hairpin covers,
        // so the old points on staves outside the new assignment must also be cleared
        const bool voice_assignment_changed = muse::contains(changes.changed_property_id_set, Pid::VOICE_ASSIGNMENT);
        if (!voice_assignment_changed) {
            request.staff_idx_from = changes.staff_idx_from;
            request.staff_idx_to = changes.staff_idx_to;
        }
    }

    if (!automation_data_) {
        automation_data_ = std::make_shared<AutomationData>();
    }

    update(request, automation_data_->curves());
}

void ScoreAutomationController::edit_points(const AutomationCurveKey &key, AutomationPointEdits &edits) {
    IF_ASSERT_FAILED(automation_data_ && score_) { return; }

    // Tempo edits can affect later relative tempo markings, so we need a full rescan
    if (key.type == AutomationType::TEMPO) {
        AutomationCurveMap curves = automation_data_->curves();
        AutomationCurve &tempo_curve = curves[key];

        for (const AutomationPointEdit &edit : edits) {
            if (const auto *set_point = std::get_if<AutomationPointEdit::SetPoint>(&edit.change)) {
                tempo_curve[edit.tick] = set_point->point;
            } else if (const auto *move_point = std::get_if<AutomationPointEdit::MovePoint>(&edit.change)) {
                if (move_point->from != edit.tick) {
                    tempo_curve.erase(move_point->from);
                }
                tempo_curve[edit.tick] = move_point->point;
            } else {
                tempo_curve.erase(edit.tick);
            }
        }

        full_rebuild(curves, /*includeTempo*/ true, /*includeDynamics*/ false);
        return;
    }

    // Dynamics/Volume/Pan edits don't affect other points, so no rescan is needed
    mirror_edits_to_repeats(key, edits);
    automation_data_->edit_points(key, edits);
}

void ScoreAutomationController::update(const UpdateRequest &request, const AutomationCurveMap &curves) {

    if (!score_) {
        return;
    }

    const RepeatList &repeat_list = score_->expanded_repeat_list();
    if (repeat_list.empty()) {
        return;
    }

    const auto repeat_from_it =
        std::find_if(repeat_list.cbegin(), repeat_list.cend(),
                     [tick_from = request.tick_from](const RepeatSegment *seg) { return seg->end_tick() > tick_from; });

    IF_ASSERT_FAILED(repeat_from_it != repeat_list.cend()) { return; }

    const StaffRange range(score_, request.staff_idx_from, request.staff_idx_to);
    const RepeatSegment *first_seg = *repeat_from_it;

    UpdateContext ctx;
    ctx.request = request;
    ctx.tempo_multiplier = tempo_timeline_.tempo_multiplier();

    // The first utick the rebuild will regenerate; everything before it is kept as is
    ctx.clear_from_u_tick = std::max(first_seg->utick, request.tick_from + (first_seg->utick - first_seg->tick));

    // Copy only the curves this update can affect, dropping the generated points that the rebuild
    // below re-creates; all other curves stay untouched
    copy_curves_for_rebuild(curves, range, ctx.clear_from_u_tick, request.include_tempo, request.include_dynamics, ctx.curves);

    AutomationCurve empty_tempo_curve;

    if (request.include_tempo) {
        ctx.tempo_curve = &ctx.curves[::mu::engraving::tempo_key];
        fill_no_repeat_tempo_curve(*ctx.tempo_curve, ctx.no_repeat_tempo_curve);
    } else {
        // Read-only access for e.g. compound dynamics,
        // which need the current tempo even when this update doesn't rebuild it
        const auto tempo_it = ctx.curves.find(::mu::engraving::tempo_key);
        ctx.tempo_curve = tempo_it != ctx.curves.end() ? &tempo_it->second : &empty_tempo_curve;
    }

    // Step 1: segment dynamics (+ tempo/pause when relevant): populates ctx.dynamicPriorities
    int prev_seg_raw_end_tick = -1;
    for (auto it = repeat_from_it; it != repeat_list.cend(); ++it) {
        const RepeatSegment *seg = *it;
        const int tick_offset = seg->utick - seg->tick;

        if (request.include_tempo && prev_seg_raw_end_tick != -1 && seg->tick != prev_seg_raw_end_tick) {
            // Playback jumped elsewhere in the score (repeat loop-back / volta switch) - this
            // pass's start needs its tempo reset from raw tick order; resolved after Step 2, once
            // any volta reset affecting the lookup has landed in noRepeatTempoCurve
            ctx.pending_tempo_resets.push_back({seg->utick, seg->tick});
        }
        prev_seg_raw_end_tick = seg->end_tick();

        // tickFrom limits only the first affected repeat segment: later segments may replay
        // measures from before tickFrom, and all their points were cleared, so rebuild them in full
        const int measure_from = (it == repeat_from_it) ? std::max(seg->tick, request.tick_from) : seg->tick;

        const std::vector<const Measure *> &seg_measures = seg->measure_list();
        for (size_t mi = 0; mi < seg_measures.size(); ++mi) {
            const Measure *measure = seg_measures[mi];
            if (measure->end_tick().ticks() <= measure_from) {
                continue;
            }

            if (request.include_tempo) {
                collect_pauses(measure, tick_offset, ctx.pauses, ctx.no_repeat_pauses);
                if (measure->is_anacrusis()) {
                    std::optional<utick_t> next_measure_u_tick;
                    if (mi + 1 < seg_measures.size()) {
                        next_measure_u_tick = seg_measures[mi + 1]->tick().ticks() + tick_offset;
                    }
                    ctx.anacrusis_measures.push_back({measure, tick_offset, next_measure_u_tick});
                }
            }

            // A MeasureRepeat can only ever sit on the measure's first ChordRest segment
            const Segment *first_chord_rest_segment = measure->first(SegmentType::CHORD_REST);
            if (first_chord_rest_segment && first_chord_rest_segment->tick().ticks() >= measure_from) {
                collect_measure_repeats(first_chord_rest_segment, tick_offset, range, ctx.measure_repeats);
            }

            for (const Segment *segment = measure->first(::relevant_segment_types); segment;
                 segment = segment->next(::relevant_segment_types)) {
                if (segment->annotations().empty() || segment->tick().ticks() < measure_from) {
                    continue;
                }

                add_segment_points(segment, tick_offset, range, ctx);
            }
        }
    }

    if (!ctx.anacrusis_measures.empty()) {
        fix_anacrusis_tempo(ctx);
    }

    // Step 2: spanner points (+ gradual tempo change when relevant):
    // ctx.dynamicPriorities fully populated; sets inValues on hairpin-end dynamics
    for (auto it = repeat_from_it; it != repeat_list.cend(); ++it) {
        const RepeatSegment *seg = *it;
        add_spanner_points(score_, seg->tick, seg->end_tick(), seg->utick - seg->tick, range, ctx);
    }

    // Step 3: resolve pending tempo resets now that noRepeatTempoCurve is fully built
    resolve_pending_tempo_resets(ctx);

    // Step 4: fill each voice curve with any points from the base (all-voice) curve it doesn't already have
    fill_voice_curves_from_base(ctx);

    // Step 5: measure repeats
    add_measure_repeat_points(ctx);

    // Step 6: Mirror directly-authored points to repeats
    mirror_authored_points_to_repeats(ctx);

    if (request.include_tempo) {
        // TempoTimeline falls back to the default tempo when the curve has no points
        tempo_timeline_.rebuild(*ctx.tempo_curve, ctx.pauses);
        flattened_tempo_timeline_.rebuild(ctx.no_repeat_tempo_curve, ctx.no_repeat_pauses);
    }

    // Passed through as-is: replaceCurves() clears a key when given an empty curve for it
    automation_data_->replace_curves(ctx.curves);
}

void ScoreAutomationController::fill_no_repeat_tempo_curve(const AutomationCurve &tempo_curve, AutomationCurve &no_repeat_tempo_curve) {

    const RepeatList &repeat_list = score_->expanded_repeat_list();

    // tempoCurve is expected to be authored-only
    for (const auto &[utick, point] : tempo_curve) {
        const auto seg_it = repeat_list.find_repeat_segment_from_u_tick(utick);
        IF_ASSERT_FAILED(seg_it != repeat_list.cend()) { continue; }

        const int seg_tick_offset = (*seg_it)->utick - (*seg_it)->tick;
        no_repeat_tempo_curve.emplace(utick - seg_tick_offset, point);
    }
}

void ScoreAutomationController::mirror_authored_points_to_repeats(UpdateContext &ctx) {

    for (auto &[key, curve] : ctx.curves) {
        if (curve.empty()) {
            continue;
        }

        AutomationPointEdits edits;
        for (const auto &[tick, point] : curve) {
            if (!point.generated) {
                edits.push_back({tick, AutomationPointEdit::SetPoint{point}});
            }
        }

        if (edits.empty()) {
            continue;
        }

        //! NOTE: appends the corresponding mirrored edit(s) - one per other repeat pass - directly to edits
        mirror_edits_to_repeats(key, edits);

        for (const AutomationPointEdit &edit : edits) {
            const auto *set_point = std::get_if<AutomationPointEdit::SetPoint>(&edit.change);
            IF_ASSERT_FAILED(set_point) { continue; }

            curve.insert_or_assign(edit.tick, set_point->point);
        }
    }
}

void ScoreAutomationController::copy_curves_for_rebuild(const AutomationCurveMap &curves, const StaffRange &range,
                                                        utick_t clear_from_u_tick, bool include_tempo, bool include_dynamics,
                                                        AutomationCurveMap &dest_curves) {

    for (const auto &[key, curve] : curves) {
        const bool is_dynamics = key.type == AutomationType::DYNAMICS;
        const bool is_tempo = key.type == AutomationType::TEMPO;

        // Instrument curves (Volume, Pan) aren't staff-scoped or score-derived, so only
        // user points survive here - mirrorAuthoredPointsToRepeats() recomputes the rest
        if (is_dynamics) {
            if (!include_dynamics) {
                continue;
            }

            const std::optional<muse::ID> staff_id = key.staff_id();
            if (!staff_id || !range.contains(*staff_id)) {
                continue;
            }
        }

        AutomationCurve &curve_copy = dest_curves.emplace_hint(dest_curves.end(), key, AutomationCurve())->second;

        for (const auto &[tick, point] : curve) {
            // Dynamics: keep generated points before clearFromUTick (the incremental rebuild regenerates the rest)
            // Tempo: keep generated points only if this update doesn't include tempo (otherwise the whole curve is regenerated below)
            // Instrument: never keep generated points (there's no generation step for them, only user edits)
            const bool keep_generated = is_dynamics ? tick < clear_from_u_tick : is_tempo && !include_tempo;
            if (!point.generated || keep_generated) {
                curve_copy.emplace_hint(curve_copy.end(), tick, point);
            }
        }
    }
}

void ScoreAutomationController::add_segment_points(const Segment *segment, int tick_offset, const StaffRange &range, UpdateContext &ctx) {

    double fermata_stretch = 0.0;
    const Fermata *fermata = nullptr;

    for (const EngravingItem *annotation : segment->annotations()) {
        if (!annotation) {
            continue;
        }

        if (annotation->is_dynamic()) {
            if (ctx.request.include_dynamics && range.contains(annotation->staff_idx())) {
                add_dynamic_points(to_dynamic(annotation), tick_offset, range, ctx);
            }
        } else if (ctx.request.include_tempo && annotation->is_fermata() && to_fermata(annotation)->play()) {
            const Fermata *f = to_fermata(annotation);
            if (f->time_stretch() > fermata_stretch) {
                fermata_stretch = f->time_stretch();
                fermata = f;
            }
        } else if (ctx.request.include_tempo && annotation->is_tempo_text()) {
            add_tempo_text_point(to_tempo_text(annotation), tick_offset, ctx);
        }
    }

    if (fermata && !muse::real_is_null(fermata_stretch) && !muse::real_is_equal(fermata_stretch, 1.0)) {
        add_fermata_stretch_points(fermata, tick_offset, fermata_stretch, ctx);
    }
}

void ScoreAutomationController::add_dynamic_points(const Dynamic *dynamic, int tick_offset, const StaffRange &range, UpdateContext &ctx) {

    if (!dynamic->play_dynamic()) {
        return;
    }

    const DynamicType dynamic_type = dynamic->dynamic_type();
    const utick_t tick = dynamic->tick().ticks() + tick_offset;

    DynamicInfo info;
    info.tick = tick;

    if (auto ordinary_it = ::mu::engraving::ordinary_dynamic_values.find(dynamic_type);
        ordinary_it != ::mu::engraving::ordinary_dynamic_values.end()) {
        info.kind = DynamicInfo::Ordinary{ordinary_it->second};
    } else if (auto single_note_it = ::mu::engraving::single_note_dynamic_values.find(dynamic_type);
               single_note_it != ::mu::engraving::single_note_dynamic_values.end()) {
        DynamicInfo::SingleNote single_note{single_note_it->second, std::nullopt};
        if (const Segment *next_seg = dynamic->segment()->next()) {
            single_note.next_tick = next_seg->tick().ticks() + tick_offset;
        }
        info.kind = single_note;
    } else if (auto compound_it = ::mu::engraving::compound_dynamic_values.find(dynamic_type);
               compound_it != ::mu::engraving::compound_dynamic_values.end()) {
        const BeatsPerSecond tempo = denormalize_tempo(current_tempo_at(*ctx.tempo_curve, tick)) * ctx.tempo_multiplier;
        info.kind = DynamicInfo::Compound{compound_it->second.first, compound_it->second.second,
                                          tick + dynamic->velocity_change_length(tempo).ticks()};
    } else {
        MUSE_NOT_SUPPORTED;
        return;
    }

    const std::vector<AutomationCurveKey> keys = resolve_keys(dynamic, AutomationType::DYNAMICS, range);
    if (keys.empty()) {
        return;
    }

    info.priority = dynamic_priority(dynamic);

    info.eid = dynamic->eid();
    if (!info.eid.is_valid()) {
        info.eid = dynamic->assign_new_eid();
    }

    for (const AutomationCurveKey &key : keys) {
        add_dynamic_points(info, key, ctx);
    }
}

void ScoreAutomationController::add_dynamic_points(const DynamicInfo &info, const AutomationCurveKey &key, UpdateContext &ctx) {
    IF_ASSERT_FAILED(key.is_valid()) { return; }

    AutomationCurve &curve = ctx.curves[key];
    std::map<utick_t, int> &tick_prio_map = ctx.dynamic_priorities[key];

    if (const auto *ordinary = std::get_if<DynamicInfo::Ordinary>(&info.kind)) {
        AutomationPoint point;
        point.value.in_value = AutomationPoint::ArrivalFromPrevious{};
        point.value.out_value = ordinary->value;
        point.item_id = info.eid;
        point.generated = true;
        add_dynamic_point(curve, tick_prio_map, info.tick, point, info.priority);
        return;
    }

    if (const auto *single_note = std::get_if<DynamicInfo::SingleNote>(&info.kind)) {
        const AutomationPoint *prev_point = single_note->next_tick ? active_point(ctx.curves, key, info.tick) : nullptr;

        AutomationPoint point;
        point.value.in_value = AutomationPoint::ArrivalFromPrevious{};
        point.value.out_value = single_note->value;
        point.item_id = info.eid;
        point.generated = true;
        add_dynamic_point(curve, tick_prio_map, info.tick, point, info.priority);

        if (single_note->next_tick) {
            // Recovers to whatever was active before this dynamic
            AutomationPoint next_point = prev_point ? *prev_point : AutomationPoint{};
            const AutomationPoint::Ease preserved_ease = ease(next_point).value_or(AutomationPoint::Ease::none());
            next_point.value.in_value = AutomationPoint::ExplicitArrival{point.value.out_value, preserved_ease};
            next_point.generated = true;
            try_add_dynamic_point(curve, tick_prio_map, *single_note->next_tick, next_point, info.priority);
        }

        return;
    }

    if (const auto *compound = std::get_if<DynamicInfo::Compound>(&info.kind)) {
        AutomationPoint start_point;
        start_point.value.in_value = AutomationPoint::ArrivalFromPrevious{};
        start_point.value.out_value = compound->start_value;
        start_point.item_id = info.eid;
        start_point.generated = true;
        add_dynamic_point(curve, tick_prio_map, info.tick, start_point, info.priority);

        AutomationPoint end_point;
        end_point.value.out_value = compound->end_value;
        end_point.value.in_value = AutomationPoint::ExplicitArrival{end_point.value.out_value, AutomationPoint::Ease::none()};
        end_point.item_id = info.eid;
        end_point.generated = true;
        try_add_dynamic_point(curve, tick_prio_map, compound->end_point_tick, end_point, info.priority);
    }
}

void ScoreAutomationController::add_spanner_points(const Score *score, int repeat_start_tick, int repeat_end_tick, int tick_offset,
                                                   const StaffRange &range, UpdateContext &ctx) {

    const SpannerMap &spanner_map = score->spanner_map();
    if (spanner_map.empty()) {
        return;
    }

    const int overlap_start = std::max(repeat_start_tick + 1, ctx.clear_from_u_tick - tick_offset);
    const int overlap_stop = repeat_end_tick - 1;
    if (overlap_start > overlap_stop) {
        return;
    }

    const auto &intervals = spanner_map.find_overlapping(overlap_start, overlap_stop);
    for (const auto &interval : intervals) {
        const Spanner *spanner = interval.value;
        if (!spanner->play_spanner()) {
            continue;
        }

        if (spanner->is_hairpin()) {
            if (!ctx.request.include_dynamics) {
                continue;
            }
            const Hairpin *hairpin = to_hairpin(spanner);
            const std::vector<AutomationCurveKey> keys = resolve_keys(hairpin, AutomationType::DYNAMICS, range);
            if (!keys.empty()) {
                add_hairpin_points(hairpin, tick_offset, keys, ctx);
            }
        } else if (ctx.request.include_tempo && spanner->is_gradual_tempo_change()) {
            add_gradual_tempo_change_points(to_gradual_tempo_change(spanner), tick_offset, ctx);
        } else if (ctx.request.include_tempo && spanner->is_volta()) {
            add_volta_tempo_reset_point(to_volta(spanner), ctx);
        }
    }
}

void ScoreAutomationController::add_hairpin_points(const Hairpin *hairpin, int tick_offset, const std::vector<AutomationCurveKey> &keys,
                                                   UpdateContext &ctx) {
    HairpinInfo info;
    info.from = hairpin->tick().ticks() + tick_offset;
    info.to = info.from + hairpin->ticks().ticks();

    // --- Check start tick
    {
        const Segment *start_segment = hairpin->start_segment();
        const track_idx_t track_idx = hairpin->track();
        const Dynamic *start_dynamic =
            start_segment ? to_dynamic(start_segment->find_annotation(ElementType::DYNAMIC, track_idx, track_idx)) : nullptr;
        if (start_dynamic && start_dynamic->play_dynamic() &&
            muse::contains(::mu::engraving::compound_dynamic_values, start_dynamic->dynamic_type())) {
            // The hairpin starts with a compound dynamic; we should start the hairpin after the transition is complete
            // This solution should be replaced once we have better infrastructure to see relations between Dynamics and Hairpins.
            const BeatsPerSecond tempo = denormalize_tempo(current_tempo_at(*ctx.tempo_curve, info.from)) * ctx.tempo_multiplier;
            info.from += start_dynamic->velocity_change_length(tempo).ticks();
        }
    }

    IF_ASSERT_FAILED(info.from < info.to) { return; }

    info.eid = hairpin->eid();
    if (!info.eid.is_valid()) {
        info.eid = hairpin->assign_new_eid();
    }

    info.priority = dynamic_priority(hairpin);
    info.is_crescendo = hairpin->is_crescendo();
    info.nominal_value_from = start_hairpin_value(hairpin);
    info.nominal_value_to = end_hairpin_value(hairpin);

    for (const AutomationCurveKey &key : keys) {
        add_hairpin_points(info, key, ctx);
    }
}

void ScoreAutomationController::add_hairpin_points(const HairpinInfo &info, const AutomationCurveKey &key, UpdateContext &ctx) {
    AutomationCurve &curve = ctx.curves[key];
    std::map<utick_t, int> &tick_prio_map = ctx.dynamic_priorities[key];

    // --- Determine valueFrom
    const AutomationPoint *prev_point = active_point(ctx.curves, key, info.from);
    // A hairpin with no preceding dynamic starts from the natural dynamic level, which is what
    // activePoint() used to return (MIDPOINT { 0.5, 0.5 }) before 628f484fbf. Defaulting to 0.0 here
    // made every "crescendo from nothing" start from silence (ppppppppp).
    const real_t prev_out_value = prev_point ? prev_point->value.out_value : ::natural_normalized_dynamic;

    // If the hairpin has no specific start value, use the currently-applicable value at the start tick of the hairpin
    const real_t value_from = info.nominal_value_from.value_or(prev_out_value);

    {
        AutomationPoint start_point;
        start_point.value.out_value = value_from;
        start_point.value.in_value = AutomationPoint::ArrivalFromPrevious{};
        start_point.item_id = info.eid;
        start_point.generated = true;
        try_add_dynamic_point(curve, tick_prio_map, info.from, start_point, info.priority);
    }

    // --- Determine valueTo
    // If there is an end dynamic marking, check if it matches the 'direction' of the hairpin (cresc. vs dim.)
    const bool use_nominal_value_to = info.nominal_value_to.has_value() && (info.is_crescendo ? info.nominal_value_to.value() > value_from :
                                                                                                info.nominal_value_to.value() < value_from);

    // --- Check end tick
    const real_t value_to =
        use_nominal_value_to ? info.nominal_value_to.value() : value_from + (info.is_crescendo ? ::dynamic_step : -::dynamic_step);

    const auto end_point_it = curve.find(info.to);
    const bool has_point_at_end = end_point_it != curve.end();

    if (has_point_at_end) {
        // A point already exists at the end tick; encode the hairpin's arrival via inValue, but only
        // if this hairpin has at least as much priority as whoever placed that point
        const auto tick_it = tick_prio_map.find(info.to);
        const bool can_modify = tick_it == tick_prio_map.end() || info.priority >= tick_it->second;
        if (can_modify) {
            const AutomationPoint::Ease preserved_ease = ease(end_point_it->second).value_or(AutomationPoint::Ease::none());
            end_point_it->second.value.in_value = AutomationPoint::ExplicitArrival{value_to, preserved_ease};
        }
        return;
    }

    if (info.from < info.to) {
        AutomationPoint point;
        point.value.out_value = value_to;
        point.value.in_value = AutomationPoint::ExplicitArrival{point.value.out_value, AutomationPoint::Ease::none()};
        point.item_id = info.eid;
        point.generated = true;
        try_add_dynamic_point(curve, tick_prio_map, info.to, point, info.priority);
    }
}

void ScoreAutomationController::fill_voice_curves_from_base(UpdateContext &ctx) {

    for (auto &[key, curve] : ctx.curves) {
        if (!key.voice_idx().has_value()) {
            continue;
        }

        const AutomationCurveKey base_key = key.without_voice();
        const auto base_it = ctx.curves.find(base_key);
        if (base_it == ctx.curves.end()) {
            continue;
        }

        curve.insert(base_it->second.cbegin(), base_it->second.cend());
    }
}

void ScoreAutomationController::collect_measure_repeats(const Segment *segment, int tick_offset, const StaffRange &range,
                                                        MeasureRepeats &result) {
    if (!segment->is_chord_rest_type()) {
        return;
    }

    for (staff_idx_t staff_idx = range.from; staff_idx <= range.to; ++staff_idx) {
        const EngravingItem *item = segment->element(staff2track(staff_idx));
        if (!item || !item->is_measure_repeat()) {
            continue;
        }

        result.emplace_back(to_measure_repeat(item), tick_offset);
    }
}

void ScoreAutomationController::add_measure_repeat_points(UpdateContext &ctx) {

    if (ctx.measure_repeats.empty()) {
        return;
    }

    std::unordered_map<uint64_t, std::vector<AutomationCurve *>> curves_by_staff;
    for (auto &[key, curve] : ctx.curves) {
        const std::optional<muse::ID> staff_id = key.staff_id();
        if (staff_id) {
            curves_by_staff[staff_id->to_uint64()].push_back(&curve);
        }
    }

    if (curves_by_staff.empty()) {
        return;
    }

    struct MeasureRange {
        int tick_shift = 0;
        utick_t src_from = 0;
        utick_t src_to = 0;
    };

    for (const auto &[mr, tick_offset] : ctx.measure_repeats) {
        const Staff *staff = mr->staff();
        if (!staff) {
            continue;
        }

        const auto staff_curves_it = curves_by_staff.find(staff->id().to_uint64());
        if (staff_curves_it == curves_by_staff.end()) {
            continue;
        }

        std::vector<MeasureRange> ranges;
        ranges.reserve(mr->num_measures());

        const Measure *curr_measure = mr->first_measure_of_group();
        for (int num = 0; curr_measure && num < mr->num_measures(); ++num, curr_measure = curr_measure->next_measure()) {
            const Measure *referring_measure = mr->referring_measure(curr_measure);
            IF_ASSERT_FAILED(referring_measure && referring_measure != curr_measure) { continue; }

            const int referring_measure_tick = referring_measure->tick().ticks();
            ranges.push_back({curr_measure->tick().ticks() - referring_measure_tick, referring_measure_tick + tick_offset,
                              referring_measure->end_tick().ticks() + tick_offset});
        }

        if (ranges.empty()) {
            continue;
        }

        for (AutomationCurve *curve : staff_curves_it->second) {
            const MeasureRange &first_range = ranges.front();
            auto src_it = curve->lower_bound(first_range.src_from);
            const auto curve_end = curve->end();
            if (src_it == curve_end) {
                continue;
            }

            const auto dest_hint = curve->lower_bound(first_range.src_from + first_range.tick_shift);

            for (const MeasureRange &range : ranges) {
                // Ranges are contiguous for consecutive measures, but a skipped measure (assert
                // failure above) can leave a gap; catch up if srcIt fell behind this range's start
                if (src_it != curve_end && src_it->first < range.src_from) {
                    src_it = curve->lower_bound(range.src_from);
                }

                for (; src_it != curve_end && src_it->first < range.src_to; ++src_it) {
                    if (src_it->second.generated) {
                        curve->insert(dest_hint, {src_it->first + range.tick_shift, src_it->second});
                    } else {
                        AutomationPoint mirrored_point = src_it->second;
                        mirrored_point.generated = true;
                        curve->insert(dest_hint, {src_it->first + range.tick_shift, mirrored_point});
                    }
                }
            }
        }
    }
}

void ScoreAutomationController::collect_pauses(const Measure *measure, int tick_offset, PausesMap &pauses, PausesMap &no_repeat_pauses) {
    const Fraction &start_tick = measure->tick();
    const int start_utick = start_tick.ticks() + tick_offset;

    // Implement section break rest
    for (MeasureBase *mb = measure->prev(); mb && mb->end_tick() == start_tick; mb = mb->prev()) {
        if (mb->pause()) {
            pauses[start_utick] = mb->pause();
            // Identical value across every pass that plays this raw tick, so last-write-wins is fine
            no_repeat_pauses[start_tick.ticks()] = mb->pause();
        }
    }

    // Add pauses from the end of the previous measure (at measure->tick()):
    for (Segment *s = measure->first(); s && s->tick() == start_tick; s = s->prev1()) {
        if (!s->is_breath_type()) {
            continue;
        }
        double length = 0.0;
        for (EngravingItem *e : s->elist()) {
            if (e && e->is_breath()) {
                length = std::max(length, to_breath(e)->pause());
            }
        }
        if (!muse::real_is_null(length)) {
            pauses[start_utick] = length;
            no_repeat_pauses[start_tick.ticks()] = length;
        }
    }

    for (const Segment &segment : measure->segments()) {
        if (!segment.is_breath_type()) {
            continue;
        }

        double length = 0.0;
        Fraction tick = segment.tick();
        // find longest pause
        for (track_idx_t i = 0, n = measure->score()->ntracks(); i < n; ++i) {
            EngravingItem *e = segment.element(i);
            if (e && e->is_breath()) {
                length = std::max(length, to_breath(e)->pause());
            }
        }
        if (!muse::real_is_null(length)) {
            pauses[tick.ticks() + tick_offset] = length;
            no_repeat_pauses[tick.ticks()] = length;
        }
    }
}

void ScoreAutomationController::add_tempo_text_point(const TempoText *tt, int tick_offset, UpdateContext &ctx) {
    if (!tt->play_tempo_text()) {
        return;
    }

    if (tt->is_normal() && !tt->is_relative() && !ctx.tempo_primo) {
        ctx.tempo_primo = round_tempo(tt->tempo());
    }

    const utick_t tick = tt->segment()->tick().ticks() + tick_offset;

    EID item_id = tt->eid();
    if (!item_id.is_valid()) {
        item_id = tt->assign_new_eid();
    }

    if (tt->is_a_tempo() && tt->follow_text()) {
        // This will effectively reset the tempo to the previous one when a progressive change was active
        set_tempo_point(tick, tick_offset, current_tempo_at(*ctx.tempo_curve, tick), ctx, item_id);
    } else if (tt->is_tempo_primo() && tt->follow_text()) {
        set_tempo_point(tick, tick_offset, normalize_tempo(ctx.tempo_primo.value_or(Constants::default_tempo)), ctx, item_id);
    } else if (tt->is_relative()) {
        const real_t before = current_tempo_at(*ctx.tempo_curve, tick);
        set_tempo_point(tick, tick_offset, before * tt->relative_value(), ctx, item_id);
    } else {
        set_tempo_point(tick, tick_offset, normalize_tempo(round_tempo(tt->tempo())), ctx, item_id);
    }
}

void ScoreAutomationController::add_fermata_stretch_points(const Fermata *fermata, int tick_offset, double stretch, UpdateContext &ctx) {
    const Segment *segment = fermata->segment();
    const utick_t tick = segment->tick().ticks() + tick_offset;

    const auto before_it = muse::find_less_or_equal(*ctx.tempo_curve, tick);
    const bool has_before = before_it != ctx.tempo_curve->cend();
    const real_t before = has_before ? before_it->second.value.out_value : ::mu::engraving::default_normalized_tempo;

    EID item_id = fermata->eid();
    if (!item_id.is_valid()) {
        item_id = fermata->assign_new_eid();
    }

    set_tempo_point(tick, tick_offset, before / stretch, ctx, item_id);

    const Segment *next_active_segment = segment->next1();
    while (next_active_segment && !next_active_segment->is_active()) {
        next_active_segment = next_active_segment->next1();
    }

    const Fraction tempo_end_tick = next_active_segment ? next_active_segment->tick() : segment->tick() + segment->ticks();
    const utick_t etick = tempo_end_tick.ticks() + tick_offset;

    // Restore to whatever was active before the fermata, so the point belongs to that marking, not the fermata
    const auto etick_it = ctx.tempo_curve->lower_bound(etick);
    if (etick_it == ctx.tempo_curve->end() || etick_it->first != etick) {
        set_tempo_point(etick_it, etick, tick_offset, before, ctx, has_before ? before_it->second.item_id : std::nullopt);
    }
}

void ScoreAutomationController::add_gradual_tempo_change_points(const GradualTempoChange *tempo_change, int tick_offset,
                                                                UpdateContext &ctx) {
    const utick_t tick_from = tempo_change->tick().ticks() + tick_offset;
    const utick_t tick_to = tick_from + tempo_change->ticks().ticks();
    const real_t normalized_current = current_tempo_at(*ctx.tempo_curve, tick_from);

    EID item_id = tempo_change->eid();
    if (!item_id.is_valid()) {
        item_id = tempo_change->assign_new_eid();
    }

    const auto tick_from_it = ctx.tempo_curve->lower_bound(tick_from);
    if (tick_from_it == ctx.tempo_curve->end() || tick_from_it->first != tick_from) {
        // Explicit flat arrival, so the ramp is anchored here rather than at whatever
        // unrelated marking last happened to be an arrival
        AutomationPoint start_point;
        start_point.value.out_value = normalized_current;
        start_point.value.in_value = AutomationPoint::ExplicitArrival{normalized_current, AutomationPoint::Ease::none()};
        start_point.item_id = item_id;
        start_point.generated = true;
        ctx.tempo_curve->emplace_hint(tick_from_it, tick_from, start_point);
        set_no_repeat_tempo_point(tick_from - tick_offset, start_point, ctx);
    }

    const real_t normalized_target = std::clamp(normalized_current * tempo_change->tempo_change_factor(),
                                                ::mu::engraving::min_normalized_tempo, ::mu::engraving::max_normalized_tempo);
    const AutomationPoint::ExplicitArrival arrival{normalized_target, change_method_to_ease(tempo_change->easing_method())};

    // Don't overwrite a tempo already set at tickTo (e.g. by a tempo marking) - just record the
    // arrival so the ramp leading up to it still interpolates correctly
    // See https://github.com/musescore/MuseScore/issues/12140
    const auto tick_to_it = ctx.tempo_curve->lower_bound(tick_to);
    if (tick_to_it != ctx.tempo_curve->end() && tick_to_it->first == tick_to) {
        tick_to_it->second.value.in_value = arrival;

        const auto raw_it = ctx.no_repeat_tempo_curve.find(tick_to - tick_offset);
        if (raw_it != ctx.no_repeat_tempo_curve.end()) {
            raw_it->second.value.in_value = arrival;
        }

        return;
    }

    AutomationPoint end_point;
    end_point.value.out_value = normalized_target;
    end_point.value.in_value = arrival;
    end_point.item_id = item_id;
    end_point.generated = true;
    ctx.tempo_curve->emplace_hint(tick_to_it, tick_to, end_point);
    set_no_repeat_tempo_point(tick_to - tick_offset, end_point, ctx);
}

void ScoreAutomationController::add_volta_tempo_reset_point(const Volta *volta, UpdateContext &ctx) {
    const Measure *start_measure = volta->start_measure();
    const Measure *end_measure = volta->end_measure();
    if (!start_measure || !end_measure || !end_measure->repeat_end()) {
        return;
    }

    // Changes noRepeatTempoCurve only, so later content (e.g. a seconda volta) sees the pre-volta tempo, not
    // the volta's own changes; findLessOrEqual so an earlier volta's own reset at this tick counts
    const auto before_it = muse::find_less_or_equal(ctx.no_repeat_tempo_curve, start_measure->tick().ticks());
    const bool has_before = before_it != ctx.no_repeat_tempo_curve.end();
    const real_t tempo_before_volta = has_before ? before_it->second.value.out_value : ::mu::engraving::default_normalized_tempo;

    AutomationPoint point;
    point.value.out_value = tempo_before_volta;
    // Recovers to whatever was active before the volta, so the point belongs to that marking, not the volta
    point.item_id = has_before ? before_it->second.item_id : std::nullopt;
    point.generated = true;

    // Don't overwrite an explicit tempo marking already sitting at this exact tick
    // (e.g. the seconda volta's own TempoText, written in Step 1 before this Step 2 pass runs)
    ctx.no_repeat_tempo_curve.try_emplace(end_measure->end_tick().ticks(), point);
}

void ScoreAutomationController::resolve_pending_tempo_resets(UpdateContext &ctx) {
    for (const auto &[utick, raw_tick] : ctx.pending_tempo_resets) {
        const auto utick_it = ctx.tempo_curve->lower_bound(utick);
        if (utick_it != ctx.tempo_curve->end() && utick_it->first == utick) {
            continue; // this pass's own marking already sits exactly here - don't overwrite it
        }

        const auto raw_it = muse::find_less_or_equal(ctx.no_repeat_tempo_curve, raw_tick);
        const real_t tempo =
            raw_it != ctx.no_repeat_tempo_curve.end() ? raw_it->second.value.out_value : ::mu::engraving::default_normalized_tempo;

        const real_t current_tempo = current_tempo_at(*ctx.tempo_curve, utick);
        if (real_is_equal(tempo, current_tempo)) {
            continue; // no actual change - don't emit a redundant point
        }

        set_tempo_point(utick_it, utick, utick - raw_tick, tempo, ctx);
    }
}

void ScoreAutomationController::fix_anacrusis_tempo(UpdateContext &ctx) {
    // An anacrusis measure with no tempo marking of its own should start at whatever tempo
    // the next (first full) measure marks, rather than whatever tempo preceded it
    for (const AnacrusisMeasureInfo &info : ctx.anacrusis_measures) {
        const utick_t measure_tick = info.measure->tick().ticks() + info.tick_offset;
        const auto measure_it = ctx.tempo_curve->lower_bound(measure_tick);
        if (measure_it != ctx.tempo_curve->end() && measure_it->first == measure_tick) {
            continue;
        }

        if (!info.next_measure_u_tick) {
            continue; // anacrusis is the last measure of its repeat pass - no "next" within this pass
        }

        const auto next_it = ctx.tempo_curve->find(*info.next_measure_u_tick);
        if (next_it != ctx.tempo_curve->end()) {
            set_tempo_point(measure_it, measure_tick, info.tick_offset, next_it->second.value.out_value, ctx);
        }
    }
}

bool ScoreAutomationController::try_add_dynamic_point(AutomationCurve &curve, std::map<utick_t, int> &tick_prio_map, utick_t tick,
                                                      const AutomationPoint &point, int priority) {
    //! See: https://github.com/musescore/MuseScore/issues/23355
    const auto prio_it = tick_prio_map.lower_bound(tick);
    const bool has_prio = prio_it != tick_prio_map.end() && prio_it->first == tick;

    if (has_prio) {
        if (priority <= prio_it->second) {
            return false;
        }

        curve[tick] = point;
        prio_it->second = priority;
        return true;
    }

    const auto curve_it = curve.lower_bound(tick);
    if (curve_it != curve.end() && curve_it->first == tick) {
        return false; // point from a prior update pass — don't overwrite
    }

    curve.emplace_hint(curve_it, tick, point);
    tick_prio_map.emplace_hint(prio_it, tick, priority);

    return true;
}

void ScoreAutomationController::add_dynamic_point(AutomationCurve &curve, std::map<utick_t, int> &tick_prio_map, utick_t tick,
                                                  const AutomationPoint &point, int priority) {
    // If a point with the same priority already exists at this tick, merge them:
    // keep the existing arrival value and use this point's departure value
    const auto prio_it = tick_prio_map.find(tick);
    if (prio_it != tick_prio_map.end() && prio_it->second == priority) {
        AutomationPoint &existing = curve[tick];
        const AutomationPoint::InValue arrival_value = existing.value.in_value;
        existing = point;
        existing.value.in_value = arrival_value;
        return;
    }

    try_add_dynamic_point(curve, tick_prio_map, tick, point, priority);
}

void ScoreAutomationController::set_no_repeat_tempo_point(utick_t raw_tick, const AutomationPoint &point, UpdateContext &ctx) {
    const auto it = ctx.no_repeat_tempo_curve.lower_bound(raw_tick);
    const bool exists = it != ctx.no_repeat_tempo_curve.end() && it->first == raw_tick;
    if (exists && !it->second.generated) {
        return; // an authored point at this raw tick wins over the generated one
    }

    if (exists) {
        it->second = point;
    } else {
        ctx.no_repeat_tempo_curve.emplace_hint(it, raw_tick, point);
    }
}

void ScoreAutomationController::set_tempo_point(utick_t tick, int tick_offset, real_t normalized_bps, UpdateContext &ctx,
                                                std::optional<EID> item_id) {
    const auto it = ctx.tempo_curve->lower_bound(tick);
    const bool exists = it != ctx.tempo_curve->end() && it->first == tick;
    if (exists && !it->second.generated) {
        return; // an authored point at this tick wins over the generated one
    }

    if (!exists) {
        set_tempo_point(it, tick, tick_offset, normalized_bps, ctx, item_id);
        return;
    }

    const AutomationPoint point = make_tempo_point(normalized_bps, item_id);
    it->second = point;
    set_no_repeat_tempo_point(tick - tick_offset, point, ctx);
}

void ScoreAutomationController::set_tempo_point(AutomationCurve::iterator hint, utick_t tick, int tick_offset, real_t normalized_bps,
                                                UpdateContext &ctx, std::optional<EID> item_id) {
    const AutomationPoint point = make_tempo_point(normalized_bps, item_id);
    ctx.tempo_curve->emplace_hint(hint, tick, point);
    set_no_repeat_tempo_point(tick - tick_offset, point, ctx);
}

std::vector<AutomationCurveKey> ScoreAutomationController::resolve_keys(const EngravingItem *item, AutomationType type,
                                                                        const StaffRange &range) {
    const Score *score = item->score();
    IF_ASSERT_FAILED(score) { return {}; }

    std::vector<AutomationCurveKey> result;

    auto try_add_staff_key = [type, score, &range, &result](staff_idx_t staff_idx, std::optional<size_t> voice_idx = std::nullopt) {
        if (!range.contains(staff_idx)) {
            return;
        }

        const Staff *staff = score->staff(staff_idx);
        IF_ASSERT_FAILED(staff) { return; }

        if (!staff->is_primary_staff()) {
            return; // ignore linked staves
        }

        result.push_back(AutomationCurveKey::staff(type, staff->id(), voice_idx));
    };

    const VoiceAssignment voice_assignment = item->get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();
    switch (voice_assignment) {
        case VoiceAssignment::ALL_VOICE_IN_INSTRUMENT: {
            const Part *part = item->part();
            IF_ASSERT_FAILED(part) { return result; }

            const TrackRange track_range = part->track_range();
            const staff_idx_t start_staff_idx = track2staff(track_range.start_track);
            const staff_idx_t end_staff_idx = track2staff(track_range.end_track);
            result.reserve(end_staff_idx - start_staff_idx);

            for (staff_idx_t staff_idx = start_staff_idx; staff_idx < end_staff_idx; ++staff_idx) {
                try_add_staff_key(staff_idx);
            }
        } break;
        case VoiceAssignment::CURRENT_VOICE_ONLY: try_add_staff_key(item->staff_idx(), item->voice()); break;
        case VoiceAssignment::ALL_VOICE_IN_STAFF: try_add_staff_key(item->staff_idx()); break;
    }

    return result;
}

void ScoreAutomationController::mirror_edits_to_repeats(const AutomationCurveKey &key, AutomationPointEdits &edits) {

    const RepeatList &repeat_list = score_->expanded_repeat_list();
    if (repeat_list.size() <= 1) {
        return;
    }

    // Measure repeats are staff-specific; global/instrument-scoped points (e.g. Tempo, Volume, Pan)
    // aren't tied to any staff, so they're mirrored to regular repeats only, not measure repeats
    std::optional<StaffRange> staff_range;
    if (const std::optional<muse::ID> staff_id = key.staff_id()) {
        const Staff *staff = score_->staff_by_id(*staff_id);
        IF_ASSERT_FAILED(staff) { return; }

        staff_range.emplace(score_, staff->idx(), staff->idx());
    }

    // Bounded by the original size, since mirrored edits are appended to the same vector below
    const size_t original_edit_count = edits.size();
    for (size_t i = 0; i < original_edit_count; ++i) {
        // Copied by value: push_back below may reallocate edits
        AutomationPointEdit local_edit = edits[i];
        const auto source_it = repeat_list.find_repeat_segment_from_u_tick(local_edit.tick);
        IF_ASSERT_FAILED(source_it != repeat_list.cend()) { continue; }

        const RepeatSegment *source_seg = *source_it;
        const int source_tick_offset = source_seg->utick - source_seg->tick;

        // Re-express the edit in tick coordinates local to sourceSeg
        local_edit.tick -= source_tick_offset;
        if (auto *move_point = std::get_if<AutomationPointEdit::MovePoint>(&local_edit.change)) {
            move_point->from -= source_tick_offset;
        }

        for (const RepeatSegment *target_seg : repeat_list) {
            if (target_seg != source_seg) {
                const MirrorRange target_range{target_seg->tick, target_seg->end_tick(), target_seg->utick - target_seg->tick};
                mirror_point_if_in_range(local_edit, target_range, edits);
            }
            if (staff_range) {
                mirror_to_measure_repeats(target_seg, *staff_range, local_edit, edits);
            }
        }
    }
}

void ScoreAutomationController::mirror_point_if_in_range(const AutomationPointEdit &local_edit, const MirrorRange &range,
                                                         AutomationPointEdits &all_edits) {
    const int local_tick = static_cast<int>(local_edit.tick);
    if (local_tick < range.from || local_tick >= range.to_exclusive) {
        return;
    }

    const utick_t mirrored_tick = static_cast<utick_t>(local_tick + range.tick_offset);

    if (std::holds_alternative<AutomationPointEdit::ErasePoint>(local_edit.change)) {
        all_edits.push_back({mirrored_tick, AutomationPointEdit::ErasePoint{}});
        return;
    }

    const auto *move_point = std::get_if<AutomationPointEdit::MovePoint>(&local_edit.change);

    // Mirrored points are derived from the user's edit, not edited directly, so they are marked generated
    AutomationPoint mirrored_point = move_point ? move_point->point : std::get<AutomationPointEdit::SetPoint>(local_edit.change).point;
    mirrored_point.generated = true;

    if (move_point && move_point->from >= range.from && move_point->from < range.to_exclusive) {
        const utick_t mirrored_move_from = static_cast<utick_t>(move_point->from + range.tick_offset);
        all_edits.push_back({mirrored_tick, AutomationPointEdit::MovePoint{mirrored_point, mirrored_move_from}});
    } else {
        all_edits.push_back({mirrored_tick, AutomationPointEdit::SetPoint{mirrored_point}});
    }
}

void ScoreAutomationController::mirror_to_measure_repeats(const RepeatSegment *target_seg, const StaffRange &range,
                                                          const AutomationPointEdit &local_edit, AutomationPointEdits &all_edits) {
    const int tick_offset = target_seg->utick - target_seg->tick;

    // A MeasureRepeat can only ever sit on the measure's first ChordRest segment
    MeasureRepeats measure_repeats;
    for (const Measure *measure : target_seg->measure_list()) {
        if (const Segment *first_chord_rest_segment = measure->first(SegmentType::CHORD_REST)) {
            collect_measure_repeats(first_chord_rest_segment, tick_offset, range, measure_repeats);
        }
    }

    for (const auto &[mr, mr_tick_offset] : measure_repeats) {
        const Measure *curr_measure = mr->first_measure_of_group();
        for (int num = 0; curr_measure && num < mr->num_measures(); ++num, curr_measure = curr_measure->next_measure()) {
            const Measure *referring_measure = mr->referring_measure(curr_measure);
            IF_ASSERT_FAILED(referring_measure && referring_measure != curr_measure) { continue; }

            const int measure_from = referring_measure->tick().ticks();
            const int measure_to_exclusive = referring_measure->end_tick().ticks();
            const int tick_shift = curr_measure->tick().ticks() - measure_from;

            const MirrorRange measure_range{measure_from, measure_to_exclusive, tick_shift + mr_tick_offset};
            mirror_point_if_in_range(local_edit, measure_range, all_edits);
        }
    }
}
