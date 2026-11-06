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

static Fraction note_type_to_fraction(const String &type) {
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

static Fraction calculate_fraction(const String &type, const int dots, const Fraction time_mod) {
    // type
    Fraction f = note_type_to_fraction(type);
    if (f.is_valid()) {
        // dot(s)
        Fraction ff = f;
        for (int i = 0; i < dots; ++i) {
            ff = ff * Fraction(1, 2);
            f += ff;
        }
        // tuplet
        if (time_mod.is_valid()) {
            f *= time_mod;
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

String MusicXmlNoteDuration::check_timing(const String &type, const bool rest, const bool grace) {
    //LOGD("type %s rest %d grace %d", muPrintable(type), rest, grace);
    String error_str;

    // normalize duration
    if (spec_dura_.is_valid()) {
        spec_dura_.reduce();
    }

    // default: use calculated duration
    calc_dura_ = calculate_fraction(type, dots_, time_mod_);
    if (calc_dura_.is_valid()) {
        dura_ = calc_dura_;
    }

    if (spec_dura_.is_valid() && calc_dura_.is_valid()) {
        if (spec_dura_ != calc_dura_) {
            error_str = String(u"calculated duration (%1) not equal to specified duration (%2)")
                            .arg(calc_dura_.to_string(), spec_dura_.to_string());
            //LOGD("rest %d type '%s' timemod %s", rest, muPrintable(type), muPrintable(_timeMod.print()));

            if (rest && type == "whole" && spec_dura_.is_valid()) {
                // Sibelius whole measure rest (not an error)
                error_str = u"";
                dura_ = spec_dura_;
            } else if (grace && spec_dura_ == Fraction(0, 1)) {
                // grace note (not an error)
                error_str = u"";
                dura_ = spec_dura_;
            } else {
                if (std::abs(calc_dura_.ticks() - spec_dura_.ticks()) <= pass1_->max_diff()) {
                    error_str += u" -> assuming rounding error";
                    pass1_->insert_adjusted_duration(dura_, calc_dura_);
                    dura_ = calc_dura_;
                    spec_dura_ = calc_dura_; // prevent changing off time
                } else {
                    error_str += u" -> using calculated duration";
                }
            }

            // Special case:
            // Encore generates rests in tuplets w/o <tuplet> or <time-modification>.
            // Detect this by comparing the actual duration with the expected duration
            // based on note type. If actual is 2/3 of expected, the rest is part
            // of a tuplet.
            if (rest) {
                if (2 * calc_dura_.ticks() == 3 * spec_dura_.ticks()) {
                    time_mod_ = Fraction(2, 3);
                    error_str += error_str.is_empty() ? u" ->" : u",";
                    error_str += u" assuming triplet";
                    dura_ = spec_dura_;
                }
            }
        }
    } else if (spec_dura_.is_valid() && !calc_dura_.is_valid()) {
        // do not report an error for typeless (whole measure) rests
        if (!(rest && type == "")) {
            error_str = u"calculated duration invalid, using specified duration";
        }
        dura_ = spec_dura_;
    } else if (!spec_dura_.is_valid() && calc_dura_.is_valid()) {
        if (!grace) {
            error_str = u"specified duration invalid, using calculated duration";
        }
    } else {
        error_str = u"calculated and specified duration invalid, using 4/4";
        dura_ = Fraction(4, 4);
    }

    pass1_->insert_seen_denominator(dura_.reduced().denominator());
    return error_str;
}

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

/**
 Parse the /score-partwise/part/measure/note/duration node.
 */

void MusicXmlNoteDuration::duration(muse::XmlStreamReader &e) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::duration", &e);

    spec_dura_.set(0, 0); // invalid unless set correctly
    int int_dura = e.read_ascii_text().to_int();
    spec_dura_ = pass1_->calc_ticks(int_dura, divs_, &e); // Duration reading (and rounding) code consolidated to pass1
}

//---------------------------------------------------------
//   readProperties
//---------------------------------------------------------

/**
 Handle selected child elements of the /score-partwise/part/measure/note/duration node.
 Return true if handled.
 */

bool MusicXmlNoteDuration::read_properties(muse::XmlStreamReader &e) {
    const AsciiStringView tag(e.name());
    //LOGD("tag %s", muPrintable(tag.toString()));
    if (tag == "dot") {
        dots_++;
        e.skip_current_element(); // skip but don't log
        return true;
    } else if (tag == "duration") {
        duration(e);
        return true;
    } else if (tag == "time-modification") {
        time_modification(e);
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

void MusicXmlNoteDuration::time_modification(muse::XmlStreamReader &e) {
    logger_->log_debug_trace(u"MusicXmlParserPass1::timeModification", &e);

    int int_actual = 0;
    int int_normal = 0;
    String str_actual;
    String str_normal;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "actual-notes") {
            str_actual = e.read_text();
        } else if (tag == "normal-notes") {
            str_normal = e.read_text();
        } else if (tag == "normal-type") {
            // "measure" is not a valid normal-type,
            // but would be accepted by setType()
            String str_normal_type = e.read_text();
            if (str_normal_type != u"measure") {
                muse::ByteArray ba = str_normal_type.to_ascii();
                normal_type_.set_type(TConv::from_xml(ba.const_char(), DurationType::V_INVALID));
            }
        } else {
            logger_->log_debug_info(String(u"skipping '%1'").arg(String::from_ascii(e.name().ascii())), &e);
            e.skip_current_element();
        }
    }

    int_actual = str_actual.to_int();
    int_normal = str_normal.to_int();
    if (int_actual > 0 && int_normal > 0) {
        time_mod_.set(int_normal, int_actual);
    } else {
        time_mod_.set(1, 1);
        logger_->log_error(String(u"illegal time-modification: actual-notes %1 normal-notes %2").arg(str_actual, str_normal), &e);
    }
}
} // namespace mu::iex::musicxml
