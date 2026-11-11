#include <vector>

#include "global/log.h"
#include "gp67dombuilder.h"

using namespace muse;

namespace mu::iex::guitarpro {
std::pair<int, std::unique_ptr<GPTrack>> GP67DomBuilder::create_gp7_track(XmlDomNode *track_node, XmlDomNode *version_node) {
    static const std::set<String> s_unused = {u"Color",
                                              u"SystemsDefautLayout",
                                              u"SystemsLayout",
                                              u"AutoBrush",
                                              u"PalmMute",
                                              u"AutoAccentuation",
                                              u"PlayingStyle",
                                              u"UseOneChannelPerString",
                                              u"IconId",
                                              u"InstrumentSet",
                                              u"ForcedSound",
                                              u"PlaybackState",
                                              u"AudioEngineState",
                                              u"Automations"};

    int track_idx = track_node->to_element().attribute("id").value().to_int();
    auto track = std::make_unique<GPTrack>(track_idx);
    XmlDomNode track_child_node = track_node->first_child();
    String version = version_node->to_element().text();

    while (!track_child_node.is_null()) {
        String node_name = track_child_node.node_name();
        if (node_name == u"Name") {
            track->set_name(track_child_node.to_element().text());
        } else if (node_name == u"RSE") {
            GPTrack::RSE rse = read_track_rse(&track_child_node);
            track->set_rse(rse);
        } else if (node_name == u"MidiConnection") {
            int midi_channel = read_midi_channel(&track_child_node);
            track->set_midi_channel(midi_channel);
        } else if (node_name == u"ShortName") {
            track->set_short_name(track_child_node.to_element().text());
        } else if (node_name == u"Sounds") {
            int programm = read_midi_programm(&track_child_node);
            auto sound_node = track_child_node.first_child();
            while (!sound_node.is_null()) {
                GPTrack::Sound sound = read_sounds(&sound_node);
                track->add_sound(sound);

                sound_node = sound_node.next_sibling();
            }
            track->set_programm(programm);
        } else if (node_name == u"Staves") {
            auto staff_node = track_child_node.first_child();
            int staff_count = 0;
            while (!staff_node.is_null()) {
                auto property_node = staff_node.first_child();
                // there is a bug in gp v 7.0.0
                // All parts marked to use flat for tuning string,
                // but in real world gp uses tuning presets
                // so we have to ignore <Flats/> and <TuningFlat> props
                read_track_properties(&property_node, track.get(), version == "7");
                staff_node = staff_node.next_sibling();
                staff_count++;
            }
            track->set_staff_count(staff_count);
        } else if (node_name == u"NotationPatch") {
            auto inner_node = track_child_node.first_child_element("LineCount");
            if (!inner_node.is_null()) {
                track->set_line_count(inner_node.to_element().text().to_int());
            }
        } else if (node_name == u"Transpose") {
            auto octave_node = track_child_node.first_child_element("Octave");
            int octave = octave_node.to_element().text().to_int();
            auto chromatic_node = track_child_node.first_child_element("Chromatic");
            int chromatic = chromatic_node.to_element().text().to_int();
            int transpose = 12 * octave + chromatic;
            track->set_transpose(transpose);
        } else if (node_name == u"Lyrics") {
            read_lyrics(track_child_node, track.get());
        } else if (node_name == u"Automations") {
            auto automation_node = track_child_node.first_child();
            while (!automation_node.is_null()) {
                GPTrack::SoundAutomation automation = read_track_automation(&automation_node);
                if (!automation.type.is_empty()) {
                    track->add_sound_automation(automation);
                }
                automation_node = automation_node.next_sibling();
            }
        } else if (s_unused.find(node_name) != s_unused.end()) {
            // Ignored nodes
        }

        track_child_node = track_child_node.next_sibling();
    }

    return std::make_pair(track_idx, std::move(track));
}

int GP67DomBuilder::read_midi_channel(XmlDomNode *track_child_node) const {
    int channel = track_child_node->first_child_element("PrimaryChannel").text().to_int();

    return channel;
}

int GP67DomBuilder::read_midi_programm(XmlDomNode *track_child_node) const {
    int programm = track_child_node->first_child().first_child_element("MIDI").first_child_element("Program").text().to_int();

    return programm;
}

GPTrack::Sound GP67DomBuilder::read_sounds(XmlDomNode *sound_node) const {
    GPTrack::Sound result;

    result.programm = sound_node->first_child_element("MIDI").first_child_element("Program").text().to_int();
    result.name = sound_node->first_child_element("Name").text();
    result.label = sound_node->first_child_element("Label").text();
    result.path = sound_node->first_child_element("Path").text();
    result.role = sound_node->first_child_element("Role").text();

    return result;
}

GPTrack::SoundAutomation GP67DomBuilder::read_track_automation(XmlDomNode *automation_node) const {
    GPTrack::SoundAutomation result;

    String type = automation_node->first_child_element("Type").text();
    if (type != u"Sound") {
        return result;
    }

    result.type = type;
    result.linear = automation_node->first_child_element("Linear").text() == u"true";
    result.bar = automation_node->first_child_element("Bar").text().to_int();
    result.value = automation_node->first_child_element("Value").text();
    result.position = automation_node->first_child_element("Position").text().to_float();

    return result;
}
} // namespace mu::iex::guitarpro
