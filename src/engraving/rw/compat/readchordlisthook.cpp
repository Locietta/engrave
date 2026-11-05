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
        oldChordDescriptionFile_ = score_->style().styleSt(Sid::chordDescriptionFile);
    }
}

void ReadChordListHook::read(XmlReader &e) {
    if (!score_) {
        return;
    }

    score_->chordList()->clear();
    score_->chordList()->read(e, score_->mscVersion());
    score_->chordList()->setCustomChordList(true);

    chordListTag_ = true;
}

void ReadChordListHook::validate() {
    if (!score_) {
        return;
    }

    if (chordListTag_) {
        // if we encountered a ChordList tag, everything is fine already
        return;
    }

    // if we just specified a new chord description file
    // and didn't encounter a ChordList tag
    // then load the chord description file

    MStyle &style = score_->style();
    ChordList *chordList = score_->chordList();

    String newChordDescriptionFile = style.styleSt(Sid::chordDescriptionFile);
    if (newChordDescriptionFile != oldChordDescriptionFile_) {
        if (!newChordDescriptionFile.startsWith(u"chords_") &&
            style.styleV(Sid::chordStyle).value<ChordStylePreset>() == ChordStylePreset::STANDARD) {
            // should not normally happen,
            // but treat as "old" (114) score just in case
            style.set(Sid::chordStyle, ChordStylePreset::CUSTOM);
            style.set(Sid::chordsXmlFile, true);
            LOGD("StyleData::load: custom chord description file %s with chordStyle == std", muPrintable(newChordDescriptionFile));
        }

        bool custom = style.styleV(Sid::chordStyle).value<ChordStylePreset>() == ChordStylePreset::CUSTOM;
        chordList->setCustomChordList(custom);

        chordList->unload();
    }

    // make sure we have a chordlist
    chordList->checkChordList(style);
}
