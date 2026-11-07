#include "gp67dombuilder.h"

#include <set>

#include "engraving/types/constants.h"
#include "global/log.h"

using namespace muse;

namespace mu::iex::guitarpro {
GP67DomBuilder::GP67DomBuilder() { gp_dom = std::make_unique<GPDomModel>(); }

void GP67DomBuilder::build_gp_dom_model(XmlDomElement *dom_elem) {
    XmlDomNode revision;
    // Score node
    XmlDomNode score_node, master_track, audio_tracks, each_track, master_bars, bars, voices, beats, notes, rhythms;

    XmlDomNode gpversion, encoding;

    std::map<String, XmlDomNode *> node_map = {{u"GPRevision", &revision},
                                               {u"Score", &score_node},
                                               {u"MasterTrack", &master_track},
                                               {u"AudioTracks", &audio_tracks},
                                               {u"Tracks", &each_track},
                                               {u"MasterBars", &master_bars},
                                               {u"Bars", &bars},
                                               {u"Voices", &voices},
                                               {u"Beats", &beats},
                                               {u"Notes", &notes},
                                               {u"Rhythms", &rhythms},
                                               {u"GPVersion", &gpversion},
                                               {u"Encoding", &encoding}};

    auto assign_map = [&node_map](XmlDomNode node) {
        auto iter = node_map.find(node.node_name());
        if (iter != node_map.end()) {
            *(iter->second) = node;
        }
    };

    for (XmlDomNode current = dom_elem->first_child(); !current.is_null(); current = current.next_sibling()) {
        assign_map(current);
    }

    build_gp_rhythms(&rhythms);
    build_gp_notes(&notes);
    build_gp_beats(&beats);
    build_gp_voices(&voices);
    build_gp_bars(&bars);
    build_gp_master_bars(&master_bars);

    build_gp_score(&score_node);
    build_gp_master_tracks(&master_track);
    build_gp_audio_tracks(&audio_tracks);
    build_gp_tracks(&each_track, &gpversion);
}

std::unique_ptr<GPDomModel> GP67DomBuilder::get_gp_dom_model() { return std::move(gp_dom); }

void GP67DomBuilder::build_gp_score(XmlDomNode *score_node) {
    // Contains list of unused info
    static const std::set<String> s_unused_nodes = {u"FirstPageFooter",           u"FirstPageHeader",    u"PageFooter", u"PageHeader",
                                                    u"ScoreSystemsDefaultLayout", u"ScoreSystemsLayout", u"PageSetup"};

    std::unique_ptr<GPScore> score = std::make_unique<GPScore>();
    XmlDomNode current_node = score_node->first_child();
    while (!current_node.is_null()) {
        String node_name = current_node.node_name();
        if (node_name == u"Title") {
            score->set_title(current_node.to_element().text());
        } else if (node_name == u"Subtitle" || node_name == u"SubTitle") {
            score->set_sub_title(current_node.to_element().text());
        } else if (node_name == u"Artist") {
            score->set_artist(current_node.to_element().text());
        } else if (node_name == u"Album") {
            score->set_album(current_node.to_element().text());
        } else if (node_name == u"Words") {
            score->set_poet(current_node.to_element().text());
        } else if (node_name == u"Music") {
            score->set_composer(current_node.to_element().text());
        } else if (node_name == u"Copyright") {
            // Currently we ignore Copyright info
        } else if (node_name == u"Tabber") {
            // Currently we ignore Tabber info
        } else if (node_name == u"Instructions" || node_name == u"Notices") {
            // Currently we ignore score unrelated texts
        } else if (node_name == u"MultiVoice") {
            /// gp saves the value "1>" instead of "1"
            String multi_voice_string = current_node.to_element().text();
            if (!multi_voice_string.empty()) {
                score->set_multi_voice(String(multi_voice_string[0]).to_int());
            }
        } else if (s_unused_nodes.find(node_name) != s_unused_nodes.end()) {
            // Ignored nodes, which specify unused specifics (e.g. default layout, footers e.t.c.)
        }

        current_node = current_node.next_sibling();
    }
    gp_dom->add_gp_score(std::move(score));
}

void GP67DomBuilder::build_gp_master_tracks(XmlDomNode *master_track) {
    std::unique_ptr<GPMasterTracks> master_tracks = std::make_unique<GPMasterTracks>();

    XmlDomNode current_node = master_track->first_child();
    while (!current_node.is_null()) {
        String node_name = current_node.node_name();
        if (node_name == u"Automations") {
            master_tracks->set_tempo_map(read_tempo_map(&current_node));
        } else if (node_name == u"RSE") {
            //! TODO volume and pan(balance) of mixer are set here
        } else if (node_name == u"Tracks") {
            String tracks = current_node.to_element().text();
            size_t tracks_count = tracks.split(u' ').size();
            master_tracks->set_tracks_count(tracks_count);
        }

        current_node = current_node.next_sibling();
    }

    gp_dom->add_gp_master_tracks(std::move(master_tracks));
}

void GP67DomBuilder::build_gp_audio_tracks(XmlDomNode *audio_track) {
    XmlDomNode current_node = audio_track->first_child();
    while (!current_node.is_null()) {
        current_node = current_node.next_sibling();
    }
}

void GP67DomBuilder::build_gp_tracks(XmlDomNode *tracks_node, XmlDomNode *version_node) {
    std::map<int, std::unique_ptr<GPTrack>> tracks;
    XmlDomNode current_node = tracks_node->first_child();
    while (!current_node.is_null()) {
        tracks.insert(create_gp_track(&current_node, version_node));
        current_node = current_node.next_sibling();
    }

    gp_dom->add_gp_tracks(std::move(tracks));
}

void GP67DomBuilder::build_gp_master_bars(XmlDomNode *master_bars_node) {
    std::vector<std::unique_ptr<GPMasterBar>> master_bars;

    XmlDomNode inner_node = master_bars_node->first_child();
    int master_bar_idx = 0;
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();

        if (node_name == u"MasterBar") {
            master_bars.push_back(create_gp_master_bar(&inner_node));
            master_bars.back()->set_id(master_bar_idx);
            master_bar_idx++;
        }

        inner_node = inner_node.next_sibling();
    }

    gp_dom->add_gp_master_bars(std::move(master_bars));
}

void GP67DomBuilder::build_gp_bars(XmlDomNode *bars_node) {
    std::unordered_map<int, std::unique_ptr<GPBar>> bars;

    XmlDomNode inner_node = bars_node->first_child();
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();
        if (node_name == u"Bar") {
            bars.insert(create_gp_bar(&inner_node));
        }

        inner_node = inner_node.next_sibling();
    }

    bars_value.swap(bars);
}

void GP67DomBuilder::build_gp_voices(XmlDomNode *voices_node) {
    std::unordered_map<int, std::unique_ptr<GPVoice>> voices;

    XmlDomNode inner_node = voices_node->first_child();
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();
        if (node_name == u"Voice") {
            voices.insert(create_gp_voice(&inner_node));
        }

        inner_node = inner_node.next_sibling();
    }

    this->voices.swap(voices);
}

void GP67DomBuilder::build_gp_beats(XmlDomNode *beats_node) {
    std::unordered_map<int, std::shared_ptr<GPBeat>> beats;

    XmlDomNode inner_node = beats_node->first_child();
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();
        if (node_name == u"Beat") {
            beats.insert(create_gp_beat(&inner_node));
        }

        inner_node = inner_node.next_sibling();
    }

    beats_value.swap(beats);
}

void GP67DomBuilder::build_gp_notes(XmlDomNode *notes_node) {
    std::unordered_map<int, std::shared_ptr<GPNote>> notes;

    XmlDomNode inner_node = notes_node->first_child();

    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();
        if (node_name == u"Note") {
            notes.insert(create_gp_note(&inner_node));
        }

        inner_node = inner_node.next_sibling();
    }

    this->notes.swap(notes);
}

void GP67DomBuilder::build_gp_rhythms(XmlDomNode *rhythms_node) {
    std::unordered_map<int, std::shared_ptr<GPRhythm>> rhythms;

    XmlDomNode inner_node = rhythms_node->first_child();
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();
        if (node_name == u"Rhythm") {
            rhythms.insert(create_gp_rhythm(&inner_node));
        }

        inner_node = inner_node.next_sibling();
    }

    rhythms_value.swap(rhythms);
}

std::vector<GPMasterTracks::Automation> GP67DomBuilder::read_tempo_map(XmlDomNode *current_node) {
    std::vector<GPMasterTracks::Automation> tempo_map;
    XmlDomNode current_automation = current_node->first_child();
    while (!current_automation.is_null()) {
        if (current_automation.node_name() == u"Automation") {
            XmlDomElement ln = current_automation.first_child_element("Linear");
            String first_name = current_automation.first_child().node_name();
            if (first_name == u"Type") {
                first_name = current_automation.first_child().to_element().text();
            }
            if (first_name == u"Tempo") {
                GPMasterTracks::Automation tempo;
                tempo.type = GPMasterTracks::Automation::Type::TEMPO;
                String str = current_automation.first_child_element("Value").to_element().text();
                StringList tempo_value = str.split(u' ');
                tempo.value = tempo_value[0].to_double();
                tempo.tempo_unit = tempo_value.size() > 1 ? static_cast<int>(tempo_value[1].to_double()) : 0;
                tempo.bar = current_automation.first_child_element("Bar").text().to_int();
                tempo.position = current_automation.first_child_element("Position").text().to_float();
                tempo.linear = (ln.to_element().text() == u"true");
                XmlDomElement label_elem = current_automation.first_child_element("Text");
                if (label_elem.has_child_nodes()) {
                    tempo.text = label_elem.to_element().text();
                }

                tempo_map.push_back(tempo);
            }
        }
        current_automation = current_automation.next_sibling();
    }

    return tempo_map;
}

std::unique_ptr<GPMasterTracks> GP67DomBuilder::create_gp_master_track([[maybe_unused]] XmlDomNode *metadata) {
    return std::make_unique<GPMasterTracks>();
}

std::unique_ptr<GPMasterBar> GP67DomBuilder::create_gp_master_bar(XmlDomNode *master_bar_node) {
    static const std::set<String> s_unused = {u"XProperties"};

    auto triplet_feel_type = [](String &&str) {
        if (str == u"Triplet8th") {
            return GPMasterBar::TripletFeelType::TRIPLET8TH;
        }
        if (str == u"Triplet16th") {
            return GPMasterBar::TripletFeelType::TRIPLET16TH;
        }
        if (str == u"Dotted8th") {
            return GPMasterBar::TripletFeelType::DOTTED8TH;
        }
        if (str == u"Dotted16th") {
            return GPMasterBar::TripletFeelType::DOTTED16TH;
        }
        if (str == u"Scottish8th") {
            return GPMasterBar::TripletFeelType::SCOTTISH8TH;
        }
        if (str == u"Scottish16th") {
            return GPMasterBar::TripletFeelType::SCOTTISH16TH;
        }
        return GPMasterBar::TripletFeelType::NONE;
    };

    std::unique_ptr<GPMasterBar> master_bar = std::make_unique<GPMasterBar>();

    auto inner_node = master_bar_node->first_child();

    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();

        if (node_name == u"Time") {
            master_bar->set_time_sig(read_time_sig(&inner_node));
        } else if (node_name == u"Repeat") {
            master_bar->set_repeat(read_repeat(&inner_node));
        } else if (node_name == u"AlternateEndings") {
            master_bar->set_alternative_ending(read_ending(&inner_node));
        } else if (node_name == u"Key") {
            master_bar->set_key_sig(read_key_sig(&inner_node), read_use_flats(&inner_node));
        } else if (node_name == u"Bars") {
            const String &bars_element = inner_node.to_element().text();
            const StringList &bars = bars_element.split(u' ');
            for (const String &bar_idx : bars) {
                int idx = bar_idx.to_int();
                std::unique_ptr<GPBar> bar;
                bar = std::move(bars_value.at(idx));
                bars_value.erase(idx);
                master_bar->add_gp_bar(std::move(bar));
            }
        } else if (node_name == u"TripletFeel") {
            master_bar->set_triplet_feel(triplet_feel_type(inner_node.to_element().text()));
        } else if (node_name == u"Fermatas") {
            master_bar->set_fermatas(read_fermatas(&inner_node));
        } else if (node_name == u"Section") {
            master_bar->set_section(read_master_bar_section(inner_node));
        } else if (node_name == u"Directions") {
            master_bar->set_directions(read_repeats_jumps(&inner_node));
        } else if (node_name == u"DoubleBar") {
            master_bar->set_barline_type(GPMasterBar::BarlineType::DOUBLE);
        } else if (node_name == u"FreeTime") {
            master_bar->set_free_time(true);
        } else if (s_unused.find(node_name) != s_unused.end()) {
            // Ignored
        }

        inner_node = inner_node.next_sibling();
    }

    return master_bar;
}

std::pair<int, std::unique_ptr<GPBar>> GP67DomBuilder::create_gp_bar(XmlDomNode *bar_node) {
    auto clef_type = [](const String &clef) {
        if (clef == u"C4") {
            return GPBar::ClefType::C4;
        } else if (clef == u"C3") {
            return GPBar::ClefType::C3;
        } else if (clef == u"F4") {
            return GPBar::ClefType::F4;
        } else if (clef == u"G2") {
            return GPBar::ClefType::G2;
        } else {
            return GPBar::ClefType::NEUTRAL;
        }
    };

    auto ottavia_type = [](const String &ott) {
        if (ott == u"8va") {
            return GPBar::OttaviaType::VA8;
        } else if (ott == u"15ma") {
            return GPBar::OttaviaType::MA15;
        } else if (ott == u"8vb") {
            return GPBar::OttaviaType::VB8;
        } else if (ott == u"15mb") {
            return GPBar::OttaviaType::MB15;
        } else {
            return GPBar::OttaviaType::REGULAR;
        }
    };
    auto simile_mark_type = [](const String &str) {
        if (str == u"Simple") {
            return GPBar::SimileMark::SIMPLE;
        } else if (str == u"FirstOfDouble") {
            return GPBar::SimileMark::FIRST_OF_DOUBLE;
        } else if (str == u"SecondOfDouble") {
            return GPBar::SimileMark::SECOND_OF_DOUBLE;
        } else {
            return GPBar::SimileMark::NONE;
        }
    };

    std::unique_ptr<GPBar> bar = std::make_unique<GPBar>();

    auto inner_node = bar_node->first_child();
    int bar_idx = bar_node->to_element().attribute("id").value().to_int();
    bar->set_id(bar_idx);

    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();

        if (node_name == u"Clef") {
            bar->set_clef_type(clef_type(inner_node.to_element().text()));
        } else if (node_name == u"Ottavia") {
            bar->set_ottavia_type(ottavia_type(inner_node.to_element().text()));
        } else if (node_name == u"SimileMark") {
            bar->set_simile_mark(simile_mark_type(inner_node.to_element().text()));
        } else if (node_name == u"Voices") {
            String voices_element = inner_node.to_element().text();
            StringList voices = voices_element.split(u' ');
            int current_position = -1;
            for (const String &voice_idx : voices) {
                current_position++;
                int idx = voice_idx.to_int();
                if (idx == -1) {
                    continue;
                }

                std::unique_ptr<GPVoice> voice;
                voice = std::move(this->voices.at(idx));
                voice->set_position(current_position);
                this->voices.erase(idx);
                bar->add_gp_voice(std::move(voice));
            }
        }

        inner_node = inner_node.next_sibling();
    }

    return std::make_pair(bar_idx, std::move(bar));
}

std::pair<int, std::unique_ptr<GPVoice>> GP67DomBuilder::create_gp_voice(XmlDomNode *voice_node) {
    std::unique_ptr<GPVoice> voice = std::make_unique<GPVoice>();

    XmlDomNode inner_node = voice_node->first_child();
    int voice_idx = voice_node->to_element().attribute("id").value().to_int();
    voice->set_id(voice_idx);

    while (!inner_node.is_null()) {
        auto node_name = inner_node.node_name();
        if (node_name == u"Beats") {
            String beats_element = inner_node.to_element().text();
            StringList beats = beats_element.split(u' ');
            for (const String &beat_idx : beats) {
                int idx = beat_idx.to_int();
                std::shared_ptr<GPBeat> beat;
                beat = beats_value.at(idx);
                voice->add_gp_beat(beat);
            }
        }

        inner_node = inner_node.next_sibling();
    }

    return std::make_pair(voice_idx, std::move(voice));
}

std::pair<int, std::shared_ptr<GPBeat>> GP67DomBuilder::create_gp_beat(XmlDomNode *beat_node) {
    static const std::set<String> s_unused = {u"Bank", u"StemOrientation", u"ConcertPitchStemOrientation", u"Ottavia"};

    auto dynamic_type = [](String &&str) -> GPBeat::DynamicType {
        if (str == u"FFF") {
            return GPBeat::DynamicType::FFF;
        } else if (str == u"FF") {
            return GPBeat::DynamicType::FF;
        } else if (str == u"F") {
            return GPBeat::DynamicType::F;
        } else if (str == u"MF") {
            return GPBeat::DynamicType::MF;
        } else if (str == u"MP") {
            return GPBeat::DynamicType::MP;
        } else if (str == u"P") {
            return GPBeat::DynamicType::P;
        } else if (str == u"PP") {
            return GPBeat::DynamicType::PP;
        } else {
            return GPBeat::DynamicType::PPP;
        }
    };
    auto legato_type = [](const String &origin, const String &destination) {
        if (origin == u"true" && destination == u"false") {
            return GPBeat::LegatoType::START;
        } else if (origin == u"true" && destination == u"true") {
            return GPBeat::LegatoType::MEDIATE;
        } else if (origin == u"false" && destination == u"true") {
            return GPBeat::LegatoType::END;
        } else {
            return GPBeat::LegatoType::NONE;
        }
    };
    auto arpeggio_type = [](const String &arp) {
        if (arp == u"Up") {
            return GPBeat::Arpeggio::UP;
        } else if (arp == u"Down") {
            return GPBeat::Arpeggio::DOWN;
        }
        return GPBeat::Arpeggio::NONE;
    };
    auto grace_notes = [](const String &gn) {
        if (gn == u"OnBeat") {
            return GPBeat::GraceNotes::ON_BEAT;
        } else if (gn == u"BeforeBeat") {
            return GPBeat::GraceNotes::BEFORE_BEAT;
        }
        return GPBeat::GraceNotes::NONE;
    };
    auto fadding_type = [](const String &str) {
        if (str == u"FadeIn") {
            return GPBeat::Fadding::FADE_IN;
        } else if (str == u"FadeOut") {
            return GPBeat::Fadding::FADE_OUT;
        } else if (str == u"VolumeSwell") {
            return GPBeat::Fadding::VOLUME_SWELL;
        }
        return GPBeat::Fadding::NONE;
    };
    auto hairpin_type = [](const String &str) {
        if (str == u"Crescendo") {
            return GPBeat::Hairpin::CRESCENDO;
        } else if (str == u"Decrescendo") {
            return GPBeat::Hairpin::DECRESCENDO;
        }
        return GPBeat::Hairpin::NONE;
    };
    auto wah_type = [](const String &str) {
        if (str == u"Open") {
            return GPBeat::Wah::OPEN;
        } else if (str == u"Closed") {
            return GPBeat::Wah::CLOSED;
        } else {
            return GPBeat::Wah::NONE;
        }
    };

    auto golpe_type = [](const String &str) {
        if (str == u"Finger") {
            return GPBeat::Golpe::FINGER;
        } else if (str == u"Thumb") {
            return GPBeat::Golpe::THUMB;
        }

        return GPBeat::Golpe::NONE;
    };

    auto ottava_type = [](const String &ott) {
        if (ott == u"8va") {
            return GPBeat::OttavaType::VA8;
        } else if (ott == u"15ma") {
            return GPBeat::OttavaType::MA15;
        } else if (ott == u"8vb") {
            return GPBeat::OttavaType::VB8;
        } else if (ott == u"15mb") {
            return GPBeat::OttavaType::MB15;
        }
        LOGE() << "wrong ottava type: " << ott;
        return GPBeat::OttavaType::NONE;
    };

    std::shared_ptr<GPBeat> beat = std::make_shared<GPBeat>();

    auto inner_node = beat_node->first_child();
    int beat_idx = beat_node->to_element().attribute("id").value().to_int();
    beat->set_id(beat_idx);

    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();

        if (node_name == u"Dynamic") {
            GPBeat::DynamicType dynamic = dynamic_type(inner_node.to_element().text());
            beat->set_dynamic(dynamic);
        } else if (node_name == u"Legato") {
            String origin = inner_node.to_element().attribute("origin").value();
            String destination = inner_node.to_element().attribute("destination").value();
            GPBeat::LegatoType legato = legato_type(origin, destination);
            beat->set_legato_type(legato);
        } else if (node_name == u"Rhythm") {
            int r_idx = inner_node.to_element().attribute("ref").value().to_int();
            beat->add_gp_rhythm(rhythms_value.at(r_idx));
        } else if (node_name == u"Notes") {
            String notes_str = inner_node.to_element().text();
            StringList str_list = notes_str.split(u' ');
            for (const auto &str_idx : str_list) {
                int idx = str_idx.to_int();
                std::shared_ptr<GPNote> note;
                note = this->notes.at(idx);
                beat->add_gp_note(note);
            }
            beat->sort_gp_notes();
        } else if (node_name == u"GraceNotes") {
            beat->set_grace_notes(grace_notes(inner_node.to_element().text()));
        } else if (node_name == u"Arpeggio") {
            beat->set_arpeggio(arpeggio_type(inner_node.to_element().text()));
        } else if (node_name == u"Properties") {
            read_beat_properties(inner_node, beat.get());
        } else if (node_name == u"Chord") {
            beat->set_diagram_idx(inner_node.to_element().text().to_int());
        } else if (node_name == u"Timer") {
            beat->set_time(inner_node.to_element().text().to_int());
        } else if (node_name == u"FreeText") {
            beat->set_free_text(inner_node.to_element().text());
        } else if (node_name == u"Fadding") {
            beat->set_fadding(fadding_type(inner_node.to_element().text()));
        } else if (node_name == u"Hairpin") {
            beat->set_hairpin(hairpin_type(inner_node.to_element().text()));
        } else if (node_name == u"Tremolo") {
            GPBeat::Tremolo tr;
            String tr_str = inner_node.to_element().text();
            StringList tr_list = tr_str.split(u'/');
            tr.numerator = tr_list.at(0).to_int();
            tr.denominator = tr_list.at(1).to_int();
            beat->set_tremolo(tr);
        } else if (node_name == u"Wah") {
            beat->set_wah(wah_type(inner_node.to_element().text()));
        } else if (node_name == u"Golpe") {
            beat->set_golpe(golpe_type(inner_node.to_element().text()));
        } else if (node_name == u"Lyrics") {
            // this code reads lyrics for the beat (only one line).

            XmlDomElement lyr_node = inner_node.first_child_element("Line");
            String str = lyr_node.to_element().text();
            beat->set_lyrics(str.to_std_string());
        } else if (node_name == u"Ottavia") {
            beat->set_ottava_type(ottava_type(inner_node.to_element().text()));
        } else if (node_name == u"Whammy" || node_name == u"WhammyExtend") {
            beat->set_whammy(read_whammy(inner_node.to_element()));
        } else if (node_name == u"DeadSlapped") {
            beat->set_dead_slapped(true);
        } else if (node_name == u"TransposedPitchStemOrientation") {
            beat->set_stem_orientation_up(inner_node.to_element().text() == u"Upward");
        } else if (node_name == u"TransposedPitchStemOrientationUserDefined") {
            beat->set_stem_orientation_user_defined(true);
        } else if (node_name == u"XProperties") {
            read_beat_x_properties(inner_node, beat.get());
        } else if (s_unused.find(node_name) != s_unused.end()) {
            // Ignored nodes
        }

        inner_node = inner_node.next_sibling();
    }

    return std::make_pair(beat_idx, std::move(beat));
}

std::pair<int, std::shared_ptr<GPNote>> GP67DomBuilder::create_gp_note(XmlDomNode *note_node) {
    auto tie_type = [](const String &origin, const String &destination) -> GPNote::TieType {
        if (origin == u"true" && destination == u"false") {
            return GPNote::TieType::START;
        } else if (origin == u"true" && destination == u"true") {
            return GPNote::TieType::MEDIATE;
        } else if (origin == u"false" && destination == u"true") {
            return GPNote::TieType::END;
        } else {
            return GPNote::TieType::NONE;
        }
    };
    auto vibrato_type = [](const String &str) {
        if (str == u"Wide") {
            return GPNote::VibratoType::WIDE;
        } else if (str == u"Slight") {
            return GPNote::VibratoType::SLIGHT;
        } else {
            return GPNote::VibratoType::NONE;
        }
    };
    auto ornament_type = [](const String &str) {
        if (str == u"UpperMordent") {
            return GPNote::Ornament::UPPER_MORDENT;
        } else if (str == u"LowerMordent") {
            return GPNote::Ornament::LOWER_MORDENT;
        } else if (str == u"InvertedTurn") {
            return GPNote::Ornament::INVERTED_TURN;
        } else if (str == u"Turn") {
            return GPNote::Ornament::TURN;
        } else {
            return GPNote::Ornament::NONE;
        }
    };

    auto note = std::make_shared<GPNote>();
    int note_idx = note_node->to_element().attribute("id").value().to_int();
    note->set_id(note_idx);

    auto inner_node = note_node->first_child();
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();

        if (node_name == u"Accidental") {
            std::map<String, int> accidentals = {{u"DoubleFlat", -2}, {u"Flat", -1}, {u"Natural", 0}, {u"Sharp", +1}, {u"DoubleSharp", +2}};

            String accidental_name = inner_node.to_element().text();

            if (accidentals.find(accidental_name) != accidentals.end()) {
                note->set_accidental(accidentals[accidental_name]);
            }
        }
        if (node_name == u"InstrumentArticulation") {}
        if (node_name == u"Properties") {
            read_note_properties(&inner_node, note.get());
        }
        if (node_name == u"XProperties") {
            read_note_x_properties(inner_node, note.get());
        }
        if (node_name == u"Tie") {
            String origin = inner_node.to_element().attribute("origin").value();
            String destination = inner_node.to_element().attribute("destination").value();
            GPNote::TieType tie = tie_type(origin, destination);
            note->set_tie_type(tie);
        }
        if (node_name == u"LetRing") {
            note->set_let_ring(true);
        } else if (node_name == u"AntiAccent") {
            note->set_ghost_note(inner_node.to_element().text() == u"Normal");
        } else if (node_name == u"Accent") {
            note->set_accent(inner_node.to_element().text().to_u_int());
        } else if (node_name == u"LeftFingering") {
            String finger = inner_node.to_element().text();
            if (finger == u"Open") {
                finger = u"0";
            } else if (finger == u"P") {
                finger = u"t";
            } else if (finger == u"I") {
                finger = u"1";
            } else if (finger == u"M") {
                finger = u"2";
            } else if (finger == u"A") {
                finger = u"3";
            } else if (finger == u"C") {
                finger = u"4";
            }
            note->set_left_fingering(finger);
        } else if (node_name == "RightFingering") {
            note->set_right_fingering(inner_node.to_element().text().to_lower());
        } else if (node_name == "Vibrato") {
            note->set_vibrato_type(vibrato_type(inner_node.to_element().text()));
        } else if (node_name == "Trill") {
            note->set_trill_fret(inner_node.to_element().text().to_int());
        } else if (node_name == "Ornament") {
            note->set_ornament(ornament_type(inner_node.to_element().text()));
        }

        inner_node = inner_node.next_sibling();
    }

    return std::make_pair(note_idx, std::move(note));
}

std::pair<int, std::shared_ptr<GPRhythm>> GP67DomBuilder::create_gp_rhythm(XmlDomNode *rhythm_node) {
    auto rhythm_type = [](const String &str) -> GPRhythm::RhytmType {
        if (str == u"Whole") {
            return GPRhythm::RhytmType::WHOLE;
        } else if (str == u"Half") {
            return GPRhythm::RhytmType::HALF;
        } else if (str == u"Quarter") {
            return GPRhythm::RhytmType::QUARTER;
        } else if (str == u"Eighth") {
            return GPRhythm::RhytmType::EIGHTH;
        } else if (str == u"16th") {
            return GPRhythm::RhytmType::SIXTEENTH;
        } else if (str == u"32nd") {
            return GPRhythm::RhytmType::THIRTY_SECOND;
        } else {
            return GPRhythm::RhytmType::SIXTY_FOURTH;
        }
    };

    std::shared_ptr<GPRhythm> rhythm = std::make_shared<GPRhythm>();

    auto inner_node = rhythm_node->first_child();
    int rhythm_idx = rhythm_node->to_element().attribute("id").value().to_int();
    while (!inner_node.is_null()) {
        String node_name = inner_node.node_name();
        if (node_name == u"NoteValue") {
            auto rh_type = rhythm_type(inner_node.to_element().text());
            rhythm->set_rhytm(rh_type);
        }
        if (node_name == u"AugmentationDot") {
            rhythm->set_dot_count(inner_node.to_element().attribute("count").value().to_int());
        } else if (node_name == u"PrimaryTuplet") {
            int num = inner_node.to_element().attribute("num").value().to_int();
            int denom = inner_node.to_element().attribute("den").value().to_int();
            rhythm->set_tuplet({num, denom});
        }

        inner_node = inner_node.next_sibling();
    }

    return std::make_pair(rhythm_idx, std::move(rhythm));
}

GPTrack::RSE GP67DomBuilder::read_track_rse(XmlDomNode *track_child_node) const {
    XmlDomNode inner_node = track_child_node->first_child();
    String node_name = inner_node.node_name();

    if (!inner_node.is_null() && node_name == u"ChannelStrip") {
        inner_node = inner_node.first_child_element("Parameters");
        if (!inner_node.is_null()) {
            String str = inner_node.to_element().text();
            StringList str_list = str.split(u' ');
            GPTrack::RSE rse;
            rse.pan = str_list.at(11).to_float();
            rse.volume = str_list.at(12).to_float();
            return rse;
        }
    }

    return GPTrack::RSE();
}

GPMasterBar::KeySig GP67DomBuilder::read_key_sig(XmlDomNode *key_node) const {
    const auto &accidental_count = key_node->first_child_element("AccidentalCount");
    const auto &mode_node = key_node->first_child_element("Mode");

    String mode_name = mode_node.to_element().text();

    GPMasterBar::KeySig::Mode mode = GPMasterBar::KeySig::Mode::MAJOR;
    if (mode_name == "Minor") {
        mode = GPMasterBar::KeySig::Mode::MINOR;
    }

    int key_count = accidental_count.to_element().text().to_int();

    return GPMasterBar::KeySig{GPMasterBar::KeySig::Accidentals(key_count), mode};
}

bool GP67DomBuilder::read_use_flats(XmlDomNode *key_node) const {
    const auto &transpose_as = key_node->first_child_element("TransposeAs");
    if (transpose_as.is_null()) {
        return false;
    }
    return transpose_as.to_element().text() == "Flats";
}

GPMasterBar::TimeSig GP67DomBuilder::read_time_sig(XmlDomNode *time_node) const {
    const String time = time_node->to_element().text();
    const StringList time_sig = time.split(u'/');
    GPMasterBar::TimeSig sig{time_sig.at(0).to_int(), time_sig.at(1).to_int()};
    return sig;
}

void GP67DomBuilder::read_note_x_properties(const XmlDomNode &properties_node, GPNote *note) {
    auto property_node = properties_node.first_child();

    while (!property_node.is_null()) {
        int property_id = property_node.to_element().attribute("id").value().to_int();

        if (property_id == 688062467) {
            note->set_trill_speed(property_node.first_child().to_element().text().to_int());
        }

        property_node = property_node.next_sibling();
    }
}

void GP67DomBuilder::read_note_properties(XmlDomNode *properties_node, GPNote *note) {
    std::unordered_set<std::unique_ptr<INoteProperty>> properties;

    auto property_node = properties_node->first_child();

    while (!property_node.is_null()) {
        auto property_name = property_node.to_element().attribute("name").value();

        if (property_name == u"Midi") {
            int midi = property_node.first_child().to_element().text().to_int();
            note->set_midi(midi);
        }
        if (property_name == u"Variation") {
            note->set_variation(property_node.first_child().to_element().text().to_int());
        }
        if (property_name == u"Element") {
            note->set_element(property_node.first_child().to_element().text().to_int());
        } else if (property_name == u"String") {
            int string = property_node.first_child().to_element().text().to_int();
            note->set_string(string);
        } else if (property_name == u"Fret") {
            int fret = property_node.first_child().to_element().text().to_int();
            note->set_fret(fret);
        } else if (property_name == u"Octave") {
            int octave = property_node.first_child().to_element().text().to_int();
            note->set_octave(octave);
        } else if (property_name == u"ConcertPitch") {
            auto pitch_node = property_node.first_child();
            auto inner_node = pitch_node.first_child();
            while (!inner_node.is_null()) {
                String node_name = inner_node.node_name();
                if (node_name == u"Accidental") {
                    std::map<String, int> accidentals = {
                        {u"bb", -2},
                        {u"b", -1},
                        {u"#", +1},
                        {u"x", +2},
                    };

                    String accidental_name = inner_node.to_element().text();
                    if (!accidental_name.is_empty() && accidentals.find(accidental_name) != accidentals.end()) {
                        note->set_accidental(accidentals[accidental_name]);
                    } else {
                        note->set_accidental(0);
                    }
                }

                inner_node = inner_node.next_sibling();
            }
        } else if (property_name == u"Tone") {
            int tone = property_node.first_child().to_element().text().to_int();
            note->set_tone(tone);
        } else if (property_name == u"Bended") {
            if (property_node.first_child().node_name() == "Enable") {
                note->set_bend(create_bend(&property_node));
            }
        } else if (property_name == u"Harmonic" || property_name == u"HarmonicFret" || property_name == u"HarmonicType") {
            read_harmonic(&property_node, note);
        } else if (property_name == u"PalmMuted") {
            if (property_node.first_child().node_name() == "Enable") {
                note->set_palm_mute(true);
            }
        } else if (property_name == u"Muted") {
            //! property muted in GP means dead note
            if (property_node.first_child().node_name() == "Enable") {
                note->set_mute(true);
            }
        } else if (property_name == u"Slide") {
            int slide_info = property_node.first_child().to_element().text().to_u_int();
            switch (slide_info) {
                case 64: note->set_pick_scrape(GPNote::PickScrape::DOWN); break;
                case 128: note->set_pick_scrape(GPNote::PickScrape::UP); break;
                default: note->set_slides(slide_info); break;
            }
        } else if (property_name == u"HopoOrigin") {
            note->set_hammer_on(GPNote::HammerOn::START);
        } else if (property_name == u"Tapped") {
            if (property_node.first_child().node_name() == "Enable") {
                note->set_right_hand_tapping(true);
            }
        } else if (property_name == u"LeftHandTapped") {
            if (property_node.first_child().node_name() == "Enable") {
                note->set_left_hand_tapped(true);
            }
        } else if (property_name == "ShowStringNumber") {
            note->set_show_string_number(true);
        }

        property_node = property_node.next_sibling();
    }

    note->add_properties(std::move(properties));
    return;
}

void GP67DomBuilder::read_beat_x_properties(const XmlDomNode &properties_node, GPBeat *beat) {
    auto property_node = properties_node.first_child();

    bool broken_beams = false;
    bool broken_secondary_beams = false;
    bool joined_beams = false;

    while (!property_node.is_null()) {
        int property_id = property_node.to_element().attribute("id").value().to_int();

        if (property_id == 687931393 || property_id == 687935489) {
            // arpeggio/brush ticks
            beat->set_arpeggio_stretch(property_node.first_child().to_element().text().to_double() / mu::engraving::Constants::division);
        } else if (property_id == 1124204546) {
            int beam_data = property_node.first_child().to_element().text().to_int();

            if (beam_data == 1) {
                joined_beams = true;
            } else if (beam_data == 2) {
                broken_beams = true;
            }
        } else if (property_id == 1124204552) {
            int beam_data = property_node.first_child().to_element().text().to_int();
            if (beam_data == 1) {
                broken_secondary_beams = true;
            }
        }

        property_node = property_node.next_sibling();
    }

    if (broken_beams) {
        beat->set_beam_mode(GPBeat::BeamMode::BROKEN);
    } else if (broken_secondary_beams) {
        beat->set_beam_mode(joined_beams ? GPBeat::BeamMode::BROKEN2_JOINED : GPBeat::BeamMode::BROKEN2);
    } else if (joined_beams) {
        beat->set_beam_mode(GPBeat::BeamMode::JOINED);
    }
}

GPBeat::Whammy GP67DomBuilder::read_whammy(const XmlDomElement &elem) const {
    GPBeat::Whammy whammy;
    whammy.origin_value = elem.attribute("originValue").value().to_float();
    whammy.destination_value = elem.attribute("destinationValue").value().to_float();
    whammy.middle_value = elem.attribute("middleValue").value().to_float();
    whammy.origin_offset = elem.attribute("originOffset").value().to_float();
    whammy.middle_offset1 = elem.attribute("middleOffset1").value().to_float();
    whammy.middle_offset2 = elem.attribute("middleOffset2").value().to_float();
    whammy.destination_offset = elem.attribute("destinationOffset").value().to_float();
    return whammy;
}

std::unique_ptr<GPNote::Bend> GP67DomBuilder::create_bend(XmlDomNode *property_node) {
    std::unique_ptr<GPNote::Bend> bend = std::make_unique<GPNote::Bend>();

    auto current_node = property_node->next_sibling();
    while (!current_node.is_null()) {
        String property_name = current_node.to_element().attribute("name").value();
        if (property_name == u"BendDestinationOffset") {
            bend->destination_offset = current_node.first_child().to_element().text().to_float();
        } else if (property_name == u"BendDestinationValue") {
            bend->destination_value = current_node.first_child().to_element().text().to_float();
        } else if (property_name == u"BendMiddleOffset1") {
            bend->middle_offset1 = current_node.first_child().to_element().text().to_float();
        } else if (property_name == u"BendMiddleOffset2") {
            bend->middle_offset2 = current_node.first_child().to_element().text().to_float();
        } else if (property_name == u"BendMiddleValue") {
            bend->middle_value = current_node.first_child().to_element().text().to_float();
        } else if (property_name == u"BendOriginOffset") {
            bend->origin_offset = current_node.first_child().to_element().text().to_float();
        } else if (property_name == u"BendOriginValue") {
            bend->origin_value = current_node.first_child().to_element().text().to_float();
        }

        current_node = current_node.next_sibling();
    }

    return bend;
}

void GP67DomBuilder::read_harmonic(XmlDomNode *property_node, GPNote *note) const {
    auto harmonic_type = [](const String &str) {
        if (str == u"Artificial") {
            return GPNote::Harmonic::Type::ARTIFICIAL;
        } else if (str == u"Semi") {
            return GPNote::Harmonic::Type::SEMI;
        } else if (str == u"Pinch") {
            return GPNote::Harmonic::Type::PINCH;
        } else if (str == u"Feedback") {
            return GPNote::Harmonic::Type::FEED_BACK;
        } else if (str == u"Natural") {
            return GPNote::Harmonic::Type::NATURAL;
        } else {
            return GPNote::Harmonic::Type::TAP;
        }
    };

    String property_name = property_node->to_element().attribute("name").value();
    if (property_name == u"HarmonicFret") {
        note->set_harmonic_fret(property_node->first_child().to_element().text().to_float());
    } else if (property_name == u"HarmonicType") {
        note->set_harmonic_type(harmonic_type(property_node->first_child().to_element().text()));
    }
}

void GP67DomBuilder::read_beat_properties(const XmlDomNode &properties_node, GPBeat *beat) const {
    auto brush_type = [](const String &brush) {
        if (brush == u"Down") {
            return GPBeat::Brush::DOWN;
        }
        return GPBeat::Brush::UP;
    };
    auto vibrato_type = [](const String &str) {
        if (str == u"Wide") {
            return GPBeat::VibratoWTremBar::WIDE;
        } else if (str == u"Slight") {
            return GPBeat::VibratoWTremBar::SLIGHT;
        } else {
            return GPBeat::VibratoWTremBar::NONE;
        }
    };
    auto rasgueado_type = [](const String &str) {
        if (str == u"ii_1") {
            return GPBeat::Rasgueado::II_1;
        } else if (str == u"mi_1") {
            return GPBeat::Rasgueado::MII_1;
        } else if (str == u"mii_1") {
            return GPBeat::Rasgueado::MII_1;
        } else if (str == u"mii_2") {
            return GPBeat::Rasgueado::MII_2;
        } else if (str == u"ami_1") {
            return GPBeat::Rasgueado::AMI_1;
        } else if (str == u"ami_2") {
            return GPBeat::Rasgueado::AMI_2;
        } else if (str == u"pai_1") {
            return GPBeat::Rasgueado::PAI_1;
        } else if (str == u"pai_2") {
            return GPBeat::Rasgueado::PAI_2;
        } else if (str == u"pei_1") {
            return GPBeat::Rasgueado::PEI_1;
        } else if (str == u"pei_2") {
            return GPBeat::Rasgueado::PEI_2;
        } else if (str == u"pmp_1") {
            return GPBeat::Rasgueado::PMP_1;
        } else if (str == u"pmp_2") {
            return GPBeat::Rasgueado::PMP_2;
        } else if (str == u"ppp_1") {
            return GPBeat::Rasgueado::PPP_1;
        } else if (str == u"amii_1") {
            return GPBeat::Rasgueado::AMII_1;
        } else if (str == u"amip_1") {
            return GPBeat::Rasgueado::AMIP_1;
        } else if (str == u"eami_1") {
            return GPBeat::Rasgueado::EAMI_1;
        } else if (str == u"eamii_1") {
            return GPBeat::Rasgueado::EAMII_1;
        } else if (str == u"peami_1") {
            return GPBeat::Rasgueado::PEAMI_1;
        } else {
            return GPBeat::Rasgueado::NONE;
        }
    };
    auto pick_stroke_type = [](const String &str) {
        if (str == u"Up") {
            return GPBeat::PickStroke::UP;
        } else if (str == u"Down") {
            return GPBeat::PickStroke::DOWN;
        }
        return GPBeat::PickStroke::NONE;
    };

    GPBeat::Whammy whammy_data;
    bool has_whammy_marker = false;
    auto property_node = properties_node.first_child();

    while (!property_node.is_null()) {
        String property_name = property_node.to_element().attribute("name").value();

        if (property_name == u"Popped") {
            if (property_node.first_child().node_name() == "Enable") {
                beat->set_popped(true);
            }
        } else if (property_name == u"Slapped") {
            if (property_node.first_child().node_name() == "Enable") {
                beat->set_slapped(true);
            }
        } else if (property_name == u"Brush") {
            beat->set_brush(brush_type(property_node.first_child().to_element().text()));
        } else if (property_name == u"VibratoWTremBar") {
            beat->set_vibrato_w_trem_bar(vibrato_type(property_node.first_child().to_element().text()));
        } else if (property_name == u"Rasgueado") {
            beat->set_rasgueado(rasgueado_type(property_node.first_child().to_element().text()));
        } else if (property_name == u"PickStroke") {
            beat->set_pick_stroke(pick_stroke_type(property_node.first_child().to_element().text()));
        } else if (property_name == u"BarreFret") {
            beat->set_barre_fret(property_node.first_child().to_element().text().to_int());
        } else if (property_name == u"BarreString") {
            beat->set_barre_string(property_node.first_child().to_element().text().to_int());
        } else if (property_name == u"WhammyBar") {
            has_whammy_marker = true;
        } else if (property_name == u"WhammyBarOriginOffset") {
            whammy_data.origin_offset = property_node.first_child().to_element().text().to_float();
        } else if (property_name == u"WhammyBarOriginValue") {
            whammy_data.origin_value = property_node.first_child().to_element().text().to_float();
        } else if (property_name == u"WhammyBarDestinationValue") {
            whammy_data.destination_value = property_node.first_child().to_element().text().to_float();
        } else if (property_name == u"WhammyBarMiddleValue") {
            whammy_data.middle_value = property_node.first_child().to_element().text().to_float();
        } else if (property_name == u"WhammyBarDestinationOffset") {
            whammy_data.destination_offset = property_node.first_child().to_element().text().to_float();
        } else if (property_name == u"WhammyBarMiddleOffset1") {
            whammy_data.middle_offset1 = property_node.first_child().to_element().text().to_float();
        } else if (property_name == u"WhammyBarMiddleOffset2") {
            whammy_data.middle_offset2 = property_node.first_child().to_element().text().to_float();
        }

        property_node = property_node.next_sibling();
    }

    if (has_whammy_marker && !whammy_data.is_empty() && !beat->has_whammy()) {
        beat->set_whammy(whammy_data);
    }
}

void GP67DomBuilder::read_track_properties(XmlDomNode *properties_node, GPTrack *track, bool ignore_tuning_flats) const {
    GPTrack::StaffProperties properties;
    properties.ignore_flats = ignore_tuning_flats;

    auto property_node = properties_node->first_child();

    while (!property_node.is_null()) {
        String property_name = property_node.to_element().attribute("name").value();

        if (property_name == u"CapoFret") {
            properties.capo_fret = property_node.first_child().to_element().text().to_int();
        } else if (property_name == u"FretCount") {
            properties.fret_count = property_node.first_child().to_element().text().to_int();
        } else if (property_name == "Tuning") {
            String tuning_str = property_node.first_child_element("Pitches").text();
            std::vector<int> tunning;
            tunning.reserve(6);
            for (const String &val : tuning_str.split(u' ')) {
                tunning.push_back(val.to_int());
            }
            properties.tuning.swap(tunning);
            properties.use_flats = !property_node.first_child_element("Flat").is_null();
        } else if (property_name == u"TuningFlat") {
            properties.use_flats = !property_node.first_child_element("Enable").is_null();
        } else if (property_name == u"DiagramCollection" || property_name == u"DiagramWorkingSet") {
            read_diagram(property_node.first_child(), track);
        }

        property_node = property_node.next_sibling();
    }

    track->set_staff_properties(properties);
}

void GP67DomBuilder::read_diagram(const XmlDomNode &items, GPTrack *track) const {
    auto item = items.first_child();

    while (!item.is_null()) {
        GPTrack::Diagram diagram;

        diagram.id = item.to_element().attribute("id").value().to_int();
        diagram.name = item.to_element().attribute("name").value();

        auto diagram_node = item.first_child();
        diagram.string_count = diagram_node.to_element().attribute("stringCount").value().to_int();
        diagram.fret_count = diagram_node.to_element().attribute("fretCount").value().to_int();
        diagram.base_fret = diagram_node.to_element().attribute("baseFret").value().to_int();

        auto fret_node = diagram_node.first_child();
        while (!fret_node.is_null()) {
            if (fret_node.node_name() == "Fret") {
                int string = fret_node.to_element().attribute("string").value().to_int();
                int fret = fret_node.to_element().attribute("fret").value().to_int();
                diagram.frets[string] = fret;
            }

            fret_node = fret_node.next_sibling();
        }

        track->add_diagram(std::make_pair(diagram.id, diagram));

        item = item.next_sibling();
    }
}

void GP67DomBuilder::read_lyrics(const XmlDomNode &items, GPTrack *track) const {
    XmlDomNode line_node = items.first_child_element("Line");

    // This code doesn't support multiple lines of lyrics.
    if (!line_node.is_null()) {
        auto text_node = line_node.first_child_element("Text");
        if (!text_node.is_null()) {
            track->set_lyrics(text_node.to_element().text().to_std_string());
        }

        auto offset_node = line_node.first_child_element("Offset");
        if (!offset_node.is_null()) {
            track->set_lyrics_offset(offset_node.to_element().text().to_int());
        }
    }
}

std::vector<int> GP67DomBuilder::read_ending(XmlDomNode *end_node) const {
    String str = end_node->to_element().text();
    StringList str_list = str.split(u' ');
    std::vector<int> ending;
    ending.reserve(str_list.size());
    for (const String &val : str_list) {
        ending.push_back(val.to_int());
    }

    return ending;
}

GPMasterBar::Repeat GP67DomBuilder::read_repeat(XmlDomNode *repeat_node) const {
    auto repeat_type = [](const String &start, const String &end) {
        if (start == u"true" && end == u"false") {
            return GPMasterBar::Repeat::Type::START;
        } else if (start == u"false" && end == u"true") {
            return GPMasterBar::Repeat::Type::END;
        } else if (start == u"true" && end == u"true") {
            return GPMasterBar::Repeat::Type::START_END;
        }
        return GPMasterBar::Repeat::Type::NONE;
    };

    String start = repeat_node->to_element().attribute("start").value();
    String end = repeat_node->to_element().attribute("end").value();
    int count = repeat_node->to_element().attribute("count").value().to_int();

    GPMasterBar::Repeat repeat{repeat_type(start, end), count};
    return repeat;
}

std::pair<String, String> GP67DomBuilder::read_master_bar_section(const XmlDomNode &section_node) const {
    std::pair<String, String> section;

    auto node = section_node.first_child();

    while (!node.is_null()) {
        String node_name = node.node_name();

        if (node_name == u"Letter") {
            section.first = node.to_element().text();
        } else if (node_name == u"Text") {
            section.second = node.to_element().text();
        }

        node = node.next_sibling();
    }

    return section;
}

std::vector<GPMasterBar::Fermata> GP67DomBuilder::read_fermatas(XmlDomNode *fermatas_node) const {
    auto fermata_type = [](const String &str) {
        if (str == u"Short") {
            return GPMasterBar::Fermata::Type::SHORT;
        } else if (str == u"Medium") {
            return GPMasterBar::Fermata::Type::MEDIUM;
        }
        return GPMasterBar::Fermata::Type::LONG;
    };

    std::vector<GPMasterBar::Fermata> fermatas;

    auto fermata_node = fermatas_node->first_child();

    while (!fermata_node.is_null()) {
        GPMasterBar::Fermata fermata;

        auto fermata_property = fermata_node.first_child();
        while (!fermata_property.is_null()) {
            String node_name = fermata_property.node_name();
            if (node_name == u"Type") {
                fermata.type = fermata_type(fermata_property.to_element().text());
            } else if (node_name == u"Offset") {
                String str = fermata_property.to_element().text();
                StringList numbers = str.split(u'/');
                fermata.offset_num = numbers.at(0).to_int();
                fermata.offset_denom = numbers.at(1).to_int();
            } else if (node_name == u"Length") {
                fermata.length = fermata_property.to_element().text().to_float();
            }

            fermata_property = fermata_property.next_sibling();
        }

        fermatas.push_back(fermata);
        fermata_node = fermata_node.next_sibling();
    }

    return fermatas;
}

std::vector<GPMasterBar::Direction> GP67DomBuilder::read_repeats_jumps(XmlDomNode *repeats_jumps_node) const {
    std::vector<GPMasterBar::Direction> repeats_jumps;

    auto inner_node = repeats_jumps_node->first_child();

    while (!inner_node.is_null()) {
        GPMasterBar::Direction repeat_jump;
        repeat_jump.type = (inner_node.node_name() == "Jump" ? GPMasterBar::Direction::Type::JUMP : GPMasterBar::Direction::Type::REPEAT);
        repeat_jump.name = inner_node.to_element().text();

        // GP encodes "To Coda" instructions as Jumps, but MuseScore uses Markers for that
        if ((repeat_jump.name == u"DaCoda") || (repeat_jump.name == u"DaDoubleCoda")) {
            repeat_jump.type = GPMasterBar::Direction::Type::MARKER;
        }

        repeats_jumps.push_back(repeat_jump);
        inner_node = inner_node.next_sibling();
    }

    return repeats_jumps;
}
} // namespace mu::iex::guitarpro
