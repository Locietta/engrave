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
#pragma once

#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "engraving/automation/automationdata.h"
#include "engraving/automation/automationtypes.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/types/types.h"

namespace mu::engraving {
struct Score;
struct Fraction;
struct EngravingItem;
struct Measure;
struct Segment;
struct Dynamic;
struct Fermata;
struct Hairpin;
struct TempoText;
struct GradualTempoChange;
struct Volta;
struct MeasureRepeat;
struct RepeatList;
struct RepeatSegment;
struct RepeatSegmentInfo;
struct ScoreChanges;

struct ScoreAutomationController {
    void init(Score *score);
    void ensure_initialized(Score *score);

    void insert_time(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &old_segments);
    void update(const ScoreChanges &changes);

    void edit_points(const AutomationCurveKey &key, AutomationPointEdits &edits);

    AutomationDataConstPtr automation_data() const { return automation_data_; }

    void set_automation_data(AutomationDataPtr data) { automation_data_ = std::move(data); }

    const TempoTimeline &tempo_timeline(bool expand_repeats = true) const;
    void set_tempo_multiplier(const BeatsPerSecond &bps);

    void set_tempo_timeline_override(std::optional<TempoTimeline> timeline);

    RuntimeTempoState runtime_tempo_state() const { return {tempo_timeline_.tempo_multiplier(), tempo_timeline_override_}; }

    void restore_runtime_tempo_state(RuntimeTempoState state) {
        set_tempo_multiplier(state.multiplier);
        tempo_timeline_override_ = std::move(state.override_timeline);
    }

private:
    struct UpdateRequest {
        int tick_from = 0;
        staff_idx_t staff_idx_from = muse::nidx;
        staff_idx_t staff_idx_to = muse::nidx;
        bool include_tempo = false;
        bool include_dynamics = false;
    };

    struct StaffRange {
        StaffRange(const Score *score, staff_idx_t staff_idx_from, staff_idx_t staff_idx_to);

        staff_idx_t from = 0;
        staff_idx_t to = 0;
        bool is_full = true;
        std::set<muse::ID> staff_ids; // only populated when !isFull

        bool contains(staff_idx_t staff_idx) const;
        bool contains(const muse::ID &staff_id) const;
    };

    struct MirrorRange {
        int from = 0;
        int to_exclusive = 0;
        int tick_offset = 0;
    };

    struct AnacrusisMeasureInfo {
        const Measure *measure = nullptr;
        int tick_offset = 0;
        std::optional<utick_t> next_measure_u_tick; // next measure in the same repeat pass, if any
    };

    using DynamicPriorities = std::map<AutomationCurveKey, std::map<utick_t, int>>;
    using MeasureRepeats = std::vector<std::pair<const MeasureRepeat *, int>>;

    struct UpdateContext {
        UpdateRequest request;
        AutomationCurveMap curves;
        utick_t clear_from_u_tick = 0;
        DynamicPriorities dynamic_priorities;
        MeasureRepeats measure_repeats;
        std::optional<BeatsPerSecond> tempo_primo;
        PausesMap pauses;
        PausesMap no_repeat_pauses; // repeat-agnostic twin of pauses
        std::vector<AnacrusisMeasureInfo> anacrusis_measures;
        AutomationCurve *tempo_curve = nullptr;
        AutomationCurve no_repeat_tempo_curve; // repeat-agnostic Tempo curve
        std::vector<std::pair<utick_t, int>> pending_tempo_resets;
        BeatsPerSecond tempo_multiplier = 1.0;
    };

    struct DynamicInfo {
        struct Ordinary {
            real_t value = 0;
        };

        struct SingleNote {
            real_t value = 0;
            std::optional<utick_t> next_tick; // tick of the next segment, if any (recovery point)
        };

        struct Compound {
            real_t start_value = 0;
            real_t end_value = 0;
            utick_t end_point_tick = 0; // tick + velocityChangeLength (arrival point)
        };

        utick_t tick = 0;
        EID eid = EID::invalid();
        int priority = 0;
        std::variant<Ordinary, SingleNote, Compound> kind = Ordinary{};
    };

    struct HairpinInfo {
        utick_t from = 0;
        utick_t to = 0;
        EID eid = EID::invalid();
        int priority = 0;
        bool is_crescendo = false;
        std::optional<real_t> nominal_value_from;
        std::optional<real_t> nominal_value_to;
    };

    void update(const UpdateRequest &request, const AutomationCurveMap &curves);
    void full_rebuild(AutomationCurveMap &curves, bool include_tempo = true, bool include_dynamics = true);

    static void copy_curves_for_rebuild(const AutomationCurveMap &curves, const StaffRange &range, utick_t clear_from_u_tick,
                                        bool include_tempo, bool include_dynamics, AutomationCurveMap &dest_curves);

    static void add_segment_points(const Segment *segment, int tick_offset, const StaffRange &range, UpdateContext &ctx);
    static void add_dynamic_points(const Dynamic *dynamic, int tick_offset, const StaffRange &range, UpdateContext &ctx);
    static void add_dynamic_points(const DynamicInfo &info, const AutomationCurveKey &key, UpdateContext &ctx);

    static void add_spanner_points(const Score *score, int repeat_start_tick, int repeat_end_tick, int tick_offset, const StaffRange &range,
                                   UpdateContext &ctx);
    static void add_hairpin_points(const Hairpin *hairpin, int tick_offset, const std::vector<AutomationCurveKey> &keys,
                                   UpdateContext &ctx);
    static void add_hairpin_points(const HairpinInfo &info, const AutomationCurveKey &key, UpdateContext &ctx);

    static void fill_voice_curves_from_base(UpdateContext &ctx);

    static void collect_measure_repeats(const Segment *segment, int tick_offset, const StaffRange &range, MeasureRepeats &result);
    static void add_measure_repeat_points(UpdateContext &ctx);

    void mirror_authored_points_to_repeats(UpdateContext &ctx);
    void fill_no_repeat_tempo_curve(const AutomationCurve &tempo_curve, AutomationCurve &no_repeat_tempo_curve);

    static void collect_pauses(const Measure *measure, int tick_offset, PausesMap &pauses, PausesMap &no_repeat_pauses);
    static void add_tempo_text_point(const TempoText *tt, int tick_offset, UpdateContext &ctx);
    static void add_fermata_stretch_points(const Fermata *fermata, int tick_offset, double stretch, UpdateContext &ctx);
    static void add_gradual_tempo_change_points(const GradualTempoChange *tempo_change, int tick_offset, UpdateContext &ctx);
    static void add_volta_tempo_reset_point(const Volta *volta, UpdateContext &ctx);
    static void resolve_pending_tempo_resets(UpdateContext &ctx);
    static void fix_anacrusis_tempo(UpdateContext &ctx);

    static bool try_add_dynamic_point(AutomationCurve &curve, std::map<utick_t, int> &tick_prio_map, utick_t tick,
                                      const AutomationPoint &point, int priority);
    static void add_dynamic_point(AutomationCurve &curve, std::map<utick_t, int> &tick_prio_map, utick_t tick, const AutomationPoint &point,
                                  int priority);
    static void set_tempo_point(utick_t tick, int tick_offset, real_t normalized_bps, UpdateContext &ctx,
                                std::optional<EID> item_id = std::nullopt);
    //! NOTE: for callers that already know no point exists at tick (e.g. via lower_bound) - skips the redundant lookup
    static void set_tempo_point(AutomationCurve::iterator hint, utick_t tick, int tick_offset, real_t normalized_bps, UpdateContext &ctx,
                                std::optional<EID> item_id = std::nullopt);
    static void set_no_repeat_tempo_point(utick_t raw_tick, const AutomationPoint &point, UpdateContext &ctx);

    static std::vector<AutomationCurveKey> resolve_keys(const EngravingItem *item, AutomationType type, const StaffRange &range);

    void mirror_edits_to_repeats(const AutomationCurveKey &key, AutomationPointEdits &edits);

    static void mirror_point_if_in_range(const AutomationPointEdit &local_edit, const MirrorRange &range, AutomationPointEdits &all_edits);

    static void mirror_to_measure_repeats(const RepeatSegment *target_seg, const StaffRange &range, const AutomationPointEdit &local_edit,
                                          AutomationPointEdits &all_edits);

    Score *score_ = nullptr;
    AutomationDataPtr automation_data_;
    TempoTimeline tempo_timeline_;
    TempoTimeline flattened_tempo_timeline_;
    std::optional<TempoTimeline> tempo_timeline_override_;
};
} // namespace mu::engraving
