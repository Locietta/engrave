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

#include "draw/types/color.h"

#include "engraving/dom/factory.h"
#include "engraving/dom/score.h"

#include "../shared/musicxmlsupport.h"
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
    const bool cautionary = e.asciiAttribute("cautionary") == "yes";
    const bool editorial = e.asciiAttribute("editorial") == "yes";
    const bool parentheses = e.asciiAttribute("parentheses") == "yes";
    const bool noParentheses = e.asciiAttribute("parentheses") == "no";
    const bool brackets = e.asciiAttribute("bracket") == "yes";
    const bool noBrackets = e.asciiAttribute("bracket") == "no";
    const bool smallAccid = e.asciiAttribute("size") == "cue" || e.asciiAttribute("size") == "grace-cue";
    const Color accColor = Color(e.asciiAttribute("color").ascii());
    const String smufl = e.attribute("smufl");

    const String s = e.readText();
    const AccidentalType type = musicXmlString2accidentalType(s, smufl);

    if (type != AccidentalType::NONE) {
        Accidental *a = Factory::createAccidental(score->dummy());
        a->setAccidentalType(type);
        if (cautionary || editorial) { // no way to tell one from the other
            a->setRole(AccidentalRole::USER);
        } // except via the use of parentheses vs. brackets
        if (noParentheses || noBrackets) {      // explicitly none wanted
        } else if (parentheses || cautionary) { // set to "yes" or "cautionary" and not set at all
            a->setBracket(AccidentalBracket(AccidentalBracket::PARENTHESIS));
        } else if (brackets || editorial) { // set to "yes" or "editorial" and not set at all
            a->setBracket(AccidentalBracket(AccidentalBracket::BRACKET));
        }
        if (accColor.isValid()) {
            a->setColor(accColor);
        }
        a->setSmall(smallAccid);
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

void MusicXmlNotePitch::displayStepOctave(muse::XmlStreamReader &e) {
    while (e.readNextStartElement()) {
        if (e.name() == "display-step") {
            const String step = e.readText();
            int pos = static_cast<int>(String(u"CDEFGAB").indexOf(step));
            if (step.size() == 1 && pos >= 0 && pos < 7) {
                displayStep_ = pos;
            } else {
                //logError(String("invalid step '%1'").arg(strStep));
                LOGD("invalid step '%s'", muPrintable(step)); // TODO
            }
        } else if (e.name() == "display-octave") {
            const String oct = e.readText();
            bool ok;
            displayOctave_ = oct.toInt(&ok);
            if (!ok || displayOctave_ < 0 || displayOctave_ > 9) {
                //logError(String("invalid octave '%1'").arg(strOct));
                LOGD("invalid octave '%s'", muPrintable(oct)); // TODO
                displayOctave_ = -1;
            }
        } else {
            e.skipCurrentElement(); // TODO log
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

    while (e.readNextStartElement()) {
        if (e.name() == "alter") {
            const String alter = e.readText();
            bool ok;
            alter_ = MusicXmlSupport::stringToInt(alter, &ok);
            if (!ok || alter_ < -3 || alter_ > 3) {
                logger_->logError(String(u"invalid alter '%1'").arg(alter), &e);
                bool ok2;
                const double altervalue = alter.toDouble(&ok2);
                if (ok2 && (std::abs(altervalue) < 2.0) && (accType_ == AccidentalType::NONE)) {
                    // try to see if a microtonal accidental is needed
                    accType_ = microtonalGuess(altervalue);

                    // If it's not a microtonal accidental we will use tuning
                    if (accType_ == AccidentalType::NONE) {
                        tuning_ = 100 * altervalue;
                    }
                }
                alter_ = 0;
            }
        } else if (e.name() == "octave") {
            const String oct = e.readText();
            bool ok;
            octave_ = oct.toInt(&ok);
            if (!ok || octave_ < 0 || octave_ > 9) {
                logger_->logError(String(u"invalid octave '%1'").arg(oct), &e);
                octave_ = -1;
            }
        } else if (e.name() == "step") {
            const String step = e.readText();
            const size_t pos = String(u"CDEFGAB").indexOf(step);
            if (step.size() == 1 && pos < 7) {
                step_ = int(pos);
            } else {
                logger_->logError(String(u"invalid step '%1'").arg(step), &e);
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

bool MusicXmlNotePitch::readProperties(muse::XmlStreamReader &e, Score *score) {
    const AsciiStringView tag(e.name());

    if (tag == "accidental") {
        acc_ = accidental(e, score);
        return true;
    } else if (tag == "unpitched") {
        unpitched_ = true;
        displayStepOctave(e);
        return true;
    } else if (tag == "pitch") {
        pitch(e);
        return true;
    }
    return false;
}
} // namespace mu::iex::musicxml
