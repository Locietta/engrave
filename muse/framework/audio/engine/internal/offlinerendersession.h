// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <optional>

#include "audio/engine/offlinerendersettings.h"
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
    void set_mode(ProcessMode);
    void process(float *, samples_t);
    // Synthesizes tracks on up to `threads` pool threads; null or 1 keeps every track on the caller.
    // Serial regardless while playback-harness capture or fault injection is active.
    void set_parallelism(WorkerPool *, std::size_t threads);

    OfflineControl &master_control() { return outer_master_; }
private:
    struct Track {
        std::unique_ptr<FluidTrackSource> source;
        std::vector<std::unique_ptr<fx::ReverbProcessor>> effects;
        OfflineControl control;
        OfflineSignal signal;
        AuxSendsParams sends;
        std::vector<float> buffer;
        // Measured when fanning out or with ENGRAVE_AUDIO_TIMING.
        double synth_seconds = 0, last_seconds = 0;
        bool processed = false;
    };

    std::size_t active_tracks() const;
    std::size_t synthesis_threads() const;
    void synthesize(samples_t);
    void mix(float *, samples_t);
    void initialize(Track &, const TrackParams &);
    static void process_track(Track &, samples_t);
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
};
} // namespace muse::audio::engine
