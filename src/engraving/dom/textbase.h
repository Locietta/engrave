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

#include <variant>

#include "draw/fontmetrics.h"

#include "../iengravingfontsprovider.h"

#include "engravingitem.h"
#include "property.h"
#include "../types/types.h"

namespace mu::engraving {
class TextBase;
class TextBlock;
class Transaction;

static constexpr double SUBSCRIPT_SIZE = 0.6;
static constexpr double SUBSCRIPT_OFFSET = 0.5;    // of x-height
static constexpr double SUPERSCRIPT_OFFSET = -0.9; // of x-height

//---------------------------------------------------------
//   FrameType
//---------------------------------------------------------

enum class FrameType : char { NO_FRAME, RECTANGLE, CIRCLE };

//---------------------------------------------------------
//   VerticalAlignment
//---------------------------------------------------------

enum class VerticalAlignment : signed char { AlignUndefined = -1, AlignNormal, AlignSuperScript, AlignSubScript };

//---------------------------------------------------------
//   FormatId
//---------------------------------------------------------

enum class FormatId : char { Bold, Italic, Underline, Strike, Valign, FontSize, FontFamily };

using FormatValue = std::variant<std::monostate, bool, int, double, String>;

//---------------------------------------------------------
//   MultiClick
//---------------------------------------------------------

enum class SelectTextType : char { Word, All };

//---------------------------------------------------------
//   CharFormat
//---------------------------------------------------------

class CharFormat {
public:
    CharFormat() {}

    CharFormat(const CharFormat &cf) { *this = cf; }

    bool operator==(const CharFormat &cf) const;

    bool operator!=(const CharFormat &cf) const { return !(*this == cf); }

    CharFormat &operator=(const CharFormat &cf);

    FontStyle style() const { return style_; }

    void setStyle(FontStyle s) { style_ = s; }

    bool bold() const { return style_ & FontStyle::Bold; }

    bool italic() const { return style_ & FontStyle::Italic; }

    bool underline() const { return style_ & FontStyle::Underline; }

    bool strike() const { return style_ & FontStyle::Strike; }

    void setBold(bool val) { style_ = val ? style_ + FontStyle::Bold : style_ - FontStyle::Bold; }

    void setItalic(bool val) { style_ = val ? style_ + FontStyle::Italic : style_ - FontStyle::Italic; }

    void setUnderline(bool val) { style_ = val ? style_ + FontStyle::Underline : style_ - FontStyle::Underline; }

    void setStrike(bool val) { style_ = val ? style_ + FontStyle::Strike : style_ - FontStyle::Strike; }

    VerticalAlignment valign() const { return valign_; }

    double fontSize() const { return fontSize_; }

    String fontFamily() const { return fontFamily_; }

    void setValign(VerticalAlignment val) { valign_ = val; }

    void setFontSize(double val) { fontSize_ = val; }

    void setFontFamily(const String &val) { fontFamily_ = val; }

    FormatValue formatValue(FormatId) const;
    void setFormatValue(FormatId, const FormatValue &val);

private:

    FontStyle style_ = FontStyle::Normal;
    VerticalAlignment valign_ = VerticalAlignment::AlignNormal;
    double fontSize_ = 12.0;
    String fontFamily_;
};

//---------------------------------------------------------
//   TextCursor
//    Contains current position and start of selection
//    during editing.
//---------------------------------------------------------

class TextCursor {
public:
    enum class MoveOperation : unsigned char { Start, Up, StartOfLine, Left, WordLeft, End, Down, EndOfLine, NextWord, Right };

    enum class MoveMode : unsigned char { MoveAnchor, KeepAnchor };

    TextCursor(TextBase *t) : text_(t) {}

    TextBase *text() const { return text_; }

    bool hasSelection() const { return (selectLine_ != row_) || (selectColumn_ != column_); }

    void clearSelection();
    void endEdit();
    void startEdit();

    bool editing() const { return editing_; }

    bool visible() const { return visible_; }

    void setVisible(bool val) { visible_ = val; }

    void toggleVisible() { setVisible(!visible_); }

    CharFormat *format() { return &format_; }

    const CharFormat *format() const { return &format_; }

    void setFormat(const CharFormat &f) { format_ = f; }

    size_t row() const { return row_; }

    size_t column() const { return column_; }

    size_t selectLine() const { return selectLine_; }

    size_t selectColumn() const { return selectColumn_; }

    void setRow(size_t val) { row_ = val; }

    void setColumn(size_t val) { column_ = val; }

    void setSelectLine(size_t val) { selectLine_ = val; }

    void setSelectColumn(size_t val) { selectColumn_ = val; }

    size_t columns() const;
    void init();

    struct Range {
        int startPosition = 0;
        int endPosition = 0;
        String text;
    };

    std::pair<size_t, size_t> positionToLocalCoord(int position) const;

    int currentPosition() const;
    Range selectionRange() const;

    const TextBlock &curLine() const;
    TextBlock &curLine();

    RectF cursorRect() const;
    RectF cursorCanvasRect() const;
    bool movePosition(TextCursor::MoveOperation op, TextCursor::MoveMode mode = TextCursor::MoveMode::MoveAnchor, int count = 1);
    void selectWord();

    void moveCursorToEnd() { movePosition(TextCursor::MoveOperation::End); }

    void moveCursorToStart() { movePosition(TextCursor::MoveOperation::Start); }

    Char currentCharacter() const;
    bool set(const PointF &p, TextCursor::MoveMode mode = TextCursor::MoveMode::MoveAnchor);
    String selectedText(bool withFormat = false) const;
    String extractText(int r1, int c1, int r2, int c2, bool withFormat = false) const;
    void updateCursorFormat();
    void setFormat(FormatId, FormatValue val);
    void changeSelectionFormat(FormatId id, const FormatValue &val);
    const CharFormat selectedFragmentsFormat() const;

private:
    Range range(int start, int end) const;
    int position(int row, int column) const;

    TextBase *text_ = nullptr;
    CharFormat format_;
    size_t row_ = 0;
    size_t column_ = 0;
    size_t selectLine_ = 0; // start of selection
    size_t selectColumn_ = 0;
    bool editing_ = false;
    bool visible_ = false;
};

//---------------------------------------------------------
//   TextFragment
//    contains a styled text
//---------------------------------------------------------

class TextFragment {
public:

public:
    mutable CharFormat format;
    PointF pos; // y is relative to TextBlock->y()
    mutable String text;

    TextFragment() = default;
    TextFragment(const String &s);
    TextFragment(TextCursor *, const String &);
    TextFragment(const TextFragment &f);

    TextFragment &operator=(const TextFragment &f);

    bool operator==(const TextFragment &f) const;

    TextFragment split(int column);
    muse::draw::Font font(const TextBase *) const;
    double calculatedFontSize(const TextBase *) const;
    int columns() const;
    void changeFormat(FormatId id, const FormatValue &data);

private:
    void resolveFallback(muse::draw::Font::Type fontType, const muse::draw::FontMetrics &fm, String &family) const;
};

//---------------------------------------------------------
//   TextBlock
//    represents a block of formatted text
//---------------------------------------------------------

class TextBlock {
public:
    TextBlock() = default;

    bool operator==(const TextBlock &x) const { return fragments_ == x.fragments_; }

    bool operator!=(const TextBlock &x) const { return fragments_ != x.fragments_; }

    const std::list<TextFragment> &fragments() const { return fragments_; }

    std::list<TextFragment> &fragments() { return fragments_; }

    std::list<TextFragment> fragmentsWithoutEmpty();

    const Shape &shape() const { return shape_; }

    Shape &shape() { return shape_; }

    const RectF &boundingRect() const { return shape_.bbox(); }

    RectF boundingRect(int col1, int col2, const TextBase *) const;
    size_t columns() const;
    void insert(TextCursor *, const String &);
    void insertEmptyFragmentIfNeeded(TextCursor *);
    void removeEmptyFragment();
    String remove(int column, TextCursor *);
    String remove(int start, int n, TextCursor *);
    int column(double x, TextBase *) const;
    TextBlock split(int column, TextCursor *cursor);
    double xpos(size_t col, const TextBase *) const;
    const CharFormat *formatAt(int) const;
    const TextFragment *fragment(int col) const;
    std::list<TextFragment>::iterator fragment(int column, int *rcol, int *ridx);

    double y() const { return y_; }

    void setY(double val) { y_ = val; }

    double lineSpacing() const { return lineSpacing_; }

    void setLineSpacing(double val) { lineSpacing_ = val; }

    String text(int, int, const TextBase *, bool = false) const;

    bool eol() const { return eol_; }

    void setEol(bool val) { eol_ = val; }

    void changeFormat(FormatId, const FormatValue &val, int start, int n);

private:
    void simplify();

    std::list<TextFragment> fragments_;
    double y_ = 0.0;
    double lineSpacing_ = 0.0;
    Shape shape_;
    bool eol_ = false;
};

//---------------------------------------------------------
//   TextBase
//---------------------------------------------------------

class TextBase : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, TextBase)

    M_PROPERTY2(bool, isTextLinkedToMaster, setTextLinkedToMaster, true)

public:

    ~TextBase();

    Text &operator=(const Text &) = delete;

    Align align() const { return align_; }

    void setAlign(Align a) { align_ = a; }

    AlignH position() const { return position_; }

    void setPosition(AlignH val) { position_ = val; }

    static String plainToXmlText(const String &s) { return s.toXmlEscaped(); }

    void setPlainText(const String &t) { setXmlText(plainToXmlText(t)); }

    virtual void setXmlText(const String &);

    void setXmlText(const char *str) { setXmlText(String::fromUtf8(str)); }

    void checkCustomFormatting(const String &);
    String xmlText() const;
    String plainText() const;
    void resetFormatting();

    void insertText(const String &);

    double lineSpacing() const;
    double lineHeight() const;
    virtual double baseLine() const override;

    bool empty() const { return xmlText().isEmpty(); }

    void clear() { setXmlText(String()); }

    virtual FontStyle fontStyle() const;
    virtual String family() const;
    virtual double size() const;

    virtual void setFontStyle(const FontStyle &val);
    virtual void setFamily(const String &val);
    virtual void setSize(const double &val);

    bool anchorToEndOfPrevious() const { return anchorToEndOfPrevious_; }

    void setAnchorToEndOfPrevious(bool v) { anchorToEndOfPrevious_ = v; }

    bool hasParentSegment() const { return ownershipParent() && parent()->isSegment(); }

    virtual bool allowTimeAnchor() const override { return hasParentSegment(); }

    void movePosition(TextCursor::MoveOperation);

    bool deleteSelectedText();

    void selectAll();
    void select(SelectTextType);

    bool isPrimed() const { return primed_; }

    void setPrimed(bool primed) { primed_ = primed; }

    virtual void paste(const String &txt);

    RectF pageRectangle() const;

    const Shape &highResShape() const { return ldata()->highResShape.value(); }

    virtual bool acceptDrop(EditData &) const override;
    virtual EngravingItem *drop(Transaction &tx, EditData &) override;

    friend class TextBlock;
    friend class TextFragment;

    static String unEscape(String s);
    static String escape(String s);

    int subtype() const override;
    TranslatableString subtypeUserName() const override;

    std::list<TextFragment> fragmentList() const; // for MusicXML formatted export

    static bool validateText(String &s);

    bool inHexState() const { return hexState_ >= 0; }

    void endHexState();

    muse::draw::Font font() const;
    muse::draw::FontMetrics fontMetrics() const;

    bool isPropertyLinkedToMaster(Pid id) const override;
    bool isUnlinkedFromMaster() const override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;
    PropertyValue propertyDefault(Pid id) const override;
    void undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    Sid getPropertyStyle(Pid) const override;
    void styleChanged() override;
    void editInsertText(TextCursor *, const String &);

    TextCursor *cursor() const { return cursor_; }

    void setTextInvalid() { textInvalid_ = true; }

    bool isTextInvalid() const { return textInvalid_; }

    // helper functions
    bool hasFrame() const { return frameType_ != FrameType::NO_FRAME; }

    bool circle() const { return frameType_ == FrameType::CIRCLE; }

    bool rectangle() const { return frameType_ == FrameType::RECTANGLE; }

    TextStyleType textStyleType() const { return textStyleType_; }

    void setTextStyleType(TextStyleType id) { textStyleType_ = id; }

    void initTextStyleType(TextStyleType id);
    void initTextStyleType(TextStyleType id, bool preserveDifferent);
    virtual void initElementStyle(const ElementStyle *) override;

    static const String UNDEFINED_FONT_FAMILY;
    static const double UNDEFINED_FONT_SIZE;

    static bool isSorted(size_t r1, size_t c1, size_t r2, size_t c2);
    static void sort(size_t &r1, size_t &c1, size_t &r2, size_t &c2);

    bool bold() const { return fontStyle() & FontStyle::Bold; }

    bool italic() const { return fontStyle() & FontStyle::Italic; }

    bool underline() const { return fontStyle() & FontStyle::Underline; }

    bool strike() const { return fontStyle() & FontStyle::Strike; }

    void setBold(bool val) { setFontStyle(val ? fontStyle() + FontStyle::Bold : fontStyle() - FontStyle::Bold); }

    void setItalic(bool val) { setFontStyle(val ? fontStyle() + FontStyle::Italic : fontStyle() - FontStyle::Italic); }

    void setUnderline(bool val) { setFontStyle(val ? fontStyle() + FontStyle::Underline : fontStyle() - FontStyle::Underline); }

    void setStrike(bool val) { setFontStyle(val ? fontStyle() + FontStyle::Strike : fontStyle() - FontStyle::Strike); }

    bool hasCustomFormatting() const;

    friend class TextCursor;
    using EngravingObject::undoChangeProperty;

    Color textColor(const rendering::PaintOptions &opt) const;

    FrameType frameType() const { return frameType_; }

    void setFrameType(FrameType val) { frameType_ = val; }

    double textLineSpacing() const { return textLineSpacing_; }

    void setTextLineSpacing(double val) { textLineSpacing_ = val; }

    Color bgColor() const { return bgColor_; }

    void setBgColor(const Color &val) { bgColor_ = val; }

    Color frameColor() const { return frameColor_; }

    void setFrameColor(const Color &val) { frameColor_ = val; }

    Spatium frameWidth() const { return frameWidth_; }

    void setFrameWidth(Spatium val) { frameWidth_ = val; }

    Spatium paddingWidth() const { return paddingWidth_; }

    void setPaddingWidth(Spatium val) { paddingWidth_ = val; }

    Spatium frameRound() const { return frameRound_; }

    void setFrameRound(Spatium val) { frameRound_ = val; }

    struct LayoutData : public EngravingItem::LayoutData {
        std::vector<TextBlock> blocks;
        bool layoutInvalid = true;

        RectF frame;

        size_t rows() const { return blocks.size(); }

        const TextBlock &textBlock(size_t i) const { return blocks.at(i); }

        TextBlock &textBlock(size_t i) { return blocks[i]; }

        ld_field<Shape> highResShape = {"[TextBase] highResShape", Shape()};
    };
    DECLARE_LAYOUTDATA_METHODS(TextBase)

    void createBlocks();
    void createBlocks(LayoutData *ldata) const;
    void layoutFrame();
    void layoutFrame(LayoutData *ldata) const;

    //! NOTE It can only be set for some types of text, see who has the setter.
    //! At the moment it's: Text, Jump, Marker
    bool layoutToParentWidth() const { return layoutToParentWidth_; }

    virtual bool positionRelativeToNoteheadRest() const = 0;

    void setVoiceAssignment(VoiceAssignment v) { voiceAssignment_ = v; }

    VoiceAssignment voiceAssignment() const { return voiceAssignment_; }

    void setDirection(DirectionV v) { direction_ = v; }

    DirectionV direction() const { return direction_; }

    void setCenterBetweenStaves(AutoOnOff v) { centerBetweenStaves_ = v; }

    AutoOnOff centerBetweenStaves() const { return centerBetweenStaves_; }

    void genText();

    double symbolSize() const { return symbolSize_; }

    void setSymbolSize(double v) { symbolSize_ = v; }

    double symbolScale() const { return symbolScale_; }

    void setSymbolScale(double v) { symbolScale_ = v; }

    bool hasSymbolScale() const;

    bool hasSymbolSize() const { return !hasSymbolScale(); }

    double spatiumScaling() const;

    double textAngle() const { return textAngle_; }

    void setTextAngle(double v) { textAngle_ = v; }

    PointF defaultPos() const override;

protected:
    TextBase(const ElementType &type, EngravingObject *parent = 0, TextStyleType tid = TextStyleType::DEFAULT,
             ElementFlags = ElementFlag::NOTHING);
    TextBase(const ElementType &type, EngravingObject *parent, ElementFlags);
    TextBase(const TextBase &);

    void insertSym(SymId id);
    void prepareFormat(const String &token, TextCursor &cursor);
    bool prepareFormat(const String &token, CharFormat &format);

    virtual void commitText();

    bool layoutToParentWidth_ = false;

private:
    void insert(TextCursor *, char32_t code, LayoutData *ldata) const;
    String genText(const LayoutData *ldata) const;

    virtual int getPropertyFlagsIdx(Pid id) const override;
    String stripText(bool, bool, bool) const;

    static String getHtmlStartTag(double, double &, const String &, String &, FontStyle, VerticalAlignment);
    static String getHtmlEndTag(FontStyle, VerticalAlignment);

    static void swap(size_t &r1, size_t &c1, size_t &r2, size_t &c2);

    Sid defaultPosSid() const override;

    void notifyAboutTextCursorChanged();
    void notifyAboutTextInserted(int startPosition, int endPosition, const String &text);
    void notifyAboutTextRemoved(int startPosition, int endPosition, const String &text);

    Align align_;

    AlignH position_ = AlignH::LEFT;

    FrameType frameType_ = FrameType::NO_FRAME;
    double textLineSpacing_ = 1.0;
    Color bgColor_;
    Color frameColor_;
    Spatium frameWidth_;
    Spatium paddingWidth_;
    Spatium frameRound_;

    // there are two representations of text; only one
    // might be valid and the other can be constructed from it

    String text_; // cached
    bool textInvalid_ = true;

    TextStyleType textStyleType_ = TextStyleType::DEFAULT; // text style id

    int hexState_ = -1;
    bool primed_ = 0;

    double symbolSize_ = 18.0;
    double symbolScale_ = 1.0;

    TextCursor *cursor_ = nullptr;

    VoiceAssignment voiceAssignment_ = VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
    DirectionV direction_ = DirectionV::AUTO;
    AutoOnOff centerBetweenStaves_ = AutoOnOff::AUTO;
    bool anchorToEndOfPrevious_ = false;

    double textAngle_ = 0.0;
};

} // namespace mu::engraving
