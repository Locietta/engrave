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

#include <functional>
#include <unordered_map>

#include "../dom/score.h"
#include "../types/types.h"
#include "flatplaybackscore.h"
#include "mpe/events.h"
#include "mpe/iarticulationprofilesrepository.h"
#include "playbackcontext.h"
#include "playbackeventsrenderer.h"
#include "playbacksetupdataresolver.h"
#include "types/id.h"

namespace mu::engraving {
struct Score;
struct Note;
struct EngravingItem;
struct Segment;
struct Instrument;
struct RepeatList;

// Follows the committed changes of the loaded score as its changes listener
// until it is destroyed or loads another score.
// The export builds a flat PlaybackScore with PlaybackScoreBuilder instead; this model is kept as the
// legacy test oracle (playback DoD design exception E1). Both run the same walk (playbackwalk.h).
struct PlaybackModel : IScoreChangesListener {
    // profilesRepository may be null (no default articulation profiles); it must
    // outlive the model.
    explicit PlaybackModel(const muse::mpe::IArticulationProfilesRepository *profiles_repository)
        : profiles_repository_(profiles_repository) {}

    ~PlaybackModel() override;

    PlaybackModel(const PlaybackModel &) = delete;
    PlaybackModel &operator=(const PlaybackModel &) = delete;

    // Owner-thread checkpoint, called only during load. May throw to abort;
    // discard this model after an aborted load. Never reenter the engine.
#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Test only: a non-null flat store receives, from the same walk, every track, setup, dynamics and event list
    // the legacy map receives during this load (dual execution); it is not kept after load returns.
    // Dual execution requires a model that has not loaded a score before.
    void load(Score *score, const std::function<void()> &checkpoint = {}, PlaybackScore *flat = nullptr);
#else
    void load(Score *score, const std::function<void()> &checkpoint = {});
#endif
    void reload();

    void set_send_events_on_score_change(const InstrumentTrackId &track_id, bool send);
    void send_events_for_changed_tracks();

    // The tracks changed since the last send, for the tracks that do not send
    // their events on each change.
    const InstrumentTrackIdSet &changed_track_id_set() const { return changed_track_id_set_; }

    bool is_play_repeats_enabled() const;
    void set_play_repeats(const bool is_enabled);

    bool is_play_chord_symbols_enabled() const;
    void set_play_chord_symbols(const bool is_enabled);

    bool use_score_dynamics_for_offstream_playback() const;
    void set_use_score_dynamics_for_offstream_playback(bool use);

    bool is_metronome_enabled() const;
    void set_is_metronome_enabled(const bool is_enabled);

    const InstrumentTrackId &metronome_track_id() const;
    InstrumentTrackId chord_symbols_track_id(const ID &part_id) const;
    bool is_chord_symbols_track(const InstrumentTrackId &track_id) const;

    bool has_sound_flags(const InstrumentTrackId &track_id) const;

    muse::mpe::PlaybackData &resolve_track_playback_data(const InstrumentTrackId &track_id);
    muse::mpe::PlaybackData &resolve_track_playback_data(const ID &part_id, const String &instrument_id);

    muse::mpe::PlaybackEventsMap trigger_events_for_items(const std::vector<const EngravingItem *> &items, muse::mpe::duration_t duration);
    muse::mpe::PlaybackEventsMap trigger_metronome(int tick);
    muse::mpe::PlaybackEventsMap trigger_count_in(int tick, muse::mpe::duration_t &count_in_duration);

    InstrumentTrackIdSet existing_track_id_set() const;

    void on_score_changes(const ScoreChanges &changes) override;

private:
    const muse::mpe::IArticulationProfilesRepository *profiles_repository() const { return profiles_repository_; }

    void detach_from_score();

    using ChangedTrackIdSet = InstrumentTrackIdSet;

    struct TickBoundaries {
        int tick_from = -1;
        int tick_to = -1;
    };

    struct TrackBoundaries {
        track_idx_t track_from = muse::nidx;
        track_idx_t track_to = muse::nidx;
    };

    InstrumentTrackId id_key(const EngravingItem *item) const;
    InstrumentTrackId id_key(const std::vector<const EngravingItem *> &items) const;
    InstrumentTrackId id_key(const ID &part_id, const String &instrument_id) const;

    // The shared score walk (playbackwalk.h) over this model's track map
    void update(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                ChangedTrackIdSet *track_changes = nullptr);

    void reload_metronome_events();

    bool has_to_reload_tracks(const ScoreChanges &changes) const;
    bool has_to_reload_score(const ScoreChanges &changes) const;
    bool has_automation_change(const ScoreChanges &changes) const;

    void clear_expired_tracks();
    void clear_expired_events(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                              ChangedTrackIdSet *track_changes = nullptr);
    void collect_changes_tracks(const InstrumentTrackId &track_id, ChangedTrackIdSet *result);
    void notify_about_changes(const InstrumentTrackIdSet &changed_tracks);

    void send_events(const InstrumentTrackId &track_id);

    void remove_events_from_range(const track_idx_t track_from, const track_idx_t track_to,
                                  const muse::mpe::timestamp_t timestamp_from = -1, const muse::mpe::timestamp_t timestamp_to = -1,
                                  ChangedTrackIdSet *track_changes = nullptr);
    void remove_track_events(const InstrumentTrackId &track_id, const muse::mpe::timestamp_t timestamp_from = -1,
                             const muse::mpe::timestamp_t timestamp_to = -1, ChangedTrackIdSet *track_changes = nullptr);

    bool should_skip_changes(const ScoreChanges &changes) const;

    TrackBoundaries track_boundaries(const ScoreChanges &changes) const;
    TickBoundaries tick_boundaries(const ScoreChanges &changes) const;

    const RepeatList &repeat_list() const;

    muse::mpe::ArticulationsProfilePtr default_acticulation_profile(const InstrumentTrackId &track_id) const;

    static void apply_tied_notes_tick_boundaries(const Note *note, TickBoundaries &tick_boundaries);
    static void apply_tie_tick_boundaries(const Tie *tie, TickBoundaries &tick_boundaries);

    const muse::mpe::IArticulationProfilesRepository *profiles_repository_ = nullptr;
    std::function<void()> checkpoint_;

    void checkpoint() const {
        if (checkpoint_) checkpoint_();
    }

    Score *score_ = nullptr;
    // The master score this model listens to; checked with Score::isAlive before detaching.
    MasterScore *listened_score_ = nullptr;
    bool expand_repeats_ = true;
    bool play_chord_symbols_ = true;
    bool use_score_dynamics_for_offstream_playback_ = true;
    bool metronome_enabled_ = true;

    PlaybackEventsRenderer renderer_;
    PlaybackSetupDataResolver setup_resolver_;

    PlaybackContextPtr playback_ctx_;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    PlaybackScore *flat_ = nullptr; // dual execution, set only during load
#endif
    std::unordered_map<InstrumentTrackId, muse::mpe::PlaybackData> playback_data_map_;
    std::unordered_map<InstrumentTrackId, bool> send_events_on_score_change_map_;

    InstrumentTrackIdSet changed_track_id_set_;
};
} // namespace mu::engraving
