#ifndef MU_IMPORTEXPORT_GP7DOMBUILDER_H
#define MU_IMPORTEXPORT_GP7DOMBUILDER_H

#include "gpdommodel.h"
#include "gp67dombuilder.h"

namespace mu::iex::guitarpro {
struct GP7DomBuilder : GP67DomBuilder {
    GP7DomBuilder() = default;

private:
    std::pair<int, std::unique_ptr<GPTrack>> create_gp_track(muse::XmlDomNode *track_node, muse::XmlDomNode *version_node) override;

    int read_midi_channel(muse::XmlDomNode *track_child_node) const;
    int read_midi_programm(muse::XmlDomNode *track_child_node) const;
    GPTrack::Sound read_sounds(muse::XmlDomNode *track_child_node) const;
    GPTrack::SoundAutomation read_track_automation(muse::XmlDomNode *automation_node) const;
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_GP7DOMBUILDER_H
