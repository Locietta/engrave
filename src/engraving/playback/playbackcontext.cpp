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

#include "playbackcontext.h"

#include <cmath>

#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/part.h"
#include "dom/playtechannotation.h"
#include "dom/repeatlist.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/soundflag.h"
#include "dom/staff.h"
#include "dom/stafftext.h"
#include "engraving/automation/automationdata.h"
#include "global/containers.h"
#include "global/realfn.h"
#include "log.h"
#include "types/constants.h"
#include "utils/arrangementutils.h"
#include "utils/expressionutils.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static bool sound_flag_playable(const SoundFlag *flag) {
    if (flag && flag->play()) {
        return !flag->sound_presets().empty() || !flag->playing_technique().empty();
    }

    return false;
}

static dynamic_level_t to_dynamic_level(double normalized_value) {
    return static_cast<dynamic_level_t>(std::lround(normalized_value * ::muse::mpe::max_dynamic_level));
}

template <typename ByTrackMap, typename ResultMap, typename MergeFn>
static void collect_by_track_range(const ByTrackMap &by_track_map, const track_idx_t track_from, const track_idx_t track_to,
                                   const Score *score, const std::set<track_idx_t> &used_tracks, ResultMap &result, MergeFn merge) {
    for (auto it = by_track_map.lower_bound(track_from); it != by_track_map.end() && it->first < track_to; ++it) {
        if (!muse::contains(used_tracks, it->first)) {
            continue;
        }

        for (const auto &pair : it->second) {
            const timestamp_t timestamp = timestamp_from_ticks(score, pair.first);
            merge(result[timestamp], pair.second);
        }
    }
}

PlaybackContext::PlaybackContext(const Score *score) : score_(score) {}

dynamic_level_t PlaybackContext::appliable_dynamic_level(const track_idx_t track_idx, const int nominal_position_tick) const {
    TRACEFUNC;

    const AutomationCurve *curve = dynamics_curve(track_idx);
    if (!curve) {
        return ::mu::engraving::natural_dynamic_level;
    }

    const auto it = muse::find_less_or_equal(*curve, nominal_position_tick);
    if (it == curve->cend()) {
        return ::mu::engraving::natural_dynamic_level;
    }

    const auto next = std::next(it);
    if (next == curve->cend()) {
        return to_dynamic_level(it->second.value.out_value);
    }

    const real_t t = static_cast<real_t>(nominal_position_tick - it->first) / static_cast<real_t>(next->first - it->first);
    return to_dynamic_level(muse::mpe::evaluate_at(next->second.value, it->second.value.out_value, t));
}

std::pair<mpe::timestamp_t, PlayingTechniqueType> PlaybackContext::playing_technique(const track_idx_t track_idx,
                                                                                     const int nominal_position_tick) const {
    auto map_it = play_techniques_by_track_.find(track_idx);
    if (map_it == play_techniques_by_track_.cend()) {
        return std::make_pair(0, PlayingTechniqueType::NATURAL);
    }

    const PlayTechniquesMap &play_techniques_map = map_it->second;

    auto it = find_less_or_equal(play_techniques_map, nominal_position_tick);
    if (it == play_techniques_map.cend()) {
        return std::make_pair(0, PlayingTechniqueType::NATURAL);
    }

    return std::make_pair(timestamp_from_ticks(score_, it->first), it->second);
}

muse::mpe::timestamp_t PlaybackContext::find_playing_technique_timestamp(const track_idx_t track_idx, PlayingTechniqueType type,
                                                                         const int start_from_tick) const {
    auto map_it = play_techniques_by_track_.find(track_idx);
    if (map_it == play_techniques_by_track_.cend()) {
        return -1;
    }

    const PlayTechniquesMap &play_techniques_map = map_it->second;

    auto it = play_techniques_map.upper_bound(start_from_tick);

    for (; it != play_techniques_map.end(); ++it) {
        if (it->second == type) {
            return timestamp_from_ticks(score_, it->first);
        }
    }

    return -1;
}

std::map<timestamp_t, SoundPresetChangeEventList> PlaybackContext::sound_presets(const track_idx_t track_from,
                                                                                 const track_idx_t track_to) const {
    std::map<timestamp_t, SoundPresetChangeEventList> result;

    collect_by_track_range(sound_presets_by_track_, track_from, track_to, score_, used_tracks_, result,
                           [](SoundPresetChangeEventList &list, const SoundPresetChangeEventList &events) {
                               list.insert(list.end(), events.begin(), events.end());
                           });

    return result;
}

SoundPresetChangeEventList PlaybackContext::sound_presets(const track_idx_t track_idx, const int nominal_position_tick) const {
    auto presets_it = sound_presets_by_track_.find(track_idx);
    if (presets_it == sound_presets_by_track_.end()) {
        return {};
    }

    const SoundPresetsMap &map = presets_it->second;
    auto it = muse::find_less_or_equal(map, nominal_position_tick);
    if (it == map.end()) {
        return {};
    }

    return it->second;
}

std::map<timestamp_t, TextArticulationEventList> PlaybackContext::text_articulations(const track_idx_t track_from,
                                                                                     const track_idx_t track_to) const {
    std::map<timestamp_t, TextArticulationEventList> result;

    collect_by_track_range(text_articulations_by_track_, track_from, track_to, score_, used_tracks_, result,
                           [](TextArticulationEventList &list, const TextArticulationEvent &event) { list.push_back(event); });

    return result;
}

TextArticulationEvent PlaybackContext::text_articulation(const track_idx_t track_idx, const int nominal_position_tick) const {
    auto articulations_it = text_articulations_by_track_.find(track_idx);
    if (articulations_it == text_articulations_by_track_.end()) {
        return {};
    }

    const TextArticulationMap &map = articulations_it->second;
    auto it = muse::find_less_or_equal(map, nominal_position_tick);
    if (it == map.end()) {
        return {};
    }

    TextArticulationEvent result = it->second;
    result.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, it->first == nominal_position_tick);

    return result;
}

std::map<timestamp_t, SyllableEventList> PlaybackContext::syllables(const track_idx_t track_from, const track_idx_t track_to) const {
    std::map<timestamp_t, SyllableEventList> result;

    collect_by_track_range(syllables_by_track_, track_from, track_to, score_, used_tracks_, result,
                           [](SyllableEventList &list, const SyllableEvent &event) { list.push_back(event); });

    return result;
}

SyllableEvent PlaybackContext::syllable(const track_idx_t track_idx, const int nominal_position_tick) const {
    auto syllables_it = syllables_by_track_.find(track_idx);
    if (syllables_it == syllables_by_track_.end()) {
        return {};
    }

    const SyllableMap &map = syllables_it->second;
    auto it = muse::find_less_or_equal(map, nominal_position_tick);
    if (it == map.end()) {
        return {};
    }

    SyllableEvent result = it->second;
    result.flags.set_flag(SyllableEvent::STARTS_AT_PLAYBACK_POSITION, it->first == nominal_position_tick);

    return result;
}

DynamicAutomationLayers PlaybackContext::dynamic_level_layers(const track_idx_t track_from, const track_idx_t track_to) const {
    TRACEFUNC;

    if (!score_ || !score_->automation_data()) {
        return {};
    }

    DynamicAutomationLayers result;
    const AutomationCurve *last_curve = nullptr;
    DynamicAutomationMap last_level_map;

    for (track_idx_t track_idx = track_from; track_idx < track_to; ++track_idx) {
        const AutomationCurve *curve = dynamics_curve(track_idx);
        if (!curve || curve->empty()) {
            continue;
        }

        if (curve != last_curve) {
            last_level_map.clear();
            last_curve = curve;
            auto hint = last_level_map.end();

            // AutomationCurve ticks are always in expanded utick space, regardless of the Play Repeats setting
            const TempoTimeline &expanded_timeline = score_->tempo_timeline(/*expandRepeats*/ true);

            if (curve->cbegin()->first > 0) {
                AutomationPoint natural_point;
                natural_point.value.out_value = real_t(::mu::engraving::natural_dynamic_level) / real_t(::muse::mpe::max_dynamic_level);
                hint = last_level_map.insert(hint, {expanded_timeline.utick2utime(0) * 1000000, natural_point.value});
            }

            for (const auto &[tick, point] : *curve) {
                hint = last_level_map.insert(hint, {expanded_timeline.utick2utime(tick) * 1000000, point.value});
            }
        }

        result[static_cast<layer_idx_t>(track_idx)] = last_level_map;
    }

    return result;
}

void PlaybackContext::update(const track_idx_t track_from, const track_idx_t track_to, const int tick_from, const int tick_to,
                             bool expand_repeats, const std::function<void()> &checkpoint) {
    TRACEFUNC;

    IF_ASSERT_FAILED(score_) { return; }

    IF_ASSERT_FAILED(track_from <= track_to) { return; }

    dynamics_curve_by_track_.clear();

    for (const RepeatSegment *repeat_segment : score_->repeat_list(expand_repeats)) {
        if (checkpoint) checkpoint();
        const int repeat_start_tick = repeat_segment->tick;
        const int repeat_end_tick = repeat_segment->end_tick();

        if (repeat_start_tick > tick_to || repeat_end_tick <= tick_from) {
            continue;
        }

        std::vector<const MeasureRepeat *> measure_repeats;
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;

        for (const Measure *measure : repeat_segment->measure_list()) {
            if (checkpoint) checkpoint();
            const int measure_start_tick = measure->tick().ticks();
            const int measure_end_tick = measure->end_tick().ticks();

            if (measure_start_tick > tick_to || measure_end_tick <= tick_from) {
                continue;
            }

            for (const Segment *segment = measure->first(); segment; segment = segment->next()) {
                if (checkpoint) checkpoint();
                const int segment_tick = segment->tick().ticks();
                if (segment_tick > tick_to) {
                    break;
                }

                if (segment_tick < tick_from) {
                    continue;
                }

                int segment_start_tick = segment_tick + tick_position_offset;

                handle_segment_elements(repeat_segment, segment, segment_start_tick, track_from, track_to, measure_repeats);
                handle_segment_annotations(segment, segment_start_tick, track_from, track_to);
            }
        }

        handle_measure_repeats(measure_repeats, tick_position_offset);
    }
}

void PlaybackContext::clear(const track_idx_t track_from, const track_idx_t track_to, const int tick_from, const int tick_to) {
    const auto erase_tick_range_by_track = [track_from, track_to, tick_from, tick_to](auto &by_track_map) {
        for (auto it = by_track_map.lower_bound(track_from); it != by_track_map.end() && it->first < track_to;) {
            auto &inner_map = it->second;
            inner_map.erase(inner_map.lower_bound(tick_from), inner_map.upper_bound(tick_to));

            if (inner_map.empty()) {
                it = by_track_map.erase(it);
            } else {
                ++it;
            }
        }
    };

    erase_tick_range_by_track(sound_presets_by_track_);
    erase_tick_range_by_track(text_articulations_by_track_);
    erase_tick_range_by_track(syllables_by_track_);
    erase_tick_range_by_track(play_techniques_by_track_);
    erase_tick_range_by_track(multi_verse_lyrics_position_map_);

    muse::remove_if(current_verse_num_by_chord_rest_, [track_from, track_to, tick_from, tick_to](const auto &pair) {
        const track_idx_t track = pair.first->track();
        const int tick = pair.first->tick().ticks();
        return track >= track_from && track < track_to && tick >= tick_from && tick <= tick_to;
    });
}

bool PlaybackContext::has_sound_flags(const track_idx_t track_from, const track_idx_t track_to) const {
    auto has_entry_in_range = [track_from, track_to](const auto &by_track_map) {
        auto it = by_track_map.lower_bound(track_from);
        return it != by_track_map.end() && it->first < track_to;
    };

    return has_entry_in_range(sound_presets_by_track_) || has_entry_in_range(text_articulations_by_track_);
}

void PlaybackContext::update_play_tech_map(const Part *part, const PlayTechAnnotation *annotation, const int segment_position_tick) {
    if (!annotation->play_play_tech_annotation()) {
        return;
    }
    const PlayingTechniqueType type = annotation->technique_type();
    if (type == PlayingTechniqueType::UNDEFINED) {
        return;
    }

    const bool cancel_play_techniques =
        (type == PlayingTechniqueType::NATURAL || type == PlayingTechniqueType::OPEN) && !text_articulations_by_track_.empty();

    TextArticulationEvent text_articulation;
    text_articulation.text = mpe::ordinary_playing_technique_code;

    const TrackRange track_range = part->track_range();
    for (track_idx_t idx = track_range.start_track; idx < track_range.end_track; ++idx) {
        play_techniques_by_track_[idx][segment_position_tick] = type;

        if (cancel_play_techniques) {
            text_articulation.layer_idx = static_cast<layer_idx_t>(idx);
            text_articulations_by_track_[idx][segment_position_tick] = text_articulation;
        }
    }
}

void PlaybackContext::update_sound_preset_and_text_articulation_map(const Part *part, const SoundFlagMap &flags_on_segment,
                                                                    const int segment_position_tick) {
    auto track_accepted = [&flags_on_segment](const SoundFlag *flag, track_idx_t track_idx) {
        staff_idx_t staff_idx = track2staff(track_idx);

        if (flag->staff_idx() == staff_idx) {
            return true;
        }

        if (flag->apply_to_all_staves()) {
            return !muse::contains(flags_on_segment, staff_idx);
        }

        return false;
    };

    const TrackRange track_range = part->track_range();

    for (const auto &pair : flags_on_segment) {
        const SoundFlag *flag = pair.second;

        for (track_idx_t track_idx = track_range.start_track; track_idx < track_range.end_track; ++track_idx) {
            if (!track_accepted(flag, track_idx)) {
                continue;
            }

            SoundPresetChangeEventList presets;

            for (const String &sound_preset : flag->sound_presets()) {
                if (sound_preset.empty()) {
                    continue;
                }

                SoundPresetChangeEvent event;
                event.code = sound_preset;
                event.layer_idx = static_cast<layer_idx_t>(track_idx);
                presets.emplace_back(std::move(event));
            }

            if (!presets.empty()) {
                SoundPresetChangeEventList &target = sound_presets_by_track_[track_idx][segment_position_tick];
                target.insert(target.end(), std::make_move_iterator(presets.begin()), std::make_move_iterator(presets.end()));
            }

            if (!flag->playing_technique().empty()) {
                TextArticulationEvent event;
                event.text = flag->playing_technique();
                event.layer_idx = static_cast<layer_idx_t>(track_idx);
                text_articulations_by_track_[track_idx][segment_position_tick] = std::move(event);
            }
        }

        if (flag->playing_technique() == mpe::ordinary_playing_technique_code) {
            for (track_idx_t track_idx = track_range.start_track; track_idx < track_range.end_track; ++track_idx) {
                play_techniques_by_track_[track_idx][segment_position_tick] = PlayingTechniqueType::NATURAL;
            }
        }
    }
}

void PlaybackContext::update_syllable_map(const TextBase *text, const int segment_position_tick) {
    IF_ASSERT_FAILED(text->is_lyrics() || text->is_sticking()) { return; }

    if (text->empty()) {
        return;
    }

    SyllableEvent syllable;
    syllable.text = text->plain_text();

    if (text->is_lyrics()) {
        const Lyrics *lyrics = to_lyrics(text);

        switch (lyrics->syllabic()) {
            case LyricsSyllabic::BEGIN:
            case LyricsSyllabic::MIDDLE: syllable.flags.set_flag(SyllableEvent::HYPHENED_TO_NEXT); break;
            case LyricsSyllabic::SINGLE:
            case LyricsSyllabic::END: break;
        }
    }

    const staff_idx_t staff_idx = text->staff_idx();

    for (voice_idx_t voice_idx = 0; voice_idx < ::mu::engraving::voices; ++voice_idx) {
        track_idx_t track_idx = staff2track(staff_idx, voice_idx);
        syllable.layer_idx = static_cast<layer_idx_t>(track_idx);
        syllables_by_track_[track_idx][segment_position_tick] = syllable;
    }
}

void PlaybackContext::handle_segment_annotations(const Segment *segment, const int segment_position_tick, const track_idx_t track_from,
                                                 const track_idx_t track_to) {
    std::unordered_map<const Part *, SoundFlagMap> sound_flags_by_part;

    for (const EngravingItem *annotation : segment->annotations()) {
        if (!annotation || !annotation->part()) {
            continue;
        }

        const Part *annotation_part = annotation->part();
        const TrackRange annotation_part_track_range = annotation_part->track_range();
        if (annotation_part_track_range.start_track >= track_to || annotation_part_track_range.end_track <= track_from) {
            continue;
        }

        if (annotation->is_play_tech_annotation()) {
            update_play_tech_map(annotation_part, to_play_tech_annotation(annotation), segment_position_tick);
            continue;
        }

        if (annotation->is_sticking()) {
            update_syllable_map(to_text_base(annotation), segment_position_tick);
            continue;
        }

        if (annotation->is_staff_text()) {
            if (const SoundFlag *flag = to_staff_text(annotation)->sound_flag()) {
                if (sound_flag_playable(flag)) {
                    sound_flags_by_part[annotation_part].emplace(flag->staff_idx(), flag);
                }
            }
        }
    }

    for (const auto &pair : sound_flags_by_part) {
        const Part *part = pair.first;
        const SoundFlagMap &flags_on_segment = pair.second;

        if (!flags_on_segment.empty()) {
            update_sound_preset_and_text_articulation_map(part, flags_on_segment, segment_position_tick);
        }
    }
}

void PlaybackContext::handle_segment_elements(const RepeatSegment *repeat, const Segment *segment, const int segment_position_tick,
                                              const track_idx_t track_from, const track_idx_t track_to,
                                              std::vector<const MeasureRepeat *> &found_measure_repeats) {
    for (track_idx_t track = track_from; track < track_to; ++track) {
        const EngravingItem *item = segment->element(track);
        if (!item) {
            continue;
        }

        if (item->is_measure_repeat()) {
            found_measure_repeats.push_back(to_measure_repeat(item));
            continue;
        }

        if (item->is_chord_rest()) {
            used_tracks_.insert(track);

            const ChordRest *chord_rest = to_chord_rest(item);
            if (chord_rest->lyrics().empty()) {
                continue;
            }

            const Lyrics *lyrics = nullptr;

            auto verse_num_it = current_verse_num_by_chord_rest_.find(chord_rest);
            if (verse_num_it == current_verse_num_by_chord_rest_.end()) {
                current_verse_num_by_chord_rest_[chord_rest] = 0;
                lyrics = chord_rest->lyrics(0);
                if (chord_rest->lyrics().size() > 1) {
                    multi_verse_lyrics_position_map_[track].insert(chord_rest->tick().ticks());
                }
            } else if (has_only_one_lyrics_verse(repeat, track)) {
                lyrics = chord_rest->lyrics(0);
            } else {
                verse_num_it->second++;
                lyrics = chord_rest->lyrics(verse_num_it->second);
            }

            if (lyrics) {
                update_syllable_map(lyrics, segment_position_tick);
            }
        }
    }
}

template <typename ItemsMap>
static void copy_items_in_range(ItemsMap &source, const int range_start_tick, const int range_end_tick, const int new_items_offset_tick) {
    auto start_it = source.lower_bound(range_start_tick);
    if (start_it == source.end()) {
        return;
    }

    auto end_it = source.lower_bound(range_end_tick);

    ItemsMap new_items;
    for (auto it = start_it; it != end_it; ++it) {
        int tick = it->first + new_items_offset_tick;
        new_items.insert_or_assign(tick, it->second);
    }

    source.merge(std::move(new_items));
}

template <typename ItemsMap>
static void copy_items_in_range(std::map<track_idx_t, ItemsMap> &source, const track_idx_t track_from, const track_idx_t track_to,
                                const int range_start_tick, const int range_end_tick, const int new_items_offset_tick) {
    for (auto it = source.lower_bound(track_from); it != source.end() && it->first < track_to; ++it) {
        copy_items_in_range(it->second, range_start_tick, range_end_tick, new_items_offset_tick);
    }
}

void PlaybackContext::handle_measure_repeats(const std::vector<const MeasureRepeat *> &measure_repeats, const int tick_position_offset) {
    for (const MeasureRepeat *mr : measure_repeats) {
        const Part *part = mr->part();
        if (!part) {
            continue;
        }

        const Measure *curr_measure = mr->first_measure_of_group();
        if (!curr_measure) {
            continue;
        }

        const Measure *referring_measure = mr->referring_measure(curr_measure);
        if (!referring_measure) {
            continue;
        }

        int current_measure_tick = curr_measure->tick().ticks();
        int referring_measure_tick = referring_measure->tick().ticks();
        int new_items_offset_tick = current_measure_tick - referring_measure_tick;

        const TrackRange track_range = part->track_range();

        for (int num = 0; num < mr->num_measures(); ++num) {
            int start_tick = referring_measure->tick().ticks() + tick_position_offset;
            int end_tick = referring_measure->end_tick().ticks() + tick_position_offset;

            copy_items_in_range(sound_presets_by_track_, track_range.start_track, track_range.end_track, start_tick, end_tick,
                                new_items_offset_tick);
            copy_items_in_range(text_articulations_by_track_, track_range.start_track, track_range.end_track, start_tick, end_tick,
                                new_items_offset_tick);
            copy_items_in_range(syllables_by_track_, track_range.start_track, track_range.end_track, start_tick, end_tick,
                                new_items_offset_tick);
            copy_items_in_range(play_techniques_by_track_, track_range.start_track, track_range.end_track, start_tick, end_tick,
                                new_items_offset_tick);

            curr_measure = curr_measure->next_measure();
            if (!curr_measure) {
                break;
            }

            referring_measure = mr->referring_measure(curr_measure);
            if (!referring_measure) {
                break;
            }
        }
    }
}

const mu::engraving::AutomationCurve *PlaybackContext::dynamics_curve(const track_idx_t track_idx) const {
    auto cache_it = dynamics_curve_by_track_.find(track_idx);
    if (cache_it != dynamics_curve_by_track_.end()) {
        return cache_it->second;
    }

    const AutomationDataConstPtr automation = score_ ? score_->automation_data() : nullptr;
    const Staff *staff = automation ? score_->staff(track2staff(track_idx)) : nullptr;

    const AutomationCurve *curve = nullptr;
    if (staff) {
        AutomationCurveKey key = AutomationCurveKey::staff(AutomationType::DYNAMICS, staff->id(), track2voice(track_idx));
        curve = &automation->curve(key);

        if (curve->empty()) {
            key = key.without_voice();
            curve = &automation->curve(key);
        }
    }

    dynamics_curve_by_track_.emplace(track_idx, curve);
    return curve;
}

bool PlaybackContext::has_only_one_lyrics_verse(const RepeatSegment *repeat, const track_idx_t track) const {
    if (multi_verse_lyrics_position_map_.empty()) {
        return true;
    }

    const auto track_it = multi_verse_lyrics_position_map_.find(track);
    if (track_it == multi_verse_lyrics_position_map_.cend()) {
        return true;
    }

    const int start_tick = repeat->tick;
    const int end_tick = repeat->end_tick();
    const auto start = track_it->second.lower_bound(start_tick);
    const auto end = track_it->second.lower_bound(end_tick);

    return start == end;
}
