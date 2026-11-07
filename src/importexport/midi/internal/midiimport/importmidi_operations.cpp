/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "importmidi_operations.h"

#include "io/file.h"
#include "log.h"
#include "serialization/xmlstreamreader.h"

namespace mu::iex::midi {
MidiOperations::Data midi_import_operations;

namespace MidiOperations {
static int read_bool_from_xml(muse::XmlStreamReader &xml) {
    int value = -1;
    const muse::AsciiStringView name = xml.name();
    xml.read_next();

    if (xml.token_type() == muse::XmlStreamReader::CHARACTERS) {
        if (xml.text() == u"true") {
            value = 1;
        } else if (xml.text() == u"false") {
            value = 0;
        } else {
            LOGD() << "Load MIDI import operations from file: unknown" << name << "value";
        }
    }
    return value;
}

// Terms: <name attribute="value">text</name>

static void set_operations_from_file(const std::string &file_name, Opers &opers) {
    if (file_name.empty()) {
        return;
    }

    muse::io::File file{muse::io::PathT(file_name)};
    if (!file.open(muse::io::IODevice::READ_ONLY)) {
        LOGD("Load MIDI import operations from file: cannot open input file");
        return;
    }

    muse::XmlStreamReader xml(&file);

    while (!xml.at_end() && !xml.is_error()) {
        const auto token = xml.read_next();

        if (token == muse::XmlStreamReader::START_DOCUMENT) {
            continue;
        }
        if (token != muse::XmlStreamReader::START_ELEMENT) { // like <elem>
            continue;
        }
        if (xml.name() == "MidiOptions") {
            continue;
        }

        if (xml.name() == "QuantValue") {
            xml.read_next();
            if (xml.token_type() == muse::XmlStreamReader::CHARACTERS) {
                bool ok = false;
                const int index = xml.text().trimmed().to_int(&ok);
                if (ok) {
                    switch (index) {
                        case 0: opers.quant_value.set_default_value(QuantValue::Q_4, false); break;
                        case 1: opers.quant_value.set_default_value(QuantValue::Q_8, false); break;
                        case 2: opers.quant_value.set_default_value(QuantValue::Q_16, false); break;
                        case 3: opers.quant_value.set_default_value(QuantValue::Q_32, false); break;
                        case 4: opers.quant_value.set_default_value(QuantValue::Q_64, false); break;
                        case 5: opers.quant_value.set_default_value(QuantValue::Q_128, false); break;
                        case 6: opers.quant_value.set_default_value(QuantValue::Q_256, false); break;
                        case 7: opers.quant_value.set_default_value(QuantValue::Q_512, false); break;
                        case 8: opers.quant_value.set_default_value(QuantValue::Q_1024, false); break;
                        default:
                            LOGD("Load MIDI import operations from file: "
                                 "unknown max quantization value");
                            break;
                    }
                }
            }
        } else if (xml.name() == "VoiceCount") {
            xml.read_next();
            if (xml.token_type() == muse::XmlStreamReader::CHARACTERS) {
                bool ok = false;
                const int index = xml.text().trimmed().to_int(&ok);
                if (ok) {
                    switch (index) {
                        case 0: opers.max_voice_count.set_default_value(VoiceCount::V_1, false); break;
                        case 1: opers.max_voice_count.set_default_value(VoiceCount::V_2, false); break;
                        case 2: opers.max_voice_count.set_default_value(VoiceCount::V_3, false); break;
                        case 3: opers.max_voice_count.set_default_value(VoiceCount::V_4, false); break;
                        default:
                            LOGD("Load MIDI import operations from file: "
                                 "unknown max voice count");
                            break;
                    }
                }
            }
        } else if (xml.name() == "Duplets") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search2plets.set_default_value(value, false);
            }
        } else if (xml.name() == "Triplets") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search3plets.set_default_value(value, false);
            }
        } else if (xml.name() == "Quadruplets") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search4plets.set_default_value(value, false);
            }
        } else if (xml.name() == "Quintuplets") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search5plets.set_default_value(value, false);
            }
        } else if (xml.name() == "Septuplets") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search7plets.set_default_value(value, false);
            }
        } else if (xml.name() == "Nonuplets") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search9plets.set_default_value(value, false);
            }
        } else if (xml.name() == "HumanPerformance") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.is_human_performance.set_default_value(value, false);
            }
        } else if (xml.name() == "MeasureCount2xLess") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.measure_count2x_less.set_default_value(value, false);
            }
        } else if (xml.name() == "SplitStaff") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.do_staff_split.set_default_value(value, false);
            }
        } else if (xml.name() == "ClefChanges") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.change_clef.set_default_value(value, false);
            }
        } else if (xml.name() == "SimplifyDurations") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.simplify_durations.set_default_value(value, false);
            }
        } else if (xml.name() == "ShowStaccato") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.show_staccato.set_default_value(value, false);
            }
        } else if (xml.name() == "DottedNotes") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.use_dots.set_default_value(value, false);
            }
        } else if (xml.name() == "RecognizePickupBar") {
            const int value = read_bool_from_xml(xml);
            if (value >= 0) {
                opers.search_pickup_measure.set_default_value(value, false);
            }
        } else if (xml.name() == "Swing") {
            xml.read_next();
            if (xml.token_type() == muse::XmlStreamReader::CHARACTERS) {
                bool ok = false;
                const int index = xml.text().trimmed().to_int(&ok);
                if (ok) {
                    switch (index) {
                        case 0: opers.swing.set_default_value(Swing::NONE, false); break;
                        case 1: opers.swing.set_default_value(Swing::SHUFFLE, false); break;
                        case 2: opers.swing.set_default_value(Swing::SWING, false); break;
                        default:
                            LOGD("Load MIDI import operations from file: "
                                 "unknown swing");
                            break;
                    }
                }
            }
        }
    }
    if (xml.is_error()) {
        LOGD("Load MIDI import operations from file: cannot parse input file");
    }
}

//-------------------------------------------------------------------------------------------

FileData *Data::data() {
    const auto it = data_value.find(current_midi_file);
    if (it != data_value.end()) {
        return &it->second;
    }
    return nullptr;
}

const FileData *Data::data() const {
    const auto it = data_value.find(current_midi_file);
    if (it != data_value.end()) {
        return &it->second;
    }
    return nullptr;
}

void Data::add_new_midi_file(const std::string &file_name) {
    if (data_value.find(file_name) != data_value.end()) {
        return;
    }
    FileData file_data;
    set_operations_from_file(midi_operations_file, file_data.track_opers);
    data_value.insert({file_name, file_data});
}

const MidiFile *Data::midi_file(const std::string &file_name) {
    const auto it = data_value.find(file_name);
    if (it != data_value.end()) {
        return &it->second.midi_file;
    }
    return nullptr;
}

std::vector<std::string> Data::all_midi_files() const {
    std::vector<std::string> list;
    for (const auto &d : data_value) {
        list.push_back(d.first);
    }
    return list;
}

void Data::set_midi_file_data(const std::string &file_name, const MidiFile &midi_file) { data_value[file_name].midi_file = midi_file; }

void Data::exclude_midi_file(const std::string &file_name) { data_value.erase(file_name); }

bool Data::has_midi_file(const std::string &file_name) { return data_value.find(file_name) != data_value.end(); }

int Data::current_track() const {
    assert(current_track_value >= 0);

    return current_track_value;
}

void Data::set_operations_file(const std::string &file_name) {
    if (muse::io::File::exists(muse::io::PathT(file_name))) {
        midi_operations_file = file_name;
    }
}
} // namespace MidiOperations
} // namespace mu::iex::midi
