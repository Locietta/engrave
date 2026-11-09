/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include <functional>

#include <amc/flatset.hpp>
#include <ankerl/unordered_dense.h>

#include "../types/types.h"
#include "engraving/automation/automationtypes.h"
#include "global/flatmap.h"
#include "mpe/events.h"
#include "mpe/mpetypes.h"

namespace mu::engraving {
struct Segment;
struct PlayTechAnnotation;
struct SoundFlag;
struct Score;
struct Part;
struct MeasureRepeat;
struct TextBase;
struct ChordRest;
struct RepeatSegment;

struct PlaybackContext {
    explicit PlaybackContext(const Score *score);
    void release(const std::function<void()> &step);

    muse::mpe::dynamic_level_t appliable_dynamic_level(const track_idx_t track_idx, const int nominal_position_tick) const;

    std::pair<muse::mpe::timestamp_t, PlayingTechniqueType> playing_technique(const track_idx_t track_idx,
                                                                              const int nominal_position_tick) const;
    muse::mpe::timestamp_t find_playing_technique_timestamp(const track_idx_t track_idx, PlayingTechniqueType type,
                                                            const int start_from_tick) const;

    muse::FlatMap<muse::mpe::timestamp_t, muse::mpe::SoundPresetChangeEventList> sound_presets(const track_idx_t track_from,
                                                                                               const track_idx_t track_to) const;
    muse::mpe::SoundPresetChangeEventList sound_presets(const track_idx_t track_idx, const int nominal_position_tick) const;

    muse::FlatMap<muse::mpe::timestamp_t, muse::mpe::TextArticulationEventList> text_articulations(const track_idx_t track_from,
                                                                                                   const track_idx_t track_to) const;
    muse::mpe::TextArticulationEvent text_articulation(const track_idx_t track_idx, const int nominal_position_tick) const;

    muse::FlatMap<muse::mpe::timestamp_t, muse::mpe::SyllableEventList> syllables(const track_idx_t track_from,
                                                                                  const track_idx_t track_to) const;
    muse::mpe::SyllableEvent syllable(const track_idx_t track_idx, const int nominal_position_tick) const;

    muse::mpe::DynamicAutomationLayers dynamic_level_layers(const track_idx_t track_from, const track_idx_t track_to) const;

    bool has_sound_flags(const track_idx_t track_from, const track_idx_t track_to) const;

    void update(const track_idx_t track_from, const track_idx_t track_to, const int tick_from, const int tick_to,
                bool expand_repeats = true, const std::function<void()> &checkpoint = {});
    void clear(const track_idx_t track_from, const track_idx_t track_to, const int tick_from, const int tick_to);

private:
    friend struct PlaybackContextReleaseTestAccess;
    using SoundPresetsMap = muse::FlatMap<int /*nominalPositionTick*/, muse::mpe::SoundPresetChangeEventList>;
    using SoundPresetsByTrack = muse::FlatMap<track_idx_t, SoundPresetsMap>;

    using TextArticulationMap = muse::FlatMap<int /*nominalPositionTick*/, muse::mpe::TextArticulationEvent>;
    using TextArticulationsByTrack = muse::FlatMap<track_idx_t, TextArticulationMap>;

    using SyllableMap = muse::FlatMap<int /*nominalPositionTick*/, muse::mpe::SyllableEvent>;
    using SyllablesByTrack = muse::FlatMap<track_idx_t, SyllableMap>;

    using PlayTechniquesMap = muse::FlatMap<int /*nominalPositionTick*/, mu::engraving::PlayingTechniqueType>;
    using PlayTechniquesByTrack = muse::FlatMap<track_idx_t, PlayTechniquesMap>;

    // Flags are applied in read/insertion order, with the first flag on each staff winning.
    // This preserves the Windows baseline when several all-staff flags compete for an unflagged staff.
    using SoundFlagMap = muse::Buf<muse::FlatMapEntry<staff_idx_t, const SoundFlag *>>;

    void update_play_tech_map(const Part *part, const PlayTechAnnotation *annotation, const int segment_position_tick);
    void update_sound_preset_and_text_articulation_map(const Part *part, const SoundFlagMap &flags_on_segment,
                                                       const int segment_position_tick);
    void update_syllable_map(const TextBase *text, const int segment_position_tick);

    void handle_segment_annotations(const Segment *segment, const int segment_position_tick, const track_idx_t track_from,
                                    const track_idx_t track_to);
    void handle_segment_elements(const RepeatSegment *repeat, const Segment *segment, const int segment_position_tick,
                                 const track_idx_t track_from, const track_idx_t track_to,
                                 std::vector<const MeasureRepeat *> &found_measure_repeats);
    void handle_measure_repeats(const std::vector<const MeasureRepeat *> &measure_repeats, const int tick_position_offset);

    const AutomationCurve *dynamics_curve(const track_idx_t track_idx) const;

    bool has_only_one_lyrics_verse(const RepeatSegment *repeat, const track_idx_t track) const;

    const Score *score_ = nullptr;

    amc::FlatSet<track_idx_t> used_tracks_;
    mutable ankerl::unordered_dense::map<track_idx_t, const AutomationCurve *> dynamics_curve_by_track_;
    SoundPresetsByTrack sound_presets_by_track_;
    TextArticulationsByTrack text_articulations_by_track_;
    SyllablesByTrack syllables_by_track_;
    PlayTechniquesByTrack play_techniques_by_track_;

    ankerl::unordered_dense::map<const ChordRest *, int> current_verse_num_by_chord_rest_;
    muse::FlatMap<track_idx_t, amc::FlatSet<int /*tick*/>> multi_verse_lyrics_position_map_;
};

using PlaybackContextPtr = std::shared_ptr<PlaybackContext>;
} // namespace mu::engraving
