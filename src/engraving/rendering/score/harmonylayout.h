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

#include <stack>
#include <vector>

#include "layoutcontext.h"
#include "dom/harmony.h"

namespace mu::engraving {
struct Segment;
struct System;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct HarmonyLayout {
    static void layout_harmony(Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx);

private:

    struct HarmonyRenderCtx {
        PointF pos = PointF(0.0, 0.0);
        std::vector<HarmonyRenderItem *> render_item_list;

        // Reset every single chord
        bool h_align = true;
        HarmonyInfo *info = nullptr;

        // Reset every render() call
        std::stack<PointF> stack;
        int tpc = Tpc::TPC_INVALID;
        NoteSpellingType note_spelling = NoteSpellingType::STANDARD;
        NoteCaseType note_case = NoteCaseType::AUTO;
        double scale = 1.0;

        double x() const { return pos.x(); }

        double y() const { return pos.y(); }

        void setx(double v) { pos.set_x(v); }

        void movex(double v) { pos.set_x(pos.x() + v); }

        void sety(double v) { pos.set_y(v); }

        void movey(double v) { pos.set_y(pos.y() + v); }
    };

    static PointF calculate_bounding_rect(const Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_modifier_parentheses(const Harmony *item, const LayoutContext &ctx);

    static void render(Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx);
    static void do_render_single_harmony(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx, int root_tpc,
                                         int bass_tpc, const LayoutContext &ctx);
    static void render_single_harmony(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx);
    static void render_roman_numeral(Harmony *item, Harmony::LayoutData *ldata);
    static void render(Harmony *item, Harmony::LayoutData *ldata, const String &str, HarmonyRenderCtx &harmony_ctx);
    static void render(Harmony *item, Harmony::LayoutData *ldata, SymId sym, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx);
    static void render(Harmony *item, Harmony::LayoutData *ldata, const std::vector<RenderActionPtr> &render_list,
                       HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx, int tpc,
                       NoteSpellingType note_spelling = NoteSpellingType::STANDARD, NoteCaseType note_case = NoteCaseType::AUTO,
                       double note_mag = 1.0);

    static void render_action(Harmony *item, Harmony::LayoutData *ldata, const RenderActionPtr &a, HarmonyRenderCtx &harmony_ctx,
                              const LayoutContext &ctx);
    static void render_action_set(Harmony *item, Harmony::LayoutData *ldata, const RenderActionSetPtr &a, HarmonyRenderCtx &harmony_ctx,
                                  const LayoutContext &ctx);
    static void render_action_move(Harmony *item, const RenderActionMovePtr &a, HarmonyRenderCtx &harmony_ctx);
    static void render_action_move_x_height(Harmony *item, const RenderActionMoveXHeightPtr &a, HarmonyRenderCtx &harmony_ctx);
    static void render_action_push(HarmonyRenderCtx &harmony_ctx);
    static void render_action_pop(const RenderActionPopPtr &a, HarmonyRenderCtx &harmony_ctx);
    static void render_action_note(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx);
    static void render_action_acc(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx);
    static void render_action_align(HarmonyRenderCtx &harmony_ctx);
    static void render_action_scale(const RenderActionScalePtr &a, HarmonyRenderCtx &harmony_ctx);
    static void render_action_paren(Harmony *item, const RenderActionParenPtr &a, HarmonyRenderCtx &harmony_ctx);

    static void kern_characters(const Harmony *item, const String &text, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx);
};
} // namespace mu::engraving::rendering::score
