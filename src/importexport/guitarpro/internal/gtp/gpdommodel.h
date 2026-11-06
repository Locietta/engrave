#ifndef MU_IMPORTEXPORT_GPDOMMODEL_H
#define MU_IMPORTEXPORT_GPDOMMODEL_H

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "gpbar.h"
#include "gpbeat.h"
#include "gpvoice.h"
#include "gpmasterbar.h"
#include "gpmastertracks.h"
#include "gpnote.h"
#include "gpscore.h"
#include "gptrack.h"
#include "inoteproperty.h"

namespace mu::iex::guitarpro {
struct GPDomModel {

    GPDomModel() = default;
    virtual ~GPDomModel() = default;

    GPDomModel(const GPDomModel &gp) = delete;
    GPDomModel &operator=(const GPDomModel &) = delete;

    void add_gp_score(std::unique_ptr<GPScore> &&score) { score_value = std::move(score); }

    void add_gp_master_tracks(std::unique_ptr<GPMasterTracks> &&m_tr) { master_tracks_value.swap(m_tr); }

    void add_gp_tracks(std::map<int, std::unique_ptr<GPTrack>> &&tr) { tracks_value.swap(tr); }

    void add_gp_master_bars(std::vector<std::unique_ptr<GPMasterBar>> &&m_b) { master_bars_value.swap(m_b); }

    const GPScore *score() const { return score_value.get(); }

    const GPMasterTracks *master_tracks() const { return master_tracks_value.get(); }

    const std::map<int, std::unique_ptr<GPTrack>> &tracks() const { return tracks_value; }

    const std::vector<std::unique_ptr<GPMasterBar>> &master_bars() const { return master_bars_value; }

private:

    std::unique_ptr<GPScore> score_value;
    std::unique_ptr<GPMasterTracks> master_tracks_value;
    std::map<int, std::unique_ptr<GPTrack>> tracks_value;
    std::vector<std::unique_ptr<GPMasterBar>> master_bars_value;
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_GPDOMMODEL_H
