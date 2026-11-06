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

#include "importmusicxmlnotepitch.h"

#include "../shared/musicxmlsupport.h"
#include "draw/types/color.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/score.h"
#include "importmusicxmllogger.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   accidental
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/accidental node.
 Return the result as an Accidental.
 */

// TODO: split in reading parameters versus creation

static Accidental *accidental(muse::XmlStreamReader &e, Score *score) {
    const bool cautionary = e.ascii_attribute("cautionary") == "yes";
    const bool editorial = e.ascii_attribute("editorial") == "yes";
    const bool parentheses = e.ascii_attribute("parentheses") == "yes";
    const bool no_parentheses = e.ascii_attribute("parentheses") == "no";
    const bool brackets = e.ascii_attribute("bracket") == "yes";
    const bool no_brackets = e.ascii_attribute("bracket") == "no";
    const bool small_accid = e.ascii_attribute("size") == "cue" || e.ascii_attribute("size") == "grace-cue";
    const Color acc_color = Color(e.ascii_attribute("color").ascii());
    const String smufl = e.attribute("smufl");

    const String s = e.read_text();
    const AccidentalType type = music_xml_string2accidental_type(s, smufl);

    if (type != AccidentalType::NONE) {
        Accidental *a = Factory::create_accidental(score->dummy());
        a->set_accidental_type(type);
        if (cautionary || editorial) { // no way to tell one from the other
            a->set_role(AccidentalRole::USER);
        } // except via the use of parentheses vs. brackets
        if (no_parentheses || no_brackets) {    // explicitly none wanted
        } else if (parentheses || cautionary) { // set to "yes" or "cautionary" and not set at all
            a->set_bracket(AccidentalBracket(AccidentalBracket::PARENTHESIS));
        } else if (brackets || editorial) { // set to "yes" or "editorial" and not set at all
            a->set_bracket(AccidentalBracket(AccidentalBracket::BRACKET));
        }
        if (acc_color.is_valid()) {
            a->set_color(acc_color);
        }
        a->set_small(small_accid);
        return a;
    }

    return nullptr;
}

//---------------------------------------------------------
//   displayStepOctave
//---------------------------------------------------------

/**
 Handle <display-step> and <display-octave> for <rest> and <unpitched>
 */

void MusicXmlNotePitch::display_step_octave(muse::XmlStreamReader &e) {
    while (e.read_next_start_element()) {
        if (e.name() == "display-step") {
            const String step = e.read_text();
            int pos = static_cast<int>(String(u"CDEFGAB").index_of(step));
            if (step.size() == 1 && pos >= 0 && pos < 7) {
                display_step_ = pos;
            } else {
                //logError(String("invalid step '%1'").arg(strStep));
                LOGD("invalid step '%s'", muPrintable(step)); // TODO
            }
        } else if (e.name() == "display-octave") {
            const String oct = e.read_text();
            bool ok;
            display_octave_ = oct.to_int(&ok);
            if (!ok || display_octave_ < 0 || display_octave_ > 9) {
                //logError(String("invalid octave '%1'").arg(strOct));
                LOGD("invalid octave '%s'", muPrintable(oct)); // TODO
                display_octave_ = -1;
            }
        } else {
            e.skip_current_element(); // TODO log
        }
    }
}

//---------------------------------------------------------
//   pitch
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/pitch node.
 */

void MusicXmlNotePitch::pitch(muse::XmlStreamReader &e) {
    // defaults
    step_ = -1;
    alter_ = 0;
    tuning_ = 0.0;
    octave_ = -1;

    while (e.read_next_start_element()) {
        if (e.name() == "alter") {
            const String alter = e.read_text();
            bool ok;
            alter_ = MusicXmlSupport::string_to_int(alter, &ok);
            if (!ok || alter_ < -3 || alter_ > 3) {
                logger_->log_error(String(u"invalid alter '%1'").arg(alter), &e);
                bool ok2;
                const double altervalue = alter.to_double(&ok2);
                if (ok2 && (std::abs(altervalue) < 2.0) && (acc_type_ == AccidentalType::NONE)) {
                    // try to see if a microtonal accidental is needed
                    acc_type_ = microtonal_guess(altervalue);

                    // If it's not a microtonal accidental we will use tuning
                    if (acc_type_ == AccidentalType::NONE) {
                        tuning_ = 100 * altervalue;
                    }
                }
                alter_ = 0;
            }
        } else if (e.name() == "octave") {
            const String oct = e.read_text();
            bool ok;
            octave_ = oct.to_int(&ok);
            if (!ok || octave_ < 0 || octave_ > 9) {
                logger_->log_error(String(u"invalid octave '%1'").arg(oct), &e);
                octave_ = -1;
            }
        } else if (e.name() == "step") {
            const String step = e.read_text();
            const size_t pos = String(u"CDEFGAB").index_of(step);
            if (step.size() == 1 && pos < 7) {
                step_ = int(pos);
            } else {
                logger_->log_error(String(u"invalid step '%1'").arg(step), &e);
            }
        } else {
            // TODO skipLogCurrElem();
        }
    }
    //LOGD("pitch step %d alter %d oct %d accid %hhd", step, alter, oct, accid);
}

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

/**
 Handle selected child elements of the /score-partwise/part/measure/note node.
 Return true if handled.
 */

bool MusicXmlNotePitch::read_properties(muse::XmlStreamReader &e, Score *score) {
    const AsciiStringView tag(e.name());

    if (tag == "accidental") {
        acc_ = accidental(e, score);
        return true;
    } else if (tag == "unpitched") {
        unpitched_ = true;
        display_step_octave(e);
        return true;
    } else if (tag == "pitch") {
        pitch(e);
        return true;
    }
    return false;
}
} // namespace mu::iex::musicxml
