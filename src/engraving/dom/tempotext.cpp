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

#include <algorithm>
#include <cmath>

#include <unordered_map>

#include "containers.h"
#include "translation.h"

#include "types/typesconv.h"
#include "types/constants.h"

#include "score.h"
#include "segment.h"
#include "tempotext.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//TODO: textChanged() needs to be called during/after editing

//---------------------------------------------------------
//   tempoStyle
//---------------------------------------------------------

static const ElementStyle tempo_style{
    {Sid::TEMPO_SYSTEM_FLAG, Pid::SYSTEM_FLAG},
    {Sid::TEMPO_PLACEMENT, Pid::PLACEMENT},
    {Sid::TEMPO_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   TempoText
//---------------------------------------------------------

TempoText::TempoText(DummyParentOr<Segment> parent)
    : TextBase(ElementType::TEMPO_TEXT, parent, TextStyleType::TEMPO, ElementFlag::SYSTEM | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::tempo_style);
    tempo_text_type_ = TempoTextType::NORMAL;
    tempo_ = 2.0; // propertyDefault(P_TEMPO).toDouble();
    follow_text_ = false;
    relative_ = 1.0;
    is_relative_ = false;

    reset_property(Pid::MUSIC_SYMBOL_SIZE);
}

void TempoText::set_tempo_text_type(TempoTextType ttt) { tempo_text_type_ = ttt; }

double TempoText::tempo_bpm() const {
    //! NOTE: find tempo in format " = 180"
    std::regex regex("\\s*=\\s*(\\d+[.]{0,1}\\d*)");
    std::smatch match;
    std::string stru8 = xml_text().to_std_string();
    std::regex_search(stru8, match, regex);
    if (match.empty() || match.size() < 1) {
        return 0;
    }

    double tempo = String::from_std_string(match[1].str()).to_double();
    return tempo;
}

//---------------------------------------------------------
//   TempoPattern
//---------------------------------------------------------

struct TempoPattern {
    const char *pattern;
    double f;
    TDuration d;

    TempoPattern(const char *s, double v, DurationType val, int dots = 0) : pattern(s), f(v), d(val) { d.set_dots(dots); }
};

// note: findTempoDuration requires the longer patterns to be before the shorter patterns in tp

static const TempoPattern tp[] = {
    TempoPattern("\uECA5\\s*\uECB7\\s*\uECB7", 1.75 / 60.0, DurationType::V_QUARTER, 2), // double dotted 1/4
    TempoPattern("\uECA5\\s*\uECB7", 1.5 / 60.0, DurationType::V_QUARTER, 1),            // dotted 1/4
    TempoPattern("\uECA5", 1.0 / 60.0, DurationType::V_QUARTER),                         // 1/4
    TempoPattern("\uECA3\\s*\uECB7\\s*\uECB7", 1.75 / 30.0, DurationType::V_HALF, 2),    // double dotted 1/2
    TempoPattern("\uECA3\\s*\uECB7", 1.5 / 30.0, DurationType::V_HALF, 1),               // dotted 1/2
    TempoPattern("\uECA3", 1.0 / 30.0, DurationType::V_HALF),                            // 1/2
    TempoPattern("\uECA7\\s*\uECB7\\s*\uECB7", 1.75 / 120.0, DurationType::V_EIGHTH, 2), // double dotted 1/8
    TempoPattern("\uECA7\\s*\uECB7", 1.5 / 120.0, DurationType::V_EIGHTH, 1),            // dotted 1/8
    TempoPattern("\uECA7", 1.0 / 120.0, DurationType::V_EIGHTH),                         // 1/8
    TempoPattern("\uECA2\\s*\uECB7", 1.5 / 15.0, DurationType::V_WHOLE, 1),              // dotted whole
    TempoPattern("\uECA2", 1.0 / 15.0, DurationType::V_WHOLE),                           // whole
    TempoPattern("\uECA9\\s*\uECB7", 1.5 / 240.0, DurationType::V_16TH, 1),              // dotted 1/16
    TempoPattern("\uECA9", 1.0 / 240.0, DurationType::V_16TH),                           // 1/16
    TempoPattern("\uECAB\\s*\uECB7", 1.5 / 480.0, DurationType::V_32ND, 1),              // dotted 1/32
    TempoPattern("\uECAB", 1.0 / 480.0, DurationType::V_32ND),                           // 1/32
    TempoPattern("\uECA1", 1.0 / 7.5, DurationType::V_BREVE),                            // longa
    TempoPattern("\uECA0", 1.0 / 7.5, DurationType::V_BREVE),                            // double whole
    TempoPattern("\uECAD", 1.0 / 960.0, DurationType::V_64TH),                           // 1/64
    TempoPattern("\uECAF", 1.0 / 1920.0, DurationType::V_128TH),                         // 1/128
    TempoPattern("\uECB1", 1.0 / 3840.0, DurationType::V_256TH),                         // 1/256
    TempoPattern("\uECB3", 1.0 / 7680.0, DurationType::V_512TH),                         // 1/512
    TempoPattern("\uECB5", 1.0 / 15360.0, DurationType::V_1024TH),                       // 1/1024
};

//---------------------------------------------------------
//   findTempoDuration
//    find the duration part (note + dot) of a tempo text in string s
//    return the match position or -1 if not found
//    set len to the match length and dur to the duration value
//---------------------------------------------------------

int TempoText::find_tempo_duration(const String &s, int &len, TDuration &dur) {
    len = 0;
    dur = TDuration();
    std::string su8 = s.to_std_string();
    for (const auto &i : tp) {
        std::regex regex(i.pattern);
        std::smatch match;
        std::regex_search(su8, match, regex);
        if (!match.empty()) {
            len = static_cast<int>(String::from_std_string(match[0].str()).size());
            dur = i.d;
            return std::distance(su8.cbegin(), match[0].first);
        }
    }
    return -1;
}

TDuration TempoText::duration() const {
    int dummy = 0;
    TDuration result;

    find_tempo_duration(plain_text(), dummy, result);

    return result;
}

static const TempoPattern tp_sym[] = {
    TempoPattern("<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>metAugmentationDot</sym>", 1.75 / 60.0,
                 DurationType::V_QUARTER, 2), // double dotted 1/4
    TempoPattern("<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5 / 60.0, DurationType::V_QUARTER,
                 1),                                                                  // dotted 1/4
    TempoPattern("<sym>metNoteQuarterUp</sym>", 1.0 / 60.0, DurationType::V_QUARTER), // 1/4
    TempoPattern("<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>metAugmentationDot</sym>", 1.75 / 30.0,
                 DurationType::V_HALF, 2), // double dotted 1/2
    TempoPattern("<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5 / 30.0, DurationType::V_HALF,
                 1),                                                            // dotted 1/2
    TempoPattern("<sym>metNoteHalfUp</sym>", 1.0 / 30.0, DurationType::V_HALF), // 1/2
    TempoPattern("<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>metAugmentationDot</sym>", 1.75 / 120.0,
                 DurationType::V_EIGHTH, 2), // double dotted 1/8
    TempoPattern("<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5 / 120.0, DurationType::V_EIGHTH,
                 1),                                                              // dotted 1/8
    TempoPattern("<sym>metNote8thUp</sym>", 1.0 / 120.0, DurationType::V_EIGHTH), // 1/8
    TempoPattern("<sym>metNoteWhole</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5 / 15.0, DurationType::V_WHOLE,
                 1),                                                            // dotted whole
    TempoPattern("<sym>metNoteWhole</sym>", 1.0 / 15.0, DurationType::V_WHOLE), // whole
    TempoPattern("<sym>metNote16thUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5 / 240.0, DurationType::V_16TH,
                 1),                                                             // dotted 1/16
    TempoPattern("<sym>metNote16thUp</sym>", 1.0 / 240.0, DurationType::V_16TH), // 1/16
    TempoPattern("<sym>metNote32ndUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", 1.5 / 480.0, DurationType::V_32ND,
                 1),                                                                       // dotted 1/32
    TempoPattern("<sym>metNote32ndUp</sym>", 1.0 / 480.0, DurationType::V_32ND),           // 1/32
    TempoPattern("<sym>metNoteDoubleWholeSquare</sym>", 1.0 / 7.5, DurationType::V_BREVE), // longa
    TempoPattern("<sym>metNoteDoubleWhole</sym>", 1.0 / 7.5, DurationType::V_BREVE),       // double whole
    TempoPattern("<sym>metNote64thUp</sym>", 1.0 / 960.0, DurationType::V_64TH),           // 1/64
    TempoPattern("<sym>metNote128thUp</sym>", 1.0 / 1920.0, DurationType::V_128TH),        // 1/128
    TempoPattern("<sym>metNote256thUp</sym>", 1.0 / 3840.0, DurationType::V_256TH),        // 1/256
    TempoPattern("<sym>metNote512thUp</sym>", 1.0 / 7680.0, DurationType::V_512TH),        // 1/512
    TempoPattern("<sym>metNote1024thUp</sym>", 1.0 / 15360.0, DurationType::V_1024TH),     // 1/1024
};

//---------------------------------------------------------
//   duration2tempoTextString
//    find the tempoText string representation for duration
//---------------------------------------------------------

String TempoText::duration2tempo_text_string(const TDuration dur) {
    for (const TempoPattern &pa : ::mu::engraving::tp_sym) {
        if (pa.d == dur) {
            String res = String::from_utf8(pa.pattern);
            return res;
        }
    }
    return u"";
}

//---------------------------------------------------------
// tempo
//---------------------------------------------------------

BeatsPerSecond TempoText::tempo() const {
    if (is_relative_) {
        const BeatsPerSecond tempo_before = score()->tempo(tick() - Fraction::eps());
        const BeatsPerSecond v = tempo_before * relative_;
        return BeatsPerSecond(std::clamp(v.val, Constants::min_tempo.val, Constants::max_tempo.val));
    }
    return tempo_;
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void TempoText::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::TEMPO_FOLLOW_TEXT) {
        EngravingObject::undo_change_property(id, v, ps);
        if (follow_text_) {
            update_tempo();
        }
    } else {
        EngravingObject::undo_change_property(id, v, ps);
    }
}

//---------------------------------------------------------
//   updateTempo
//---------------------------------------------------------

void TempoText::update_tempo() {
    // cache regexp, they are costly to create
    static std::unordered_map<String, std::regex> regexps;
    static std::unordered_map<String, std::regex> regexps2;
    String s = plain_text();
    s.replace(u",", u".");
    s.replace(u"<sym>space</sym>", u" ");
    s.replace(u"≒", u"=");
    s.replace(u"≈", u"=");
    s.replace(u"~", u"=");
    s.replace(u"ca.", u"");
    s.replace(u"c.", u"");
    s.replace(u"approx.", u"");
    std::string su8 = s.to_std_string();
    for (const TempoPattern &pa : tp) {
        String pattern = String::from_utf8(pa.pattern);
        std::regex re;
        if (!muse::contains(regexps, String::from_utf8(pa.pattern))) {
            re = std::regex(String(u"%1\\s*=\\s*(\\d+[.]{0,1}\\d*)\\s*").arg(pattern).to_std_string());
            regexps[pattern] = re;
        }
        re = muse::value(regexps, pattern);
        std::smatch match;
        std::regex_search(su8, match, re);
        if (!match.empty()) {
            if (match.size() == 2) {
                BeatsPerSecond nt = BeatsPerSecond(String::from_std_string(match[1].str()).to_double() * pa.f);
                if (is_relative_ || nt != tempo_) {
                    undo_change_property(Pid::TEMPO, PropertyValue(nt), property_flags(Pid::TEMPO));
                    relative_ = 1.0;
                    is_relative_ = false;
                }
                break;
            }
        } else {
            for (const TempoPattern &pa2 : tp) {
                String pattern2 = String::from_utf8(pa2.pattern);
                String key = String(u"%1_%2").arg(pattern, pattern2);
                std::regex re2;
                if (!muse::contains(regexps2, key)) {
                    re2 = std::regex(String(u"%1\\s*=\\s*%2\\s*").arg(pattern, pattern2).to_std_string());
                    regexps2[key] = re2;
                }
                re2 = muse::value(regexps2, key);
                std::smatch match2;
                std::regex_search(su8, match2, re2);
                if (!match2.empty()) {
                    relative_ = pa2.f / pa.f;
                    is_relative_ = true;
                    return;
                }
            }
        }
    }
}

//---------------------------------------------------------
//   setTempo
//---------------------------------------------------------

void TempoText::set_tempo(BeatsPerSecond v) {
    if (v.val < Constants::min_tempo.val) {
        v = Constants::min_tempo;
    } else if (v.val > Constants::max_tempo.val) {
        v = Constants::max_tempo;
    }
    tempo_ = v;
    relative_ = 1.0;
    is_relative_ = false;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TempoText::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PLAY: return play_tempo_text_;
        case Pid::TEMPO: return tempo_;
        case Pid::TEMPO_FOLLOW_TEXT: return follow_text_;
        case Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK: return align_right_of_rehearsal_mark_;
        default: return TextBase::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TempoText::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::PLAY: set_play_tempo_text(v.to_bool()); break;
        case Pid::TEMPO: set_tempo(v.value<BeatsPerSecond>()); break;
        case Pid::TEMPO_FOLLOW_TEXT: set_follow_text(v.to_bool()); break;
        case Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK: align_right_of_rehearsal_mark_ = v.to_bool(); break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TempoText::property_default(Pid id) const {
    switch (id) {
        case Pid::PLAY: return true;
        case Pid::TEXT_STYLE: return TextStyleType::TEMPO;
        case Pid::TEMPO: return BeatsPerSecond(2.0);
        case Pid::TEMPO_FOLLOW_TEXT: return false;
        case Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK: return true;
        default: return TextBase::property_default(id);
    }
}

//---------------------------------------------------------
//   duration2userName
//---------------------------------------------------------

String TempoText::duration2user_name(const TDuration t) {
    String dots;
    switch (t.dots()) {
        case 1: dots = muse::mtrc("engraving", "Dotted %1").arg(TConv::translated_user_name(t.type())); break;
        case 2: dots = muse::mtrc("engraving", "Double dotted %1").arg(TConv::translated_user_name(t.type())); break;
        case 3: dots = muse::mtrc("engraving", "Triple dotted %1").arg(TConv::translated_user_name(t.type())); break;
        case 4: dots = muse::mtrc("engraving", "Quadruple dotted %1").arg(TConv::translated_user_name(t.type())); break;
        default: dots = TConv::translated_user_name(t.type()); break;
    }
    return dots;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

String TempoText::tempo_info() const {
    TDuration t1;
    TDuration t2;
    int len1;
    int len2;
    String text = plain_text();
    String first_part = text.split(u" = ").front();
    String second_part = text.split(u" = ").back();
    int x1 = find_tempo_duration(first_part, len1, t1);
    int x2 = -1;
    if (relative_) {
        x2 = find_tempo_duration(second_part, len2, t2);
    }

    String info;

    if (x1 != -1) {
        String dots1 = duration2user_name(t1);
        if (x2 != -1) {
            String dots2 = duration2user_name(t2);
            //: %1 and %2 are note durations. If your language does not have different terms
            //: for "quarter note" and "quarter" (for example), or if the translations for
            //: the durations as separate strings are not suitable to be used as adjectives
            //: here, translate "%1 note" with "%1" and "%2 note" with "%2", so that just the
            //: duration will be shown.
            info = muse::mtrc("engraving", "%1 note = %2 note").arg(dots1, dots2);
        } else {
            //: %1 is a note duration. If your language does not have different terms for
            //: "quarter note" and "quarter" (for example), or if the translations for the
            //: durations as separate strings are not suitable to be used as adjectives here,
            //: translate "%1 note" with "%1", so that just the duration will be shown.
            info = muse::mtrc("engraving", "%1 note = %2").arg(dots1, second_part);
        }
    }

    return info;
}

void TempoText::commit_text() {
    if (follow_text_) {
        update_tempo();
    }

    TextBase::commit_text();
}
} // namespace mu::engraving
