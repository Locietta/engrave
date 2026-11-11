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
#include "scorerenderer.h"

#include "chordlayout.h"
#include "dom/arpeggio.h"
#include "dom/beam.h"
#include "dom/chord.h"
#include "dom/harmony.h"
#include "dom/textlinebase.h"
#include "headerfooterlayout.h"
#include "horizontalspacing.h"
#include "layoutcontext.h"
#include "log.h"
#include "paint.h"
#include "scorelayout.h"
#include "scorepageviewlayout.h"
#include "slurtielayout.h"
#include "tdraw.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void ScoreRenderer::layout_score(Score *score, const Fraction &st, const Fraction &et) const { ScoreLayout::layout_range(score, st, et); }

void ScoreRenderer::layout_headers_footers(Score *score) const {
    LayoutContext ctx(score);
    if (!ctx.conf().is_mode(LayoutMode::PAGE) && !ctx.conf().is_mode(LayoutMode::FLOAT)) {
        return;
    }

    ScorePageViewLayout::layout_headers_footers(ctx);
}

SizeF ScoreRenderer::page_size_inch(const Score *score) const { return Paint::page_size_inch(score); }

SizeF ScoreRenderer::page_size_inch(const Score *score, const ScorePaintOptions &opt) const { return Paint::page_size_inch(score, opt); }

void ScoreRenderer::paint_score(Painter *painter, Score *score, const ScorePaintOptions &opt) const {
    Paint::paint_score(painter, score, opt);
}

void ScoreRenderer::paint_item(Painter &painter, const EngravingItem *item, const PaintOptions &opt) const {
    Paint::paint_item(painter, item, opt);
}

void ScoreRenderer::do_layout_item(EngravingItem *item) {
    LayoutContext ctx(item->score());
    TLayout::layout_item(item, ctx);
}

void ScoreRenderer::do_draw_item(const EngravingItem *item, Painter *p, const PaintOptions &opt) { TDraw::draw_item(item, p, opt); }

void ScoreRenderer::layout_text1(TextBase *item, bool base) {
    LayoutContext ctx(item->score());
    if (base) {
        TextLayout::layout_base_text_base1(item, ctx);
    } else if (Harmony::classof(item)) {
        TLayout::layout_harmony(to_harmony(item), to_harmony(item)->mutldata(), ctx);
    } else {
        TextLayout::layout_base_text_base1(item, ctx);
    }
}

void ScoreRenderer::compute_bezier(TieSegment *tie_seg, PointF shoulder_offset) { SlurTieLayout::compute_bezier(tie_seg, shoulder_offset); }

void ScoreRenderer::compute_bezier(SlurSegment *slur_seg, PointF shoulder_offset) {
    SlurTieLayout::compute_bezier(slur_seg, shoulder_offset);
}

void ScoreRenderer::layout_text_line_base_segment(TextLineBaseSegment *item) {
    LayoutContext ctx(item->score());
    TLayout::layout_text_line_base_segment(item, ctx);
}

void ScoreRenderer::layout_beam1(Beam *item) {
    LayoutContext ctx(item->score());
    TLayout::layout_beam1(item, ctx);
}

void ScoreRenderer::layout_stem(Chord *item) {
    LayoutContext ctx(item->score());
    ChordLayout::layout_stem(item, ctx);
}

bool ScoreRenderer::score_has_timestamp_headers_footers(const Score *score) const {

    return HeaderFooterLayout::score_has_timestamp_headers_footers(score);
}
