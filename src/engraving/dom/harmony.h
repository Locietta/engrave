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
class ParsedChord;
class Score;
class Transaction;

enum class HarmonyType : unsigned char { STANDARD, ROMAN, NASHVILLE };

enum class HarmonyRenderItemType : unsigned char { TEXT, PAREN };

struct HarmonyRenderItem {
    virtual ~HarmonyRenderItem() = default;

    HarmonyRenderItem(bool align, double _x, double _y) : hAlign_(align), pos_(_x, _y) {}

    PointF pos() const { return pos_ + offset_; }

    double x() const { return pos_.x(); }

    double y() const { return pos_.y(); }

    void movex(double v) { pos_.rx() += v; }

    void movey(double v) { pos_.ry() += v; }

    void setx(double v) { pos_.rx() = v; }

    void sety(double v) { pos_.ry() = v; }

    void setOffset(const PointF &p) { offset_ = p; }

    virtual double height() const = 0;

    bool align() const { return hAlign_; }

    virtual RectF boundingRect() const = 0;
    virtual RectF tightBoundingRect() const = 0;

    virtual double leftPadding() const = 0;
    virtual double rightPadding() const = 0;

    virtual HarmonyRenderItemType type() const = 0;

private:
    bool hAlign_ = true;
    PointF offset_; // Offset for placing within the TextBase.
    PointF pos_;    // Position of segments relative to Harmony position
};

struct ChordSymbolParen : HarmonyRenderItem {
    ChordSymbolParen(Parenthesis *p, bool hAlign, double _x, double _y) : HarmonyRenderItem(hAlign, _x, _y), parenItem(p) {}

    ~ChordSymbolParen() { delete parenItem; }

    RectF boundingRect() const override { return parenItem->shape().bbox(); }

    RectF tightBoundingRect() const override { return parenItem->shape().bbox(); }

    double top = DBL_MAX;
    double bottom = -DBL_MAX;
    double closingParenPos = -DBL_MAX;

    Parenthesis *parenItem = nullptr;

    double height() const override { return parenItem->height(); }

    double leftPadding() const override { return parenItem->direction() == DirectionH::LEFT ? OUTER_PADDING : INNER_PADDING; }

    double rightPadding() const override { return parenItem->direction() == DirectionH::LEFT ? INNER_PADDING : OUTER_PADDING; }

    HarmonyRenderItemType type() const override { return HarmonyRenderItemType::PAREN; }

private:
    static constexpr double INNER_PADDING = 0.05;
    static constexpr double OUTER_PADDING = 0.1;
};

//---------------------------------------------------------
//   TextSegment
//---------------------------------------------------------

struct TextSegment : HarmonyRenderItem {
    double width() const;
    double capHeight() const;
    RectF boundingRect() const override;
    RectF tightBoundingRect() const override;

    void setText(const String &t) { text_ = t; }

    String text() const { return text_; }

    muse::draw::Font font() const { return font_; }

    void setFont(const muse::draw::Font &f);

    double leftPadding() const override { return 0.0; }

    double rightPadding() const override { return 0.0; }

    double height() const override { return tightBoundingRect().height(); }

    HarmonyRenderItemType type() const override { return HarmonyRenderItemType::TEXT; }

    TextSegment(const String &s, const muse::draw::Font &f, double _x, double _y, bool align) : HarmonyRenderItem(align, _x, _y) {
        setText(s);
        setFont(f);
    }

private:
    muse::draw::Font font_;
    String text_;
};

class HDegree;

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

class HarmonyInfo {
public:
    HarmonyInfo(int id, int rootTpc, int bassTpc, String textName, ParsedChord *pc, Score *score)
        : id_(id), rootTpc_(rootTpc), bassTpc_(bassTpc), textName_(textName), parsedChord_(pc), score_(score) {}

    HarmonyInfo(Score *score) : score_(score) {}

    HarmonyInfo(const HarmonyInfo &h);
    ~HarmonyInfo();

    int id() const { return id_; }

    void setId(int v) { id_ = v; }

    int rootTpc() const { return rootTpc_; }

    void setRootTpc(int v) { rootTpc_ = v; }

    int bassTpc() const { return bassTpc_; }

    void setBassTpc(int v) { bassTpc_ = v; }

    String textName() const { return textName_; }

    void setTextName(const String &v) { textName_ = v; }

    ChordList *chordList() const { return score_ ? score_->chordList() : nullptr; }

    const ChordDescription *descr() const;
    const ChordDescription *descr(const String &, const ParsedChord *pc = 0) const;
    const ChordDescription *getDescription();
    const ChordDescription *getDescription(const String &, const ParsedChord *pc = 0);
    const ChordDescription *generateDescription();

    void setParsedChord(ParsedChord *v) { parsedChord_ = v; }

    ParsedChord *parsedChord() const { return parsedChord_; }

    ParsedChord *getParsedChord();

    bool hasModifiers() const;

private:
    int id_ = -1;                    // >0 = id of matched chord from chord list, if applicable
                                     // -1 = invalid chord
                                     // <-10000 = private id of generated chord or matched chord with no id
    int rootTpc_ = Tpc::TPC_INVALID; // root note for chord
    int bassTpc_ = Tpc::TPC_INVALID; // bass note or chord bass; used for "slash" chords
                                     // or notation of bass note in chord

    String textName_;                    // name recognized from chord list, read from score file, or constructed from imported source
                                         // Also stores the whole RNA string to be rendered
    ParsedChord *parsedChord_ = nullptr; // parsed form of chord
    Score *score_ = nullptr;
};

//---------------------------------------------------------
//   @@ Harmony
//      This DOM class represents the chord symbol in the score. It contains the necessary information to
//      layout standard chords and polychords
//---------------------------------------------------------

class Harmony final : public TextBase {
    OBJECT_ALLOCATOR(engraving, Harmony)
    DECLARE_CLASSOF(ElementType::HARMONY)

public:
    Harmony(EngravingObject *parent = 0); // Segment or FretDiagram
    Harmony(const Harmony &);
    ~Harmony();

    Harmony *clone() const override { return new Harmony(*this); }

    int id() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED

    bool play() const { return play_; }

    void setBassCase(NoteCaseType c) { bassCase_ = c; }

    NoteCaseType bassCase() const { return bassCase_; }

    void setRootCase(NoteCaseType c) { rootCase_ = c; }

    NoteCaseType rootCase() const { return rootCase_; }

    Segment *getParentSeg() const;
    FretDiagram *getParentFretDiagram() const;
    Harmony *findNext() const;
    Harmony *findPrev() const;
    Fraction ticksTillNext(int utick, bool stopAtMeasureEnd = false) const;

    RealizedHarmony &realizedHarmony();
    const RealizedHarmony &getRealizedHarmony() const;

    bool isPlayable() const override;

    bool isRealizable() const;
    bool isInFretBox() const;

    int bassTpc() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED
    int rootTpc() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED
    void addDegree(const HDegree &d);
    const std::vector<HDegree> &degreeList() const;

    HarmonyType harmonyType() const { return harmonyType_; }

    void setHarmonyType(HarmonyType val);

    const ParsedChord *parsedForm() const; // WILL BE DEPRECATED AFTER RelaizedHarmony IS UPDATED

    void afterRead();
    void render();

    bool isPolychord() const { return chords_.size() > 1; }

    const std::vector<HarmonyInfo *> chords() const { return chords_; }

    void addChord(HarmonyInfo *info) { chords_.push_back(info); }

    bool hasModifiers() const;

    String displayText() const;
    String harmonyName() const;

    double baseLine() const override;
    void spatiumChanged(double oldValue, double newValue) override;
    void localSpatiumChanged(double oldValue, double newValue) override;
    void setHarmony(const String &s);

    int subtype() const override { return static_cast<int>(harmonyType()); }

    TranslatableString typeUserName() const override;

    String generateScreenReaderInfo() const;

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    using EngravingObject::undoChangeProperty;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue propertyDefault(Pid id) const override;

    bool positionRelativeToNoteheadRest() const override;

    double mag() const override;

    double bassScale() const { return bassScale_; }

    void setBassScale(double v) { bassScale_ = v; }

    Color curColor(const rendering::PaintOptions &opt) const override;
    void setColor(const Color &color) override;

    bool doNotStackModifiers() const { return doNotStackModifiers_; }

    NoteCaseType rootRenderCase(HarmonyInfo *info) const;
    NoteCaseType bassRenderCase() const;

    FontStyle fontStyle() const override { return fontStyle_; }

    String family() const override { return fontFamily_; }

    double size() const override { return fontSize_; }

    void setFontStyle(const FontStyle &val) override { fontStyle_ = val; }

    void setFamily(const String &val) override { fontFamily_ = val; }

    void setSize(const double &val) override { fontSize_ = val; }

    struct LayoutData : public TextBase::LayoutData {
        ld_field<double> harmonyHeight = {"[Harmony] harmonyHeight", 0.0}; // used for calculating the height is frame while editing.
        ld_field<std::vector<LineF>> polychordDividerLines = {"[Harmony] polychordDividerLine", std::vector<LineF>()};
        ld_field<double> polychordDividerOffset = {"[Harmony] polychordDividerOffset", 0.0};
        ld_field<double> baseline = {"[Harmony] baseline", 0.0};
        ld_field<std::vector<muse::draw::Font>> fontList = "[Harmony] fontList";                // temp values used in render()
        ld_field<std::vector<HarmonyRenderItem *>> renderItemList = "[Harmony] renderItemList"; // rendered chord
    };
    DECLARE_LAYOUTDATA_METHODS(Harmony)

private:
    std::vector<HarmonyInfo *> chords_;

    const std::vector<const ChordDescription *> parseHarmony(const String &s, bool syntaxOnly = false);
    const ChordDescription *parseSingleHarmony(const String &s, HarmonyInfo *info, bool syntaxOnly = false);

    Sid getPropertyStyle(Pid) const override;

    Harmony *findInSeg(Segment *seg) const;

    bool isMisspelled_ = false;                       // show spell check warning
    HarmonyType harmonyType_ = HarmonyType::STANDARD; // used to control rendering, transposition, export, etc.

    mutable RealizedHarmony realizedHarmony_; // the realized harmony used for playback

    std::vector<HDegree> degreeList_;

    bool play_ = true; // whether or not to play back the harmony
    bool doNotStackModifiers_ = false;

    NoteCaseType rootCase_ = NoteCaseType::AUTO;
    NoteCaseType bassCase_ = NoteCaseType::AUTO; // case as typed

    double bassScale_ = 1.0;

    // Overridden textbase properties to apply to whole item
    double fontSize_ = 10.0;
    String fontFamily_ = u"";
    FontStyle fontStyle_;
};
} // namespace mu::engraving
