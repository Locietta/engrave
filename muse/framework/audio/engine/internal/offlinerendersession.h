// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "iaudiofactory.h"
#include "mixer.h"
#include "nodes/trackchain.h"

namespace muse::audio::engine {
// Owns one render graph. Synth and effect resolver caches are released with it.
// The former offline ContextPlayer stayed stopped: control/FX time is zero.
struct OfflineRenderSession {
    OfflineRenderSession(IAudioFactory &factory, const OutputSpec &spec);
    ~OfflineRenderSession();
    OfflineRenderSession(const OfflineRenderSession &) = delete;
    OfflineRenderSession &operator=(const OfflineRenderSession &) = delete;
    RetVal2<TrackId, TrackParams> add_track(const std::string &name, const mpe::TrackEvents &events, const TrackParams &params);
    RetVal2<TrackId, TrackParams> add_aux_track(const std::string &name, const TrackParams &params);

    IAudioNodePtr output() const { return master_; }

    void reset_sources();
private:
    RetVal2<TrackId, TrackParams> add(const std::string &name, const mpe::TrackEvents *data, const TrackParams &params);
    IAudioFactory &factory_;
    OutputSpec spec_;
    std::shared_ptr<Mixer> mixer_;
    TrackChainPtr master_;
    std::vector<TrackChainPtr> tracks_;
    TrackId last_id_ = ::muse::audio::master_track_id;
};
} // namespace muse::audio::engine
