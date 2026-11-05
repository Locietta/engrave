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

#include "log.h"
#include "engraving/types/fraction.h"
#include "engraving/dom/interval.h"
#include "engraving/types/typesconv.h"

#include "importmusicxmllogger.h"
#include "importmusicxmlnoteduration.h"
#include "importmusicxmlpass1.h"

using namespace mu::engraving;

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   noteTypeToFraction
//---------------------------------------------------------

/**
 Convert MusicXML note type to fraction.
 */

static Fraction noteTypeToFraction(const String &type) {
    if (type == u"1024th") {
        return Fraction(1, 1024);
    } else if (type == u"512th") {
        return Fraction(1, 512);
    } else if (type == u"256th") {
        return Fraction(1, 256);
    } else if (type == u"128th") {
        return Fraction(1, 128);
    } else if (type == u"64th") {
        return Fraction(1, 64);
    } else if (type == u"32nd") {
        return Fraction(1, 32);
    } else if (type == u"16th") {
        return Fraction(1, 16);
    } else if (type == u"eighth") {
        return Fraction(1, 8);
    } else if (type == u"quarter") {
        return Fraction(1, 4);
    } else if (type == u"half") {
        return Fraction(1, 2);
    } else if (type == u"whole") {
        return Fraction(1, 1);
    } else if (type == u"breve") {
        return Fraction(2, 1);
    } else if (type == u"long") {
        return Fraction(4, 1);
    } else if (type == u"maxima") {
        return Fraction(8, 1);
    } else {
        return Fraction(0, 0);
    }
}

//---------------------------------------------------------
//   calculateFraction
//---------------------------------------------------------

/**
 Convert note type, number of dots and actual and normal notes into a duration
 */

static Fraction calculateFraction(const String &type, const int dots, const Fraction timeMod) {
    // type
    Fraction f = noteTypeToFraction(type);
    if (f.isValid()) {
        // dot(s)
        Fraction ff = f;
        for (int i = 0; i < dots; ++i) {
            ff = ff * Fraction(1, 2);
            f += ff;
        }
        // tuplet
        if (timeMod.isValid()) {
            f *= timeMod;
        }
        // clean up (just in case)
        f.reduce();
    }
    return f;
}

//---------------------------------------------------------
//   checkTiming
//---------------------------------------------------------

/**
 Do timing error checks.
 Return empty string if OK, message in case of error.
 */

String MusicXmlNoteDuration::checkTiming(const String &type, const bool rest, const bool grace) {
    //LOGD("type %s rest %d grace %d", muPrintable(type), rest, grace);
    String errorStr;

    // normalize duration
    if (specDura_.isValid()) {
        specDura_.reduce();
    }

    // default: use calculated duration
    calcDura_ = calculateFraction(type, dots_, timeMod_);
    if (calcDura_.isValid()) {
        dura_ = calcDura_;
    }

    if (specDura_.isValid() && calcDura_.isValid()) {
        if (specDura_ != calcDura_) {
            errorStr =
                String(u"calculated duration (%1) not equal to specified duration (%2)").arg(calcDura_.toString(), specDura_.toString());
            //LOGD("rest %d type '%s' timemod %s", rest, muPrintable(type), muPrintable(_timeMod.print()));

            if (rest && type == "whole" && specDura_.isValid()) {
                // Sibelius whole measure rest (not an error)
                errorStr = u"";
                dura_ = specDura_;
            } else if (grace && specDura_ == Fraction(0, 1)) {
                // grace note (not an error)
                errorStr = u"";
                dura_ = specDura_;
            } else {
                if (std::abs(calcDura_.ticks() - specDura_.ticks()) <= pass1_->maxDiff()) {
                    errorStr += u" -> assuming rounding error";
                    pass1_->insertAdjustedDuration(dura_, calcDura_);
                    dura_ = calcDura_;
                    specDura_ = calcDura_; // prevent changing off time
                } else {
                    errorStr += u" -> using calculated duration";
                }
            }

            // Special case:
            // Encore generates rests in tuplets w/o <tuplet> or <time-modification>.
            // Detect this by comparing the actual duration with the expected duration
            // based on note type. If actual is 2/3 of expected, the rest is part
            // of a tuplet.
            if (rest) {
                if (2 * calcDura_.ticks() == 3 * specDura_.ticks()) {
                    timeMod_ = Fraction(2, 3);
                    errorStr += errorStr.isEmpty() ? u" ->" : u",";
                    errorStr += u" assuming triplet";
                    dura_ = specDura_;
                }
            }
        }
    } else if (specDura_.isValid() && !calcDura_.isValid()) {
        // do not report an error for typeless (whole measure) rests
        if (!(rest && type == "")) {
            errorStr = u"calculated duration invalid, using specified duration";
        }
        dura_ = specDura_;
    } else if (!specDura_.isValid() && calcDura_.isValid()) {
        if (!grace) {
            errorStr = u"specified duration invalid, using calculated duration";
        }
    } else {
        errorStr = u"calculated and specified duration invalid, using 4/4";
        dura_ = Fraction(4, 4);
    }

    pass1_->insertSeenDenominator(dura_.reduced().denominator());
    return errorStr;
}

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/duration node.
 */

void MusicXmlNoteDuration::duration(muse::XmlStreamReader &e) {
    logger_->logDebugTrace(u"MusicXmlParserPass1::duration", &e);

    specDura_.set(0, 0); // invalid unless set correctly
    int intDura = e.readAsciiText().toInt();
    specDura_ = pass1_->calcTicks(intDura, divs_, &e); // Duration reading (and rounding) code consolidated to pass1
}

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

/**
 Handle selected child elements of the /score-partwise/part/measure/note/duration node.
 Return true if handled.
 */

bool MusicXmlNoteDuration::readProperties(muse::XmlStreamReader &e) {
    const AsciiStringView tag(e.name());
    //LOGD("tag %s", muPrintable(tag.toString()));
    if (tag == "dot") {
        dots_++;
        e.skipCurrentElement(); // skip but don't log
        return true;
    } else if (tag == "duration") {
        duration(e);
        return true;
    } else if (tag == "time-modification") {
        timeModification(e);
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   timeModification
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/time-modification node.
 */

void MusicXmlNoteDuration::timeModification(muse::XmlStreamReader &e) {
    logger_->logDebugTrace(u"MusicXmlParserPass1::timeModification", &e);

    int intActual = 0;
    int intNormal = 0;
    String strActual;
    String strNormal;

    while (e.readNextStartElement()) {
        const AsciiStringView tag(e.name());
        if (tag == "actual-notes") {
            strActual = e.readText();
        } else if (tag == "normal-notes") {
            strNormal = e.readText();
        } else if (tag == "normal-type") {
            // "measure" is not a valid normal-type,
            // but would be accepted by setType()
            String strNormalType = e.readText();
            if (strNormalType != u"measure") {
                muse::ByteArray ba = strNormalType.toAscii();
                normalType_.setType(TConv::fromXml(ba.constChar(), DurationType::V_INVALID));
            }
        } else {
            logger_->logDebugInfo(String(u"skipping '%1'").arg(String::fromAscii(e.name().ascii())), &e);
            e.skipCurrentElement();
        }
    }

    intActual = strActual.toInt();
    intNormal = strNormal.toInt();
    if (intActual > 0 && intNormal > 0) {
        timeMod_.set(intNormal, intActual);
    } else {
        timeMod_.set(1, 1);
        logger_->logError(String(u"illegal time-modification: actual-notes %1 normal-notes %2").arg(strActual, strNormal), &e);
    }
}
} // namespace mu::iex::musicxml
