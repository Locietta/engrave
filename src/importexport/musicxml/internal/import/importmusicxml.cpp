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

#include "importmusicxml.h"

#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/engravingerrors.h"
#include "engraving/types/types.h"
#include "global/io/file.h"
#include "global/serialization/xmlstreamreader.h"
#include "global/serialization/zipreader.h"
#include "global/translation.h"
#include "importmusicxmllogger.h"
#include "importmusicxmlpass1.h"
#include "importmusicxmlpass2.h"
#include "log.h"
#include "musicxmlvalidation.h"

using namespace muse;
using namespace mu::engraving;

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   musicXmlImportErrorDialog
//---------------------------------------------------------

/**
 Show a dialog displaying the MusicXML import error(s).
 */

//---------------------------------------------------------
//   importMusicXmlfromBuffer
//---------------------------------------------------------

Err import_music_xmlfrom_buffer(Score *score, const String & /*name*/, const ByteArray &data, const MusicXmlConfiguration &configuration) {
    MusicXmlLogger logger;
    logger.set_logging_level(MusicXmlLogger::Level::MXML_ERROR); // errors only
    //logger.setLoggingLevel(MusicXmlLogger::Level::MXML_INFO);
    //logger.setLoggingLevel(MusicXmlLogger::Level::MXML_TRACE); // also include tracing

    // pass 1
    MusicXmlParserPass1 pass1(score, configuration, &logger);
    Err res = pass1.parse(data);
    const String pass1_errors = pass1.errors();

    // pass 2
    MusicXmlParserPass2 pass2(score, pass1, &logger);
    if (res == Err::NO_ERROR) {
        res = pass2.parse(data);
    }

    for (const Part *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            pair.second->update_instrument_id();
        }
    }

    // report result
    const String pass2_errors = pass2.errors();
    if (!(pass1_errors.is_empty() && pass2_errors.is_empty())) {}

    return res;
}

//---------------------------------------------------------
//   check assertions for tuplet handling
//---------------------------------------------------------

/**
 Check assertions for tuplet handling. If this fails, MusicXML
 import will almost certainly break in non-obvious ways.
 */

static_assert(int(DurationType::V_BREVE) == int(DurationType::V_LONG) + 1 && int(DurationType::V_WHOLE) == int(DurationType::V_BREVE) + 1 &&
              int(DurationType::V_HALF) == int(DurationType::V_WHOLE) + 1 &&
              int(DurationType::V_QUARTER) == int(DurationType::V_HALF) + 1 &&
              int(DurationType::V_EIGHTH) == int(DurationType::V_QUARTER) + 1 &&
              int(DurationType::V_16TH) == int(DurationType::V_EIGHTH) + 1 && int(DurationType::V_32ND) == int(DurationType::V_16TH) + 1 &&
              int(DurationType::V_64TH) == int(DurationType::V_32ND) + 1 && int(DurationType::V_128TH) == int(DurationType::V_64TH) + 1 &&
              int(DurationType::V_256TH) == int(DurationType::V_128TH) + 1 &&
              int(DurationType::V_512TH) == int(DurationType::V_256TH) + 1 &&
              int(DurationType::V_1024TH) == int(DurationType::V_512TH) + 1);

//---------------------------------------------------------
//   extractRootfile
//---------------------------------------------------------

/**
Extract rootfile from compressed MusicXML file \a qf, return true if OK and false on error.
*/

static bool extract_rootfile(const String &name, ByteArray &data) {
    ZipReader zip(name);

    ByteArray ba = zip.file_data("META-INF/container.xml");

    XmlStreamReader e(ba);

    String rootfile;
    while (e.read_next_start_element()) {
        if (e.name() != "container") {
            break;
        }
        while (e.read_next_start_element()) {
            if (e.name() != "rootfiles") {
                break;
            }
            while (e.read_next_start_element()) {
                if (e.name() == "rootfile") {
                    rootfile = e.attribute("full-path");
                    break;
                }
            }
        }
    }

    data = zip.file_data(rootfile.to_std_string());
    return true;
}

//---------------------------------------------------------
//   doValidateAndImport
//---------------------------------------------------------

/**
 Validate and import MusicXML data from file \a name contained in ByteArray \a data into score \a score.
 */

static Err do_validate_and_import(Score *score, const String &name, const ByteArray &data, bool force_mode,
                                  const MusicXmlConfiguration &configuration) {
    Err res;

    if (!force_mode) {
        // Validate the file
        res = MusicXmlValidation::validate(name, data);
        if (res != Err::NO_ERROR) {
            return res;
        }
    }

    // actually do the import
    res = import_music_xmlfrom_buffer(score, name, data, configuration);
    //LOGD("res %d", static_cast<int>(res));
    return res;
}

//---------------------------------------------------------
//   importMusicXml
//    return Err::File* errors
//---------------------------------------------------------

Err import_music_xml(MasterScore *score, const String &name, bool force_mode, const MusicXmlConfiguration &configuration) {
    ScoreLoad sl; // suppress warnings for undo push/pop

    //LOGD("importMusicXml(%p, %s)", score, muPrintable(name));

    // open the MusicXML file
    io::File xml_file(name);
    if (!xml_file.exists()) {
        return Err::FILE_NOT_FOUND;
    }

    if (!xml_file.open(io::IODevice::READ_ONLY)) {
        LOGE() << "could not open MusicXML file: " << name;
        return Err::FILE_OPEN_ERROR;
    }

    const ByteArray data = xml_file.read_all();
    xml_file.close();

    // and import it
    return do_validate_and_import(score, name, data, force_mode, configuration);
}

//---------------------------------------------------------
//   importCompressedMusicXml
//    return false on error
//---------------------------------------------------------

/**
 Import compressed MusicXML file \a name into the Score.
 */

Err import_compressed_music_xml(MasterScore *score, const String &name, bool force_mode, const MusicXmlConfiguration &configuration) {
    //LOGD("importCompressedMusicXml(%p, %s)", score, muPrintable(name));

    if (!io::File::exists(name)) {
        return Err::FILE_NOT_FOUND;
    }

    // extract the root file
    ByteArray data;
    if (!extract_rootfile(name, data)) {
        return Err::FILE_BAD_FORMAT; // appropriate error message has been printed by extractRootfile
    }

    // and import it
    return do_validate_and_import(score, name, data, force_mode, configuration);
}
} // namespace mu::iex::musicxml
