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

#include "fret.h"

#include "../editing/addremoveelement.h"
#include "../editing/editfretboarddiagram.h"
#include "anchors.h"
#include "chord.h"
#include "factory.h"
#include "harmony.h"
#include "io/file.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "pitchspelling.h"
#include "rw/harmonytodiagramreader.h"
#include "rw/read460/tread.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stringdata.h"
#include "system.h"
#include "translation.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//    parent() is Segment or Box
//

//---------------------------------------------------------
//   fretStyle
//---------------------------------------------------------

static const ElementStyle fret_style{
    {Sid::FRET_NUM_POS, Pid::FRET_NUM_POS},
    {Sid::FRET_MAG, Pid::MAG},
    {Sid::FRET_PLACEMENT, Pid::PLACEMENT},
    {Sid::FRET_STRINGS, Pid::FRET_STRINGS},
    {Sid::FRET_FRETS, Pid::FRET_FRETS},
    {Sid::FRET_NUT, Pid::FRET_NUT},
    {Sid::FRET_MIN_DISTANCE, Pid::MIN_DISTANCE},
    {Sid::FRET_ORIENTATION, Pid::ORIENTATION},
    {Sid::FRET_SHOW_FINGERINGS, Pid::FRET_SHOW_FINGERINGS},
};

//---------------------------------------------------------
//   FretDiagram
//---------------------------------------------------------

struct HarmonyMapKey {
    HarmonyMapKey() {}

    HarmonyMapKey(int k, int r, int b) : keys(k), root_tpc(r), bass_tpc(b) {}

    int keys = 0;
    int root_tpc = Tpc::TPC_INVALID;
    int bass_tpc = Tpc::TPC_INVALID;

    bool operator<(const HarmonyMapKey &other) const {
        return std::tie(keys, root_tpc, bass_tpc) < std::tie(other.keys, other.root_tpc, other.bass_tpc);
    }

    bool operator==(const HarmonyMapKey &other) const {
        return std::tie(keys, root_tpc, bass_tpc) == std::tie(other.keys, other.root_tpc, other.bass_tpc);
    }
};

static std::map<HarmonyMapKey /*key*/, std::vector<DiagramInfo>> s_harmony_to_diagram_map;
static std::unordered_map<String /*pattern*/, std::vector<String /*harmonyName*/>> s_diagram_pattern_to_harmonies_map;

static const muse::io::PathT harmony_to_diagram_file_path(":/data/harmony_to_diagram.xml");

static int canonical_enharmonic_tpc(int tpc) {
    // Normalize to one canonical TPC per pitch class
    int mod = (tpc - Tpc::TPC_G_B) % ::mu::engraving::tpc_delta_enharmonic;
    return Tpc::TPC_G_B + (mod < 0 ? mod + ::mu::engraving::tpc_delta_enharmonic : mod);
}

static HarmonyMapKey create_harmony_map_key(const String &harmony, const NoteSpellingType &spelling_type, const ChordList *cl) {
    String s = harmony;
    NoteCaseType note_case;
    size_t idx;
    int root_tpc = convert_note(s, spelling_type, note_case, idx);

    int bass_tpc = Tpc::TPC_INVALID;
    size_t slash = s.last_index_of(u'/');
    if (slash != muse::nidx) {
        String bs = s.mid(slash + 1).simplified();
        s = s.mid(idx, slash - idx).simplified();
        size_t idx2 = 0;
        bass_tpc = convert_note(bs, spelling_type, note_case, idx2);

        if (!tpc_is_valid(bass_tpc)) {
            // if what follows after slash is not (just) a TPC
            // then reassemble chord and try to parse with the slash
            s = s + u"/" + bs;
        }
    } else {
        s = s.mid(idx);
    }

    ParsedChord chord;
    if (!chord.parse(s, cl)) {
        LOGE() << "Error parse " << harmony;
    }

    int keys = chord.keys();

    return HarmonyMapKey(keys, root_tpc, bass_tpc);
}

static DiagramInfo resolve_diagram(const std::vector<DiagramInfo> &diagrams) {
    auto is_simpler = [](const DiagramInfo &a, const DiagramInfo &b) {
        const String &harmony_name_a = a.harmony_name;
        const String &harmony_name_b = b.harmony_name;

        if (harmony_name_a.size() != harmony_name_b.size()) {
            return harmony_name_a.size() < harmony_name_b.size();
        }

        return harmony_name_a < harmony_name_b;
    };

    DiagramInfo result = diagrams.front();
    for (size_t i = 1; i < diagrams.size(); ++i) {
        if (!is_simpler(result, diagrams[i])) {
            result = diagrams[i];
        }
    }

    return result;
}

FretDiagram::FretDiagram(DummyParentOr<Segment> parent)
    : EngravingItem(ElementType::FRET_DIAGRAM, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_default_values();

    init_element_style(&::mu::engraving::fret_style);
}

FretDiagram::FretDiagram(const FretDiagram &f) : EngravingItem(f) {
    strings_ = f.strings_;
    frets_ = f.frets_;
    fret_offset_ = f.fret_offset_;
    max_frets_ = f.max_frets_;
    user_mag_ = f.user_mag_;
    dots_ = f.dots_;
    markers_ = f.markers_;
    barres_ = f.barres_;
    show_nut_ = f.show_nut_;
    orientation_ = f.orientation_;
    show_fingering_ = f.show_fingering_;
    fingering_ = f.fingering_;

    if (f.harmony_) {
        Harmony *h = new Harmony(*f.harmony_);
        add(h);
    }
}

void FretDiagram::init_default_values() {
    strings_ = 6;
    frets_ = 4;
    fret_offset_ = 0;
    max_frets_ = 24;
    show_nut_ = true;
    orientation_ = Orientation::VERTICAL;

    user_mag_ = 1.0;

    show_fingering_ = false;
    fingering_ = std::vector<int>(strings_, 0);
}

FretDiagram::~FretDiagram() {
    if (harmony_) {
        delete harmony_;
    }
}

//---------------------------------------------------------
//   linkedClone
//---------------------------------------------------------

EngravingItem *FretDiagram::linked_clone() {
    FretDiagram *e = clone();
    e->set_autoplace(true);
    if (harmony_) {
        EngravingItem *new_harmony = harmony_->linked_clone();
        e->add(new_harmony);
    }
    score()->undo(new Link(e, this));
    return e;
}

Segment *FretDiagram::segment() const {
    EngravingObject *parent = ownership_parent();
    if (!parent || !parent->is_segment()) {
        return nullptr;
    }

    return to_segment(ownership_parent());
}

void FretDiagram::update_diagram(const String &harmony_name) {
    std::vector<DiagramInfo> available_diagrams = patterns_from_harmony(harmony_name);
    if (available_diagrams.empty()) {
        return;
    }

    String diagram_xml = resolve_diagram(available_diagrams).diagram_xml;

    if (diagram_xml.empty()) {
        return;
    }

    clear();

    read460::ReadContext ctx(score());
    XmlReader reader(diagram_xml.to_utf8());

    read460::TRead::read(this, reader, ctx);

    trigger_layout();
    return;
}

double FretDiagram::main_width() const {
    double main_width = 0.0;
    if (orientation() == Orientation::VERTICAL) {
        main_width = ldata()->string_dist * (strings() - 1);
    } else if (orientation() == Orientation::HORIZONTAL) {
        main_width = ldata()->fret_dist * frets();
    }
    return main_width;
}

//---------------------------------------------------------
//   setStrings
//---------------------------------------------------------

void FretDiagram::set_strings(int n) {
    int difference = n - strings_;
    if (difference == 0 || n <= 0) {
        return;
    }

    // Move all dots, makers, barres to the RIGHT, so we add strings to the left
    // This is more useful - few instruments need strings added to the right.
    DotMap temp_dots;
    MarkerMap temp_markers;

    for (int string = 0; string < strings_; ++string) {
        if (string + difference < 0) {
            continue;
        }

        for (auto const &d : dot(string)) {
            if (d.exists()) {
                temp_dots[string + difference].push_back(FretItem::Dot(d));
            }
        }

        if (marker(string).exists()) {
            temp_markers[string + difference] = marker(string);
        }
    }

    dots_ = temp_dots;
    markers_ = temp_markers;

    for (int fret = 1; fret <= frets_; ++fret) {
        if (barre(fret).exists()) {
            if (barres_[fret].start_string + difference <= 0) {
                remove_barre(fret);
                continue;
            }

            barres_[fret].start_string = std::max(0, barres_[fret].start_string + difference);
            barres_[fret].end_string = barres_[fret].end_string == -1 ? -1 : barres_[fret].end_string + difference;
        }
    }

    if (difference > 0) {
        for (int i = 0; i < difference; ++i) {
            fingering_.push_back(0);
        }
    } else {
        fingering_.erase(fingering_.end() + difference, fingering_.end());
    }

    strings_ = n;
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void FretDiagram::init(StringData *string_data, Chord *chord) {
    if (!string_data) {
        set_strings(6);
    } else {
        set_strings(static_cast<int>(string_data->strings()));
    }
    if (string_data) {
        for (int string = 0; string < strings_; ++string) {
            set_marker(string, FretMarkerType::CROSS);
        }
        for (const Note *note : chord->notes()) {
            int string;
            int fret;
            if (string_data->convert_pitch(note->pitch(), chord->staff(), &string, &fret)) {
                set_dot(string, fret);
            }
        }
        frets_ = string_data->frets();
    } else {
        max_frets_ = 6;
    }
}

//---------------------------------------------------------
//   setDot
//    take a fret value of 0 to mean remove the dot, except with add
//    where we actually need to pass a fret val.
//---------------------------------------------------------

void FretDiagram::set_dot(int string, int fret, bool add /*= false*/, FretDotType dtype /*= FretDotType::NORMAL*/) {
    if (fret == 0) {
        remove_dot(string, fret);
    } else if (string >= 0 && string < strings_) {
        // Special case - with add, if there is a dot in the position, remove it
        // If not, add it.
        if (add) {
            if (dot(string, fret)[0].exists()) {
                remove_dot(string, fret);
                return; // We are done here, all we needed to do was remove a single dot
            }
        } else {
            dots_[string].clear();
        }

        dots_[string].push_back(FretItem::Dot(fret, dtype));
        if (!add) {
            set_marker(string, FretMarkerType::NONE);
        }
    }
}

void FretDiagram::add_dot_for_dot_style_barre(int string, int fret) {
    if (dots_[string].empty()) {
        dots_[string].push_back(FretItem::Dot(fret, FretDotType::NORMAL, /*isPartOfSlurBarre*/ true));
    }
}

void FretDiagram::remove_dot_for_dot_style_barre(int string, int fret) { remove_dot(string, fret); }

//---------------------------------------------------------
//   setMarker
//    Removal of dots and barres if "Multiple dots" is inactive
//    is handled in FretCanvas::mousePressEvent()
//---------------------------------------------------------

void FretDiagram::set_marker(int string, FretMarkerType mtype) {
    if (string >= 0 && string < strings_) {
        markers_[string] = FretItem::Marker(mtype);
        if (mtype != FretMarkerType::NONE) {
            remove_dot(string);
            remove_barres(string);
        }
    }
}

//---------------------------------------------------------
//   setBarre
//    We'll accept a value of -1 for the end string, to denote
//    that the barre goes as far right as possible.
//    Take a start string value of -1 to mean 'remove this barre'
//---------------------------------------------------------

void FretDiagram::set_barre(int start_string, int end_string, int fret) {
    if (start_string == -1) {
        remove_barre(fret);
    } else if (start_string >= 0 && end_string >= -1 && start_string < strings_ && end_string < strings_) {
        barres_[fret] = FretItem::Barre(start_string, end_string);
    }
}

//---------------------------------------------------------
//    This version is for clicks on a dot with shift.
//    If there is no barre at fret, then add one with the string as the start.
//    If there is a barre with a -1 end string, set the end string to string.
//    If there is a barre with a set start and end, remove it.
//    Add may be used in the future if we decide to add dots as default with barres.
//---------------------------------------------------------

void FretDiagram::set_barre(int string, int fret, [[maybe_unused]] bool add /*= false*/) {

    FretItem::Barre b = barre(fret);
    if (!b.exists()) {
        if (string < strings_ - 1) {
            barres_[fret] = FretItem::Barre(string, -1);
            remove_dots_markers(string, -1, fret);
        }
    } else if (b.end_string == -1 && b.start_string < string) {
        barres_[fret].end_string = string;
    } else {
        remove_dots_markers(b.start_string, b.end_string, fret);
        remove_barre(fret);
    }
}

//---------------------------------------------------------
//   undoSetFretDot
//---------------------------------------------------------

void FretDiagram::undo_set_fret_dot(int input_string, int input_fret, bool input_add /*= true*/,
                                    FretDotType input_dtype /*= FretDotType::NORMAl*/) {
    for (EngravingObject *e : link_list()) {
        FretDiagram *fd = to_fret_diagram(e);
        fd->score()->undo(new FretDot(fd, input_string, input_fret, input_add, input_dtype));
    }
}

//---------------------------------------------------------
//   undoSetFretMarker
//---------------------------------------------------------

void FretDiagram::undo_set_fret_marker(int input_string, FretMarkerType input_mtype) {
    for (EngravingObject *e : link_list()) {
        FretDiagram *fd = to_fret_diagram(e);
        fd->score()->undo(new FretMarker(fd, input_string, input_mtype));
    }
}

//---------------------------------------------------------
//   undoSetFretBarre
//    add refers to using multiple dots per string when adding dots automatically
//---------------------------------------------------------

void FretDiagram::undo_set_fret_barre(int input_string, int input_fret, bool input_add /*= false*/) {
    for (EngravingObject *e : link_list()) {
        FretDiagram *fd = to_fret_diagram(e);
        fd->score()->undo(new FretBarre(fd, input_string, input_fret, input_add));
    }
}

//---------------------------------------------------------
//   removeBarre
//    Remove a barre on a given fret.
//---------------------------------------------------------

void FretDiagram::remove_barre(int f) { barres_.erase(f); }

//---------------------------------------------------------
//   removeBarres
//    Remove barres crossing a certain point. Fret of 0 means any point along
//    the string.
//---------------------------------------------------------

void FretDiagram::remove_barres(int string, int fret /*= 0*/) {
    auto iter = barres_.begin();
    while (iter != barres_.end()) {
        int bfret = iter->first;
        FretItem::Barre b = iter->second;

        if (b.exists() && b.start_string <= string && (b.end_string >= string || b.end_string == -1)) {
            if (fret > 0 && fret != bfret) {
                ++iter;
            } else {
                iter = barres_.erase(iter);
            }
        } else {
            ++iter;
        }
    }
}

//---------------------------------------------------------
//   removeMarker
//---------------------------------------------------------

void FretDiagram::remove_marker(int s) {
    auto it = markers_.find(s);
    markers_.erase(it);
}

//---------------------------------------------------------
//   removeDot
//    take a fret value of 0 to mean remove all dots
//---------------------------------------------------------

void FretDiagram::remove_dot(int s, int f /*= 0*/) {
    if (f > 0) {
        std::vector<FretItem::Dot> temp_dots;
        for (auto const &d : dot(s)) {
            if (d.exists() && d.fret != f) {
                temp_dots.push_back(FretItem::Dot(d));
            }
        }

        dots_[s] = temp_dots;
    } else {
        dots_[s].clear();
    }

    if (dots_[s].size() == 0) {
        auto it = dots_.find(s);
        dots_.erase(it);
    }
}

//---------------------------------------------------------
//   removeDotsMarkers
//    removes all markers between [ss, es] and dots between [ss, es],
//    where the dots have a fret of fret.
//---------------------------------------------------------

void FretDiagram::remove_dots_markers(int ss, int es, int fret) {
    if (ss == -1) {
        return;
    }

    int end = es == -1 ? strings_ : es;
    for (int string = ss; string <= end; ++string) {
        remove_dot(string, fret);

        if (marker(string).exists()) {
            remove_marker(string);
        }
    }
}

//---------------------------------------------------------
//   Fill diagram from string like "XO-[1-O][1-X,2-O,3-S][3-T];B1[0-5];B..."
//   - Each character or bracketed block represents a string, from lowest to highest.
//   - 'X' = muted string (cross marker)
//   - 'O' = open string (circle marker)
//   - '-' = empty or unused string
//   - [fret-type,...] = one or more fretted dots on that string:
//       • fret is absolute (already includes offset)
//       • type is:
//           - O = circle
//           - X = cross
//           - S = square
//           - T = triangle
//   - Example: [1-X,2-O,3-S] means three dots on frets 1, 2, and 3 with different types
//   - Barre chords follow after ';', in the format: B{fret}[{start}-{end}]
//     e.g. B1[0-5] = barre on fret 1 from string 0 to 5
//---------------------------------------------------------

void FretDiagram::apply_diagram_pattern(FretDiagram *diagram, const String &pattern) {
    diagram->clear();

    const std::vector<String> parts = pattern.split(';');
    if (parts.empty()) {
        return;
    }

    const String &main_part = parts[0];
    std::vector<String> string_tokens;

    for (size_t i = 0; i < main_part.size();) {
        if (main_part[i] == u'[') {
            size_t rb = main_part.index_of(u']', i);
            if (rb != muse::nidx) {
                string_tokens.push_back(main_part.mid(i, rb - i + 1));
                i = rb + 1;
                continue;
            }
        }
        string_tokens.push_back(main_part.mid(i, 1));
        ++i;
    }

    const int string_count = static_cast<int>(string_tokens.size());
    diagram->set_strings(string_count);
    diagram->set_frets(4);
    diagram->set_property_flags(Pid::FRET_STRINGS, PropertyFlags::UNSTYLED);
    diagram->set_property_flags(Pid::FRET_FRETS, PropertyFlags::UNSTYLED);

    int offset = 0;

    for (int i = 0; i < string_count; ++i) {
        const String &token = string_tokens[i];

        if (token == u"-") {
            continue;
        }

        if (token.starts_with(u"[") && token.ends_with(u"]")) {
            // Example: [1-O,2-X,3-S]
            String inner = token.mid(1, token.size() - 2);
            std::vector<String> pairs = inner.split(u',');

            for (const String &p : pairs) {
                size_t dash = p.index_of(u'-');
                if (dash > 0) {
                    int fret = p.left(dash).to_int();
                    Char type_char = p.mid(dash + 1, 1).at(0);
                    FretDotType dt;

                    switch (type_char.unicode()) {
                        case 'X': dt = FretDotType::CROSS; break;
                        case 'O': dt = FretDotType::NORMAL; break;
                        case 'S': dt = FretDotType::SQUARE; break;
                        case 'T': dt = FretDotType::TRIANGLE; break;
                        default: dt = FretDotType::NORMAL; break;
                    }

                    diagram->set_dot(i, fret - offset, true, dt);
                    if (fret - 3 > 0 && offset < fret - 3) {
                        offset = fret - 3;
                    }
                }
            }
        } else if (token == u"X") {
            diagram->set_marker(i, FretMarkerType::CROSS);
        } else if (token == u"O") {
            diagram->set_marker(i, FretMarkerType::CIRCLE);
        }
    }

    if (offset > 0) {
        diagram->set_fret_offset(offset);
    }

    for (size_t i = 1; i < parts.size(); ++i) {
        const String &barre_part = parts[i];
        if (!barre_part.starts_with(u'B')) {
            continue;
        }

        size_t lb = barre_part.index_of(u'[');
        size_t rb = barre_part.index_of(u']');
        if (lb < 2 || rb < lb) {
            continue;
        }

        int fret = barre_part.mid(1, lb - 1).to_int();
        if (fret <= 0) {
            continue;
        }

        String range = barre_part.mid(lb + 1, rb - lb - 1);
        size_t dash = range.index_of('-');
        if (dash > 0) {
            int start = range.left(dash).to_int();
            int end = range.mid(dash + 1).to_int();
            if (start >= 0 && end >= start && end < string_count) {
                diagram->set_barre(start, end, fret - offset);
            }
        }
    }
}

String FretDiagram::pattern_from_diagram() const {
    const int diagram_strings = strings();
    const int offset = fret_offset();

    StringList pattern_parts;
    const DotMap &dots_map = dots();

    for (int i = 0; i < diagram_strings; ++i) {
        const FretItem::Marker fret_marker = marker(i);

        if (fret_marker.mtype == FretMarkerType::CROSS) {
            pattern_parts.push_back(u"X");
            continue;
        } else if (fret_marker.mtype == FretMarkerType::CIRCLE) {
            pattern_parts.push_back(u"O");
            continue;
        }

        const auto it = dots_map.find(i);
        std::vector<FretItem::Dot> dot_list;
        if (it != dots_map.end()) {
            const std::vector<FretItem::Dot> &dots = it->second;
            for (const FretItem::Dot &dot : dots) {
                if (!dot.is_part_of_slur_barre) { // Don't write dot if part of slur barré
                    dot_list.push_back(dot);
                }
            }
        }

        if (!dot_list.empty()) {
            const auto &dot_list2 = it->second;
            StringList dot_descriptions;
            for (const auto &dot : dot_list2) {
                if (dot.is_part_of_slur_barre) {
                    continue;
                }

                int actual_fret = dot.fret + offset;
                Char type_char = u'O';

                switch (dot.dtype) {
                    case FretDotType::NORMAL: type_char = u'O'; break;
                    case FretDotType::CROSS: type_char = u'X'; break;
                    case FretDotType::SQUARE: type_char = u'S'; break;
                    case FretDotType::TRIANGLE: type_char = u'T'; break;
                    default: break;
                }

                dot_descriptions.push_back(String::number(actual_fret) + u'-' + type_char);
            }

            pattern_parts.push_back(u'[' + dot_descriptions.join(u",") + u']');
        } else {
            pattern_parts.push_back(u"-");
        }
    }

    String pattern = pattern_parts.join(u"");

    const BarreMap &barres_value = barres();
    StringList barre_parts;
    for (const auto &[fret, b] : barres_value) {
        if (!b.exists()) {
            continue;
        }

        int adjusted_fret = fret + offset;
        int start = b.start_string;
        int end = (b.end_string != -1) ? b.end_string : diagram_strings - 1;

        barre_parts.push_back(u'B' + String::number(adjusted_fret) + u'[' + String::number(start) + u'-' + String::number(end) + u']');
    }

    if (!barre_parts.empty()) {
        pattern += u';' + barre_parts.join(u";");
    }

    return pattern;
}

std::vector<String> FretDiagram::harmonies_from_pattern(const String &pattern) const {
    if (s_diagram_pattern_to_harmonies_map.empty()) {
        read_harmony_to_diagram_file(::mu::engraving::harmony_to_diagram_file_path);
    }
    return muse::value(s_diagram_pattern_to_harmonies_map, pattern);
}

std::vector<DiagramInfo> FretDiagram::patterns_from_harmony(const String &harmony_name) {
    if (s_harmony_to_diagram_map.empty()) {
        read_harmony_to_diagram_file(::mu::engraving::harmony_to_diagram_file_path);
    }

    NoteSpellingType spelling_type = style().style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();
    HarmonyMapKey key = create_harmony_map_key(harmony_name, spelling_type, score()->chord_list());

    if (tpc_is_valid(key.root_tpc)) {
        key.root_tpc = canonical_enharmonic_tpc(key.root_tpc);
    }
    if (tpc_is_valid(key.bass_tpc)) {
        key.bass_tpc = canonical_enharmonic_tpc(key.bass_tpc);
    }

    return muse::value(s_harmony_to_diagram_map, key);
}

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void FretDiagram::clear() {
    barres_.clear();
    dots_.clear();
    markers_.clear();
    fret_offset_ = 0;
}

bool FretDiagram::is_clear() const { return barres_.empty() && dots_.empty() && markers_.empty(); }

//---------------------------------------------------------
//   undoFretClear
//---------------------------------------------------------

void FretDiagram::undo_fret_clear() {
    for (EngravingObject *e : link_list()) {
        FretDiagram *fd = to_fret_diagram(e);
        fd->score()->undo(new FretClear(fd));
    }
}

int FretDiagram::num_pos() const { return style().style_i(Sid::FRET_NUM_POS); }

//---------------------------------------------------------
//   dot
//    take fret value of zero to mean all dots
//---------------------------------------------------------

std::vector<FretItem::Dot> FretDiagram::dot(int s, int f /*= 0*/) const {
    if (dots_.find(s) != dots_.end()) {
        if (f != 0) {
            for (auto const &d : dots_.at(s)) {
                if (d.fret == f) {
                    return std::vector<FretItem::Dot>{FretItem::Dot(d)};
                }
            }
        } else {
            return dots_.at(s);
        }
    }
    return std::vector<FretItem::Dot>{FretItem::Dot(0)};
}

//---------------------------------------------------------
//   marker
//---------------------------------------------------------

FretItem::Marker FretDiagram::marker(int s) const {
    if (markers_.find(s) != markers_.end()) {
        return markers_.at(s);
    }
    return FretItem::Marker(FretMarkerType::NONE);
}

//---------------------------------------------------------
//   barre
//---------------------------------------------------------

FretItem::Barre FretDiagram::barre(int f) const {
    if (barres_.find(f) != barres_.end()) {
        return barres_.at(f);
    }
    return FretItem::Barre(-1, -1);
}

Font FretDiagram::fret_num_font() const {
    const MStyle &st = style();
    Font f(st.style_st(Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_FACE), Font::Type::TEXT);
    f.set_point_size_f(st.style_d(Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_SIZE) * user_mag());
    FontStyle f_style = st.style_v(Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_STYLE).value<FontStyle>();
    f.set_bold(f_style & FontStyle::BOLD);
    f.set_italic(f_style & FontStyle::ITALIC);
    f.set_underline(f_style & FontStyle::UNDERLINE);
    f.set_strike(f_style & FontStyle::STRIKE);
    return f;
}

Font FretDiagram::fingering_font() const {
    const MStyle &st = style();
    Font f(st.style_st(Sid::FRET_DIAGRAM_FINGERING_FONT_FACE), Font::Type::TEXT);
    f.set_point_size_f(st.style_d(Sid::FRET_DIAGRAM_FINGERING_FONT_SIZE) * user_mag());
    FontStyle f_style = st.style_v(Sid::FRET_DIAGRAM_FINGERING_FONT_STYLE).value<FontStyle>();
    f.set_bold(f_style & FontStyle::BOLD);
    f.set_italic(f_style & FontStyle::ITALIC);
    f.set_underline(f_style & FontStyle::UNDERLINE);
    f.set_strike(f_style & FontStyle::STRIKE);
    return f;
}

String FretDiagram::harmony_plain_text() const { return harmony_ ? harmony_->plain_text() : String(); }

String FretDiagram::harmony_display_text() const { return harmony_ ? harmony_->display_text() : String(); }

//---------------------------------------------------------
//   setHarmony
///   if this is being done by the user, use undoSetHarmony instead
//---------------------------------------------------------

void FretDiagram::set_harmony(String harmony_text) {
    if (!harmony_) {
        Harmony *h = new Harmony(this);
        add(h);
    }

    harmony_->set_harmony(harmony_text);
    harmony_->set_xml_text(harmony_->harmony_name());
    trigger_layout();
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void FretDiagram::add(EngravingItem *e) {
    e->set_ownership_parent(this);
    if (e->is_harmony()) {
        harmony_ = to_harmony(e);

        harmony_->set_track(track());

        if (harmony_->harmony_name().empty()) {
            if (s_diagram_pattern_to_harmonies_map.empty()) {
                read_harmony_to_diagram_file(::mu::engraving::harmony_to_diagram_file_path);
            }

            String pattern = pattern_from_diagram();
            if (!pattern.empty()) {
                std::vector<String> matched_harmonies = muse::value(s_diagram_pattern_to_harmonies_map, pattern);
                if (!matched_harmonies.empty()) {
                    harmony_->set_harmony(matched_harmonies.front());
                }
            }
        }

        e->added();
    } else {
        LOGW("FretDiagram: cannot add <%s>\n", e->type_name());
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void FretDiagram::remove(EngravingItem *e) {
    if (e == harmony_) {
        harmony_ = nullptr;
        e->removed();
    } else {
        LOGW("FretDiagram: cannot remove <%s>\n", e->type_name());
    }
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool FretDiagram::accept_drop(EditData &data) const { return data.drop_element->is_harmony(); }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *FretDiagram::drop(Transaction &, EditData &data) {
    EngravingItem *e = data.drop_element;
    if (e->is_harmony()) {
        Harmony *h = to_harmony(e);
        h->set_ownership_parent(ownership_parent());
        h->set_track(track());
        score()->undo_add_element(h);
    } else {
        LOGW("FretDiagram: cannot drop <%s>\n", e->type_name());
        delete e;
        e = 0;
    }
    return e;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void FretDiagram::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);

    // don't display harmony in palette
    if (harmony_) {
        func(harmony_);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

void FretDiagram::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::EXCLUDE_VERTICAL_ALIGN) {
        EngravingItem::undo_change_property(id, v, ps);

        bool val = v.to_bool();
        Harmony *h = harmony();
        if (h && h->exclude_vertical_align() != val) {
            h->undo_change_property(Pid::EXCLUDE_VERTICAL_ALIGN, val, ps);
        }
        Segment *parent_seg = segment();
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

    EngravingItem::undo_change_property(id, v, ps);
}

PropertyValue FretDiagram::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::MAG: return user_mag();
        case Pid::FRET_STRINGS: return strings();
        case Pid::FRET_FRETS: return frets();
        case Pid::FRET_NUT: return show_nut();
        case Pid::FRET_OFFSET: return fret_offset();
        case Pid::ORIENTATION: return orientation_;
        case Pid::FRET_SHOW_FINGERINGS: return show_fingering_;
        case Pid::FRET_FINGERING: return fingering_;
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool FretDiagram::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::MAG: set_user_mag(v.to_double()); break;
        case Pid::FRET_STRINGS: set_strings(v.to_int()); break;
        case Pid::FRET_FRETS: set_frets(v.to_int()); break;
        case Pid::FRET_NUT: set_show_nut(v.to_bool()); break;
        case Pid::FRET_OFFSET: set_fret_offset(v.to_int()); break;
        case Pid::ORIENTATION: orientation_ = v.value<Orientation>(); break;
        case Pid::FRET_SHOW_FINGERINGS: set_show_fingering(v.to_bool()); break;
        case Pid::FRET_FINGERING: set_fingering(v.value<std::vector<int>>()); break;
        case Pid::EXCLUDE_VERTICAL_ALIGN: {
            set_exclude_vertical_align(v.to_bool());
            break;
        }
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue FretDiagram::property_default(Pid pid) const {
    // We shouldn't style the fret offset
    if (pid == Pid::FRET_OFFSET) {
        return PropertyValue(0);
    } else if (pid == Pid::FRET_FINGERING) {
        return std::vector<int>(strings_, 0);
    }

    for (const StyledProperty &p : *styled_properties()) {
        if (p.pid == pid) {
            if (property_type(pid) == PType::ABSOLUTE) {
                return style().style_absolute(p.sid);
            }
            return style().style_v(p.sid);
        }
    }
    return EngravingItem::property_default(pid);
}

void FretDiagram::set_track(track_idx_t val) {
    EngravingItem::set_track(val);

    if (harmony_) {
        harmony_->set_track(val);
    }
}

//---------------------------------------------------------
//   screenReaderInfo
//---------------------------------------------------------

void FretDiagram::set_fingering(std::vector<int> v) { fingering_ = std::move(v); }

FretDiagram *FretDiagram::make_from_harmony_or_fret_diagram(const EngravingItem *harmony_or_fret_diagram) {
    IF_ASSERT_FAILED(!harmony_or_fret_diagram || !harmony_or_fret_diagram->is_harmony() || !harmony_or_fret_diagram->is_fret_diagram()) {
        return nullptr;
    }

    FretDiagram *fret_diagram = nullptr;

    if (harmony_or_fret_diagram->is_harmony() && !harmony_or_fret_diagram->ownership_parent()->is_fret_diagram()) {
        Harmony *harmony = to_harmony(harmony_or_fret_diagram)->clone();

        fret_diagram = Factory::create_fret_diagram(harmony_or_fret_diagram->score()->dummy());

        fret_diagram->update_diagram(harmony->plain_text());

        harmony->reset_property(Pid::OFFSET);
        harmony->set_property(Pid::ALIGN, Align(AlignH::HCENTER, AlignV::BASELINE));
        harmony->set_property_flags(Pid::ALIGN, PropertyFlags::UNSTYLED);

        fret_diagram->add(harmony);
    } else if (harmony_or_fret_diagram->is_harmony() && harmony_or_fret_diagram->ownership_parent()->is_fret_diagram()) {
        fret_diagram = to_fret_diagram(harmony_or_fret_diagram->ownership_parent())->clone();
    } else if (harmony_or_fret_diagram->is_fret_diagram()) {
        fret_diagram = to_fret_diagram(harmony_or_fret_diagram)->clone();
        fret_diagram->set_offset(PointF());
        if (!fret_diagram->harmony()) {
            //! generate from diagram and add harmony
            fret_diagram->add(Factory::create_harmony(fret_diagram));
        }
    }

    return fret_diagram;
}

bool FretDiagram::is_in_fret_box() const {
    EngravingObject *parent = ownership_parent();
    return parent ? parent->is_f_box() : false;
}

bool FretDiagram::is_custom(const String &harmony_name_for_compare) const {
    if (harmony_name_for_compare.empty()) {
        return false;
    }

    static constexpr std::array props{Pid::FRET_STRINGS, Pid::FRET_FRETS, Pid::FRET_NUT, Pid::FRET_FINGERING};

    for (const Pid &pid : props) {
        if (pid == Pid::FRET_FINGERING) {
            if (custom(Pid::FRET_SHOW_FINGERINGS)) {
                return true;
            }
        } else if (custom(pid)) {
            return true;
        }
    }

    if (s_harmony_to_diagram_map.empty()) {
        read_harmony_to_diagram_file(::mu::engraving::harmony_to_diagram_file_path);
    }

    NoteSpellingType spelling_type = style().style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();
    HarmonyMapKey key = create_harmony_map_key(harmony_name_for_compare, spelling_type, score()->chord_list());

    if (tpc_is_valid(key.root_tpc)) {
        key.root_tpc = canonical_enharmonic_tpc(key.root_tpc);
    }
    if (tpc_is_valid(key.bass_tpc)) {
        key.bass_tpc = canonical_enharmonic_tpc(key.bass_tpc);
    }

    std::vector<DiagramInfo> available_diagrams = muse::value(s_harmony_to_diagram_map, key);
    if (available_diagrams.empty()) {
        return true;
    }

    String current_pattern = pattern_from_diagram();

    for (const DiagramInfo &diagram : available_diagrams) {
        if (diagram.diagram_pattern == current_pattern) {
            return false;
        }
    }

    return true;
}

void FretDiagram::read_harmony_to_diagram_file(const muse::io::PathT &file_path) const {

    muse::io::File file(file_path);
    if (!file.open()) {
        LOGE() << file.error_string();
        return;
    }

    XmlReader reader(&file);

    std::unordered_map<String, rw::HarmonyToDiagramReader::FretDiagramInfo> harmony_to_diagram_map =
        rw::HarmonyToDiagramReader::read(reader);

    const ChordList *chord_list = score()->chord_list();
    const NoteSpellingType spelling_type = NoteSpellingType::STANDARD;

    for (auto &[key, value] : harmony_to_diagram_map) {
        HarmonyMapKey map_key = create_harmony_map_key(key, spelling_type, chord_list);

        if (tpc_is_valid(map_key.root_tpc)) {
            map_key.root_tpc = canonical_enharmonic_tpc(map_key.root_tpc);
        }
        if (tpc_is_valid(map_key.bass_tpc)) {
            map_key.bass_tpc = canonical_enharmonic_tpc(map_key.bass_tpc);
        }

        s_harmony_to_diagram_map[map_key].push_back({key, value.xml, value.pattern});
        s_diagram_pattern_to_harmonies_map[value.pattern].push_back(key);
    }
}

//---------------------------------------------------------
//   markerToChar
//---------------------------------------------------------

Char FretItem::marker_to_char(FretMarkerType t) {
    switch (t) {
        case FretMarkerType::CIRCLE: return Char(u'O');
        case FretMarkerType::CROSS: return Char(u'X');
        case FretMarkerType::NONE:
        default: return Char();
    }
}

//---------------------------------------------------------
//   markerTypeToName
//---------------------------------------------------------

const std::vector<FretItem::MarkerTypeNameItem> FretItem::marker_type_name_map = {
    {FretMarkerType::CIRCLE, "circle"}, {FretMarkerType::CROSS, "cross"}, {FretMarkerType::NONE, "none"}};

String FretItem::marker_type_to_name(FretMarkerType t) {
    for (auto i : FretItem::marker_type_name_map) {
        if (i.mtype == t) {
            return String::from_ascii(i.name);
        }
    }

    ASSERT_X("Unrecognised FretMarkerType!");
    return String(); // prevent compiler warnings
}

//---------------------------------------------------------
//   nameToMarkerType
//---------------------------------------------------------

FretMarkerType FretItem::name_to_marker_type(String n) {
    for (auto i : FretItem::marker_type_name_map) {
        if (String::from_ascii(i.name) == n) {
            return i.mtype;
        }
    }
    LOGW("Unrecognised marker name!");
    return FretMarkerType::NONE; // default
}

//---------------------------------------------------------
//   dotTypeToName
//---------------------------------------------------------

const std::vector<FretItem::DotTypeNameItem> FretItem::dot_type_name_map = {
    {FretDotType::NORMAL, "normal"},
    {FretDotType::CROSS, "cross"},
    {FretDotType::SQUARE, "square"},
    {FretDotType::TRIANGLE, "triangle"},
};

String FretItem::dot_type_to_name(FretDotType t) {
    for (auto i : FretItem::dot_type_name_map) {
        if (i.dtype == t) {
            return String::from_ascii(i.name);
        }
    }

    ASSERT_X("Unrecognised FretDotType!");
    return String(); // prevent compiler warnings
}

//---------------------------------------------------------
//   nameToDotType
//---------------------------------------------------------

FretDotType FretItem::name_to_dot_type(String n) {
    for (auto i : FretItem::dot_type_name_map) {
        if (String::from_ascii(i.name) == n) {
            return i.dtype;
        }
    }
    LOGW("Unrecognised dot name!");
    return FretDotType::NORMAL; // default
}
} // namespace mu::engraving
