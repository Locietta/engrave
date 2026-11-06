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

#include <vector>

#include "chordlist.h"
#include "parenthesis.h"
#include "draw/types/font.h"

#include "textbase.h"
#include "score.h"

#include "pitchspelling.h"
#include "realizedharmony.h"

namespace mu::engraving {
struct ChordDescription;
struct ParsedChord;
struct Score;
struct Transaction;

enum class HarmonyType : unsigned char { STANDARD, ROMAN, NASHVILLE };

enum class HarmonyRenderItemType : unsigned char { TEXT, PAREN };

struct HarmonyRenderItem {
    virtual ~HarmonyRenderItem() = default;

    HarmonyRenderItem(bool align, double input_x, double input_y) : h_align_(align), pos_(input_x, input_y) {}

    PointF pos() const { return pos_ + offset_; }

    double x() const { return pos_.x(); }

    double y() const { return pos_.y(); }

    void movex(double v) { pos_.rx() += v; }

    void movey(double v) { pos_.ry() += v; }

    void setx(double v) { pos_.rx() = v; }

    void sety(double v) { pos_.ry() = v; }

    void set_offset(const PointF &p) { offset_ = p; }

    virtual double height() const = 0;

    bool align() const { return h_align_; }

    virtual RectF bounding_rect() const = 0;
    virtual RectF tight_bounding_rect() const = 0;

    virtual double left_padding() const = 0;
    virtual double right_padding() const = 0;

    virtual HarmonyRenderItemType type() const = 0;

private:
    bool h_align_ = true;
    PointF offset_; // Offset for placing within the TextBase.
    PointF pos_;    // Position of segments relative to Harmony position
};

struct ChordSymbolParen : HarmonyRenderItem {
    ChordSymbolParen(Parenthesis *p, bool h_align, double input_x, double input_y)
        : HarmonyRenderItem(h_align, input_x, input_y), paren_item(p) {}

    ~ChordSymbolParen() { delete paren_item; }

    RectF bounding_rect() const override { return paren_item->shape().bbox(); }

    RectF tight_bounding_rect() const override { return paren_item->shape().bbox(); }

    double top = DBL_MAX;
    double bottom = -DBL_MAX;
    double closing_paren_pos = -DBL_MAX;

    Parenthesis *paren_item = nullptr;

    double height() const override { return paren_item->height(); }

    double left_padding() const override {
        return paren_item->direction() == DirectionH::LEFT ? ::mu::engraving::ChordSymbolParen::outer_padding :
                                                             ::mu::engraving::ChordSymbolParen::inner_padding;
    }

    double right_padding() const override {
        return paren_item->direction() == DirectionH::LEFT ? ::mu::engraving::ChordSymbolParen::inner_padding :
                                                             ::mu::engraving::ChordSymbolParen::outer_padding;
    }

    HarmonyRenderItemType type() const override { return HarmonyRenderItemType::PAREN; }

private:
    static constexpr double inner_padding = 0.05;
    static constexpr double outer_padding = 0.1;
};

//---------------------------------------------------------
//   TextSegment
//---------------------------------------------------------

struct TextSegment : HarmonyRenderItem {
    double width() const;
    double cap_height() const;
    RectF bounding_rect() const override;
    RectF tight_bounding_rect() const override;

    void set_text(const String &t) { text_ = t; }

    String text() const { return text_; }

    muse::draw::Font font() const { return font_; }

    void set_font(const muse::draw::Font &f);

    double left_padding() const override { return 0.0; }

    double right_padding() const override { return 0.0; }

    double height() const override { return tight_bounding_rect().height(); }

    HarmonyRenderItemType type() const override { return HarmonyRenderItemType::TEXT; }

    TextSegment(const String &s, const muse::draw::Font &f, double input_x, double input_y, bool align)
        : HarmonyRenderItem(align, input_x, input_y) {
        set_text(s);
        set_font(f);
    }

private:
    muse::draw::Font font_;
    String text_;
};

struct HDegree;

//---------------------------------------------------------
//   @@ HarmonyInfo
//      Contains all identifying information for a chord. The Harmony DOM class can have many of these
//      as polychords
//
//   @P bassTpc_       int             bass note as "tonal pitch class"
//   @P rootTpc_       int             root note as "tonal pitch class"
//   @P id_            int             harmony identifier
//   @P textName_      String          plaintext representation of chord
//   @P parsedChord_   ParsedChord*    parsed representation of the plaintext chord
//---------------------------------------------------------

struct HarmonyInfo {
    HarmonyInfo(int id, int root_tpc, int bass_tpc, String text_name, ParsedChord *pc, Score *score)
        : id_(id), root_tpc_(root_tpc), bass_tpc_(bass_tpc), text_name_(text_name), parsed_chord_(pc), score_(score) {}

    HarmonyInfo(Score *score) : score_(score) {}

    HarmonyInfo(const HarmonyInfo &h);
    ~HarmonyInfo();

    int id() const { return id_; }

    void set_id(int v) { id_ = v; }

    int root_tpc() const { return root_tpc_; }

    void set_root_tpc(int v) { root_tpc_ = v; }

    int bass_tpc() const { return bass_tpc_; }

    void set_bass_tpc(int v) { bass_tpc_ = v; }

    String text_name() const { return text_name_; }

    void set_text_name(const String &v) { text_name_ = v; }

    ChordList *chord_list() const { return score_ ? score_->chord_list() : nullptr; }

    const ChordDescription *descr() const;
    const ChordDescription *descr(const String &, const ParsedChord *pc = 0) const;
    const ChordDescription *get_description();
    const ChordDescription *get_description(const String &, const ParsedChord *pc = 0);
    const ChordDescription *generate_description();

    void set_parsed_chord(ParsedChord *v) { parsed_chord_ = v; }

    ParsedChord *parsed_chord() const { return parsed_chord_; }

    ParsedChord *get_parsed_chord();

    bool has_modifiers() const;

private:
    int id_ = -1;                     // >0 = id of matched chord from chord list, if applicable
                                      // -1 = invalid chord
                                      // <-10000 = private id of generated chord or matched chord with no id
    int root_tpc_ = Tpc::TPC_INVALID; // root note for chord
    int bass_tpc_ = Tpc::TPC_INVALID; // bass note or chord bass; used for "slash" chords
                                      // or notation of bass note in chord

    String text_name_;                    // name recognized from chord list, read from score file, or constructed from imported source
                                          // Also stores the whole RNA string to be rendered
    ParsedChord *parsed_chord_ = nullptr; // parsed form of chord
    Score *score_ = nullptr;
};

//---------------------------------------------------------
//   @@ Harmony
//      This DOM class represents the chord symbol in the score. It contains the necessary information to
//      layout standard chords and polychords
//---------------------------------------------------------

struct Harmony final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, Harmony)
    DECLARE_CLASSOF(ElementType::HARMONY)

public:
    Harmony(EngravingObject *parent = 0); // Segment or FretDiagram
    Harmony(const Harmony &);
    ~Harmony();

    Harmony *clone() const override { return new Harmony(*this); }

    int id() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED

    bool play() const { return play_; }

    void set_bass_case(NoteCaseType c) { bass_case_ = c; }

    NoteCaseType bass_case() const { return bass_case_; }

    void set_root_case(NoteCaseType c) { root_case_ = c; }

    NoteCaseType root_case() const { return root_case_; }

    Segment *get_parent_seg() const;
    FretDiagram *get_parent_fret_diagram() const;
    Harmony *find_next() const;
    Harmony *find_prev() const;
    Fraction ticks_till_next(int utick, bool stop_at_measure_end = false) const;

    RealizedHarmony &realized_harmony();
    const RealizedHarmony &get_realized_harmony() const;

    bool is_playable() const override;

    bool is_realizable() const;
    bool is_in_fret_box() const;

    int bass_tpc() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED
    int root_tpc() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED
    void add_degree(const HDegree &d);
    const std::vector<HDegree> &degree_list() const;

    HarmonyType harmony_type() const { return harmony_type_; }

    void set_harmony_type(HarmonyType val);

    const ParsedChord *parsed_form() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED

    void after_read();
    void render();

    bool is_polychord() const { return chords_.size() > 1; }

    const std::vector<HarmonyInfo *> chords() const { return chords_; }

    void add_chord(HarmonyInfo *info) { chords_.push_back(info); }

    bool has_modifiers() const;

    String display_text() const;
    String harmony_name() const;

    double base_line() const override;
    void spatium_changed(double old_value, double new_value) override;
    void local_spatium_changed(double old_value, double new_value) override;
    void set_harmony(const String &s);

    int subtype() const override { return static_cast<int>(harmony_type()); }

    TranslatableString type_user_name() const override;

    String generate_screen_reader_info() const;

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    using EngravingObject::undo_change_property;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;

    bool position_relative_to_notehead_rest() const override;

    double mag() const override;

    double bass_scale() const { return bass_scale_; }

    void set_bass_scale(double v) { bass_scale_ = v; }

    Color cur_color(const rendering::PaintOptions &opt) const override;
    void set_color(const Color &color) override;

    bool do_not_stack_modifiers() const { return do_not_stack_modifiers_; }

    NoteCaseType root_render_case(HarmonyInfo *info) const;
    NoteCaseType bass_render_case() const;

    FontStyle font_style() const override { return font_style_; }

    String family() const override { return font_family_; }

    double size() const override { return font_size_; }

    void set_font_style(const FontStyle &val) override { font_style_ = val; }

    void set_family(const String &val) override { font_family_ = val; }

    void set_size(const double &val) override { font_size_ = val; }

    struct LayoutData : public TextBase::LayoutData {
        ld_field<double> harmony_height = {"[Harmony] harmonyHeight", 0.0}; // used for calculating the height is frame while editing.
        ld_field<std::vector<LineF>> polychord_divider_lines = {"[Harmony] polychordDividerLine", std::vector<LineF>()};
        ld_field<double> polychord_divider_offset = {"[Harmony] polychordDividerOffset", 0.0};
        ld_field<double> baseline = {"[Harmony] baseline", 0.0};
        ld_field<std::vector<muse::draw::Font>> font_list = "[Harmony] fontList";                 // temp values used in render()
        ld_field<std::vector<HarmonyRenderItem *>> render_item_list = "[Harmony] renderItemList"; // rendered chord
    };
    DECLARE_LAYOUTDATA_METHODS(Harmony)

private:
    std::vector<HarmonyInfo *> chords_;

    const std::vector<const ChordDescription *> parse_harmony(const String &s, bool syntax_only = false);
    const ChordDescription *parse_single_harmony(const String &s, HarmonyInfo *info, bool syntax_only = false);

    Sid get_property_style(Pid) const override;

    Harmony *find_in_seg(Segment *seg) const;

    bool is_misspelled_ = false;                       // show spell check warning
    HarmonyType harmony_type_ = HarmonyType::STANDARD; // used to control rendering, transposition, export, etc.

    mutable RealizedHarmony realized_harmony_; // the realized harmony used for playback

    std::vector<HDegree> degree_list_;

    bool play_ = true; // whether or not to play back the harmony
    bool do_not_stack_modifiers_ = false;

    NoteCaseType root_case_ = NoteCaseType::AUTO;
    NoteCaseType bass_case_ = NoteCaseType::AUTO; // case as typed

    double bass_scale_ = 1.0;

    // Overridden textbase properties to apply to whole item
    double font_size_ = 10.0;
    String font_family_ = u"";
    FontStyle font_style_;
};
} // namespace mu::engraving
