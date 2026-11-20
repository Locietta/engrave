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

#include "tempotext.h"

#include <algorithm>
#include <cmath>

#include "containers.h"
#include "engraving/style/styletypes.h"
#include "global/types/textscan.h"
#include "log.h"
#include "score.h"
#include "segment.h"
#include "translation.h"
#include "types/constants.h"
#include "types/typesconv.h"

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

static std::u16string_view tempo_number(std::u16string_view text) {
    size_t size = muse::textscan::digit_count(text);
    if (size != 0 && size < text.size() && text[size] == u'.') {
        size += 1 + muse::textscan::digit_count(text.substr(size + 1));
    }
    return text.substr(0, size);
}

double TempoText::tempo_bpm() const {
    const String xml = xml_text();
    const auto text = xml.view();
    for (auto eq = text.find(u'='); eq != text.npos; eq = text.find(u'=', eq + 1)) {
        const auto number = tempo_number(muse::textscan::trim_ascii_start(text.substr(eq + 1)));
        if (!number.empty()) {
            return String(number).to_double();
        }
    }
    return 0;
}

//---------------------------------------------------------
//   TempoPattern
//---------------------------------------------------------

struct TempoPattern {
    char16_t glyph;
    double factor;
    TDuration duration;

    TempoPattern(char16_t glyph, double factor, DurationType type, int dots = 0) : glyph(glyph), factor(factor), duration(type) {
        duration.set_dots(dots);
    }

    size_t match(std::u16string_view text) const {
        if (text.empty() || text.front() != glyph) {
            return 0;
        }
        auto tail = text.substr(1);
        for (int i = 0; i < duration.dots(); ++i) {
            tail = muse::textscan::trim_ascii_start(tail);
            if (tail.empty() || tail.front() != u'\uECB7') {
                return 0;
            }
            tail.remove_prefix(1);
        }
        return text.size() - tail.size();
    }
};

// Dotted patterns precede undotted patterns so a match includes the dots.
static const TempoPattern tempo_patterns[] = {
    TempoPattern(u'\uECA5', 1.75 / 60.0, DurationType::V_QUARTER, 2), // double dotted 1/4
    TempoPattern(u'\uECA5', 1.5 / 60.0, DurationType::V_QUARTER, 1),  // dotted 1/4
    TempoPattern(u'\uECA5', 1.0 / 60.0, DurationType::V_QUARTER),     // 1/4
    TempoPattern(u'\uECA3', 1.75 / 30.0, DurationType::V_HALF, 2),    // double dotted 1/2
    TempoPattern(u'\uECA3', 1.5 / 30.0, DurationType::V_HALF, 1),     // dotted 1/2
    TempoPattern(u'\uECA3', 1.0 / 30.0, DurationType::V_HALF),        // 1/2
    TempoPattern(u'\uECA7', 1.75 / 120.0, DurationType::V_EIGHTH, 2), // double dotted 1/8
    TempoPattern(u'\uECA7', 1.5 / 120.0, DurationType::V_EIGHTH, 1),  // dotted 1/8
    TempoPattern(u'\uECA7', 1.0 / 120.0, DurationType::V_EIGHTH),     // 1/8
    TempoPattern(u'\uECA2', 1.5 / 15.0, DurationType::V_WHOLE, 1),    // dotted whole
    TempoPattern(u'\uECA2', 1.0 / 15.0, DurationType::V_WHOLE),       // whole
    TempoPattern(u'\uECA9', 1.5 / 240.0, DurationType::V_16TH, 1),    // dotted 1/16
    TempoPattern(u'\uECA9', 1.0 / 240.0, DurationType::V_16TH),       // 1/16
    TempoPattern(u'\uECAB', 1.5 / 480.0, DurationType::V_32ND, 1),    // dotted 1/32
    TempoPattern(u'\uECAB', 1.0 / 480.0, DurationType::V_32ND),       // 1/32
    TempoPattern(u'\uECA1', 1.0 / 7.5, DurationType::V_BREVE),        // longa
    TempoPattern(u'\uECA0', 1.0 / 7.5, DurationType::V_BREVE),        // double whole
    TempoPattern(u'\uECAD', 1.0 / 960.0, DurationType::V_64TH),       // 1/64
    TempoPattern(u'\uECAF', 1.0 / 1920.0, DurationType::V_128TH),     // 1/128
    TempoPattern(u'\uECB1', 1.0 / 3840.0, DurationType::V_256TH),     // 1/256
    TempoPattern(u'\uECB3', 1.0 / 7680.0, DurationType::V_512TH),     // 1/512
    TempoPattern(u'\uECB5', 1.0 / 15360.0, DurationType::V_1024TH),   // 1/1024
};

//---------------------------------------------------------
//   findTempoDuration
//    find the duration part (note + dot) of a tempo text in string s
//    return the match position or -1 if not found
//    set len to the match length and dur to the duration value
//---------------------------------------------------------

int TempoText::find_tempo_duration(std::u16string_view text, int &len, TDuration &dur) {
    len = 0;
    dur = TDuration();
    for (const auto &pattern : tempo_patterns) {
        for (auto pos = text.find(pattern.glyph); pos != text.npos; pos = text.find(pattern.glyph, pos + 1)) {
            if (const size_t size = pattern.match(text.substr(pos))) {
                len = static_cast<int>(size);
                dur = pattern.duration;
                return static_cast<int>(pos);
            }
        }
    }
    return -1;
}

TDuration TempoText::duration() const {
    int dummy = 0;
    TDuration result;

    find_tempo_duration(plain_text().view(), dummy, result);

    return result;
}

struct TempoMarkup {
    std::u16string_view text;
    TDuration duration;

    TempoMarkup(std::u16string_view text, DurationType type, int dots = 0) : text(text), duration(type) { duration.set_dots(dots); }
};

static const TempoMarkup tempo_markups[] = {
    {u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>metAugmentationDot</sym>", DurationType::V_QUARTER, 2},
    {u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", DurationType::V_QUARTER, 1},
    {u"<sym>metNoteQuarterUp</sym>", DurationType::V_QUARTER},
    {u"<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>metAugmentationDot</sym>", DurationType::V_HALF, 2},
    {u"<sym>metNoteHalfUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", DurationType::V_HALF, 1},
    {u"<sym>metNoteHalfUp</sym>", DurationType::V_HALF},
    {u"<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym><sym>metAugmentationDot</sym>", DurationType::V_EIGHTH, 2},
    {u"<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", DurationType::V_EIGHTH, 1},
    {u"<sym>metNote8thUp</sym>", DurationType::V_EIGHTH},
    {u"<sym>metNoteWhole</sym><sym>space</sym><sym>metAugmentationDot</sym>", DurationType::V_WHOLE, 1},
    {u"<sym>metNoteWhole</sym>", DurationType::V_WHOLE},
    {u"<sym>metNote16thUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", DurationType::V_16TH, 1},
    {u"<sym>metNote16thUp</sym>", DurationType::V_16TH},
    {u"<sym>metNote32ndUp</sym><sym>space</sym><sym>metAugmentationDot</sym>", DurationType::V_32ND, 1},
    {u"<sym>metNote32ndUp</sym>", DurationType::V_32ND},
    {u"<sym>metNoteDoubleWholeSquare</sym>", DurationType::V_BREVE},
    {u"<sym>metNoteDoubleWhole</sym>", DurationType::V_BREVE},
    {u"<sym>metNote64thUp</sym>", DurationType::V_64TH},
    {u"<sym>metNote128thUp</sym>", DurationType::V_128TH},
    {u"<sym>metNote256thUp</sym>", DurationType::V_256TH},
    {u"<sym>metNote512thUp</sym>", DurationType::V_512TH},
    {u"<sym>metNote1024thUp</sym>", DurationType::V_1024TH},
};

//---------------------------------------------------------
//   duration2tempoTextString
//    find the tempoText string representation for duration
//---------------------------------------------------------

String TempoText::duration2tempo_text_string(const TDuration dur) {
    for (const auto &markup : tempo_markups) {
        if (markup.duration == dur) {
            return String(markup.text);
        }
    }
    return {};
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
    String s = plain_text();
    s.replace(u",", u".");
    s.replace(u"<sym>space</sym>", u" ");
    s.replace(u"≒", u"=");
    s.replace(u"≈", u"=");
    s.replace(u"~", u"=");
    s.replace(u"ca.", u"");
    s.replace(u"c.", u"");
    s.replace(u"approx.", u"");
    const auto text = s.view();
    for (const TempoPattern &left : tempo_patterns) {
        const TempoPattern *relative_right = std::end(tempo_patterns);
        for (auto pos = text.find(left.glyph); pos != text.npos; pos = text.find(left.glyph, pos + 1)) {
            const size_t length = left.match(text.substr(pos));
            if (length == 0) {
                continue;
            }
            auto right = muse::textscan::trim_ascii_start(text.substr(pos + length));
            if (right.empty() || right.front() != u'=') {
                continue;
            }
            right = muse::textscan::trim_ascii_start(right.substr(1));
            if (const auto number = tempo_number(right); !number.empty()) {
                const BeatsPerSecond tempo(String(number).to_double() * left.factor);
                if (is_relative_ || tempo != tempo_) {
                    undo_change_property(Pid::TEMPO, PropertyValue(tempo), property_flags(Pid::TEMPO));
                    relative_ = 1.0;
                    is_relative_ = false;
                }
                return;
            }
            for (const TempoPattern *pattern = std::begin(tempo_patterns); pattern < relative_right; ++pattern) {
                if (pattern->match(right)) {
                    relative_right = pattern;
                    break;
                }
            }
        }
        if (relative_right != std::end(tempo_patterns)) {
            relative_ = relative_right->factor / left.factor;
            is_relative_ = true;
            return;
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
    int x1 = find_tempo_duration(first_part.view(), len1, t1);
    int x2 = -1;
    if (relative_) {
        x2 = find_tempo_duration(second_part.view(), len2, t2);
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

} // namespace mu::engraving
