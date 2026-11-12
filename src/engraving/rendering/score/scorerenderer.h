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

#include <functional>
#include <type_traits>
#include <variant>

#include "engraving/rendering/paintoptions.h"
#include "engraving/types/types.h"

namespace muse::draw {
struct Painter;
} // namespace muse::draw

namespace mu::engraving {
struct Score;
struct EngravingItem;
struct Page;

struct Accidental;
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

namespace mu::engraving::rendering::score {
struct ScoreRenderer {
    // Test-only observation of item-level rendering entry points.
    static inline void (*layout_hook)(const EngravingItem *) = nullptr;

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

    // Temporary compatibility interface
    using Supported = std::variant<std::monostate, Accidental *, Arpeggio *, BarLine *, Box *, Bracket *, Clef *, Dynamic *,
                                   FiguredBassItem *, FretDiagram *, Harmony *, Image *, IndicatorIcon *, KeySig *, LedgerLine *, SLine *,
                                   LineSegment *, Lyrics *, NoteDot *, Parenthesis *, Rest *, Spanner *, Slur *, SlurTie *, Spacer *,
                                   StaffText *, Stem *, TextBase *, Text *, TimeSig *>;

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

    void draw_item(const EngravingItem *item, muse::draw::Painter *p, const PaintOptions &opt) { do_draw_item(item, p, opt); }

    // Main interface
    void layout_score(Score *score, const Fraction &st, const Fraction &et) const;
    void layout_headers_footers(Score *score) const;

    SizeF page_size_inch(const Score *score) const;
    SizeF page_size_inch(const Score *score, const ScorePaintOptions &opt) const;
    void paint_score(muse::draw::Painter *painter, Score *score, const ScorePaintOptions &opt) const;
    void paint_item(muse::draw::Painter &painter, const EngravingItem *item, const PaintOptions &opt) const;

    //! TODO Investigation is required, probably these functions or their calls should not be.
    // Other
    void layout_text_line_base_segment(TextLineBaseSegment *item);
    void layout_beam1(Beam *item);
    void layout_stem(Chord *item);

    // Layout Text 1
    void layout_text1(TextBase *item, bool base = false);

    void compute_bezier(TieSegment *tie_seg, PointF shoulder_offset = PointF());
    void compute_bezier(SlurSegment *slur_seg, PointF shoulder_offset = PointF());

    bool score_has_timestamp_headers_footers(const Score *score) const;

private:
    // Layout Single Item
    void do_layout_item(EngravingItem *item);

    void do_draw_item(const EngravingItem *item, muse::draw::Painter *p, const PaintOptions &opt);
};
} // namespace mu::engraving::rendering::score
