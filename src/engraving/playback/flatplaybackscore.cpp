// SPDX-License-Identifier: GPL-3.0-only
#include "flatplaybackscore.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <span>

#include "playbackwalk.h"

namespace mu::engraving {
namespace {
bool track_less(const std::pair<InstrumentTrackId, muse::mpe::TrackEvents> &entry, const InstrumentTrackId &id) { return entry.first < id; }

struct FlatStore {
    PlaybackScore &score;
    muse::mpe::FreezeProgress progress;
    muse::FnRef<void()> on_grow;

    bool contains(const InstrumentTrackId &track_id) const { return score.find(track_id) != nullptr; }

    void touch(const InstrumentTrackId &track_id) {
        std::ignore = score.track(track_id);
        if (on_grow) on_grow();
    }

    template <class Resolve>
    void resolve_setup(const InstrumentTrackId &track_id, Resolve &&resolve) {
        resolve(score.track(track_id).setup);
        if (on_grow) on_grow();
    }

    const muse::mpe::PlaybackSetupData *find_setup(const InstrumentTrackId &track_id) const {
        const muse::mpe::TrackEvents *track = score.find(track_id);
        return track ? &track->setup : nullptr;
    }

    template <class Walk>
    void apply_context(const InstrumentTrackId &track_id, const PlaybackPartData &data, const Walk &walk, bool) {
        muse::mpe::TrackEvents &track = score.track(track_id);
        muse::mpe::freeze_dynamics(track, data.dynamics, progress);
        if (on_grow) on_grow();

        const auto append_events = [&](const auto &events) {
            for (const auto &pair : events) {
                walk.checkpoint();
                muse::mpe::freeze_cold_events(track, pair.first, std::span{pair.second}, progress);
                if (on_grow) on_grow();
            }
        };

        append_events(data.sound_presets);
        append_events(data.text_articulations);
        append_events(data.syllables);
    }

    template <class Render>
    void emit(const InstrumentTrackId &track_id, Render &&render) {
        render(score.track(track_id), progress);
        if (on_grow) on_grow();
    }
};

std::uint64_t track_working_bytes(const muse::mpe::TrackEvents &track) {
    return static_cast<std::uint64_t>(track.notes.capacity()) * sizeof(muse::mpe::NoteRecord) +
           static_cast<std::uint64_t>(track.articulations.capacity()) * sizeof(muse::mpe::ArticulationRecord) +
           static_cast<std::uint64_t>(track.curve_points.capacity()) * sizeof(muse::mpe::CurvePoint) +
           static_cast<std::uint64_t>(track.dynamic_layers.capacity()) * sizeof(muse::mpe::DynamicLayer) +
           static_cast<std::uint64_t>(track.dynamic_points.capacity()) * sizeof(muse::mpe::DynamicPoint) +
           static_cast<std::uint64_t>(track.empty_groups.capacity()) * sizeof(muse::mpe::EmptyGroup) +
           static_cast<std::uint64_t>(track.cold.capacity()) * sizeof(muse::mpe::ColdEvent);
}

std::uint64_t score_working_bytes(const PlaybackScore &score) {
    std::uint64_t total =
        static_cast<std::uint64_t>(score.tracks.capacity()) * sizeof(std::pair<InstrumentTrackId, muse::mpe::TrackEvents>);
    for (const auto &[id, events] : score.tracks) {
        total += track_working_bytes(events);
    }
    return total;
}
} // namespace

muse::mpe::TrackEvents &PlaybackScore::track(const InstrumentTrackId &id) {
    auto it = std::lower_bound(tracks.begin(), tracks.end(), id, track_less);
    if (it == tracks.end() || it->first != id) {
        it = tracks.emplace(it, id, muse::mpe::TrackEvents{});
    }
    return it->second;
}

const muse::mpe::TrackEvents *PlaybackScore::find(const InstrumentTrackId &id) const {
    const auto it = std::lower_bound(tracks.begin(), tracks.end(), id, track_less);
    return it == tracks.end() || it->first != id ? nullptr : &it->second;
}

void PlaybackScore::release(muse::FnRef<void()> checkpoint) {
    std::exception_ptr error;
    std::size_t remaining = 4096;
    const auto poll = [&] {
        if (checkpoint && !error) {
            try {
                checkpoint();
            } catch (...) {
                error = std::current_exception();
            }
        }
    };
    const auto step = [&] {
        if (--remaining == 0) {
            poll();
            remaining = 4096;
        }
    };
    poll();
    for (auto &[id, events] : tracks) {
        while (!events.cold.empty()) {
            events.cold.pop_back();
            step();
        }
    }
    if (context) context->release(step);
    context.reset();
    tracks.clear();
    poll();
    if (error) std::rethrow_exception(error);
}

PlaybackScore PlaybackScoreBuilder::build(Score *score, muse::FnRef<void()> checkpoint,
                                          muse::FnRef<void(std::uint64_t)> charge_bytes) const {
    PlaybackScore result;
    if (!score) return result;

    std::uint64_t charged_bytes = 0;
    const auto charge_growth = [&] {
        if (!charge_bytes) return;
        const std::uint64_t current = score_working_bytes(result);
        if (current > charged_bytes) {
            const std::uint64_t delta = current - charged_bytes;
            charged_bytes = current;
            charge_bytes(delta);
        }
    };

    if (score->measures()->empty() || !score->last_measure()) {
        // Legacy export creates this entry before the empty-score load returns.
        std::ignore = result.track(playback_metronome_track_id());
        charge_growth();
        return result;
    }

    const auto polled_checkpoint = [&] {
        charge_growth();
        if (checkpoint) checkpoint();
    };
    const muse::FnRef<void()> active_checkpoint = (checkpoint || charge_bytes) ? muse::FnRef<void()>{polled_checkpoint} : checkpoint;
    const muse::FnRef<void()> on_grow = charge_bytes ? muse::FnRef<void()>{charge_growth} : muse::FnRef<void()>{};

    FlatStore store{result, {}, on_grow};
    store.progress.checkpoint = active_checkpoint;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    auto dump_note_cb = [this](const muse::mpe::TrackEvents &track, const muse::mpe::NoteRecord &note,
                               const muse::mpe::ArticulationMap &articulations) { return dump_note(track, note, articulations); };
    if (dump_note) {
        store.progress.dump_note = decltype(store.progress.dump_note)(dump_note_cb);
    }
#endif
    if (active_checkpoint) {
        active_checkpoint();
    }

    result.context = std::make_shared<PlaybackContext>(score);
    const PlaybackContextPtr &playback_ctx = result.context;
    const PlaybackEventsRenderer renderer;
    PlaybackSetupDataResolver setup_resolver;
    PlaybackWalk<FlatStore> walk{score,        playback_ctx,      renderer, setup_resolver, &profiles, active_checkpoint,
                                 play_repeats, metronome_enabled, store};

    struct ReleaseGuard {
        PlaybackScore &score;
        bool committed = false;

        ~ReleaseGuard() noexcept {
            if (!committed) score.release();
        }
    } guard{result};

    walk.update(0, score->last_measure()->end_tick().ticks(), 0, score->ntracks());
    charge_growth();
    guard.committed = true;
    return result;
}
} // namespace mu::engraving
