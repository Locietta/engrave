/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include "../types/types.h"
#include "paintoptions.h"

namespace muse::draw {
struct Painter;
} // namespace muse::draw

namespace mu::engraving {
struct Score;
struct EngravingItem;
struct Page;

struct Accidental;
struct ActionIcon;
struct Arpeggio;

struct BarLine;
struct Beam;
struct Box;
struct Bracket;

struct Chord;
struct Clef;

struct Dynamic;

struct EngravingItem;

struct FiguredBassItem;

struct FretDiagram;

struct Harmony;

struct Image;

struct IndicatorIcon;

enum class KerningType : unsigned char;
struct KeySig;

struct LedgerLine;
struct SLine;
struct LineSegment;
struct Lyrics;

struct NoteDot;

struct Parenthesis;

struct Rest;

struct ShadowNote;
struct Spanner;
struct Slur;
struct SlurSegment;
struct SlurTie;
struct Spacer;
struct StaffText;
struct Stem;

struct TextBase;
struct Text;
struct TextLineBaseSegment;
struct TieSegment;
struct TimeSig;
} // namespace mu::engraving

namespace mu::engraving::rendering {
struct IScoreRenderer {
    virtual ~IScoreRenderer() = default;

    // Main interface

    virtual void layout_score(Score *score, const Fraction &st, const Fraction &et) const = 0;
    virtual void layout_headers_footers(Score *score) const = 0;

    struct ScorePaintOptions : public PaintOptions {
        bool is_set_viewport = true;
        bool is_multi_page = false;
        bool print_page_background = true;
        RectF frame_rect;
        int from_page = -1; // 0 is first
        int to_page = -1;
        int copy_count = 1;
        int trim_margin_pixel_size = -1;
        int device_dpi = -1;

        std::function<void(muse::draw::Painter *painter, const Page *page, const RectF &page_rect)> on_paint_page_sheet;
        std::function<void()> on_new_page;
    };

    virtual SizeF page_size_inch(const Score *score) const = 0;
    virtual SizeF page_size_inch(const Score *score, const ScorePaintOptions &opt) const = 0;
    virtual void paint_score(muse::draw::Painter *painter, Score *score, const ScorePaintOptions &opt) const = 0;
    virtual void paint_item(muse::draw::Painter &painter, const EngravingItem *item, const PaintOptions &opt) const = 0;

    // Temporary compatibility interface
    using Supported = std::variant<std::monostate, Accidental *, ActionIcon *, Arpeggio *, BarLine *, Box *, Bracket *, Clef *, Dynamic *,
                                   FiguredBassItem *, FretDiagram *, Harmony *, Image *, IndicatorIcon *, KeySig *, LedgerLine *, SLine *,
                                   LineSegment *, Lyrics *, NoteDot *, Parenthesis *, Rest *, ShadowNote *, Spanner *, Slur *, SlurTie *,
                                   Spacer *, StaffText *, Stem *, TextBase *, Text *, TimeSig *>;

    template <typename T>
    static void check_supported_static(T item) {
        if constexpr (std::is_same<T, EngravingItem *>::value) {
            // supported
        } else {
            [[maybe_unused]] Supported check(item);
        }
    }

    template <typename T>
    void layout_item(T item) {
#ifndef NDEBUG
        check_supported_static(item);
#endif
        do_layout_item(static_cast<EngravingItem *>(item));
    }

    //! TODO Investigation is required, probably these functions or their calls should not be.
    // Other
    virtual void layout_text_line_base_segment(TextLineBaseSegment *item) = 0;
    virtual void layout_beam1(Beam *item) = 0;
    virtual void layout_stem(Chord *item) = 0;

    // Layout Text 1
    virtual void layout_text1(TextBase *item, bool base = false) = 0;

    void draw_item(const EngravingItem *item, muse::draw::Painter *p, const PaintOptions &opt) { do_draw_item(item, p, opt); }

    virtual void compute_bezier(TieSegment *tie_seg, PointF shoulder_offset = PointF()) = 0;
    virtual void compute_bezier(SlurSegment *slur_seg, PointF shoulder_offser = PointF()) = 0;

    virtual bool score_has_timestamp_headers_footers(const Score *score) const = 0;

private:
    // Layout Single Item
    virtual void do_layout_item(EngravingItem *item) = 0;

    virtual void do_draw_item(const EngravingItem *item, muse::draw::Painter *p, const PaintOptions &opt) = 0;
};
} // namespace mu::engraving::rendering
