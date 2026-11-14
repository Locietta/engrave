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

void mu::engraving::apply_style_replacement(Score &owner, const MStyle &style, bool overlap) {
    auto *score = &owner;
    const double before_spatium = score->style().spatium();

    if (score->style().style_v(Sid::CONCERT_PITCH) != style.value(Sid::CONCERT_PITCH)) {
        score->cmd_concert_pitch_changed(style.value(Sid::CONCERT_PITCH).to_bool());
    }
    if (score->style().style_v(Sid::MUSICAL_SYMBOL_FONT) != style.value(Sid::MUSICAL_SYMBOL_FONT)) {
        score->set_engraving_font(score->engraving_fonts()->font_by_name(style.style_st(Sid::MUSICAL_SYMBOL_FONT).to_std_string()));
    }

    score->set_style(style, overlap);
    change_chord_style(score);
    if (before_spatium != style.spatium()) {
        score->spatium_changed(before_spatium, style.spatium());
    }
    score->style_changed();
}

//---------------------------------------------------------
//   ChangeStyleValues
//----------------------------------------------------------

bool mu::engraving::style_value_refreshes_chords(Sid id) {
    switch (id) {
        case Sid::CHORD_EXTENSION_MAG:
        case Sid::CHORD_EXTENSION_ADJUST:
        case Sid::CHORD_MODIFIER_MAG:
        case Sid::CHORD_MODIFIER_ADJUST:
        case Sid::CHORD_DESCRIPTION_FILE:
        case Sid::VERTICALLY_STACK_MODIFIERS:
        case Sid::CHORD_ALIGNMENT_EXCLUDE_MODIFIERS:
        case Sid::MUSICAL_TEXT_FONT: return true;
        default: return false;
    }
}

static void change_style_value(Score *score, Sid idx, const PropertyValue &old_value, const PropertyValue &new_value) {
    score->style().set(idx, new_value);
    if (style_value_refreshes_chords(idx)) {
        change_chord_style(score);
        return;
    }
    switch (idx) {
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

void mu::engraving::apply_style_values(Score &score, std::span<const StyleValue> values) {
    bool changed = false;
    for (const auto &value : values) {
        const auto before = score.style().style_v(value.id);
        if (before == value.value) continue;
        change_style_value(&score, value.id, before, value.value);
        changed = true;
    }
    if (changed) score.style_changed();
}

//---------------------------------------------------------
//   EditStyle
//---------------------------------------------------------

bool EditStyle::load_style(Transaction &tx, Score *score, muse::io::IODevice &dev, bool ign, bool overlap) {

    if (!dev.open(muse::io::IODevice::READ_ONLY)) {
        LOGE() << "The style data is not available.";
        return false;
    }

    bool success = false;
    MStyle st = score->style();
    if (st.read(&dev, ign)) {
        tx.replace_style(*score, st, overlap);
        success = true;
    } else {
        LOGE() << "The style data is not compatible with this version of MuseScore Studio.";
    }

    dev.close();
    return success;
}
