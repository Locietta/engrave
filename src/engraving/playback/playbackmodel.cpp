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

#include "playbackmodel.h"

#include <span>
#include <stdexcept>
#include <tuple>

#include "defer.h"
#include "dom/fret.h"
#include "dom/harmony.h"
#include "dom/instrument.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/part.h"
#include "dom/repeatlist.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/tie.h"
#include "dom/tremolotwochord.h"
#include "editing/transaction/undoablecommand.h"
#include "log.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::mpe;

static const String metronome_instrument_id(u"metronome");
static const String chord_symbols_instrument_id(u"chord_symbols");

const InstrumentTrackId PlaybackModel::metronome_track_id_value = {999, ::metronome_instrument_id};

static const Harmony *find_chord_symbol(const EngravingItem *item) {
    if (item->is_harmony()) {
        return to_harmony(item);
    } else if (item->is_fret_diagram()) {
        return to_fret_diagram(item)->harmony();
    }

    return nullptr;
}

static bool is_scoped_automation_change(const ScoreChanges &changes) {
    if (!changes.is_valid_boundary()) {
        return false;
    }

    bool has_automation_change = false;

    for (const auto &pair : changes.changed_objects) {
        if (pair.first->type() != ElementType::SCORE) {
            continue;
        }

        if (pair.second.size() != 1 || !muse::contains(pair.second, CommandType::EDIT_AUTOMATION_POINTS)) {
            return false;
        }

        has_automation_change = true;
    }

    return has_automation_change;
}

PlaybackModel::~PlaybackModel() { detach_from_score(); }

void PlaybackModel::detach_from_score() {
    if (listened_score_ && Score::is_alive(listened_score_)) {
        listened_score_->remove_changes_listener(this);
    }
    listened_score_ = nullptr;
}

void PlaybackModel::load(Score *score, const std::function<void()> &checkpoint_callback, PlaybackScore *flat) {
    TRACEFUNC;

    if (!score || score->measures()->empty() || !score->last_measure()) {
        return;
    }

    // The flat store mirrors map entries as the walk creates them, so it needs a model that never loaded
    if (flat && score_) {
        throw std::logic_error("PlaybackModel: dual execution needs a model that has not loaded a score");
    }

    checkpoint_ = checkpoint_callback;
    flat_ = flat;
    if (flat_) {
        // Bounded polls inside the freeze loops, through the same latched checkpoint as the walk
        flat_->progress.checkpoint = [this] { checkpoint(); };
    }
    DEFER {
        checkpoint_ = {};
        if (flat_) {
            flat_->progress.checkpoint = {};
        }
        flat_ = nullptr;
    };
    checkpoint();
    score_ = score;
    playback_ctx_ = std::make_shared<PlaybackContext>(score);

    detach_from_score();
    listened_score_ = score->master_score();
    listened_score_->add_changes_listener(this);

    update(0, score_->last_measure()->end_tick().ticks(), 0, score_->ntracks());

    changed_track_id_set_.clear();
}

void PlaybackModel::on_score_changes(const ScoreChanges &changes) {
    if (should_skip_changes(changes)) {
        return;
    }

    const TickBoundaries tick_range = tick_boundaries(changes);
    const TrackBoundaries track_range = track_boundaries(changes);
    ChangedTrackIdSet track_changes;

    clear_expired_tracks();
    playback_ctx_->clear(track_range.track_from, track_range.track_to, tick_range.tick_from, tick_range.tick_to);
    clear_expired_events(tick_range.tick_from, tick_range.tick_to, track_range.track_from, track_range.track_to, &track_changes);

    update(tick_range.tick_from, tick_range.tick_to, track_range.track_from, track_range.track_to, &track_changes);

    notify_about_changes(track_changes);
}

void PlaybackModel::reload() {
    if (!score_) {
        return;
    }

    TRACEFUNC;

    const int track_from = 0;
    const size_t track_to = score_->ntracks();

    const Measure *last_measure = score_->last_measure();

    const int tick_from = 0;
    const int tick_to = last_measure ? last_measure->end_tick().ticks() : 0;

    clear_expired_tracks();
    playback_ctx_->clear(track_from, track_to, tick_from, tick_to);

    for (auto &pair : playback_data_map_) {
        pair.second.origin_events.clear();
    }

    update(tick_from, tick_to, track_from, track_to);

    changed_track_id_set_.clear();
}

void PlaybackModel::set_send_events_on_score_change(const InstrumentTrackId &track_id, bool send) {
    send_events_on_score_change_map_[track_id] = send;

    if (send) {
        auto it = changed_track_id_set_.find(track_id);
        if (it != changed_track_id_set_.end()) {
            send_events(track_id);
            changed_track_id_set_.erase(it);
        }
    }
}

void PlaybackModel::send_events_for_changed_tracks() {
    if (changed_track_id_set_.empty()) {
        return;
    }

    TRACEFUNC;

    for (const InstrumentTrackId &track_id : changed_track_id_set_) {
        send_events(track_id);
    }

    changed_track_id_set_.clear();
}

bool PlaybackModel::is_play_repeats_enabled() const { return expand_repeats_; }

void PlaybackModel::set_play_repeats(const bool is_enabled) { expand_repeats_ = is_enabled; }

bool PlaybackModel::is_play_chord_symbols_enabled() const { return play_chord_symbols_; }

void PlaybackModel::set_play_chord_symbols(const bool is_enabled) { play_chord_symbols_ = is_enabled; }

bool PlaybackModel::use_score_dynamics_for_offstream_playback() const { return use_score_dynamics_for_offstream_playback_; }

void PlaybackModel::set_use_score_dynamics_for_offstream_playback(bool use) { use_score_dynamics_for_offstream_playback_ = use; }

bool PlaybackModel::is_metronome_enabled() const { return metronome_enabled_; }

void PlaybackModel::set_is_metronome_enabled(const bool is_enabled) {
    if (metronome_enabled_ == is_enabled) {
        return;
    }

    metronome_enabled_ = is_enabled;
    reload_metronome_events();
}

const InstrumentTrackId &PlaybackModel::metronome_track_id() const { return ::mu::engraving::PlaybackModel::metronome_track_id_value; }

InstrumentTrackId PlaybackModel::chord_symbols_track_id(const ID &part_id) const { return {part_id, ::chord_symbols_instrument_id}; }

bool PlaybackModel::is_chord_symbols_track(const InstrumentTrackId &track_id) const {
    return track_id == chord_symbols_track_id(track_id.part_id);
}

bool PlaybackModel::has_sound_flags(const InstrumentTrackId &track_id) const {
    const Part *part = score_ ? score_->part_by_id(track_id.part_id) : nullptr;
    if (!part) {
        return false;
    }

    const TrackRange track_range = part->track_range();
    return playback_ctx_->has_sound_flags(track_range.start_track, track_range.end_track);
}

PlaybackData &PlaybackModel::resolve_track_playback_data(const InstrumentTrackId &track_id) {
    auto search = playback_data_map_.find(track_id);

    if (search != playback_data_map_.cend()) {
        return search->second;
    }

    const Part *part = score_ ? score_->part_by_id(track_id.part_id.to_uint64()) : nullptr;

    if (!part) {
        static PlaybackData empty;
        return empty;
    }

    const TrackRange track_range = part->track_range();
    update(0, score_->last_measure()->tick().ticks(), track_range.start_track, track_range.end_track);

    return playback_data_map_[track_id];
}

PlaybackData &PlaybackModel::resolve_track_playback_data(const ID &part_id, const String &instrument_id) {
    return resolve_track_playback_data(id_key(part_id, instrument_id));
}

PlaybackEventsMap PlaybackModel::trigger_events_for_items(const std::vector<const EngravingItem *> &items, muse::mpe::duration_t duration) {
    if (items.empty()) {
        return {};
    }

    InstrumentTrackId track_id = id_key(items);
    if (!track_id.is_valid()) {
        return {};
    }

    auto track_playback_data_it = playback_data_map_.find(track_id);
    if (track_playback_data_it == playback_data_map_.cend()) {
        return {};
    }

    PlaybackData &track_playback_data = track_playback_data_it->second;
    ArticulationsProfilePtr profile =
        profiles_repository() ? profiles_repository()->default_profile(track_playback_data.setup_data.category) : nullptr;
    if (!profile) {
        LOGE() << "unsupported instrument family: " << track_id.part_id.to_uint64();
        return {};
    }

    const RepeatList &repeats = repeat_list();
    const int first_item_utick = repeats.tick2utick(items.front()->tick().ticks());
    const track_idx_t first_item_track_idx = items.front()->track();
    constexpr timestamp_t timestamp = 0;

    PlaybackEventsMap result;
    PlaybackEventList &events = result[timestamp];

    SoundPresetChangeEventList sound_presets = playback_ctx_->sound_presets(first_item_track_idx, first_item_utick);
    if (!sound_presets.empty()) {
        events.insert(events.end(), std::make_move_iterator(sound_presets.begin()), std::make_move_iterator(sound_presets.end()));
    }

    const TextArticulationEvent text_articulation = playback_ctx_->text_articulation(first_item_track_idx, first_item_utick);
    if (!text_articulation.text.empty()) {
        events.push_back(text_articulation);
    }

    const SyllableEvent syllable = playback_ctx_->syllable(first_item_track_idx, first_item_utick);
    if (!syllable.text.empty()) {
        events.push_back(syllable);
    }

    dynamic_level_t dynamic_level = dynamic_level_from_type(muse::mpe::DynamicType::NATURAL);

    for (const EngravingItem *item : items) {
        if (use_score_dynamics_for_offstream_playback_) {
            if (!item->is_note() || to_note(item)->user_velocity() == 0) {
                const int utick = repeats.tick2utick(item->tick().ticks());
                dynamic_level = playback_ctx_->appliable_dynamic_level(item->track(), utick);
            }
        }

        if (item->is_harmony()) {
            renderer_.render_chord_symbol(to_harmony(item), timestamp, duration, dynamic_level, profile, result);
            continue;
        }

        renderer_.render(item, timestamp, duration, dynamic_level, playback_ctx_, profile, result);
    }

    return result;
}

PlaybackEventsMap PlaybackModel::trigger_metronome(int tick) {
    auto track_playback_data = playback_data_map_.find(::mu::engraving::PlaybackModel::metronome_track_id_value);
    if (track_playback_data == playback_data_map_.cend()) {
        return {};
    }

    const ArticulationsProfilePtr profile = default_acticulation_profile(::mu::engraving::PlaybackModel::metronome_track_id_value);

    PlaybackEventsMap result;
    renderer_.render_metronome(score_, tick, 0, profile, result);
    return result;
}

PlaybackEventsMap PlaybackModel::trigger_count_in(int tick, muse::mpe::duration_t &count_in_duration) {
    auto track_playback_data = playback_data_map_.find(::mu::engraving::PlaybackModel::metronome_track_id_value);
    if (track_playback_data == playback_data_map_.cend()) {
        return {};
    }

    const ArticulationsProfilePtr profile = default_acticulation_profile(::mu::engraving::PlaybackModel::metronome_track_id_value);

    PlaybackEventsMap result;
    renderer_.render_count_in(score_, tick, 0, profile, result, count_in_duration);
    return result;
}

InstrumentTrackIdSet PlaybackModel::existing_track_id_set() const {
    InstrumentTrackIdSet result;
    result.reserve(playback_data_map_.size());

    for (const auto &pair : playback_data_map_) {
        result.insert(pair.first);
    }

    return result;
}

void PlaybackModel::update(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                           ChangedTrackIdSet *track_changes) {
    score_->master_score()->set_expand_repeats(expand_repeats_);
    update_setup_data();
    update_context(track_from, track_to, tick_from, tick_to);
    update_events(tick_from, tick_to, track_from, track_to, track_changes);
}

void PlaybackModel::update_setup_data() {
    EID score_eid = score_->eid();
    if (!score_eid.is_valid()) {
        score_eid = score_->assign_new_eid();
    }

    std::string score_id = score_eid.to_std_string();

    for (const Part *part : score_->parts()) {
        checkpoint();
        for (const auto &pair : part->instruments()) {
            checkpoint();
            InstrumentTrackId track_id = id_key(part->id(), pair.second->id());
            if (!track_id.is_valid() || muse::contains(playback_data_map_, track_id)) {
                continue;
            }

            PlaybackSetupData &setup_data = track_data(track_id).setup_data;
            setup_resolver_.resolve_setup_data(pair.second, setup_data);
            setup_data.score_id = score_id;
            emit_setup(track_id, setup_data);
        }

        if (part->has_chord_symbol()) {
            InstrumentTrackId track_id = chord_symbols_track_id(part->id());
            PlaybackSetupData &setup_data = track_data(track_id).setup_data;
            setup_resolver_.resolve_chord_symbols_setup_data(part->instrument(), setup_data);
            setup_data.score_id = score_id;
            emit_setup(track_id, setup_data);
        }
    }

    PlaybackSetupData &metronome_setup_data = track_data(::mu::engraving::PlaybackModel::metronome_track_id_value).setup_data;
    setup_resolver_.resolve_metronome_setup_data(metronome_setup_data);
    metronome_setup_data.score_id = score_id;
    emit_setup(::mu::engraving::PlaybackModel::metronome_track_id_value, metronome_setup_data);
}

PlaybackData &PlaybackModel::track_data(const InstrumentTrackId &track_id) {
    if (flat_) {
        flat_->track(track_id);
    }
    return playback_data_map_[track_id];
}

void PlaybackModel::emit_setup(const InstrumentTrackId &track_id, const PlaybackSetupData &setup_data) {
    if (flat_) {
        flat_->track(track_id).setup = setup_data;
    }
}

template <typename Render>
void PlaybackModel::emit_events(const InstrumentTrackId &track_id, Render &&render) {
    PlaybackEventsMap &events = track_data(track_id).origin_events;
    if (flat_) {
        DualEventsSink sink{{events}, {flat_->track(track_id), flat_->progress}};
        render(sink);
        return;
    }
    EventsMapSink sink{events};
    render(sink);
}

void PlaybackModel::update_context(const track_idx_t track_from, const track_idx_t track_to, const int tick_from, const int tick_to) {
    playback_ctx_->update(track_from, track_to, tick_from, tick_to, expand_repeats_, checkpoint_);

    for (const Part *part : score_->parts()) {
        checkpoint();
        const TrackRange track_range = part->track_range();
        if (track_to <= track_range.start_track || track_from >= track_range.end_track) {
            continue;
        }

        InstrumentTrackIdSet track_ids = part->instrument_track_id_set();
        if (part->has_chord_symbol()) {
            track_ids.insert(chord_symbols_track_id(part->id()));
        }

        if (track_ids.empty()) {
            continue;
        }

        const PartData data{playback_ctx_->dynamic_level_layers(track_range.start_track, track_range.end_track),
                            playback_ctx_->sound_presets(track_range.start_track, track_range.end_track),
                            playback_ctx_->text_articulations(track_range.start_track, track_range.end_track),
                            playback_ctx_->syllables(track_range.start_track, track_range.end_track)};

        for (const InstrumentTrackId &track_id : track_ids) {
            checkpoint();
            apply_context_to_track_data(track_id, data);
        }
    }
}

void PlaybackModel::apply_context_to_track_data(const InstrumentTrackId &track_id, const PartData &data) {
    PlaybackData &playback_data = this->track_data(track_id);
    playback_data.dynamics = data.dynamics;
    if (flat_) {
        freeze_dynamics(flat_->track(track_id), data.dynamics, flat_->progress);
    }

    std::set<timestamp_t> new_event_timestamps;

    const auto append_events = [this, &track_id, &playback_data, &new_event_timestamps](const auto &events) {
        for (const auto &pair : events) {
            checkpoint();
            PlaybackEventList &list = playback_data.origin_events[pair.first];

            //! NOTE: this assumes that the list has already been cleared in clearExpiredEvents
            //! Necessary to prevent event duplication (unchanged lists should not be modified)
            if (list.empty() || muse::contains(new_event_timestamps, pair.first)) {
                list.insert(list.end(), pair.second.begin(), pair.second.end());
                new_event_timestamps.insert(pair.first);
                // The seam for context events, which bypass the renderer sinks: the same list, after the legacy insert
                if (flat_) {
                    freeze_cold_events(flat_->track(track_id), pair.first, std::span{pair.second}, flat_->progress);
                }
            }
        }
    };

    append_events(data.sound_presets);
    append_events(data.text_articulations);
    append_events(data.syllables);
}

void PlaybackModel::process_segment(const int tick_position_offset, const Segment *segment, const std::set<staff_idx_t> &staff_idx_set,
                                    bool is_first_chord_rest_segment_of_measure, ChangedTrackIdSet *track_changes) {
    for (const EngravingItem *item : segment->annotations()) {
        checkpoint();
        if (!item || !item->part()) {
            continue;
        }

        const Harmony *chord_symbol = find_chord_symbol(item);
        if (!chord_symbol) {
            continue;
        }

        staff_idx_t staff_idx = item->staff_idx();
        if (staff_idx_set.find(staff_idx) == staff_idx_set.cend()) {
            continue;
        }

        InstrumentTrackId track_id = chord_symbols_track_id(item->part()->id());

        ArticulationsProfilePtr profile = default_acticulation_profile(track_id);
        if (!profile) {
            LOGE() << "unsupported instrument family: " << item->part()->id();
            continue;
        }

        if (chord_symbol->play()) {
            emit_events(track_id, [&](auto &sink) {
                renderer_.render_chord_symbol(chord_symbol, tick_position_offset, profile, playback_ctx_, sink);
            });
        }

        collect_changes_tracks(track_id, track_changes);
    }

    if (segment->is_time_tick_type()) {
        return; // optimization: search only for annotations
    }

    for (const EngravingItem *item : segment->elist()) {
        checkpoint();
        if (!item || !item->is_chord_rest() || !item->part()) {
            continue;
        }

        staff_idx_t staff_idx = item->staff_idx();
        if (staff_idx_set.find(staff_idx) == staff_idx_set.cend()) {
            continue;
        }

        InstrumentTrackId track_id = id_key(item);

        if (!track_id.is_valid()) {
            continue;
        }

        if (is_first_chord_rest_segment_of_measure) {
            if (item->is_measure_repeat()) {
                const MeasureRepeat *measure_repeat = to_measure_repeat(item);
                const Measure *current_measure = measure_repeat->measure();

                process_measure_repeat(tick_position_offset, measure_repeat, current_measure, staff_idx, track_changes);

                continue;
            } else if (item->voice() == 0) {
                const Measure *current_measure = segment->measure();

                if (current_measure->measure_repeat_count(staff_idx) > 0) {
                    const MeasureRepeat *measure_repeat = current_measure->measure_repeat_element(staff_idx);

                    process_measure_repeat(tick_position_offset, measure_repeat, current_measure, staff_idx, track_changes);
                    continue;
                }
            }
        }

        if (item->is_rest_family()) {
            continue;
        }

        ArticulationsProfilePtr profile = default_acticulation_profile(track_id);
        if (!profile) {
            LOGE() << "unsupported instrument family: " << item->part()->id();
            continue;
        }

        emit_events(track_id, [&](auto &sink) { renderer_.render(item, tick_position_offset, profile, playback_ctx_, sink); });

        collect_changes_tracks(track_id, track_changes);
    }
}

void PlaybackModel::process_measure_repeat(const int tick_position_offset, const MeasureRepeat *measure_repeat,
                                           const Measure *current_measure, const staff_idx_t staff_idx, ChangedTrackIdSet *track_changes) {
    if (!measure_repeat || !current_measure) {
        return;
    }

    const Measure *referring_measure = measure_repeat->referring_measure(current_measure);
    if (!referring_measure) {
        return;
    }

    IF_ASSERT_FAILED(referring_measure != current_measure) { return; }

    int current_measure_tick = current_measure->tick().ticks();
    int referring_measure_tick = referring_measure->tick().ticks();
    int repeat_position_tick_offset = current_measure_tick - referring_measure_tick;
    int tick_from = tick_position_offset + repeat_position_tick_offset;

    std::set<staff_idx_t> staff_to_process_idx_set{staff_idx};
    int chord_rest_segment_num = -1;

    for (const Segment *seg = referring_measure->first(); seg; seg = seg->next()) {
        checkpoint();
        if (!seg->is_chord_rest_type() && !seg->is_time_tick_type()) {
            continue;
        }

        if (seg->is_chord_rest_type()) {
            chord_rest_segment_num++;
        }

        process_segment(tick_from, seg, staff_to_process_idx_set, chord_rest_segment_num == 0, track_changes);
    }
}

void PlaybackModel::update_events(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                                  ChangedTrackIdSet *track_changes) {
    TRACEFUNC;

    std::set<staff_idx_t> staff_to_process_idx_set = score_->staff_idx_set_from_range(track_from, track_to, [](const Staff &staff) {
        return staff.is_primary_staff(); // skip linked staves
    });

    const ArticulationsProfilePtr metronome_profile =
        default_acticulation_profile(::mu::engraving::PlaybackModel::metronome_track_id_value);
    // The metronome entry exists before the walk, as the legacy sink construction here always created it
    std::ignore = track_data(::mu::engraving::PlaybackModel::metronome_track_id_value);

    for (const RepeatSegment *repeat_segment : repeat_list()) {
        checkpoint();
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;
        int repeat_start_tick = repeat_segment->tick;
        int repeat_end_tick = repeat_segment->end_tick();

        if (repeat_start_tick > tick_to || repeat_end_tick <= tick_from) {
            continue;
        }

        for (const Measure *measure : repeat_segment->measure_list()) {
            checkpoint();
            int measure_start_tick = measure->tick().ticks();
            int measure_end_tick = measure->end_tick().ticks();

            if (measure_start_tick > tick_to || measure_end_tick <= tick_from) {
                continue;
            }

            int chord_rest_segment_num = -1;

            for (const Segment *segment = measure->first(); segment; segment = segment->next()) {
                checkpoint();
                if (!segment->is_chord_rest_type() && !segment->is_time_tick_type()) {
                    continue;
                }

                int segment_start_tick = segment->tick().ticks();
                int segment_end_tick = segment_start_tick + segment->ticks().ticks();

                if (segment_start_tick > tick_to || segment_end_tick <= tick_from) {
                    continue;
                }

                if (segment->is_chord_rest_type()) {
                    chord_rest_segment_num++;
                }

                process_segment(tick_position_offset, segment, staff_to_process_idx_set, chord_rest_segment_num == 0, track_changes);
            }

            if (metronome_enabled_) {
                emit_events(::mu::engraving::PlaybackModel::metronome_track_id_value, [&](auto &sink) {
                    renderer_.render_metronome(score_, measure, tick_position_offset, metronome_profile, sink);
                });
                collect_changes_tracks(::mu::engraving::PlaybackModel::metronome_track_id_value, track_changes);
            }
        }
    }
}

void PlaybackModel::reload_metronome_events() {
    TRACEFUNC;

    PlaybackData &metronome_data = playback_data_map_[::mu::engraving::PlaybackModel::metronome_track_id_value];
    metronome_data.origin_events.clear();

    if (!metronome_enabled_) {
        return;
    }

    const ArticulationsProfilePtr metronome_profile =
        default_acticulation_profile(::mu::engraving::PlaybackModel::metronome_track_id_value);
    EventsMapSink metronome_sink{metronome_data.origin_events};

    for (const RepeatSegment *repeat_segment : repeat_list()) {
        checkpoint();
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;

        for (const Measure *measure : repeat_segment->measure_list()) {
            checkpoint();
            renderer_.render_metronome(score_, measure, tick_position_offset, metronome_profile, metronome_sink);
        }
    }

    muse::remove(changed_track_id_set_, ::mu::engraving::PlaybackModel::metronome_track_id_value);
}

bool PlaybackModel::has_to_reload_tracks(const ScoreChanges &changes) const {
    static const std::unordered_set<ElementType> required_types{
        ElementType::PART,       ElementType::PLAYTECH_ANNOTATION, ElementType::CAPO,        ElementType::DYNAMIC,
        ElementType::HAIRPIN,    ElementType::HAIRPIN_SEGMENT,     ElementType::HARMONY,     ElementType::STAFF_TEXT,
        ElementType::SOUND_FLAG, ElementType::MEASURE_REPEAT,      ElementType::GUITAR_BEND, ElementType::GUITAR_BEND_SEGMENT,
        ElementType::BREATH,
    };

    for (const ElementType type : changes.changed_types) {
        if (muse::contains(required_types, type)) {
            return true;
        }
    }

    if (changes.is_valid_boundary()) {
        const Measure *measure_to = score_->tick2measure(Fraction::from_ticks(changes.tick_to));
        if (!measure_to) {
            return false;
        }

        if (measure_to->contains_measure_repeat(changes.staff_idx_from, changes.staff_idx_to)) {
            return true;
        }

        const Measure *next_measure = measure_to->next_measure();

        for (int i = 0; i < MeasureRepeat::max_num_measures && next_measure; ++i) {
            if (next_measure->contains_measure_repeat(changes.staff_idx_from, changes.staff_idx_to)) {
                return true;
            }

            next_measure = next_measure->next_measure();
        }
    }

    return false;
}

bool PlaybackModel::has_to_reload_score(const ScoreChanges &changes) const {
    static const std::unordered_set<ElementType> required_types{
        ElementType::SCORE,
        ElementType::GRADUAL_TEMPO_CHANGE,
        ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT,
        ElementType::TEMPO_TEXT,
        ElementType::LAYOUT_BREAK,
        ElementType::FERMATA,
        ElementType::VOLTA,
        ElementType::VOLTA_SEGMENT,
        ElementType::SYSTEM_TEXT,
        ElementType::JUMP,
        ElementType::MARKER,
        ElementType::BREATH,
        ElementType::INSTRUMENT_CHANGE,
    };

    for (const ElementType type : changes.changed_types) {
        // Automation edits on one staff shouldn't reload all tracks
        if (type == ElementType::SCORE && is_scoped_automation_change(changes)) {
            continue;
        }

        if (muse::contains(required_types, type)) {
            return true;
        }
    }

    static const std::unordered_set<mu::engraving::Pid> required_properties{
        mu::engraving::Pid::REPEAT_START,
        mu::engraving::Pid::REPEAT_END,
        mu::engraving::Pid::REPEAT_JUMP,
        mu::engraving::Pid::REPEAT_COUNT,
    };

    for (const Pid pid : changes.changed_property_id_set) {
        if (muse::contains(required_properties, pid)) {
            return true;
        }
    }

    return false;
}

bool PlaybackModel::has_automation_change(const ScoreChanges &changes) const {
    for (const auto &pair : changes.changed_objects) {
        if (muse::contains(pair.second, CommandType::EDIT_AUTOMATION_POINTS)) {
            return true;
        }
    }

    return false;
}

void PlaybackModel::clear_expired_tracks() {
    auto need_remove_track = [this](const InstrumentTrackId &track_id) {
        const Part *part = score_->part_by_id(track_id.part_id.to_uint64());

        if (!part) {
            return true;
        }

        if (track_id.instrument_id == ::chord_symbols_instrument_id) {
            return !part->has_chord_symbol();
        }

        return !part->instruments().contains(track_id.instrument_id);
    };

    auto it = playback_data_map_.cbegin();

    while (it != playback_data_map_.cend()) {
        if (it->first == ::mu::engraving::PlaybackModel::metronome_track_id_value) {
            ++it;
            continue;
        }

        if (need_remove_track(it->first)) {
            it = playback_data_map_.erase(it);
            continue;
        }

        ++it;
    }
}

void mu::engraving::PlaybackModel::remove_events_from_range(const track_idx_t track_from, const track_idx_t track_to,
                                                            const timestamp_t timestamp_from, const timestamp_t timestamp_to,
                                                            ChangedTrackIdSet *track_changes) {
    for (const Part *part : score_->parts()) {
        const TrackRange track_range = part->track_range();
        if (track_range.start_track > track_to || track_range.end_track <= track_from) {
            continue;
        }

        for (const InstrumentTrackId &track_id : part->instrument_track_id_set()) {
            remove_track_events(track_id, timestamp_from, timestamp_to, track_changes);
        }

        remove_track_events(chord_symbols_track_id(part->id()), timestamp_from, timestamp_to, track_changes);
    }
}

void PlaybackModel::clear_expired_events(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                                         ChangedTrackIdSet *track_changes) {
    TRACEFUNC;

    if (!score_) {
        return;
    }

    const Measure *last_measure = score_->last_measure();
    if (!last_measure) {
        return;
    }

    if (tick_from == 0 && last_measure->end_tick().ticks() == tick_to) {
        remove_events_from_range(track_from, track_to, -1 /*timestampFrom*/, -1 /*timestampTo*/, track_changes);
        remove_track_events(::mu::engraving::PlaybackModel::metronome_track_id_value, -1 /*timestampFrom*/, -1 /*timestampTo*/,
                            track_changes);
        return;
    }

    for (const RepeatSegment *repeat_segment : repeat_list()) {
        const int tick_position_offset = repeat_segment->utick - repeat_segment->tick;
        const int repeat_start_tick = repeat_segment->tick;
        const int repeat_end_tick = repeat_segment->end_tick();

        if (repeat_start_tick > tick_to || repeat_end_tick <= tick_from) {
            continue;
        }

        int remove_events_from_tick = std::max(tick_from, repeat_start_tick);
        timestamp_t remove_events_from = timestamp_from_ticks(score_, remove_events_from_tick + tick_position_offset);

        //! NOTE: the end tick of the current repeat segment == the start tick of the next repeat segment
        //! so subtract 1 to avoid removing events belonging to the next segment
        int remove_events_to_tick = std::min(tick_to, repeat_end_tick - 1);
        timestamp_t remove_events_to = timestamp_from_ticks(score_, remove_events_to_tick + tick_position_offset);

        remove_events_from_range(track_from, track_to, remove_events_from, remove_events_to, track_changes);

        if (!metronome_enabled_) {
            continue;
        }

        for (const Measure *measure : repeat_segment->measure_list()) {
            const int measure_start_tick = measure->tick().ticks();
            const int measure_end_tick = measure->end_tick().ticks();

            if (measure_start_tick > tick_to || measure_end_tick <= tick_from) {
                continue;
            }

            remove_events_from = timestamp_from_ticks(score_, measure_start_tick + tick_position_offset);
            remove_events_to = timestamp_from_ticks(score_, measure_end_tick + tick_position_offset - 1);

            remove_track_events(::mu::engraving::PlaybackModel::metronome_track_id_value, remove_events_from, remove_events_to,
                                track_changes);
        }
    }
}

void PlaybackModel::collect_changes_tracks(const InstrumentTrackId &track_id, ChangedTrackIdSet *result) {
    if (!result) {
        return;
    }

    result->insert(track_id);
}

void PlaybackModel::notify_about_changes(const InstrumentTrackIdSet &changed_tracks) {
    for (const InstrumentTrackId &track_id : changed_tracks) {
        if (muse::value(send_events_on_score_change_map_, track_id, false)) {
            send_events(track_id);
        } else {
            changed_track_id_set_.insert(track_id);
        }
    }
}

void PlaybackModel::send_events(const InstrumentTrackId &track_id) {
    auto it = playback_data_map_.find(track_id);
    if (it == playback_data_map_.cend()) {
        return;
    }

    PlaybackData &data = it->second;
}

void PlaybackModel::remove_track_events(const InstrumentTrackId &track_id, const muse::mpe::timestamp_t timestamp_from,
                                        const muse::mpe::timestamp_t timestamp_to, ChangedTrackIdSet *track_changes) {
    IF_ASSERT_FAILED(timestamp_from <= timestamp_to) { return; }

    auto search = playback_data_map_.find(track_id);
    if (search == playback_data_map_.cend()) {
        return;
    }

    PlaybackData &track_playback_data = search->second;
    const size_t old_size = track_playback_data.origin_events.size();

    DEFER {
        if (old_size != track_playback_data.origin_events.size()) {
            collect_changes_tracks(track_id, track_changes);
        }
    };

    if (timestamp_from == -1 && timestamp_to == -1) {
        search->second.origin_events.clear();
        return;
    }

    PlaybackEventsMap::iterator lower_bound_value;

    if (timestamp_from == 0) {
        //!Note Some events might be started RIGHT before the "official" start of the track
        //!     Need to make sure that we don't miss those events
        lower_bound_value = track_playback_data.origin_events.begin();
    } else {
        lower_bound_value = track_playback_data.origin_events.lower_bound(timestamp_from);
    }

    if (lower_bound_value == track_playback_data.origin_events.end()) {
        return;
    }

    auto upper_bound_value = track_playback_data.origin_events.upper_bound(timestamp_to);
    track_playback_data.origin_events.erase(lower_bound_value, upper_bound_value);
}

bool PlaybackModel::should_skip_changes(const ScoreChanges &changes) const {
    if (!changes.is_valid() || changes.is_text_editing) {
        return true;
    }

    if (changes.changed_objects.size() != 1) {
        return false;
    }

    const auto it = changes.changed_objects.begin();
    if (!it->first->is_text_base()) {
        return false;
    }

    const TextBase *text = to_text_base(it->first);
    const bool empty = text->empty();
    if (!empty) {
        return false;
    }

    if (text->is_harmony() && play_chord_symbols_) {
        const InstrumentTrackId track_id = chord_symbols_track_id(text->part()->id());
        if (!muse::contains(playback_data_map_, track_id)) {
            return false;
        }
    }

    const std::unordered_set<CommandType> &commands = it->second;
    if (muse::contains(commands, CommandType::REMOVE_ELEMENT)) {
        return false;
    }

    return true;
}

PlaybackModel::TrackBoundaries PlaybackModel::track_boundaries(const ScoreChanges &changes) const {
    TrackBoundaries result;

    result.track_from = staff2track(changes.staff_idx_from, 0);
    result.track_to = staff2track(changes.staff_idx_to, ::mu::engraving::voices);

    if (has_to_reload_score(changes) || !changes.is_valid_boundary()) {
        result.track_from = 0;
        result.track_to = score_->ntracks();
    }

    return result;
}

PlaybackModel::TickBoundaries PlaybackModel::tick_boundaries(const ScoreChanges &changes) const {
    TickBoundaries result;

    result.tick_from = changes.tick_from;
    result.tick_to = changes.tick_to;

    if (has_to_reload_tracks(changes) || has_to_reload_score(changes) || has_automation_change(changes) || !changes.is_valid_boundary()) {
        const Measure *last_measure = score_->last_measure();
        result.tick_from = 0;
        result.tick_to = last_measure ? last_measure->end_tick().ticks() : 0;

        return result;
    }

    for (const auto &pair : changes.changed_objects) {
        if (!pair.first->is_engraving_item()) {
            continue;
        }

        const EngravingItem *item = to_engraving_item(pair.first);

        if (item->is_note()) {
            const Note *note = to_note(item);
            const Chord *chord = note->chord();
            const TremoloTwoChord *tremolo_two = chord->tremolo_two_chord();

            if (tremolo_two) {
                const Chord *start_chord = tremolo_two->chord1();
                const Chord *end_chord = tremolo_two->chord2();

                IF_ASSERT_FAILED(start_chord && end_chord) { continue; }

                result.tick_from = std::min(result.tick_from, start_chord->tick().ticks());
                result.tick_to = std::max(result.tick_to, end_chord->tick().ticks());
            }

            apply_tied_notes_tick_boundaries(note, result);
        } else if (item->is_tie()) {
            apply_tie_tick_boundaries(to_tie(item), result);
        }

        const EngravingObject *parent = item->ownership_parent();
        if (!parent) {
            continue;
        }

        if (parent->is_chord()) {
            const Chord *chord = to_chord(parent);

            for (const Note *note : chord->notes()) {
                apply_tied_notes_tick_boundaries(note, result);
            }

            for (const Spanner *spanner : chord->starting_spanners()) {
                if (spanner->is_trill() && result.tick_to < spanner->tick2().ticks()) {
                    result.tick_to = spanner->tick2().ticks();
                }
            }
        } else if (parent->is_note()) {
            apply_tied_notes_tick_boundaries(to_note(parent), result);
        }
    }

    return result;
}

const RepeatList &PlaybackModel::repeat_list() const {
    score_->master_score()->set_expand_repeats(expand_repeats_);

    return score_->repeat_list();
}

InstrumentTrackId PlaybackModel::id_key(const EngravingItem *item) const {
    if (item->is_harmony()) {
        return chord_symbols_track_id(item->part()->id());
    }

    return make_instrument_track_id(item);
}

InstrumentTrackId PlaybackModel::id_key(const std::vector<const EngravingItem *> &items) const {
    InstrumentTrackId result;

    for (const EngravingItem *item : items) {
        InstrumentTrackId item_track_id = id_key(item);
        if (result.is_valid() && result != item_track_id) {
            LOGE() << "Triggering events for elements with different tracks";
            return InstrumentTrackId();
        }

        result = item_track_id;
    }

    return result;
}

InstrumentTrackId PlaybackModel::id_key(const ID &part_id, const String &instrument_id) const { return {part_id, instrument_id}; }

muse::mpe::ArticulationsProfilePtr PlaybackModel::default_acticulation_profile(const InstrumentTrackId &track_id) const {
    auto it = playback_data_map_.find(track_id);
    if (it == playback_data_map_.cend()) {
        return nullptr;
    }

    if (!profiles_repository()) {
        return nullptr;
    }

    return profiles_repository()->default_profile(it->second.setup_data.category);
}

void PlaybackModel::apply_tied_notes_tick_boundaries(const Note *note, TickBoundaries &tick_boundaries) {
    const Tie *tie;
    if ((tie = note->tie_for())) {
        apply_tie_tick_boundaries(tie, tick_boundaries);
    } else if ((tie = note->tie_back())) {
        apply_tie_tick_boundaries(tie, tick_boundaries);
    }
}

void PlaybackModel::apply_tie_tick_boundaries(const Tie *tie, TickBoundaries &tick_boundaries) {
    const Note *start_note = tie->start_note();
    const Note *end_note = tie->end_note();
    if (!start_note || !end_note) {
        return;
    }

    const Note *first_tied_note = start_note->first_tied_note();
    const Note *last_tied_note = end_note->last_tied_note();
    IF_ASSERT_FAILED(first_tied_note && last_tied_note) { return; }

    tick_boundaries.tick_from = std::min(tick_boundaries.tick_from, first_tied_note->tick().ticks());
    tick_boundaries.tick_to = std::max(tick_boundaries.tick_to, last_tied_note->tick().ticks());
}
