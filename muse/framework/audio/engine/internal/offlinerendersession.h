// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "audio/engine/offlinerendersettings.h"
#include "fx/reverb/reverbprocessor.h"
#include "nodes/fluidtracksource.h"
#include "nodes/offlinecontrol.h"
#include "nodes/offlinesignal.h"

namespace muse::audio::engine {
// All state belongs to this export; resources and configuration outlive it.
struct OfflineRenderSession {
    OfflineRenderSession(const OutputSpec &, AudioResources &, const OfflineRenderSettings &, MidiEventSink);
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

    OfflineControl &master_control() { return outer_master_; }
private:
    struct Track {
        std::unique_ptr<FluidTrackSource> source;
        std::vector<std::unique_ptr<fx::ReverbProcessor>> effects;
        OfflineControl control;
        OfflineSignal signal;
        AuxSendsParams sends;
        std::vector<float> buffer;
        bool processed = false;
    };

    void initialize(Track &, const TrackParams &);
    void mix(float *, samples_t);
    static void process_track(Track &, samples_t);
    AudioResources &resources_;
    OfflineRenderSettings config_;
    MidiEventSink sink_;
    OutputSpec spec_;
    std::vector<Track> tracks_;
    Track reverb_;
    // Aux 2 has no input, effects or sends; retain its cancellation checkpoint.
    bool has_reverb_ = false;
    bool has_aux2_ = false;
    OfflineControl session_master_, outer_master_;
    OfflineSignal session_signal_, outer_signal_;
    TrackId last_id_ = ::muse::audio::master_track_id;
};
} // namespace muse::audio::engine
