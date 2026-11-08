/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#include "chordarticulationsrenderer.h"

#include <array>

#include "arpeggiorenderer.h"
#include "bendsrenderer.h"
#include "gracechordcontext.h"
#include "noterenderer.h"
#include "ornamentsrenderer.h"
#include "playback/utils/expressionutils.h"
#include "tremolorenderer.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

const mpe::ArticulationTypeSet &ChordArticulationsRenderer::supported_types() {
    static mpe::ArticulationTypeSet supported_types;

    if (supported_types.empty()) {
        supported_types = ::mu::engraving::grace_note_articulation_types;

        supported_types.insert(OrnamentsRenderer::supported_types().cbegin(), OrnamentsRenderer::supported_types().cend());
        supported_types.insert(TremoloRenderer::supported_types().cbegin(), TremoloRenderer::supported_types().cend());
        supported_types.insert(ArpeggioRenderer::supported_types().cbegin(), ArpeggioRenderer::supported_types().cend());
    }

    return supported_types;
}

void ChordArticulationsRenderer::do_render(const EngravingItem *item, const mpe::ArticulationType /*type*/, const RenderingContext &ctx,
                                           mpe::PlaybackEventList &result) {
    IF_ASSERT_FAILED(item->is_chord()) { return; }

    const mpe::ArticulationType grace_type = select_grace_type(ctx.common_articulations);
    if (grace_type != mpe::ArticulationType::UNDEFINED) {
        render_chord_with_grace_chords(to_chord(item), grace_type, ctx, result);
        return;
    }

    render_chord(to_chord(item), ctx, result);
}

void ChordArticulationsRenderer::render_chord(const Chord *chord, const RenderingContext &ctx, muse::mpe::PlaybackEventList &result) {
    if (render_chord_articulations(chord, ctx, result)) {
        return;
    }

    const bool supports_multibend = ctx.profile->contains(ArticulationType::MULTIBEND);

    for (const Note *note : chord->notes()) {
        if (supports_multibend && BendsRenderer::is_multibend_part(note)) {
            BendsRenderer::render(note, ctx, result);
        } else {
            NoteRenderer::render(note, ctx, result);
        }
    }
}

void ChordArticulationsRenderer::render_chord_with_grace_chords(const Chord *chord, const muse::mpe::ArticulationType type,
                                                                const RenderingContext &ctx, muse::mpe::PlaybackEventList &result) {
    const GraceChordCtx grace_chord_ctx = GraceChordCtx::build_ctx(chord, type, ctx);

    if (is_grace_note_placed_before_principal_note(type)) {
        for (const auto &pair : grace_chord_ctx.grace_chord_ctx_list) {
            render_chord(pair.first, pair.second, result);
        }
        render_chord(chord, grace_chord_ctx.principal_chord_ctx, result);
    } else {
        render_chord(chord, grace_chord_ctx.principal_chord_ctx, result);
        for (const auto &pair : grace_chord_ctx.grace_chord_ctx_list) {
            render_chord(pair.first, pair.second, result);
        }
    }
}

//! A rolled chord stays rolled: arpeggios win over ornaments and tremolo, then ascending ArticulationType
static constexpr std::array<mpe::ArticulationType, 5> chord_articulation_precedence{
    mpe::ArticulationType::ARPEGGIO,
    mpe::ArticulationType::ARPEGGIO_UP,
    mpe::ArticulationType::ARPEGGIO_DOWN,
    mpe::ArticulationType::ARPEGGIO_STRAIGHT_UP,
    mpe::ArticulationType::ARPEGGIO_STRAIGHT_DOWN,
};

bool ChordArticulationsRenderer::render_chord_articulations(const Chord *chord, const RenderingContext &ctx,
                                                            mpe::PlaybackEventList &result) {
    const bool has_tremolo = chord->tremolo_single_chord() || chord->tremolo_two_chord();
    const mpe::ArticulationType type =
        mpe::select_articulation(ctx.common_articulations, chord_articulation_precedence, [chord, has_tremolo](mpe::ArticulationType t) {
            return OrnamentsRenderer::is_able_to_render(t) || (TremoloRenderer::is_able_to_render(t) && has_tremolo) ||
                   (ArpeggioRenderer::is_able_to_render(t) && chord->arpeggio());
        });

    if (type == mpe::ArticulationType::UNDEFINED) {
        return false;
    }

    if (OrnamentsRenderer::is_able_to_render(type)) {
        OrnamentsRenderer::render(chord, type, ctx, result);
    } else if (TremoloRenderer::is_able_to_render(type)) {
        TremoloRenderer::render(chord, type, ctx, result);
    } else {
        ArpeggioRenderer::render(chord, type, ctx, result);
    }

    return true;
}
