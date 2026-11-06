#ifndef MU_IMPORTEXPORT_GP6DOMBUILDER_H
#define MU_IMPORTEXPORT_GP6DOMBUILDER_H

#include "gp67dombuilder.h"

namespace mu::iex::guitarpro {
struct GP6DomBuilder : GP67DomBuilder {
    GP6DomBuilder() = default;

private:
    virtual std::pair<int, std::unique_ptr<GPTrack>> create_gp_track(muse::XmlDomNode *track_node, muse::XmlDomNode *version_node);
    void set_up_instrument(muse::XmlDomNode *track_child_node, GPTrack *track);
    GPTrack::SoundAutomation read_rse_pick_up(muse::XmlDomNode &rse_node) const;
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_GP6DOMBUILDER_H
