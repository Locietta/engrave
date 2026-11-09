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

#include <set>
#include <span>
#include <stdexcept>
#include <tuple>

#include "defer.h"
#include "dom/instrument.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/part.h"
#include "dom/repeatlist.h"
#include "dom/tie.h"
#include "dom/tremolotwochord.h"
#include "editing/transaction/undoablecommand.h"
#include "log.h"
#include "playbackwalk.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::mpe;

namespace {
//! The legacy model's walk target: the track map, with its incremental-update semantics. In the
//! harness build a flat store may mirror it from the same walk (dual execution, test only)
struct LegacyStore {
    std::unordered_map<InstrumentTrackId, PlaybackData> &map;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    PlaybackScore *flat = nullptr;
#endif

    PlaybackData &track(const InstrumentTrackId &track_id) {
#ifdef ENGRAVE_PLAYBACK_HARNESS
        if (flat) {
            flat->track(track_id);
        }
#endif
        return map[track_id];
    }

    bool contains(const InstrumentTrackId &track_id) const { return muse::contains(map, track_id); }

    void touch(const InstrumentTrackId &track_id) { std::ignore = track(track_id); }

    template <class Resolve>
    void resolve_setup(const InstrumentTrackId &track_id, Resolve &&resolve) {
        PlaybackSetupData &setup_data = track(track_id).setup_data;
        resolve(setup_data);
#ifdef ENGRAVE_PLAYBACK_HARNESS
        if (flat) {
            flat->track(track_id).setup = setup_data;
        }
#endif
    }

    const PlaybackSetupData *find_setup(const InstrumentTrackId &track_id) const {
        const auto it = map.find(track_id);
        return it == map.cend() ? nullptr : &it->second.setup_data;
    }

    template <class Walk>
    void apply_context(const InstrumentTrackId &track_id, const PlaybackPartData &data, const Walk &walk) {
        PlaybackData &playback_data = track(track_id);
        playback_data.dynamics = data.dynamics;
#ifdef ENGRAVE_PLAYBACK_HARNESS
        if (flat) {
            freeze_dynamics(flat->track(track_id), data.dynamics, flat->progress);
        }
#endif

        std::set<timestamp_t> new_event_timestamps;

        const auto append_events = [&](const auto &events) {
            for (const auto &pair : events) {
                walk.checkpoint();
                PlaybackEventList &list = playback_data.origin_events[pair.first];

                //! NOTE: this assumes that the list has already been cleared in clearExpiredEvents
                //! Necessary to prevent event duplication (unchanged lists should not be modified)
                if (list.empty() || muse::contains(new_event_timestamps, pair.first)) {
                    list.insert(list.end(), pair.second.begin(), pair.second.end());
                    new_event_timestamps.insert(pair.first);
#ifdef ENGRAVE_PLAYBACK_HARNESS
                    // The seam for context events, which bypass the renderer sinks: the same list, after the legacy insert
                    if (flat) {
                        freeze_cold_events(flat->track(track_id), pair.first, std::span{pair.second}, flat->progress);
                    }
#endif
                }
            }
        };

        append_events(data.sound_presets);
        append_events(data.text_articulations);
        append_events(data.syllables);
    }

    template <class Render>
    void emit(const InstrumentTrackId &track_id, Render &&render) {
        PlaybackEventsMap &events = track(track_id).origin_events;
#ifdef ENGRAVE_PLAYBACK_HARNESS
        if (flat) {
            DualEventsSink sink{{events}, {flat->track(track_id), flat->progress}};
            render(sink);
            return;
        }
#endif
        EventsMapSink sink{events};
        render(sink);
    }
};
} // namespace

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

#ifdef ENGRAVE_PLAYBACK_HARNESS
void PlaybackModel::load(Score *score, const std::function<void()> &checkpoint_callback, PlaybackScore *flat) {
#else
void PlaybackModel::load(Score *score, const std::function<void()> &checkpoint_callback) {
#endif
    TRACEFUNC;

    if (!score || score->measures()->empty() || !score->last_measure()) {
        return;
    }

#ifdef ENGRAVE_PLAYBACK_HARNESS
    // The flat store mirrors map entries as the walk creates them, so it needs a model that never loaded
    if (flat && score_) {
        throw std::logic_error("PlaybackModel: dual execution needs a model that has not loaded a score");
    }
#endif

    checkpoint_ = checkpoint_callback;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    flat_ = flat;
    if (flat_) {
        // Bounded polls inside the freeze loops, through the same latched checkpoint as the walk
        flat_->progress.checkpoint = [this] { checkpoint(); };
    }
#endif
    DEFER {
        checkpoint_ = {};
#ifdef ENGRAVE_PLAYBACK_HARNESS
        if (flat_) {
            flat_->progress.checkpoint = {};
        }
        flat_ = nullptr;
#endif
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

const InstrumentTrackId &PlaybackModel::metronome_track_id() const { return playback_metronome_track_id(); }

InstrumentTrackId PlaybackModel::chord_symbols_track_id(const ID &part_id) const { return playback_chord_symbols_track_id(part_id); }

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
    auto track_playback_data = playback_data_map_.find(playback_metronome_track_id());
    if (track_playback_data == playback_data_map_.cend()) {
        return {};
    }

    const ArticulationsProfilePtr profile = default_acticulation_profile(playback_metronome_track_id());

    PlaybackEventsMap result;
    renderer_.render_metronome(score_, tick, 0, profile, result);
    return result;
}

PlaybackEventsMap PlaybackModel::trigger_count_in(int tick, muse::mpe::duration_t &count_in_duration) {
    auto track_playback_data = playback_data_map_.find(playback_metronome_track_id());
    if (track_playback_data == playback_data_map_.cend()) {
        return {};
    }

    const ArticulationsProfilePtr profile = default_acticulation_profile(playback_metronome_track_id());

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
    LegacyStore store{playback_data_map_};
#ifdef ENGRAVE_PLAYBACK_HARNESS
    store.flat = flat_;
#endif
    PlaybackWalk<LegacyStore> walk{score_,          playback_ctx_,      renderer_, setup_resolver_, profiles_repository_, checkpoint_,
                                   expand_repeats_, metronome_enabled_, store};
    walk.update(tick_from, tick_to, track_from, track_to, track_changes);
}

void PlaybackModel::reload_metronome_events() {
    TRACEFUNC;

    PlaybackData &metronome_data = playback_data_map_[playback_metronome_track_id()];
    metronome_data.origin_events.clear();

    if (!metronome_enabled_) {
        return;
    }

    const ArticulationsProfilePtr metronome_profile = default_acticulation_profile(playback_metronome_track_id());
    EventsMapSink metronome_sink{metronome_data.origin_events};

    for (const RepeatSegment *repeat_segment : repeat_list()) {
        checkpoint();
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;

        for (const Measure *measure : repeat_segment->measure_list()) {
            checkpoint();
            renderer_.render_metronome(score_, measure, tick_position_offset, metronome_profile, metronome_sink);
        }
    }

    muse::remove(changed_track_id_set_, playback_metronome_track_id());
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

        if (is_playback_chord_symbols_track(track_id)) {
            return !part->has_chord_symbol();
        }

        return !part->instruments().contains(track_id.instrument_id);
    };

    auto it = playback_data_map_.cbegin();

    while (it != playback_data_map_.cend()) {
        if (it->first == playback_metronome_track_id()) {
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
        remove_track_events(playback_metronome_track_id(), -1 /*timestampFrom*/, -1 /*timestampTo*/, track_changes);
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

            remove_track_events(playback_metronome_track_id(), remove_events_from, remove_events_to, track_changes);
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
