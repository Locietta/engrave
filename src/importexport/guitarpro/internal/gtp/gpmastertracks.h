#ifndef MU_IMPORTEXPORT_GPMASTERTRACKS_H
#define MU_IMPORTEXPORT_GPMASTERTRACKS_H

#include <stddef.h>
#include <vector>

#include "types/string.h"

namespace mu::iex::guitarpro {
struct GPMasterTracks {
    struct Automation {
        enum class Type { TEMPO, VOLUME, PAN };
        int bar{0};
        Type type{Type::TEMPO};
        float position{0};
        bool linear{false};
        double value{0};
        int tempo_unit{0};
        muse::String text;

        friend bool operator<(const Automation &l, const Automation &r) { return l.bar < r.bar; }
    };

    GPMasterTracks() = default;
    ~GPMasterTracks() = default;
    GPMasterTracks(const GPMasterTracks &master_track) = default;
    GPMasterTracks(GPMasterTracks &&master_track) = default;
    GPMasterTracks &operator=(const GPMasterTracks &m_t) = default;
    GPMasterTracks &operator=(GPMasterTracks &&m_t) = default;

    void set_tempo_map(std::vector<Automation> &&t_m) { tempo_map_value.swap(t_m); }

    const std::vector<Automation> &tempo_map() const { return tempo_map_value; }

    void set_tracks_count(size_t tc) { tracks_count_value = tc; }

    size_t tracks_count() { return tracks_count_value; }

private:
    std::vector<Automation> tempo_map_value;
    size_t tracks_count_value;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPMASTERTRACKS_H
