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

#include "importgtp.h"

#include <algorithm>
#include <stdexcept>

#include "serialization/xmldom.h"

#include "gtp/gp6dombuilder.h"
#include "gtp/gpconverter.h"

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/box.h"
#include "engraving/dom/bracket.h"
#include "engraving/dom/bracketitem.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/note.h"
#include "engraving/dom/notedot.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/stringdata.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolobar.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/dom/stringtunings.h"

#include "engraving/types/symid.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::iex::guitarpro {
namespace {
constexpr int gpx_max_depth = 4;
constexpr int gpx_max_file_bytes = 64 * 1024 * 1024;
constexpr int gpx_max_decompressed_bytes = 128 * 1024 * 1024;

// The compressed bits ended in the middle of a token. The token stream of the GPX
// fixtures can stop a few bytes short of the length it declares (the tail of the
// last sector is padding the writer never compressed), so the decompressor ends the
// stream there instead of rejecting the file.
struct GpxEndOfData : std::runtime_error {
    GpxEndOfData() : std::runtime_error("Unexpected end of compressed Guitar Pro data") {}
};
} // namespace

const std::map<std::string, std::string> GuitarPro6::instrument_mapping = {
    {"2Mrcs", "maracas"},
    {"a-bass4", "acoustic-bass"},
    {"a-bass5", "acoustic-bass"},
    {"a-bass6", "acoustic-bass"},
    {"alt-c", "alto"},
    {"alt-s", "alto"},
    {"a-piano-gs", "piano"},
    {"a-piano-ss", "piano"},
    {"bass-c", "bass"},
    {"bass-flt-c", "bass-flute"},
    {"bassn", "bassoon"},
    {"bass-s", "bass"},
    {"basstuba-eb", "bass-eb-tuba"},
    {"bnj4", "banjo"},
    {"bnj5", "banjo"},
    {"bnj6", "banjo"},
    {"bongo", "bongos"},
    {"brthns", "baritone-horn"},
    {"brtn-c", "baritone"},
    {"brtn-s", "baritone"},
    {"cbs", "cabasa"},
    {"cello", "violoncello"},
    {"china", "chinese-tom-toms"},
    {"clrnt-a", "a-clarinet"},
    {"clrnt-bb-bass", "bass-clarinet"},
    {"clrnt-bb", "bb-clarinet"},
    {"clrnt-c", "c-clarinet"},
    {"clrnt-d", "d-clarinet"},
    {"clrnt-eb", "eb-clarinet"},
    {"clrnt", "clarinet"},
    {"clst-gs", "celesta"},
    {"clst-ss", "celesta"},
    {"clvs", "claves"},
    {"cngKit", "congas"},
    {"conga", "congas"},
    {"cowbell", "cowbell"},
    {"crash", "crash-cymbal"},
    {"cstnt", "castanets"},
    {"ctbassn", "contrabassoon"},
    {"ctbass", "contrabass"},
    {"cuicaKit", "cuica"},
    {"cuica", "cuica"},
    {"drmkt", "drumset"},
    {"e-bass4", "bass-guitar"},
    {"e-bass5", "bass-guitar"},
    {"e-bass6", "bass-guitar"},
    {"e-gtr12", "electric-guitar-treble-clef"},
    {"e-gtr6", "electric-guitar-treble-clef"},
    {"e-gtr7", "electric-guitar-treble-clef"},
    {"e-gtr8", "electric-guitar-treble-clef"},
    {"em-organ-gs", "organ"},
    {"em-organ-ss", "organ"},
    {"en-horn", "english-horn"},
    {"e-piano-gs", "electric-piano"},
    {"e-piano-ss", "electric-piano"},
    {"flt-c", "flute"},
    {"flt-g", "alto-flute"},
    {"flt-whstl", "tin-whistle"},
    {"fr-horn", "horn"},
    {"grcss", "bass-drum"}, //grancassa is an alternative name for bass drum
    {"guiro", "guiro"},
    {"harp-gs", "harp"},
    {"harp-ss", "harp"},
    {"hclap", "hand-clap"},
    {"hihat", "hi-hat"},
    {"hrpch-gs", "harpsichord"},
    {"hrpch-ss", "harpsichord"},
    {"jngl-bell", "sleigh-bells"},
    {"klmb", "kalimba"},
    {"mrcs", "maracas"},
    {"n-gtr6", "guitar-nylon-treble-clef"},
    {"n-gtr7", "guitar-nylon-treble-clef"},
    {"n-gtr8", "guitar-nylon-treble-clef"},
    {"oboe", "oboe"},
    {"ocrn", "ocarina"},
    {"pccl", "piccolo"},
    {"pedalhihat", "hi-hat"},
    {"pnflt", "pan-flute"},
    {"ptt", "cymbal"}, //piatti is cymbal in italian
    {"rec", "recorder"},
    {"ride", "ride-cymbal"},
    {"rvs-cymb", "cymbal"},
    {"sax-alt-eb", "alto-saxophone"},
    {"sax-bar-eb", "baritone-saxophone"},
    {"sax-bass-eb", "bass-saxophone"},
    {"sax-ms-f", "mezzo-soprano-saxophone"},
    {"sax-sop-bb", "soprano-saxophone"},
    {"sax-ten-bb", "tenor-saxophone"},
    {"sax-ten-c", "melody-saxophone"},
    {"s-bass4", "electric-bass"},
    {"s-bass5", "5-string-electric-bass"},
    {"s-gtr12", "guitar-steel-treble-clef"},
    {"s-gtr6", "guitar-steel-treble-clef"},
    {"s-gtr7", "guitar-steel-treble-clef"},
    {"s-gtr8", "guitar-steel-treble-clef"},
    {"shkr", "percussion"},
    {"shmsn", "shamisen"},
    {"shn", "sheng"},
    {"snare", "snare-drum"},
    {"snr", "snare-drum"},
    {"snt-brass-gs", "brass-synthesizer"},
    {"snt-brass-ss", "brass-synthesizer"},
    //{"snt-key-gs",    ""},
    //{"snt-key-ss",    ""},
    //{"snt-seq-gs",    ""},
    //{"snt-seq-ss",    ""},
    {"snt-lead-gs", "poly-synth"},
    {"snt-lead-ss", "poly-synth"},
    {"snt-pad", "pad-synth"},
    {"snt-pad-gs", "pad-synth"},
    {"snt-pad-ss", "pad-synth"},
    {"splash", "splash-cymbal"},
    {"sprn-c", "soprano"},
    {"sprn-s", "soprano"},
    {"tmbrn", "tambourine"}, // to be mapped
    {"Tambourine-Perc", "tambourine"},
    {"Tambourine", "tambourine"},
    {"tmblKit", "timbales"},
    {"tmbl", "timbales"},
    {"tmpn", "timpani"},
    {"tnklbll", "tubular-bells"}, //The short form does not match but this is very likely due to the description
    {"tnr-c", "tenor"},
    {"tnr-s", "tenor"},
    {"Triangle-Percu", "triangle"},
    {"trmbn-bb-bass", "bass-trombone"},
    {"trmbn-bb", "tenor-trombone"},
    {"trmbn-bb-treble", "trombone-treble"},
    {"trmbn-eb", "alto-trombone"},
    {"trmpt-a", "a-trumpet"},
    {"trmpt-bb", "bb-trumpet"},
    {"trmpt-c-bass", "c-bass-trumpet"},
    {"trmpt-c", "c-trumpet"},
    {"trmpt-d", "d-trumpet"},
    {"trmpt-eb-bass", "eb-bass-trumpet"},
    {"trmpt-eb", "eb-trumpet"},
    {"trmpt-e", "e-trumpet"},
    {"trmpt-f", "f-trumpet"},
    {"trmpt-flgh", "flugelhorn"},
    {"trngl", "triangle"},
    {"ukll4", "ukulele"},
    {"vbrphn", "vibraphone"},
    {"vbrslp", "vibraslap"}, // to be mapped
    {"vla", "viola"},
    {"vln", "violin"},
    {"wdblckKit", "wood-blocks"},
    {"wdblck", "wood-blocks"},
    {"whstlKit", "slide-whistle"},
    {"whstl", "tin-whistle"},
    {"xlphn", "xylophone"}};

std::unique_ptr<IGPDomBuilder> GuitarPro6::create_gp_dom_builder() const { return std::make_unique<GP6DomBuilder>(); }

//---------------------------------------------------------
//   readBit
//---------------------------------------------------------

int GuitarPro6::read_bit(ByteArray *buffer) {
    // calculate the byte index by dividing the position in bits by the bits per byte
    size_t byte_index = static_cast<size_t>(position) / bits_in_byte;
    if (byte_index >= buffer->size()) {
        throw GpxEndOfData();
    }

    // calculate our offset so we know how much to bit shift
    int byte_offset = ((bits_in_byte - 1) - (position % bits_in_byte));

    // calculate the bit which we want to read
    char byte = (*buffer)[byte_index];
    int bit = (((byte & 0xff) >> byte_offset) & 0x01);

    // increment our current position so we know this bit has been read
    position++;
    return bit; // return the bit we calculated
}

//---------------------------------------------------------
//   readBits
//---------------------------------------------------------

int GuitarPro6::read_bits(ByteArray *buffer, int bits_to_read) {
    if (bits_to_read < 0 || bits_to_read > 32) {
        throw std::runtime_error("Invalid bit count in Guitar Pro data");
    }
    uint32_t bits = 0;
    for (int i = (bits_to_read - 1); i >= 0; i--) {
        bits |= (static_cast<uint32_t>(read_bit(buffer)) << i);
    }
    return static_cast<int>(bits);
}

//---------------------------------------------------------
//   readBitsReversed
//---------------------------------------------------------

int GuitarPro6::read_bits_reversed(ByteArray *buffer, int bits_to_read) {
    if (bits_to_read < 0 || bits_to_read > 32) {
        throw std::runtime_error("Invalid bit count in Guitar Pro data");
    }
    uint32_t bits = 0;
    for (int i = 0; i < bits_to_read; i++) {
        bits |= static_cast<uint32_t>(read_bit(buffer)) << i;
    }
    return static_cast<int>(bits);
}

//---------------------------------------------------------
//   getBytes
//---------------------------------------------------------

ByteArray GuitarPro6::get_bytes(ByteArray *buffer, int offset, int length) {
    if (offset < 0 || length < 0) {
        throw std::runtime_error("Invalid Guitar Pro file range");
    }
    const size_t start = static_cast<size_t>(offset);
    if (length == 0) {
        return ByteArray();
    }
    if (start >= buffer->size()) {
        throw std::runtime_error("Guitar Pro file block is outside the archive");
    }

    ByteArray new_bytes;
    // compute new bytes from our buffer and return byte array
    for (int i = 0; i < length && start + static_cast<size_t>(i) < buffer->size(); i++) {
        new_bytes.push_back((*buffer)[start + static_cast<size_t>(i)]);
    }
    return new_bytes;
}

//---------------------------------------------------------
//   readInteger
//---------------------------------------------------------

int GuitarPro6::read_integer(ByteArray *buffer, int offset) {
    if (offset < 0 || static_cast<size_t>(offset) > buffer->size() || buffer->size() - static_cast<size_t>(offset) < 4) {
        throw std::runtime_error("Unexpected end of compressed Guitar Pro data");
    }
    // assign four bytes and take them from the buffer
    char bytes[4];
    bytes[0] = (*buffer)[static_cast<size_t>(offset) + 0];
    bytes[1] = (*buffer)[static_cast<size_t>(offset) + 1];
    bytes[2] = (*buffer)[static_cast<size_t>(offset) + 2];
    bytes[3] = (*buffer)[static_cast<size_t>(offset) + 3];
    // increment positioning so we keep track of where we are
    position += sizeof(int) * bits_in_byte;
    // bit shift in order to compute our integer value and return
    const uint32_t value = (static_cast<uint32_t>(bytes[3]) & 0xff) << 24 | (static_cast<uint32_t>(bytes[2]) & 0xff) << 16 |
                           (static_cast<uint32_t>(bytes[1]) & 0xff) << 8 | (static_cast<uint32_t>(bytes[0]) & 0xff);
    return static_cast<int>(value);
}

//---------------------------------------------------------
//   readString
//---------------------------------------------------------

ByteArray GuitarPro6::read_string(ByteArray *buffer, int offset, int length) {
    if (offset < 0 || length < 0 || static_cast<size_t>(offset) > buffer->size()) {
        throw std::runtime_error("Unexpected end of compressed Guitar Pro data");
    }

    ByteArray filename;
    const size_t start = static_cast<size_t>(offset);
    const size_t available = buffer->size() - start;
    const size_t limit = std::min(static_cast<size_t>(length), available);
    // compute the string by iterating through the buffer
    for (size_t i = 0; i < limit; i++) {
        int char_value = ((*buffer)[start + i] & 0xff);
        if (char_value == 0) {
            break;
        }
        filename.push_back((char) char_value);
    }
    return filename;
}

//---------------------------------------------------------
//   unhandledNode
//---------------------------------------------------------

void GuitarPro6::unhandled_node(String node_name) { LOGD() << "WARNING: Discovered unhandled node name" << node_name; }

//---------------------------------------------------------
//   getNode
//---------------------------------------------------------

XmlDomNode GuitarPro6::get_node(const String &id, XmlDomNode current_dom_node) {
    while (!(current_dom_node).is_null()) {
        String current_id = current_dom_node.to_element().attribute("id").value();
        if (current_id == id) {
            return current_dom_node;
        }
        current_dom_node = current_dom_node.next_sibling();
    }
    LOGD() << "WARNING: A null node was returned when search for the identifier" << id << ". Your Guitar Pro file may be corrupted.";
    return current_dom_node;
}

//---------------------------------------------------------
//   readGpif
//---------------------------------------------------------

void GuitarPro6::read_gpif(ByteArray *data) {
    if (!data || data->empty()) {
        throw std::runtime_error("Guitar Pro file has no score.gpif data");
    }

    XmlDomDocument dom_doc;
    dom_doc.set_content(*data);
    if (dom_doc.has_error()) {
        throw std::runtime_error("Malformed Guitar Pro score.gpif");
    }
    XmlDomElement dom_elem = dom_doc.root_element();
    if (dom_elem.is_null()) {
        throw std::runtime_error("Malformed Guitar Pro score.gpif");
    }

    auto builder = create_gp_dom_builder();
    if (!builder) {
        throw std::runtime_error("Unsupported Guitar Pro score.gpif");
    }
    builder->build_gp_dom_model(&dom_elem);

    GPConverter score_builder(score, builder->get_gp_dom_model());
    score_builder.convert_gp();
    gpif_read_ = true;
}

//---------------------------------------------------------
//   parseFile
//---------------------------------------------------------

void GuitarPro6::parse_file(const char *filename, ByteArray *data) {
    // test to check if we are dealing with the score
    if (!strcmp(filename, "score.gpif")) {
        read_gpif(data);
    }
}

//---------------------------------------------------------
//   readGPX
//---------------------------------------------------------

void GuitarPro6::read_gpx(ByteArray *buffer) { read_gpx(buffer, 0); }

void GuitarPro6::read_gpx(ByteArray *buffer, int depth) {
    if (!buffer || buffer->size() < sizeof(int)) {
        throw std::runtime_error("Unexpected end of Guitar Pro data");
    }
    if (depth >= ::mu::iex::guitarpro::gpx_max_depth) {
        throw std::runtime_error("Guitar Pro data is nested too deeply");
    }

    // Each decompressed buffer is a complete GPX stream of its own.
    position = 0;
    // start by reading the file header. It will tell us if the byte array is compressed.
    int file_header = read_integer(buffer, 0);

    if (file_header == gpx_header_compressed) {
        // this is  a compressed file.
        if (buffer->size() < 2 * sizeof(int)) {
            throw std::runtime_error("Unexpected end of compressed Guitar Pro data");
        }
        int length = read_integer(buffer, position / bits_in_byte);
        if (length <= 0 || length > ::mu::iex::guitarpro::gpx_max_decompressed_bytes) {
            throw std::runtime_error("Invalid compressed Guitar Pro data length");
        }

        ByteArray bcfs_buffer;
        bcfs_buffer.reserve(std::min<size_t>(static_cast<size_t>(length), 1024 * 1024));
        while (bcfs_buffer.size() < static_cast<size_t>(length)) {
            try {
                // read the bit indicating compression information
                int flag = read_bits(buffer, 1);

                if (flag) {
                    int bits = read_bits(buffer, 4);
                    int offs = read_bits_reversed(buffer, bits);
                    int size = read_bits_reversed(buffer, bits);

                    if (offs < 0 || offs > static_cast<int>(bcfs_buffer.size()) || (size > 0 && offs == 0)) {
                        throw std::runtime_error("Invalid back-reference in compressed Guitar Pro data");
                    }
                    if (bcfs_buffer.size() + static_cast<size_t>(size) > ::mu::iex::guitarpro::gpx_max_decompressed_bytes) {
                        throw std::runtime_error("Decompressed Guitar Pro data is too large");
                    }
                    // A back-reference may overlap what it writes (size > offs): the
                    // encoder's runs repeat the pattern, so the decoder does too. Taking
                    // only min(size, offs) bytes leaves the stream one short of the
                    // length it declares, which the length guard then rejects.
                    const size_t pos = bcfs_buffer.size() - static_cast<size_t>(offs);
                    for (int i = 0; i < size; i++) {
                        bcfs_buffer.push_back(bcfs_buffer[pos + static_cast<size_t>(i) % static_cast<size_t>(offs)]);
                    }
                } else {
                    int size = read_bits_reversed(buffer, 2);
                    if (size < 0 || bcfs_buffer.size() + static_cast<size_t>(size) > ::mu::iex::guitarpro::gpx_max_decompressed_bytes) {
                        throw std::runtime_error("Decompressed Guitar Pro data is too large");
                    }
                    for (int i = 0; i < size; i++) {
                        bcfs_buffer.push_back(static_cast<uint8_t>(read_bits(buffer, 8)));
                    }
                }
            } catch (const GpxEndOfData &) {
                // The stream ended inside the last token: keep what was decoded. The
                // container's own table sizes its files, so a truncated tail of the
                // last sector is dropped by the reader below, not carried as music.
                break;
            }
        }
        // recurse on the decompressed file stored as a byte array
        read_gpx(&bcfs_buffer, depth + 1);
    } else if (file_header == gpx_header_uncompressed) {
        // this is an uncompressed file - strip the header off
        *buffer = buffer->right(buffer->size() - sizeof(int));
        const size_t sector_size = 0x1000;
        int offset = 0;
        while ((offset = (offset + static_cast<int>(sector_size))) + 3 < static_cast<int>(buffer->size())) {
            int new_int = read_integer(buffer, offset);
            if (new_int == 2) {
                const int index_file_name = (offset + 4);
                const int index_file_size = (offset + 0x8C);
                const int index_of_block = (offset + 0x94);

                // create a byte array and put information about files found in it
                ByteArray file_bytes;
                int block_count = 0;
                while (true) {
                    const int block = read_integer(buffer, index_of_block + (4 * block_count));
                    ++block_count;
                    if (block == 0) {
                        break;
                    }
                    if (block < 0 || static_cast<size_t>(block) > buffer->size() / sector_size) {
                        throw std::runtime_error("Invalid Guitar Pro file block");
                    }
                    file_bytes.push_back(get_bytes(buffer, block * static_cast<int>(sector_size), static_cast<int>(sector_size)));
                    if (file_bytes.size() > static_cast<size_t>(::mu::iex::guitarpro::gpx_max_file_bytes)) {
                        throw std::runtime_error("Guitar Pro file is too large");
                    }
                }
                // get file information and read the file
                int file_size = read_integer(buffer, index_file_size);
                if (file_size < 0 || file_size > ::mu::iex::guitarpro::gpx_max_file_bytes) {
                    throw std::runtime_error("Invalid Guitar Pro file size");
                }
                if (file_bytes.size() >= static_cast<size_t>(file_size)) {
                    ByteArray filename_bytes = read_string(buffer, index_file_name, 127);
                    // readString() does not append a terminator; parseFile() uses strcmp().
                    filename_bytes.push_back(static_cast<uint8_t>(0));
                    const char *filename = filename_bytes.const_char();
                    ByteArray data = get_bytes(&file_bytes, 0, file_size);
                    parse_file(filename, &data);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool GuitarPro6::read(IODevice *io) {
    f = io;
    previous_tempo = -1;
    position = 0;
    gpif_read_ = false;
    // decompress and read files contained within GPX file
    ByteArray ba = io->read_all();
    if (ba.empty()) {
        return false;
    }

    read_gpx(&ba);

    return gpif_read_;
}
} // namespace mu::iex::guitarpro
