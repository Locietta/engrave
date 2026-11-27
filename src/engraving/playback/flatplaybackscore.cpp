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
    void apply_context(const InstrumentTrackId &track_id, const PlaybackPartData &data, const Walk &walk, bool) {
        muse::mpe::TrackEvents &track = score.track(track_id);
        muse::mpe::freeze_dynamics(track, data.dynamics, progress);

        const auto append_events = [&](const auto &events) {
            for (const auto &pair : events) {
                walk.checkpoint();
                muse::mpe::freeze_cold_events(track, pair.first, std::span{pair.second}, progress);
            }
        };

        append_events(data.sound_presets);
        append_events(data.text_articulations);
        append_events(data.syllables);
    }

    template <class Render>
    void emit(const InstrumentTrackId &track_id, Render &&render) {
        render(score.track(track_id), progress);
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

PlaybackScore PlaybackScoreBuilder::build(Score *score, muse::FnRef<void()> checkpoint) const {
    PlaybackScore result;
    if (!score) return result;
    if (score->measures()->empty() || !score->last_measure()) {
        // Legacy export creates this entry before the empty-score load returns.
        std::ignore = result.track(playback_metronome_track_id());
        return result;
    }

    FlatStore store{result, {}};
    store.progress.checkpoint = checkpoint;
#ifdef ENGRAVE_PLAYBACK_HARNESS
    auto dump_note_cb = [this](const muse::mpe::TrackEvents &track, const muse::mpe::NoteRecord &note,
                               const muse::mpe::ArticulationMap &articulations) { return dump_note(track, note, articulations); };
    if (dump_note) {
        store.progress.dump_note = decltype(store.progress.dump_note)(dump_note_cb);
    }
#endif
    if (checkpoint) {
        checkpoint();
    }

    result.context = std::make_shared<PlaybackContext>(score);
    const PlaybackContextPtr &playback_ctx = result.context;
    const PlaybackEventsRenderer renderer;
    PlaybackSetupDataResolver setup_resolver;
    PlaybackWalk<FlatStore> walk{score,      playback_ctx, renderer,          setup_resolver, &profiles,
                                 checkpoint, play_repeats, metronome_enabled, store};

    struct ReleaseGuard {
        PlaybackScore &score;
        bool committed = false;

        ~ReleaseGuard() noexcept {
            if (!committed) score.release();
        }
    } guard{result};

    walk.update(0, score->last_measure()->end_tick().ticks(), 0, score->ntracks());
    guard.committed = true;
    return result;
}
} // namespace mu::engraving
