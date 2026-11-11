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

#pragma once

#include <map>

#include "../types/types.h"
#include "global/types/string.h"

namespace muse::io {
struct IODevice;
} // namespace muse::io

namespace mu::engraving::compat {
struct ReadChordListHook;
} // namespace mu::engraving::compat

namespace mu::engraving {
struct ChordList;
struct MStyle;
struct XmlWriter;
struct XmlReader;

//---------------------------------------------------------
//   class HDegree
//---------------------------------------------------------

enum class HDegreeType : char { UNDEF, ADD, ALTER, SUBTRACT };

struct HDegree {
    HDegree() = default;

    HDegree(int v, int a, HDegreeType t) {
        value_ = v;
        alter_ = a;
        type_ = t;
    }

    int value() const { return value_; }

    int alter() const { return alter_; }

    HDegreeType type() const { return type_; }

    String text() const;

private:
    int value_ = 0;
    int alter_ = 0; // -1, 0, 1  (b - - #)
    HDegreeType type_ = HDegreeType::UNDEF;
};

//---------------------------------------------------------
//   HChord
//---------------------------------------------------------

struct HChord {
    HChord() = default;

    HChord(int k) { keys_ = k; }

    HChord(int a, int b, int c = -1, int d = -1, int e = -1, int f = -1, int g = -1, int h = -1, int i = -1, int k = -1, int l = -1);
    HChord(const String &);

    void rotate(int semi_tones);

    bool contains(int key) const // key in chord?
    {
        return (1 << (key % 12)) & keys_;
    }

    HChord &operator+=(int key) {
        keys_ |= (1 << (key % 12));
        return *this;
    }

    HChord &operator-=(int key) {
        keys_ &= ~(1 << (key % 12));
        return *this;
    }

    bool operator==(const HChord &o) const { return keys_ == o.keys_; }

    bool operator!=(const HChord &o) const { return keys_ != o.keys_; }

    int get_keys() const { return keys_; }

    void print() const;

    String name(int tpc) const;
    String voicing() const;
    void add(const std::vector<HDegree> &degree_list);

protected:
    int keys_ = 0;

private:
    String str_;
};

//---------------------------------------------------------
//   RenderAction
//    Render commands to be passed to and executed by Harmony
//---------------------------------------------------------

struct RenderAction {
    enum class RenderActionType : char { SET, MOVE, MOVEXHEIGHT, PUSH, POP, NOTE, ACCIDENTAL, STOPHALIGN, SCALE, PAREN };

    virtual RenderActionType action_type() const = 0;

    virtual void print() const { print(action_type(), u""); }

    virtual ~RenderAction() {}

protected:
    void print(RenderActionType type, const String &info) const;
};

struct RenderActionMove : RenderAction {
    RenderActionMove() {}

    RenderActionMove(double movex, double movey) : vec_(PointF(movex, movey)) {}

    RenderActionType action_type() const override { return RenderActionType::MOVE; }

    void print() const override;

    double x() const { return vec_.x(); }

    double y() const { return vec_.y(); }

    PointF vec() const { return vec_; }

    bool scaled() const { return scaled_; }

protected:
    RenderActionMove(double movex, double movey, bool scaled) : vec_(PointF(movex, movey)), scaled_(scaled) {}

private:
    PointF vec_ = PointF(0.0, 0.0);
    bool scaled_ = false;
};

struct RenderActionMoveScaled : RenderActionMove {
    RenderActionMoveScaled(double movex, double movey) : RenderActionMove(movex, movey, true) {}

    void print() const override;
};

struct RenderActionMoveXHeight : RenderAction {
    RenderActionMoveXHeight(bool up) : up_(up) {}

    RenderActionType action_type() const override { return RenderActionType::MOVEXHEIGHT; }

    bool up() const { return up_; }

    bool scaled() const { return scaled_; }

    void print() const override;

protected:
    RenderActionMoveXHeight(bool up, bool scaled) : up_(up), scaled_(scaled) {}

private:
    bool up_ = true;
    bool scaled_ = false;
};

struct RenderActionMoveXHeightScaled : RenderActionMoveXHeight {
    RenderActionMoveXHeightScaled(bool up) : RenderActionMoveXHeight(up, true) {}
};

struct RenderActionSet : RenderAction {
    RenderActionSet() {}

    RenderActionSet(String s) : text_(s) {}

    RenderActionType action_type() const override { return RenderActionType::SET; }

    void print() const override;

    const String &text() const { return text_; }

    bool render_text() const { return render_text_; }

protected:
    RenderActionSet(String s, bool render_text) : text_(s), render_text_(render_text) {}

private:
    String text_;
    bool render_text_ = true;
};

struct RenderActionMoveTextWidth : RenderActionSet {
    RenderActionMoveTextWidth(String s) : RenderActionSet(s, false) {}
};

struct RenderActionPush : RenderAction {
    RenderActionPush() {}

    RenderActionType action_type() const override { return RenderActionType::PUSH; }
};

struct RenderActionPop : RenderAction {
    RenderActionPop() {}

    RenderActionType action_type() const override { return RenderActionType::POP; }

    void print() const override;

    double pop_x() const { return popx_; }

    double pop_y() const { return popy_; }
protected:
    RenderActionPop(bool popx, bool popy) : popx_(popx), popy_(popy) {}
private:
    bool popx_ = true, popy_ = true;
};

struct RenderActionPopX : RenderActionPop {
    RenderActionPopX() : RenderActionPop(true, false) {}
};

struct RenderActionPopY : RenderActionPop {
    RenderActionPopY() : RenderActionPop(false, true) {}
};

struct RenderActionNote : RenderAction {
    RenderActionNote() {}

    RenderActionType action_type() const override { return RenderActionType::NOTE; }
};

struct RenderActionAccidental : RenderAction {
    RenderActionAccidental() {}

    RenderActionType action_type() const override { return RenderActionType::ACCIDENTAL; }
};

struct RenderActionStopHAlign : RenderAction {
    RenderActionStopHAlign() {}

    RenderActionType action_type() const override { return RenderActionType::STOPHALIGN; }
};

struct RenderActionScale : RenderAction {
    RenderActionScale() {}

    RenderActionScale(double scale) : scale_(scale) {}

    RenderActionType action_type() const override { return RenderActionType::SCALE; }

    double scale() const { return scale_; }

    void print() const override;

private:
    double scale_ = 1.0;
};

struct RenderActionParen : RenderAction {
    virtual DirectionH direction() const = 0;

    RenderActionType action_type() const override { return RenderActionType::PAREN; }

    void print() const override;
};

struct RenderActionParenLeft : RenderActionParen {
    RenderActionParenLeft() {}

    DirectionH direction() const override { return DirectionH::LEFT; }
};

struct RenderActionParenRight : RenderActionParen {
    RenderActionParenRight() {}

    DirectionH direction() const override { return DirectionH::RIGHT; }
};

using RenderActionPtr = std::shared_ptr<RenderAction>;
using RenderActionMovePtr = std::shared_ptr<RenderActionMove>;
using RenderActionMoveXHeightPtr = std::shared_ptr<RenderActionMoveXHeight>;
using RenderActionPopPtr = std::shared_ptr<RenderActionPop>;
using RenderActionScalePtr = std::shared_ptr<RenderActionScale>;
using RenderActionSetPtr = std::shared_ptr<RenderActionSet>;
using RenderActionParenPtr = std::shared_ptr<RenderActionParen>;

//---------------------------------------------------------
//   ChordToken
//    Reads/represents the <token> tag in chord XML files
//---------------------------------------------------------

enum class ChordTokenClass : char { ALL, QUALITY, EXTENSION, MODIFIER, TYPE, ACCIDENTAL };

struct ChordToken {
    ChordTokenClass token_class;
    StringList names;
    std::vector<RenderActionPtr> render_list;
    void read(XmlReader &, int msc_version);
    void write(XmlWriter &) const;

    bool is_valid() const { return !names.empty(); }
};

//---------------------------------------------------------
//   ParsedChord
//    Extracts quality, extension & modifiers from a string eg. "m7#11"
//    Generates a list of render instructions to lay this chord out
//---------------------------------------------------------

struct ParsedChord {
    ParsedChord() = default;

    bool parse(const String &, const ChordList *, bool syntax_only = false, bool prefer_minor = false);
    String from_xml(const String &, const String &, const String &, const String &, const std::vector<HDegree> &, const ChordList *);
    const std::vector<RenderActionPtr> &render_list(const ChordList *cl, bool stacked);

    bool parseable() const { return parseable_; }

    bool understandable() const { return understandable_; }

    const String &name() const { return name_; }

    const String &quality() const { return quality_; }

    const String &extension() const { return extension_; }

    const String &modifiers() const { return modifiers_; }

    const StringList &modifier_list() const { return modifier_list_; }

    const String &xml_kind() const { return xml_kind_; }

    const String &xml_text() const { return xml_text_; }

    const String &xml_symbols() const { return xml_symbols_; }

    const String &xml_parens() const { return xml_parens_; }

    const StringList &xml_degrees() const { return xml_degrees_; }

    int keys() const { return chord_.get_keys(); }

    const String &handle() const { return handle_; }

    operator String() const { return handle_; }

    bool operator==(const ParsedChord &c) const { return this->handle_ == c.handle_; }

    bool operator!=(const ParsedChord &c) const { return !(*this == c); }

private:
    void configure(const ChordList *);
    void correct_xml_text(const String &s = String());
    void add_token(String, ChordTokenClass);

    String name_;
    String handle_;
    String quality_;
    String extension_;
    String modifiers_;
    StringList modifier_list_;
    std::vector<ChordToken> token_list_;
    std::vector<RenderActionPtr> render_list_;
    String xml_kind_;
    String xml_text_;
    String xml_symbols_;
    String xml_parens_;
    StringList xml_degrees_;
    StringList major_, minor_, diminished_, augmented_, lower_, raise_, mod_, symbols_;
    HChord chord_;
    bool parseable_ = false;
    bool understandable_ = false;
};

//---------------------------------------------------------
//   ChordDescription
//    Describes chord quality, extensions and modifiers. eg. m7#11
//---------------------------------------------------------

struct ChordDescription {
    int id = 0;       // Chord id number (Band In A Box Chord Number)
    StringList names; // list of alternative chord names
                      // that will by recognized from keyboard entry (without root/base)
    std::vector<ParsedChord> parsed_chords;
    // parsed forms of primary name (optionally also include parsed forms of other names)
    String xml_kind;        // MusicXML: kind
    String xml_text;        // MusicXML: kind text=
    String xml_symbols;     // MusicXML: kind use-symbols=
    String xml_parens;      // MusicXML: kind parentheses-degrees=
    StringList xml_degrees; // MusicXML: list of degrees (if any)
    HChord chord;           // C based chord
    std::vector<RenderActionPtr> render_list;
    std::vector<RenderActionPtr> render_list_stacked;
    bool generated = false;
    bool render_list_generated = false;
    bool export_ok = false;

    ChordDescription() {}

    ChordDescription(int);
    ChordDescription(const String &);

    String quality() const { return quality_; }

    void complete(ParsedChord *pc, const ChordList *);
    void read(XmlReader &, int msc_version);
    void write(XmlWriter &) const;

private:
    String quality_;
};

//---------------------------------------------------------
//   ChordSymbol
//    Represents the <sym> tag in chord XML files
//---------------------------------------------------------

struct ChordSymbol {
    int font_idx = -1;
    String name;
    String value;
    Char code;

    bool is_valid() const { return font_idx != -1; }
};

//---------------------------------------------------------
//   ChordFont
//    Represents the <font> tag in chord XML files
//---------------------------------------------------------

struct ChordFont {
    String family;
    String font_class;
    double mag = 1.0;
    bool music_symbol_text = false;
};

//---------------------------------------------------------
//   ChordList
//    Reads chord XML files and stores the list of known chords
//---------------------------------------------------------

struct ChordList : std::map<int, ChordDescription> {

public:
    std::vector<ChordFont> fonts;
    std::vector<RenderActionPtr> render_list_root;
    std::vector<RenderActionPtr> render_list_function;
    std::vector<RenderActionPtr> render_list_bass;
    std::vector<RenderActionPtr> render_list_bass_offset;
    std::vector<ChordToken> chord_token_list;
    static int private_id;

    bool auto_adjust() const { return auto_adjust_; }

    double nominal_mag() const { return nmag_; }

    double nominal_adjust() const { return nadjust_; }

    bool stack_modifiers() const { return stack_modifiers_; }

    bool exclude_mods_h_align() const { return exclude_mods_h_align_; }

    double stacked_modifier_mag() const { return stackedmmag_; }

    const ChordStylePreset &chord_preset() const { return chord_preset_; }

    void configure_auto_adjust(double emag = 1.0, double eadjust = 0.0, double mmag = 1.0, double madjust = 0.0, double stackedmmag = 0.0,
                               bool stack_modifiers = false, bool exclude_mods_h_align = false, String symbol_font = u"",
                               const ChordStylePreset &preset = ChordStylePreset::STANDARD);
    double position(const StringList &names, bool stack_modifiers, bool super_script, ChordTokenClass ctc, size_t modifier_idx,
                    size_t nmodifiers) const;

    void check_chord_list(const MStyle &style);
    bool read(const String &name);
    bool read(muse::io::IODevice *device);
    bool write(const String &) const;
    bool write(muse::io::IODevice *device) const;
    bool loaded() const;
    void unload();

    const ChordDescription *description(int id) const;
    ChordSymbol symbol(const String &s) const;
    ChordToken token(const String &s, ChordTokenClass) const;

    void set_custom_chord_list(bool t) { custom_chord_list_ = t; }

    bool custom_chord_list() const { return custom_chord_list_; }

private:

    friend struct compat::ReadChordListHook;

    void read(XmlReader &xml, int msc_version);
    void write(XmlWriter &xml) const;

    std::map<String, ChordSymbol> symbols_;
    bool auto_adjust_ = false;
    bool stack_modifiers_ = false;
    bool exclude_mods_h_align_ = false;
    ChordStylePreset chord_preset_ = ChordStylePreset::STANDARD;
    double nmag_ = 1.0, nadjust_ = 0.0; // adjust values are measured in percentage
    double emag_ = 1.0, eadjust_ = 0.0; // (which is then applied to the height of the font)
    double mmag_ = 1.0, madjust_ = 0.0, stackedmmag_ = 0.0;
    String symbol_text_font_ = u"";

    bool custom_chord_list_ = false; // if true, chordlist will be saved as part of score
};
} // namespace mu::engraving
