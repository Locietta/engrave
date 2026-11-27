// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <chrono>
#include <optional>

#include "audio/engine/offlinerendersettings.h"
#include "denormalguard.h"
#include "fx/reverb/reverbprocessor.h"
#include "global/unique_fn.h"
#include "nodes/fluidtracksource.h"
#include "nodes/offlinecontrol.h"
#include "nodes/offlinesignal.h"

namespace muse::audio::engine {
struct WorkerPool;

// All state belongs to this export; borrowed resources outlive it; render settings are owned values.
// Each track's synth, effects and buffer are touched by one thread per block; summing, the reverb
// and the master stages run on the calling thread in track order, so output is independent of the
// thread count.
//
// When synthesis fans out, process() can also start synthesizing the next block while it mixes the
// current one. Each track then alternates between two output slots: workers write only the next
// block's slot and the track's synth/DSP/signal state, while the mix reads only the current slot
// (its buffer and the silence flag snapshotted after synthesis) and the immutable sends, and writes
// only the reverb and master stages.
struct OfflineRenderSession {
    OfflineRenderSession(const OutputSpec &, AudioResources &, const OfflineRenderSettings &,
                         muse::UniqueFn<void(const midi::Event &) const>);
    ~OfflineRenderSession();
    OfflineRenderSession(const OfflineRenderSession &) = delete;
    OfflineRenderSession &operator=(const OfflineRenderSession &) = delete;
    RetVal2<TrackId, TrackParams> add_track(const std::string &, const mpe::TrackEvents &, const TrackParams &);
    RetVal2<TrackId, TrackParams> add_aux_track(const std::string &, const TrackParams &);
#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Throw with N published tracks plus a fully constructed unpublished track.
    static thread_local int fail_after_tracks;
#endif
    void reset_sources();
    // Leaving PLAYING_OFFLINE first abandons a prefetched block.
    void set_mode(ProcessMode);
    // Renders `frames` into `out`. A non-zero `next_frames` lets a fanned-out session synthesize the
    // following block of that size on the pool while this one is mixed; the next call must then ask
    // for exactly `next_frames`.
    void process(float *out, samples_t frames, samples_t next_frames = 0);
    // Stops a prefetched block and waits for its workers. Its tracks are left mid-block, so a session
    // that abandoned a block must not render again.
    void cancel_prefetch() noexcept;
    // Synthesizes tracks on up to `threads` pool threads; null or 1 keeps every track on the caller.
    // Serial regardless while playback-harness capture or fault injection is active.
    void set_parallelism(WorkerPool *, std::size_t threads);
    // True while playback-harness capture or fault injection requires a single-threaded render.
    static bool serial_only();

    OfflineControl &master_control() { return outer_master_; }

    std::size_t track_count() const noexcept { return tracks_.size(); }
private:
    struct Slot {
        std::vector<float> buffer;
        bool processed = false;
        bool silent = true;
    };

    struct Track {
        std::unique_ptr<FluidTrackSource> source;
        std::vector<std::unique_ptr<fx::ReverbProcessor>> effects;
        OfflineControl control;
        OfflineSignal signal;
        AuxSendsParams sends;
        Slot slots[2];
        // Measured when fanning out or with ENGRAVE_AUDIO_TIMING.
        double synth_seconds = 0, last_seconds = 0;
    };

    std::size_t active_tracks() const;
    std::size_t synthesis_threads() const;
    // Synthesizes into slots[slot]. With `async`, returns once the block has started on the pool
    // (false when it could not start); otherwise returns after synthesis.
    bool synthesize(samples_t, int slot, bool async);
    void render_track(Track &, samples_t, int slot, bool measure);
    void mix(float *, samples_t, int slot);
    void initialize(Track &, const TrackParams &);
    static void process_track(Track &, float *buffer, samples_t);
    AudioResources &resources_;
    OfflineRenderSettings config_;
    muse::UniqueFn<void(const midi::Event &) const> sink_;
    OutputSpec spec_;
    std::vector<Track> tracks_;
    Track reverb_;
    // Aux 2 has no input, effects or sends; retain its cancellation checkpoint.
    bool has_reverb_ = false;
    bool has_aux2_ = false;
    OfflineControl session_master_, outer_master_;
    OfflineSignal session_signal_, outer_signal_;
    TrackId last_id_ = ::muse::audio::master_track_id;
    WorkerPool *pool_ = nullptr;
    std::size_t threads_ = 1;
    std::optional<std::size_t> granted_workers_;
    std::vector<std::size_t> order_;
    // The slot the next process() mixes.
    int slot_ = 0;
    // Parameters of the block on the pool. Set before a block starts and read by its tasks.
    samples_t block_frames_ = 0;
    int block_slot_ = 0;
    bool block_measure_ = false;
    ScopedDenormalGuard::Mode block_fp_mode_;

    struct BlockTask {
        OfflineRenderSession *self;
        void operator()(std::size_t index) const;
    };

    const BlockTask block_task_{this};
    // Non-zero while a prefetched block of that size has started and not been finished.
    samples_t prefetched_frames_ = 0;
    std::chrono::steady_clock::time_point prefetch_start_;
};
} // namespace muse::audio::engine
