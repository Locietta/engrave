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
#include "readchordlisthook.h"

#include "style/style.h"

#include "dom/score.h"

#include "log.h"

using namespace mu::engraving::compat;
using namespace mu::engraving;

ReadChordListHook::ReadChordListHook(Score *score) : score_(score) {
    if (score_) {
        old_chord_description_file_ = score_->style().style_st(Sid::CHORD_DESCRIPTION_FILE);
    }
}

void ReadChordListHook::read(XmlReader &e) {
    if (!score_) {
        return;
    }

    score_->chord_list()->clear();
    score_->chord_list()->read(e, score_->msc_version());
    score_->chord_list()->set_custom_chord_list(true);

    chord_list_tag_ = true;
}

void ReadChordListHook::validate() {
    if (!score_) {
        return;
    }

    if (chord_list_tag_) {
        // if we encountered a ChordList tag, everything is fine already
        return;
    }

    // if we just specified a new chord description file
    // and didn't encounter a ChordList tag
    // then load the chord description file

    MStyle &style = score_->style();
    ChordList *chord_list = score_->chord_list();

    String new_chord_description_file = style.style_st(Sid::CHORD_DESCRIPTION_FILE);
    if (new_chord_description_file != old_chord_description_file_) {
        if (!new_chord_description_file.starts_with(u"chords_") &&
            style.style_v(Sid::CHORD_STYLE).value<ChordStylePreset>() == ChordStylePreset::STANDARD) {
            // should not normally happen,
            // but treat as "old" (114) score just in case
            style.set(Sid::CHORD_STYLE, ChordStylePreset::CUSTOM);
            style.set(Sid::CHORDS_XML_FILE, true);
            LOGD("StyleData::load: custom chord description file %s with chordStyle == std", muPrintable(new_chord_description_file));
        }

        bool custom = style.style_v(Sid::CHORD_STYLE).value<ChordStylePreset>() == ChordStylePreset::CUSTOM;
        chord_list->set_custom_chord_list(custom);

        chord_list->unload();
    }

    // make sure we have a chordlist
    chord_list->check_chord_list(style);
}
