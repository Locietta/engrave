/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "editstyle.h"

#include "../dom/chordlist.h"
#include "../dom/score.h"
#include "editing/editpagelocks.h"
#include "editing/editsystemlocks.h"
#include "editing/transaction/transaction.h"

#include "log.h"

using namespace mu::engraving;

static void change_chord_style(Score *score) {
    const MStyle &style = score->style();
    score->chord_list()->unload();
    double emag = style.style_d(Sid::CHORD_EXTENSION_MAG);
    double eadjust = style.style_d(Sid::CHORD_EXTENSION_ADJUST);
    double mmag = style.style_d(Sid::CHORD_MODIFIER_MAG);
    double madjust = style.style_d(Sid::CHORD_MODIFIER_ADJUST);
    double stackedmmag = style.style_d(Sid::CHORD_STACKED_MODIFIER_MAG);
    bool mstack_modifiers = style.style_b(Sid::VERTICALLY_STACK_MODIFIERS);
    bool mexclude_mods_h_align = style.style_b(Sid::CHORD_ALIGNMENT_EXCLUDE_MODIFIERS);
    String msymbol_font = style.style_st(Sid::MUSICAL_TEXT_FONT);
    ChordStylePreset preset = style.style_v(Sid::CHORD_STYLE).value<ChordStylePreset>();
    score->chord_list()->configure_auto_adjust(emag, eadjust, mmag, madjust, stackedmmag, mstack_modifiers, mexclude_mods_h_align,
                                               msymbol_font, preset);
    if (score->style().style_b(Sid::CHORDS_XML_FILE)) {
        score->chord_list()->read(u"chords.xml");
    }
    score->chord_list()->read(style.style_st(Sid::CHORD_DESCRIPTION_FILE));
    score->chord_list()->set_custom_chord_list(style.style_v(Sid::CHORD_STYLE).value<ChordStylePreset>() == ChordStylePreset::CUSTOM);
}

//---------------------------------------------------------
//   ChangeStyle
//----------------------------------------------------------

ChangeStyle::ChangeStyle(Score *s, const MStyle &st, const bool overlap_only) : score(s), style(st), overlap(overlap_only) {}

StyleIdSet ChangeStyle::changed_ids() const {
    StyleIdSet result;
    for (int sid_value = 0; sid_value < static_cast<int>(Sid::STYLES); ++sid_value) {
        Sid sid = static_cast<Sid>(sid_value);
        if (score->style().style_v(sid) != style.value(sid)) {
            result.insert(sid);
        }
    }
    return result;
}

void ChangeStyle::flip() {
    MStyle tmp = score->style();

    if (score->style().style_v(Sid::CONCERT_PITCH) != style.value(Sid::CONCERT_PITCH)) {
        score->cmd_concert_pitch_changed(style.value(Sid::CONCERT_PITCH).to_bool());
    }
    if (score->style().style_v(Sid::MUSICAL_SYMBOL_FONT) != style.value(Sid::MUSICAL_SYMBOL_FONT)) {
        score->set_engraving_font(score->engraving_fonts()->font_by_name(style.style_st(Sid::MUSICAL_SYMBOL_FONT).to_std_string()));
    }

    score->set_style(style, overlap);
    change_chord_style(score);
    if (tmp.spatium() != style.spatium()) {
        score->spatium_changed(tmp.spatium(), style.spatium());
    }
    score->style_changed();
    style = tmp;
}

void ChangeStyle::undo() {
    overlap = false;
    UndoableCommand::undo();
}

//---------------------------------------------------------
//   ChangeStyleValues
//----------------------------------------------------------

static void change_style_value(Score *score, Sid idx, const PropertyValue &old_value, const PropertyValue &new_value) {
    score->style().set(idx, new_value);
    switch (idx) {
        case Sid::CHORD_EXTENSION_MAG:
        case Sid::CHORD_EXTENSION_ADJUST:
        case Sid::CHORD_MODIFIER_MAG:
        case Sid::CHORD_MODIFIER_ADJUST:
        case Sid::CHORD_DESCRIPTION_FILE:
        case Sid::VERTICALLY_STACK_MODIFIERS:
        case Sid::CHORD_ALIGNMENT_EXCLUDE_MODIFIERS:
        case Sid::MUSICAL_TEXT_FONT: {
            change_chord_style(score);
        } break;
        case Sid::SPATIUM: score->spatium_changed(old_value.to_double(), new_value.to_double()); break;
        case Sid::DEFAULTS_VERSION: score->style().set_default_style_version(new_value.to_int()); break;
        case Sid::CREATE_MULTI_MEASURE_RESTS:
            if (old_value.to_bool() == true && new_value.to_bool() == false) {
                Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();
                EditSystemLocks::remove_system_locks_containing_mm_rests(tx, score);
                EditPageLocks::remove_page_locks_containing_mm_rests(tx, score);
            }
            break;
        default: break;
    }
}

void ChangeStyleValues::flip() {
    if (!score_) {
        return;
    }

    bool style_changed = false;
    const MStyle &style = score_->style();

    for (auto &pair : values_) {
        PropertyValue old_value = style.style_v(pair.first);
        if (old_value == pair.second) {
            continue;
        }

        change_style_value(score_, pair.first, old_value, pair.second);
        pair.second = old_value;

        style_changed = true;
    }

    if (style_changed) {
        score_->style_changed();
    }
}

//---------------------------------------------------------
//   EditStyle
//---------------------------------------------------------

bool EditStyle::load_style(Transaction &tx, Score *score, muse::io::IODevice &dev, bool ign, bool overlap) {
    TRACEFUNC;

    if (!dev.open(muse::io::IODevice::READ_ONLY)) {
        LOGE() << "The style data is not available.";
        return false;
    }

    bool success = false;
    MStyle st = score->style();
    if (st.read(&dev, ign)) {
        tx.push(new ChangeStyle(score, st, overlap));
        success = true;
    } else {
        LOGE() << "The style data is not compatible with this version of MuseScore Studio.";
    }

    dev.close();
    return success;
}
