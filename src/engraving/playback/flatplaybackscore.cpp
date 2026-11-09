// SPDX-License-Identifier: GPL-3.0-only
#include "flatplaybackscore.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <span>

#include "playbackeventsrenderer_impl.h"
#include "playbackwalk.h"

namespace mu::engraving {
namespace {
bool track_less(const std::pair<InstrumentTrackId, muse::mpe::TrackEvents> &entry, const InstrumentTrackId &id) { return entry.first < id; }

//! The builder's walk target: the flat store only. A fresh build has no earlier lists, so the context
//! events go straight to the freeze (the legacy gate in LegacyStore::apply_context is always true there)
struct FlatStore {
    PlaybackScore &score;

    bool contains(const InstrumentTrackId &track_id) const { return score.find(track_id) != nullptr; }

    void touch(const InstrumentTrackId &track_id) { std::ignore = score.track(track_id); }

    template <class Resolve>
    void resolve_setup(const InstrumentTrackId &track_id, Resolve &&resolve) {
        resolve(score.track(track_id).setup);
    }

    const muse::mpe::PlaybackSetupData *find_setup(const InstrumentTrackId &track_id) const {
        const muse::mpe::TrackEvents *track = score.find(track_id);
        return track ? &track->setup : nullptr;
    }

    template <class Walk>
    void apply_context(const InstrumentTrackId &track_id, const PlaybackPartData &data, const Walk &walk) {
        muse::mpe::TrackEvents &track = score.track(track_id);
        muse::mpe::freeze_dynamics(track, data.dynamics, score.progress);

        const auto append_events = [&](const auto &events) {
            for (const auto &pair : events) {
                walk.checkpoint();
                muse::mpe::freeze_cold_events(track, pair.first, std::span{pair.second}, score.progress);
            }
        };

        append_events(data.sound_presets);
        append_events(data.text_articulations);
        append_events(data.syllables);
    }

    template <class Render>
    void emit(const InstrumentTrackId &track_id, Render &&render) {
        FlatEventsSink sink{score.track(track_id), score.progress};
        render(sink);
    }
};
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

void PlaybackScore::release(const std::function<void()> &checkpoint) {
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

PlaybackScore PlaybackScoreBuilder::build(Score *score, const std::function<void()> &checkpoint) const {
    PlaybackScore result;
    if (!score) return result;
    if (score->measures()->empty() || !score->last_measure()) {
        // Legacy export creates this entry before the empty-score load returns.
        std::ignore = result.track(playback_metronome_track_id());
        return result;
    }

    // The latched checkpoint also bounds the freeze loops; it is not kept in the returned store
    result.progress.checkpoint = checkpoint;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    result.progress.dump_note = dump_note;
#endif
    if (checkpoint) {
        checkpoint();
    }

    result.context = std::make_shared<PlaybackContext>(score);
    const PlaybackContextPtr &playback_ctx = result.context;
    const PlaybackEventsRenderer renderer;
    PlaybackSetupDataResolver setup_resolver;
    FlatStore store{result};
    PlaybackWalk<FlatStore> walk{score,      playback_ctx, renderer,          setup_resolver, profiles_repository,
                                 checkpoint, play_repeats, metronome_enabled, store};
    try {
        walk.update(0, score->last_measure()->end_tick().ticks(), 0, score->ntracks());
    } catch (...) {
        result.release();
        throw;
    }
    result.progress.checkpoint = {};
#ifdef ENGRAVE_PLAYBACK_HARNESS
    result.progress.dump_note = {};
#endif
    return result;
}
} // namespace mu::engraving

// The renderer entry points for the flat sinks live with the sinks, not in the renderer unit
ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(mu::engraving::FlatEventsSink);
#ifdef ENGRAVE_PLAYBACK_HARNESS
ENGRAVE_INSTANTIATE_PLAYBACK_EVENTS_RENDERER(mu::engraving::DualEventsSink);
#endif
