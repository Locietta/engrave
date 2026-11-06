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

#include "harmony.h"

#include "containers.h"
#include "translation.h"

#include "draw/fontmetrics.h"

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "../editing/transpose.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transaction/undostack.h"
#include "../rendering/iscorerenderer.h"

#include "chordlist.h"
#include "fret.h"
#include "linkedobjects.h"
#include "measure.h"
#include "mscore.h"
#include "part.h"
#include "parenthesis.h"
#include "pitchspelling.h"
#include "repeatlist.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "textbase.h"
#include "utils.h"

#include "log.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
static const char *fallback_symboltext_font = "Bravura Text";

//---------------------------------------------------------
//   descr
//    look up id in chord list
//    return chord description if found, or null
//---------------------------------------------------------

const ChordDescription *HarmonyInfo::descr() const {
    if (!chord_list()) {
        return nullptr;
    }
    return chord_list()->description(id());
}

//---------------------------------------------------------
//   descr
//    look up name in chord list
//    optionally look up by parsed chord as fallback
//    return chord description if found, or null
//---------------------------------------------------------

const ChordDescription *HarmonyInfo::descr(const String &name, const ParsedChord *pc) const {
    const ChordDescription *match = nullptr;
    if (!chord_list()) {
        return nullptr;
    }
    for (const auto &p : *chord_list()) {
        const ChordDescription &cd = p.second;
        for (const String &s : cd.names) {
            if (s == name) {
                return &cd;
            }
            if (!pc) {
                continue;
            }
            for (const ParsedChord &s_parsed : cd.parsed_chords) {
                if (s_parsed == *pc) {
                    match = &cd;
                }
            }
        }
    }
    // exact match failed, so fall back on parsed match if one was found
    return match;
}

//---------------------------------------------------------
//   getDescription
//    look up id in chord list
//    return chord description if found
//    if not found, and chord is parseable,
//    generate a new chord description
//    and add to chord list
//---------------------------------------------------------

const ChordDescription *HarmonyInfo::get_description() {
    const ChordDescription *cd = descr();
    if (cd && !cd->names.empty()) {
        text_name_ = cd->names.front();
    } else if (!text_name_.empty()) {
        cd = generate_description();
        id_ = cd->id;
    }
    return cd;
}

//---------------------------------------------------------
//   getDescription
//    same but lookup by name and optionally parsed chord
//---------------------------------------------------------

const ChordDescription *HarmonyInfo::get_description(const String &name, const ParsedChord *pc) {
    const ChordDescription *cd = descr(name, pc);
    if (cd) {
        id_ = cd->id;
    } else {
        cd = generate_description();
        id_ = cd->id;
    }
    return cd;
}

//---------------------------------------------------------
//   generateDescription
//    generate new chord description from _textName
//    add to chord list using private id
//---------------------------------------------------------

const ChordDescription *HarmonyInfo::generate_description() {
    ChordDescription cd(text_name_);
    cd.complete(get_parsed_chord(), chord_list());
    // remove parsed chord from description
    // so we will only match it literally in the future
    cd.parsed_chords.clear();
    chord_list()->insert({cd.id, cd});
    return &chord_list()->at(cd.id);
}

ParsedChord *HarmonyInfo::get_parsed_chord() {
    if (!parsed_chord_) {
        parsed_chord_ = new ParsedChord();
        parsed_chord_->parse(text_name_, chord_list(), false);
    }
    return parsed_chord_;
}

bool HarmonyInfo::has_modifiers() const { return parsed_chord_ ? parsed_chord_->modifiers().size() > 0 : false; }

String Harmony::display_text() const {
    // String representation of what the user sees on the score
    // Used in fretbox
    String name;
    for (const HarmonyRenderItem *segment : ldata()->render_item_list()) {
        if (const TextSegment *text_seg = dynamic_cast<const TextSegment *>(segment)) {
            String t = text_seg->text();
            t.replace(u"\uE87C", u"/");
            name += t;
        } else if (const ChordSymbolParen *paren_seg = dynamic_cast<const ChordSymbolParen *>(segment)) {
            name += paren_seg->paren_item->direction() == DirectionH::LEFT ? u"(" : u")";
        }
    }

    return name;
}

//---------------------------------------------------------
//   harmonyName
//---------------------------------------------------------

String Harmony::harmony_name() const {
    String name;

    if (left_paren()) {
        name = u"(";
    }

    for (size_t i = 0; i < chords_.size(); i++) {
        const HarmonyInfo *info = chords_.at(i);
        HChord hc = info->descr() ? info->descr()->chord : HChord();
        String s, r, e, b;

        if (i != 0) {
            name += u"|";
        }

        if (harmony_type_ == HarmonyType::STANDARD && tpc_is_valid(info->root_tpc())) {
            NoteSpellingType spelling = style().style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();
            r = tpc2name(info->root_tpc(), spelling, root_case_);
        } else if (harmony_type_ == HarmonyType::NASHVILLE && tpc_is_valid(info->root_tpc())) {
            const Staff *st = staff();
            Key key = st ? st->key(tick()) : Key::INVALID;
            r = tpc2_function(info->root_tpc(), key);
        }

        if (!info->text_name().empty()) {
            e = info->text_name();
            if (harmony_type_ != HarmonyType::ROMAN) {
                e.remove(u'=');
            }
        } else if (!degree_list_.empty()) {
            hc.add(degree_list_);
            // try to find the chord in chordList
            const ChordDescription *new_extension = nullptr;
            const ChordList *cl = score()->chord_list();
            for (const auto &p : *cl) {
                const ChordDescription &cd = p.second;
                if (cd.chord == hc && !cd.names.empty()) {
                    new_extension = &cd;
                    break;
                }
            }
            // now determine the chord name
            if (new_extension) {
                e = new_extension->names.front();
            } else {
                // not in table, fallback to using HChord.name()
                r = hc.name(info->root_tpc());
                e = u"";
            }
        }

        if (tpc_is_valid(info->bass_tpc())) {
            NoteSpellingType spelling = style().style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();
            b = u"/" + tpc2name(info->bass_tpc(), spelling, bass_case_);
        }

        s += r + e + b;

        name += s;
    }

    if (right_paren()) {
        name += u")";
    }

    return name;
}

bool Harmony::is_realizable() const {
    if (chords_.empty()) {
        return false;
    }
    for (const HarmonyInfo *info : chords_) {
        if (!tpc_is_valid(info->root_tpc())) {
            return false;
        }
    }
    return true;
}

int Harmony::bass_tpc() const {
    if (chords_.empty()) {
        return Tpc::TPC_INVALID;
    }
    return chords_.front()->bass_tpc();
}

int Harmony::root_tpc() const {
    if (chords_.empty()) {
        return Tpc::TPC_INVALID;
    }
    return chords_.front()->root_tpc();
}

bool Harmony::is_in_fret_box() const {
    EngravingObject *parent = ownership_parent();
    if (!parent) {
        return false;
    }

    if (parent->is_fret_diagram()) {
        return to_fret_diagram(parent)->is_in_fret_box();
    }

    return parent->is_f_box();
}

//---------------------------------------------------------
//   chordSymbolStyle
//---------------------------------------------------------

const ElementStyle chord_symbol_style{
    {Sid::HARMONY_PLACEMENT, Pid::PLACEMENT},
    {Sid::MIN_HARMONY_DISTANCE, Pid::MIN_DISTANCE},
    {Sid::CHORD_BASS_NOTE_SCALE, Pid::HARMONY_BASS_SCALE},
    {Sid::HARMONY_VOICE_LITERAL, Pid::HARMONY_VOICE_LITERAL},
    {Sid::HARMONY_VOICING, Pid::HARMONY_VOICING},
    {Sid::HARMONY_DURATION, Pid::HARMONY_DURATION},
};

//---------------------------------------------------------
//   Harmony
//---------------------------------------------------------

Harmony::Harmony(EngravingObject *parent)
    : TextBase(ElementType::HARMONY, parent, TextStyleType::HARMONY_A, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    assert(!parent || parent->is_segment() || parent->is_fret_diagram() || parent->is_type(ElementType::DUMMY));

    root_case_ = NoteCaseType::CAPITAL;
    bass_case_ = NoteCaseType::CAPITAL;
    harmony_type_ = HarmonyType::STANDARD;
    play_ = true;
    realized_harmony_ = RealizedHarmony(this);
    init_element_style(&::mu::engraving::chord_symbol_style);
}

Harmony::Harmony(const Harmony &h) : TextBase(h) {
    root_case_ = h.root_case_;
    bass_case_ = h.bass_case_;
    bass_scale_ = h.bass_scale_;
    degree_list_ = h.degree_list_;
    harmony_type_ = h.harmony_type_;
    play_ = h.play_;
    realized_harmony_ = h.realized_harmony_;
    realized_harmony_.set_harmony(this);
    for (const HarmonyInfo *hi : h.chords_) {
        HarmonyInfo *new_info = new HarmonyInfo(*hi);
        chords_.push_back(new_info);
    }

    font_family_ = h.font_family_;
    font_style_ = h.font_style_;
    font_size_ = h.font_size_;
}

//---------------------------------------------------------
//   ~Harmony
//---------------------------------------------------------

Harmony::~Harmony() {
    for (const HarmonyRenderItem *render_item : mutldata()->render_item_list()) {
        delete render_item;
    }
    for (const HarmonyInfo *info : chords_) {
        delete info;
    }
}

int Harmony::id() const {
    if (chords_.empty()) {
        return -1;
    }
    return chords_.front()->id();
}

//---------------------------------------------------------
//   getParentSeg
///   gets the parent segment of this harmony
//---------------------------------------------------------

Segment *Harmony::get_parent_seg() const {
    Segment *seg = nullptr;
    if (!ownership_parent()) {
        return nullptr;
    }

    if (ownership_parent()->is_fret_diagram()) {
        // When this harmony is the child of a fret diagram, we need to go up twice
        // to get to the parent seg.
        seg = to_fret_diagram(ownership_parent())->segment();
    } else {
        seg = to_segment(ownership_parent());
    }
    return seg;
}

FretDiagram *Harmony::get_parent_fret_diagram() const {
    if (ownership_parent() && ownership_parent()->is_fret_diagram()) {
        return to_fret_diagram(ownership_parent());
    }

    return nullptr;
}

void Harmony::after_read() {
    // TODO: now that we can render arbitrary chords,
    // we could try to construct a full representation from a degree list.
    // These will typically only exist for chords imported from MusicXML prior to MuseScore 2.0
    // or constructed in the Chord Symbol Properties dialog.

    for (HarmonyInfo *info : chords_) {
        if (tpc_is_valid(info->root_tpc())) {
            if (info->id() > 0) {
                // positive id will happen only for scores that were created with explicit chord lists
                // lookup id in chord list and generate new description if necessary
                info->get_description();
            } else {
                // default case: look up by name
                // description will be found for any chord already read in this score
                // and we will generate a new one if necessary
                info->get_description(info->text_name());
            }
        } else if (info->text_name().empty()) {
            // unrecognized chords prior to 2.0 were stored as text with markup
            // we need to strip away the markup
            // this removes any user-applied formatting,
            // but we no longer support user-applied formatting for chord symbols anyhow
            // with any luck, the resulting text will be parseable now, so give it a shot
            create_blocks();
            String s = plain_text();
            if (!s.is_empty()) {
                set_harmony(s);
                return;
            }
            // empty text could also indicate a root-less slash chord ("/E")
            // we'll fall through and render it normally
        }
    }

    // render chord from description (or _textName)
    set_plain_text(harmony_name());
}

bool Harmony::has_modifiers() const {
    for (const HarmonyInfo *info : chords_) {
        if (info->has_modifiers()) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   parseHarmony
//    determine root and bass tpc & case
//    compare body of chordname against chord list
//    return true if chord is recognized
//---------------------------------------------------------

const std::vector<const ChordDescription *> Harmony::parse_harmony(const String &ss, bool syntax_only) {
    // pre-process for parentheses
    String s = ss.simplified();
    if (s.starts_with('(')) {
        set_parentheses_mode(right_paren() ? ParenthesesMode::BOTH : ParenthesesMode::LEFT, true, false);
        s.remove(0, 1);
    } else {
        set_parentheses_mode(right_paren() ? ParenthesesMode::RIGHT : ParenthesesMode::NONE, true, false);
    }

    if (s.ends_with(')') && s.count('(') < s.count(')')) {
        set_parentheses_mode(left_paren() ? ParenthesesMode::BOTH : ParenthesesMode::RIGHT, true, false);
        s.remove(s.size() - 1, 1);
    } else {
        set_parentheses_mode(left_paren() ? ParenthesesMode::LEFT : ParenthesesMode::NONE, true, false);
    }

    if (parentheses_mode() == ParenthesesMode::BOTH) {
        s = s.simplified(); // in case of spaces inside parentheses
    }

    muse::delete_all(chords_);
    chords_.clear();

    std::vector<const ChordDescription *> descriptions;
    if (s.is_empty()) {
        return descriptions;
    }

    StringList chords = s.split('|');

    for (const String &sub_chord : chords) {
        if (sub_chord.empty()) {
            continue;
        }
        HarmonyInfo *info = new HarmonyInfo(score());
        const ChordDescription *cd = parse_single_harmony(sub_chord, info, syntax_only);
        descriptions.push_back(cd);
        chords_.push_back(info);
    }

    return descriptions;
}

const ChordDescription *Harmony::parse_single_harmony(const String &ss, HarmonyInfo *info, bool syntax_only) {
    String s = ss.simplified();

    if (harmony_type_ == HarmonyType::ROMAN) {
        info->set_text_name(s);
        info->set_root_tpc(Tpc::TPC_INVALID);
        info->set_bass_tpc(Tpc::TPC_INVALID);
        return nullptr;
    }

    bool use_literal = false;
    if (ss.ends_with(' ')) {
        use_literal = true;
    }

    // pre-process for lower case minor chords
    bool prefer_minor;
    if (style().style_b(Sid::LOWER_CASE_MINOR_CHORDS) && s.at(0).is_lower()) {
        prefer_minor = true;
    } else {
        prefer_minor = false;
    }

    size_t idx = 0;
    const Staff *st = staff();
    Key key = st ? st->key(tick()) : Key::INVALID;
    NoteSpellingType spelling = style().style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();

    int r = Tpc::TPC_INVALID;
    if (harmony_type_ == HarmonyType::STANDARD) {
        r = convert_note(s, spelling, root_case_, idx);
    } else if (harmony_type_ == HarmonyType::NASHVILLE) {
        r = function2_tpc(s, key, idx);
    }
    if (!tpc_is_valid(r)) {
        if (s.at(0) == '/') {
            idx = 0;
        } else {
            LOGD("failed <%s>", muPrintable(ss));
            info->set_text_name(s);
            return 0;
        }
    }
    info->set_root_tpc(r);
    info->set_bass_tpc(Tpc::TPC_INVALID);
    size_t slash = s.last_index_of(u'/');
    if (slash != muse::nidx) {
        String bs = s.mid(slash + 1).simplified();
        s = s.mid(idx, slash - idx).simplified();
        size_t idx2 = 0;
        if (harmony_type_ == HarmonyType::STANDARD) {
            info->set_bass_tpc(convert_note(bs, spelling, bass_case_, idx2));
        } else if (harmony_type_ == HarmonyType::NASHVILLE) {
            info->set_bass_tpc(function2_tpc(bs, key, idx2));
        }

        if (idx2 != bs.size()) {
            info->set_bass_tpc(Tpc::TPC_INVALID);
        }
        if (!tpc_is_valid(info->bass_tpc())) {
            // if what follows after slash is not (just) a TPC
            // then reassemble chord and try to parse with the slash
            s = s + u"/" + bs;
        }
    } else {
        s = s.mid(idx); // don't simplify; keep leading space before extension if present
    }

    const ChordList *cl = score()->chord_list();
    const ChordDescription *cd = 0;
    if (use_literal) {
        cd = info->descr(s);
    } else {
        ParsedChord *pc = new ParsedChord();
        pc->parse(s, cl, syntax_only, prefer_minor);
        // parser prepends "=" to name of implied minor chords
        // use this here as well
        if (prefer_minor) {
            s = pc->name();
        }
        // look up to see if we already have a descriptor (chord has been used before)
        cd = info->descr(s, pc);
        info->set_parsed_chord(pc);
    }
    if (cd) {
        // descriptor found; use its information
        info->set_id(cd->id);
        if (!cd->names.empty()) {
            info->set_text_name(cd->names.front());
        }
    } else {
        // no descriptor yet; just set textname
        // we will generate descriptor later if necessary (when we are done editing this chord)
        info->set_text_name(s);
    }
    return cd;
}

NoteCaseType Harmony::root_render_case(HarmonyInfo *info) const {
    // case
    // always use case as typed if automatic capitalization is off
    NoteCaseType note_case = root_case_;
    if (!style().style_b(Sid::AUTOMATIC_CAPITALIZATION)) {
        return note_case;
    }

    // set default
    if (style().style_b(Sid::ALL_CAPS_NOTE_NAMES)) {
        note_case = NoteCaseType::UPPER;
    } else {
        note_case = NoteCaseType::CAPITAL;
    }

    // override for minor chords
    if (style().style_b(Sid::LOWER_CASE_MINOR_CHORDS)) {
        const ChordDescription *cd = info->descr();
        String quality;
        if (cd) {
            // use chord description if possible
            // this is the usual case
            quality = cd->quality();
        } else if (info->get_parsed_chord()) {
            // this happens on load of new chord list
            // for chord symbols that were added/edited since the score was loaded
            // or read aloud with screenreader
            // parsed form is usable even if out of date with respect to chord list
            quality = info->get_parsed_chord()->quality();
        } else {
            // this happens on load of new chord list
            // for chord symbols that have not been edited since the score was loaded
            // we need to parse this chord for now to determine quality
            // but don't keep the parsed form around as we're not ready for it yet
            quality = info->get_parsed_chord()->quality();
            delete info->parsed_chord();
            info->set_parsed_chord(nullptr);
        }
        if (quality == "minor" || quality == "diminished" || quality == "half-diminished") {
            note_case = NoteCaseType::LOWER;
        }
    }

    return note_case;
}

NoteCaseType Harmony::bass_render_case() const {
    // case
    // always use case as typed if automatic capitalization is off
    NoteCaseType note_case = bass_case_;
    if (!style().style_b(Sid::AUTOMATIC_CAPITALIZATION)) {
        return note_case;
    }

    // set default
    if (style().style_b(Sid::ALL_CAPS_NOTE_NAMES)) {
        note_case = NoteCaseType::UPPER;
    } else {
        note_case = NoteCaseType::CAPITAL;
    }

    // override for bass note
    if (style().style_b(Sid::LOWER_CASE_BASS_NOTES)) {
        note_case = NoteCaseType::LOWER;
    }

    return note_case;
}

//---------------------------------------------------------
//   endEdit
//---------------------------------------------------------

bool Harmony::is_playable() const { return !is_in_fret_box(); }

//---------------------------------------------------------
//   setHarmony
//---------------------------------------------------------

void Harmony::set_harmony(const String &s) {
    realized_harmony_.set_dirty(true);

    std::vector<const ChordDescription *> descriptions = parse_harmony(s);
    for (size_t i = 0; i < chords_.size(); i++) {
        HarmonyInfo *info = chords_.at(i);
        const ChordDescription *cd = i < descriptions.size() ? descriptions.at(i) : nullptr;

        if (!cd && info->get_parsed_chord()->parseable()) {
            // our first time encountering this chord
            // generate a descriptor and use it
            cd = info->generate_description();
            info->set_id(cd->id);
        }
        if (!cd) {
            // unparseable chord or roman numeral, render as plain text
            info->set_root_tpc(Tpc::TPC_INVALID);
            info->set_bass_tpc(Tpc::TPC_INVALID);
            info->set_id(-1);
        }
    }
}

//---------------------------------------------------------
//   baseLine
//---------------------------------------------------------

double Harmony::base_line() const {
    if (ldata()->render_item_list().empty() || !ldata()->baseline.has_value()) {
        return TextBase::base_line();
    }

    return ldata()->baseline.value();
}

//---------------------------------------------------------
//   text
//---------------------------------------------------------

String HDegree::text() const {
    if (type_ == HDegreeType::UNDEF) {
        return String();
    }
    String degree;
    switch (type_) {
        case HDegreeType::UNDEF: break;
        case HDegreeType::ADD: degree = u"add"; break;
        case HDegreeType::ALTER: degree = u"alt"; break;
        case HDegreeType::SUBTRACT: degree = u"sub"; break;
    }

    switch (alter_) {
        case -1: degree += u"b"; break;
        case 1: degree += u"#"; break;
        default: break;
    }
    String s = String::number(value_);
    String ss = degree + s;
    return ss;
}

//---------------------------------------------------------
//   findInSeg
///   find a Harmony in a given segment on the same track as this harmony.
///
///   returns 0 if there is none
//---------------------------------------------------------

Harmony *Harmony::find_in_seg(Segment *seg) const {
    // Find harmony as parent of fret diagram on same track
    EngravingItem *fde = seg->find_annotation(ElementType::FRET_DIAGRAM, track(), track());
    if (fde) {
        FretDiagram *fd = to_fret_diagram(fde);
        if (fd->harmony()) {
            return to_harmony(fd->harmony());
        }
    }

    // Find harmony on same track
    EngravingItem *e = seg->find_annotation(ElementType::HARMONY, track(), track());
    if (e) {
        return to_harmony(e);
    }
    return nullptr;
}

//---------------------------------------------------------
//   findNext
///   find the next Harmony in the score
///
///   returns 0 if there is none
//---------------------------------------------------------

Harmony *Harmony::find_next() const {
    const Segment *segment = get_parent_seg();
    Segment *cur = segment ? segment->next1() : nullptr;
    while (cur) {
        Harmony *h = find_in_seg(cur);
        if (h) {
            return h;
        }
        cur = cur->next1();
    }
    return nullptr;
}

//---------------------------------------------------------
//   findPrev
///   find the previous Harmony in the score
///
///   returns 0 if there is none
//---------------------------------------------------------

Harmony *Harmony::find_prev() const {
    const Segment *segment = get_parent_seg();
    Segment *cur = segment ? segment->prev1() : nullptr;
    while (cur) {
        Harmony *h = find_in_seg(cur);
        if (h) {
            return h;
        }
        cur = cur->prev1();
    }
    return nullptr;
}

//---------------------------------------------------------
//   ticksTillNext
///   finds ticks until the next chord symbol or end of score
///
///   utick is our current playback position to start from
///
///   stopAtMeasureEnd being set to true will have the loop
///   stop at measure end.
//---------------------------------------------------------
Fraction Harmony::ticks_till_next(int utick, bool stop_at_measure_end) const {
    const Segment *seg = get_parent_seg();
    if (!seg) {
        return Fraction(-1, 1);
    }

    Fraction duration = seg->ticks();

    const RepeatList &repeats = score()->repeat_list();
    auto rs_it = repeats.find_repeat_segment_from_u_tick(utick);
    if (rs_it == repeats.cend()) {
        return duration;
    }

    Segment *cur = seg->next();
    Measure const *current_measure = seg->measure();
    Measure const *end_measure = (stop_at_measure_end) ? current_measure : (*rs_it)->last_measure();
    Harmony const *next_harmony = nullptr;

    do {
        // Loop over segments of this measure
        while (cur) {
            //find harmony on same track
            EngravingItem *e = cur->find_annotation(ElementType::HARMONY, track(), track());
            if (e) {
                next_harmony = to_harmony(e);
            } else {
                // no harmony; look for fret diagram
                e = cur->find_annotation(ElementType::FRET_DIAGRAM, track(), track());
                if (e) {
                    next_harmony = to_fret_diagram(e)->harmony();
                }
            }
            if (next_harmony != nullptr) {
                //we have found the next chord symbol
                break;
            }
            //keep adding the duration of the current segment
            //in case we are not able to find a next
            //chord symbol
            duration += cur->ticks();
            cur = cur->next();
        }
        // Move segment reference to next measure
        if (current_measure != end_measure) {
            current_measure = current_measure->next_measure();
            if (current_measure && current_measure->mm_rest()) {
                duration += current_measure->mm_rest()->ticks();
                current_measure = current_measure->mm_rest()->next_measure();
            }
            cur = (current_measure) ? current_measure->first() : nullptr;
        } else {
            // End of repeatSegment or search boundary reached
            if (stop_at_measure_end) {
                break;
            } else if (!next_harmony) {
                // move to next RepeatSegment
                if (++rs_it != repeats.end()) {
                    current_measure = (*rs_it)->first_measure();
                    end_measure = (*rs_it)->last_measure();
                    cur = current_measure->first();
                }
            }
        }
    } while ((next_harmony == nullptr) && (cur != nullptr));

    if (next_harmony && rs_it != repeats.end()) {
        int tick_offset = (*rs_it)->utick - (*rs_it)->tick;
        int next_harmony_utick = next_harmony->tick().ticks() + tick_offset;
        duration = Fraction::from_ticks(next_harmony_utick - utick);
    }

    return duration;
}

//---------------------------------------------------------
//   getRealizedHarmony
//    get realized harmony or create one for the current symbol
//    also updates the realized harmony and accounts for
//    transposition. RealizedHarmony objects cannot be cached
//    since the notes generated depends on context rather than
//    just root, bass, chord symbol, and voicing.
//---------------------------------------------------------

const RealizedHarmony &Harmony::get_realized_harmony() const {
    const Staff *st = staff();
    IF_ASSERT_FAILED(st) { return realized_harmony_; }

    const Fraction tick = this->tick();
    const CapoParams &capo = st->capo(tick);

    int offset = 0;
    if (capo.active && CapoParams::TransposeMode::TAB_ONLY != capo.transpose_mode) {
        offset = capo.fret_position;
    }

    Interval interval = origin_part()->instrument(tick)->transpose();
    if (!style().style_b(Sid::CONCERT_PITCH)) {
        offset += interval.chromatic;
    }

    HarmonyInfo *info = chords_.empty() ? nullptr : chords_.front();
    int root = info ? info->root_tpc() : Tpc::TPC_INVALID;
    int bass = info ? info->bass_tpc() : Tpc::TPC_INVALID;

    realized_harmony_.update(root, bass, offset);

    return realized_harmony_;
}

//---------------------------------------------------------
//   realizedHarmony
//    get realized harmony or create one for the current symbol
//    without updating the realized harmony
//---------------------------------------------------------

RealizedHarmony &Harmony::realized_harmony() { return realized_harmony_; }

const ParsedChord *Harmony::parsed_form() const {
    if (chords_.empty()) {
        return nullptr;
    }
    return chords_.front()->get_parsed_chord();
}

Color Harmony::cur_color(const rendering::PaintOptions &opt) const {
    if (!opt.is_printing && is_misspelled_) {
        return configuration()->critical_color();
    }

    return EngravingItem::cur_color(opt);
}

void Harmony::set_color(const Color &color) {
    EngravingItem::set_color(color);

    for (HarmonyRenderItem *item : ldata()->render_item_list()) {
        if (ChordSymbolParen *paren = dynamic_cast<ChordSymbolParen *>(item)) {
            paren->paren_item->set_color(color);
        }
    }
}

//---------------------------------------------------------
//   width
//---------------------------------------------------------

double TextSegment::width() const { return FontMetrics::width(font_, text_); }

double TextSegment::cap_height() const { return FontMetrics::cap_height(font_); }

RectF TextSegment::tight_bounding_rect() const { return FontMetrics::tight_bounding_rect(font_, text_); }

RectF TextSegment::bounding_rect() const { return FontMetrics::bounding_rect(font_, text_); }

void TextSegment::set_font(const muse::draw::Font &f) {
    font_ = f;
    if (f.type() != Font::Type::MUSIC_SYMBOL_TEXT) {
        return;
    }

    // Check all symbols in string are available in this font
    FontMetrics fm(f);
    bool fail = false;
    for (size_t i = 0; i < text_.size(); ++i) {
        const Char &c = text_.at(i);
        if (c.is_high_surrogate()) {
            if (i + 1 == text_.size()) {
                ASSERT_X("bad string");
            }
            const Char &c2 = text_.at(i + 1);
            ++i;
            char32_t v = Char::surrogate_to_ucs4(c, c2);
            if (!fm.in_font(v)) {
                fail = true;
                break;
            }
        } else {
            if (!fm.in_font(c.unicode())) {
                fail = true;
                break;
            }
        }
    }
    // Fallback to default musical text font
    if (fail) {
        font_.set_family(String::from_utf8(fallback_symboltext_font), Font::Type::MUSIC_SYMBOL_TEXT);
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void Harmony::spatium_changed(double old_value, double new_value) { TextBase::spatium_changed(old_value, new_value); }

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Harmony::local_spatium_changed(double old_value, double new_value) { TextBase::local_spatium_changed(old_value, new_value); }

//---------------------------------------------------------
//   addDegree
//---------------------------------------------------------

void Harmony::add_degree(const HDegree &d) { degree_list_.push_back(d); }

//---------------------------------------------------------
//   degreeList
//---------------------------------------------------------

const std::vector<HDegree> &Harmony::degree_list() const { return degree_list_; }

//---------------------------------------------------------
//   setHarmonyType
//---------------------------------------------------------

void Harmony::set_harmony_type(HarmonyType val) {
    harmony_type_ = val;
    set_placement(property_default(Pid::PLACEMENT).value<PlacementV>());
    switch (harmony_type_) {
        case HarmonyType::STANDARD: init_text_style_type(TextStyleType::HARMONY_A); break;
        case HarmonyType::ROMAN: init_text_style_type(TextStyleType::HARMONY_ROMAN); break;
        case HarmonyType::NASHVILLE: init_text_style_type(TextStyleType::HARMONY_NASHVILLE); break;
    }
    // TODO: convert text
}

//---------------------------------------------------------
//   typeUserName
//---------------------------------------------------------

TranslatableString Harmony::type_user_name() const {
    switch (harmony_type_) {
        case HarmonyType::ROMAN: return TranslatableString("engraving", "Roman numeral");
        case HarmonyType::NASHVILLE: return TranslatableString("engraving", "Nashville number");
        case HarmonyType::STANDARD: break;
    }
    return EngravingItem::type_user_name();
}

//---------------------------------------------------------
//   generateScreenReaderInfo
//---------------------------------------------------------

String Harmony::generate_screen_reader_info() const {
    String rez;
    for (size_t i = 0; i < chords_.size(); i++) {
        HarmonyInfo *info = chords_.at(i);
        if (!tpc_is_valid(info->root_tpc())) {
            continue;
        }
        if (i != 0) {
            rez += u" | ";
        }

        switch (harmony_type_) {
            case HarmonyType::ROMAN: {
                if (chords_.empty()) {
                    return u"";
                }
                String aux = info->text_name();
                bool has_upper = aux.contains(u'I') || aux.contains(u'V');
                bool has_lower = aux.contains(u'i') || aux.contains(u'v');
                if (has_lower && !has_upper) {
                    rez = String(u"%1 %2").arg(rez, muse::mtrc("engraving", "lower case"));
                }
                aux = aux.to_lower();
                static const std::vector<std::pair<String, String>> rna_replacements{
                    {u"vii", u"7"}, {u"vi", u"6"}, {u"iv", u"4"}, {u"v", u"5"}, {u"iii", u"3"}, {u"ii", u"2"}, {u"i", u"1"},
                };
                static const std::vector<std::pair<String, String>> symbol_replacements{
                    {u"bb", u"𝄫"}, {u"##", u"𝄪"},
                    {u"h", u"♮"},  {u"\\♮", u"h"}, // \h should be h, so need to correct replacing in the previous step
                    {u"#", u"♯"},  {u"\\♯", u"#"}, // \# should be #, so need to correct replacing in the previous step
                    {u"b", u"♭"},  {u"\\♭", u"b"}, // \b should be b, so need to correct replacing in the previous step
                    // TODO: use SMuFL glyphs and translate
                    //{ "o", ""},
                    //{ "0", ""},
                    //{ "\+", ""},
                    //{ "\^", ""},
                };
                for (auto const &r : rna_replacements) {
                    aux.replace(r.first, r.second);
                }
                for (auto const &r : symbol_replacements) {
                    aux.replace(r.first, r.second);
                }
                // construct string one  character at a time
                for (size_t j = 0; j < aux.size(); ++j) {
                    rez = String(u"%1 %2").arg(rez).arg(aux.at(j));
                }
            }
                return rez;
            case HarmonyType::NASHVILLE: {
                const Staff *st = staff();
                Key key = st ? st->key(tick()) : Key::INVALID;
                rez = String(u"%1 %2").arg(rez, tpc2_function(info->root_tpc(), key));
                break;
            }
            case HarmonyType::STANDARD:
            default: rez = String(u"%1 %2").arg(rez, tpc2name(info->root_tpc(), NoteSpellingType::STANDARD, NoteCaseType::AUTO, true));
        }

        if (!info->text_name().is_empty()) {
            String aux = info->get_parsed_chord()->handle();
            aux = aux.replace(u"#", u"♯").replace(u"<", u"");
            String extension;

            for (String s : aux.split(u'>', muse::SKIP_EMPTY_PARTS)) {
                if (!s.contains(u"blues")) {
                    s.replace(u"b", u"♭");
                }
                extension += s + u' ';
            }
            rez = String(u"%1 %2").arg(rez, extension);
        } else {
            rez = String(u"%1 %2").arg(rez, info->text_name());
        }

        if (tpc_is_valid(info->bass_tpc())) {
            rez = String(u"%1 / %2").arg(rez, tpc2name(info->bass_tpc(), NoteSpellingType::STANDARD, NoteCaseType::AUTO, true));
        }
    }

    return rez;
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Harmony::accept_drop(EditData &data) const {
    EngravingItem *e = data.drop_element;
    if (e->is_fret_diagram()) {
        return true;
    } else if (e->is_symbol() || e->is_f_symbol()) {
        // symbols can be added in edit mode
        if (data.get_data(this)) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Harmony::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;
    if (e->is_fret_diagram()) {
        FretDiagram *fd = to_fret_diagram(e);
        fd->set_ownership_parent(ownership_parent());
        fd->set_track(track());
        score()->undo_add_element(fd);
    } else if (e->is_symbol() || e->is_f_symbol()) {
        TextBase::drop(tx, data);
        renderer()->layout_text1(this);
        e = 0; // cannot select
    } else {
        LOGW("Harmony: cannot drop <%s>\n", e->type_name());
        delete e;
        e = 0;
    }
    return e;
}

void Harmony::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::FONT_STYLE || id == Pid::FONT_FACE || id == Pid::FONT_SIZE) {
        EngravingItem::undo_change_property(id, v, ps);
    } else if (id == Pid::EXCLUDE_VERTICAL_ALIGN) {
        TextBase::undo_change_property(id, v, ps);

        bool val = v.to_bool();
        FretDiagram *fd = get_parent_fret_diagram();
        if (fd && fd->exclude_vertical_align() != val) {
            fd->undo_change_property(Pid::EXCLUDE_VERTICAL_ALIGN, val, ps);
        }
        Segment *parent_seg = get_parent_seg();
        if (!parent_seg) {
            return;
        }
        for (EngravingItem *item : parent_seg->annotations()) {
            if ((!item->is_fret_diagram() && !item->is_harmony()) || item == this || track2staff(item->track()) != staff_idx()) {
                continue;
            }

            if (item->exclude_vertical_align() != val) {
                item->undo_change_property(Pid::EXCLUDE_VERTICAL_ALIGN, val, ps);
            }
        }
        return;
    }

    TextBase::undo_change_property(id, v, ps);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Harmony::get_property(Pid pid) const {
    switch (pid) {
        case Pid::PLAY: return PropertyValue(play_);
        case Pid::HARMONY_TYPE: return PropertyValue(int(harmony_type_));
        case Pid::HARMONY_BASS_SCALE: return bass_scale_;
        case Pid::HARMONY_VOICE_LITERAL: return realized_harmony_.literal();
        case Pid::HARMONY_VOICING: return int(realized_harmony_.voicing());
        case Pid::HARMONY_DURATION: return int(realized_harmony_.duration());
        case Pid::HARMONY_DO_NOT_STACK_MODIFIERS: return do_not_stack_modifiers_;
        case Pid::FONT_SIZE: return font_size_;
        case Pid::FONT_STYLE: return static_cast<int>(font_style_);
        case Pid::FONT_FACE: return font_family_;
        default: return TextBase::get_property(pid);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Harmony::set_property(Pid pid, const PropertyValue &v) {
    switch (pid) {
        case Pid::PLAY: play_ = v.to_bool(); break;
        case Pid::HARMONY_TYPE: set_harmony_type(HarmonyType(v.to_int())); break;
        case Pid::HARMONY_BASS_SCALE: set_bass_scale(v.to_double()); break;
        case Pid::HARMONY_VOICE_LITERAL: realized_harmony_.set_literal(v.to_bool()); break;
        case Pid::HARMONY_VOICING: realized_harmony_.set_voicing(Voicing(v.to_int())); break;
        case Pid::HARMONY_DURATION: realized_harmony_.set_duration(HDuration(v.to_int())); break;
        case Pid::EXCLUDE_VERTICAL_ALIGN: {
            set_exclude_vertical_align(v.to_bool());
            break;
        }
        case Pid::HARMONY_DO_NOT_STACK_MODIFIERS: do_not_stack_modifiers_ = v.to_bool(); break;
        case Pid::TEXT: {
            String cur_text = xml_text();
            TextBase::set_property(pid, v);
            set_harmony(v.value<String>());
            String new_text = xml_text();
            if (new_text != cur_text) {
                FretDiagram *fret_diagram = ownership_parent()->is_fret_diagram() ? to_fret_diagram(ownership_parent()) : nullptr;
                if (fret_diagram && !fret_diagram->is_custom(cur_text) && configuration()->auto_update_fretboard_diagrams()) {
                    fret_diagram->update_diagram(plain_text());
                }
                score()->rebuild_fret_box();
            }
            break;
        }
        default: return TextBase::set_property(pid, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Harmony::property_default(Pid id) const {
    PropertyValue v;
    switch (id) {
        case Pid::HARMONY_TYPE: v = int(HarmonyType::STANDARD); break;
        case Pid::TEXT_STYLE: {
            switch (harmony_type_) {
                case HarmonyType::STANDARD: v = TextStyleType::HARMONY_A; break;
                case HarmonyType::ROMAN: v = TextStyleType::HARMONY_ROMAN; break;
                case HarmonyType::NASHVILLE: v = TextStyleType::HARMONY_NASHVILLE; break;
            }
        } break;
        case Pid::HARMONY_BASS_SCALE: v = style().style_v(Sid::CHORD_BASS_NOTE_SCALE).to_double(); break;
        case Pid::HARMONY_DO_NOT_STACK_MODIFIERS: return false;
        case Pid::PLAY: v = true; break;
        // fall-through
        default: v = TextBase::property_default(id); break;
    }
    return v;
}

bool Harmony::position_relative_to_notehead_rest() const { return !parent()->is_fret_diagram(); }

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid Harmony::get_property_style(Pid pid) const {
    if (pid == Pid::PLACEMENT) {
        switch (harmony_type_) {
            case HarmonyType::STANDARD: return Sid::HARMONY_PLACEMENT;
            case HarmonyType::ROMAN: return Sid::ROMAN_NUMERAL_PLACEMENT;
            case HarmonyType::NASHVILLE: return Sid::NASHVILLE_NUMBER_PLACEMENT;
        }
    }
    return TextBase::get_property_style(pid);
}

double Harmony::mag() const { return EngravingItem::mag(); }

HarmonyInfo::HarmonyInfo(const HarmonyInfo &h) {
    id_ = h.id_;
    bass_tpc_ = h.bass_tpc_;
    root_tpc_ = h.root_tpc_;
    text_name_ = h.text_name_;
    score_ = h.score_;
    parsed_chord_ = h.parsed_chord_ ? new ParsedChord(*h.parsed_chord_) : 0;
}

HarmonyInfo::~HarmonyInfo() {
    if (parsed_chord_) {
        delete parsed_chord_;
    }
}
} // namespace mu::engraving
