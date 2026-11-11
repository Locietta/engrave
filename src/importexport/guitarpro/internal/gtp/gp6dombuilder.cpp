#include <set>

#include "global/log.h"
#include "gp67dombuilder.h"

using namespace muse;

namespace mu::iex::guitarpro {
std::pair<int, std::unique_ptr<GPTrack>> GP67DomBuilder::create_gp6_track(XmlDomNode *track_node,
                                                                          [[maybe_unused]] XmlDomNode *version_node) {
    static const std::set<String> s_unused_nodes = {
        u"Color", // we don't use icon color for the tracks
        u"SystemsDefaultLayout",
        u"SystemsLayout",          // we have our own layout algorithms
        u"SystemsDefautLayout",    // GP has a typo here :)
        u"PalmMute",               // currently our synthesizer is unable to simulate this feature
        u"AutoAccentuation",       // currently our synthesizer is unable to simulate this feature
        u"PlayingStyle",           // currently we ignore playing style
        u"UseOneChannelPerString", // we have our own channel management system in synth
        u"PartSounding",           // don't know what this is
        u"PlaybackState"           // ignored
    };

    int track_idx = track_node->to_element().attribute("id").value().to_int();
    auto track = std::make_unique<GPTrack>(track_idx);
    XmlDomNode track_child_node = track_node->first_child();

    while (!track_child_node.is_null()) {
        String node_name = track_child_node.node_name();
        if (node_name == u"Name") {
            track->set_name(track_child_node.to_element().text());
        } else if (node_name == u"RSE") {
            GPTrack::RSE rse = read_track_rse(&track_child_node);
            track->set_rse(rse);

            XmlDomNode bank_changes_node = track_child_node.first_child_element("BankChanges");
            if (!bank_changes_node.is_null()) {
                XmlDomNode bank_change = bank_changes_node.first_child();
                while (!bank_change.is_null()) {
                    GPTrack::SoundAutomation automation = read_rse_pick_up(bank_change);
                    track->add_sound_automation(automation);
                    bank_change = bank_change.next_sibling();
                }
            }
        } else if (node_name == u"GeneralMidi") {
            if (track_child_node.has_child_nodes()) {
                auto programm = track_child_node.first_child_element("Program").text().to_int();
                track->set_programm(programm);
                int midi_channel = track_child_node.first_child_element("PrimaryChannel").text().to_int();
                track->set_midi_channel(midi_channel);
            }
        } else if (node_name == u"ShortName") {
            track->set_short_name(track_child_node.to_element().text());
        } else if (node_name == u"Properties") {
            read_track_properties(&track_child_node, track.get(), false);
        } else if (node_name == u"Instrument") {
            set_up_instrument(&track_child_node, track.get());
        } else if (node_name == u"Lyrics") {
            read_lyrics(track_child_node, track.get());
        } else if (s_unused_nodes.find(node_name) != s_unused_nodes.end()) {
            // these nodes are not used (see comment to the sUnusedNodes variable)
        }

        track_child_node = track_child_node.next_sibling();
    }

    return std::make_pair(track_idx, std::move(track));
}

void GP67DomBuilder::set_up_instrument(XmlDomNode *track_child_node, GPTrack *track) {
    String ref = track_child_node->to_element().attribute("ref").value();
    track->set_instrument(ref);
    if (ref.ends_with(u"-gs") || ref.starts_with(u'2')) { // grand staff
        track->set_staff_count(2);
    }

    if (ref.contains(u"gtr") || ref.contains(u"bass")) {
        //! NOTE Guitar notation is transposed to octave by default (see music grammar)
        track->set_transpose(-12);

        track->set_is_guitar(true);
    }
}

GPTrack::SoundAutomation GP67DomBuilder::read_rse_pick_up(XmlDomNode &bank_changes_node) const {
    GPTrack::SoundAutomation result;

    result.bar = bank_changes_node.to_element().attribute("barIndex").value().to_int();
    result.value = bank_changes_node.to_element().attribute("bankId").value();
    result.position = bank_changes_node.to_element().attribute("tickOffset").value().to_float();

    return result;
}
} // namespace mu::iex::guitarpro
