#ifndef MU_IMPORTEXPORT_GPVOICE_H
#define MU_IMPORTEXPORT_GPVOICE_H

#include <vector>

#include "gpbeat.h"

namespace mu::iex::guitarpro {
struct GPVoice {

    void add_gp_beat(const std::shared_ptr<GPBeat> &b) { beats_value.push_back(b); }

    void set_id(int id) { id_value = id; }

    void set_position(int pos) { pos_value = pos; }

    int position() const { return pos_value; }

    const std::vector<std::shared_ptr<GPBeat>> &beats() const { return beats_value; }

private:

    int id_value = -1; // imported id
    int pos_value = 0; // for defining correct track number
    std::vector<std::shared_ptr<GPBeat>> beats_value;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPVOICE_H
