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
#include "paddingtable.h"

#include "thirdparty/kors_logger/src/log_base.h"

#include "dom/engravingitem.h"
#include "style/style.h"

using namespace mu::engraving;

void PaddingTable::init(double min_pad_unit) {
    PaddingTable &table = *this;

    for (size_t i = 0; i < ::mu::engraving::tot_element_types; ++i) {
        for (size_t j = 0; j < ::mu::engraving::tot_element_types; ++j) {
            table[i][j] = min_pad_unit;
        }
    }
}

void PaddingTable::update(const MStyle &style) {
    const double spatium = style.spatium();
    const double min_pad_unit = 0.1 * spatium;
    init(min_pad_unit);

    const double ledger_pad = 0.25 * spatium;
    const double ledger_length = style.style_absolute(Sid::LEDGER_LINE_LENGTH);

    PaddingTable &table = *this;

    // NOTE: we don't set note-note padding to minNoteDistance
    // because there are cases when they should be allowed to get closer.
    // minNoteDistance is applied during the padding calculations.
    table[ElementType::NOTE][ElementType::NOTE] = min_pad_unit;
    table[ElementType::NOTE][ElementType::LEDGER_LINE] = 0.35 * spatium;
    table[ElementType::NOTE][ElementType::ACCIDENTAL] =
        std::max(static_cast<double>(style.style_absolute(Sid::ACCIDENTAL_NOTE_DISTANCE)), 0.35 * spatium);
    table[ElementType::NOTE][ElementType::REST] = 0.5 * spatium;
    table[ElementType::NOTE][ElementType::CLEF] = 0.8 * spatium;
    table[ElementType::NOTE][ElementType::ARPEGGIO] = 0.6 * spatium;
    table[ElementType::NOTE][ElementType::BAR_LINE] = style.style_absolute(Sid::NOTE_BAR_DISTANCE);
    table[ElementType::NOTE][ElementType::KEYSIG] = 0.75 * spatium;
    table[ElementType::NOTE][ElementType::TIMESIG] = 0.75 * spatium;

    table[ElementType::LEDGER_LINE][ElementType::NOTE] = table[ElementType::NOTE][ElementType::LEDGER_LINE];
    table[ElementType::LEDGER_LINE][ElementType::LEDGER_LINE] = ledger_pad;
    table[ElementType::LEDGER_LINE][ElementType::ACCIDENTAL] =
        std::max(static_cast<double>(style.style_absolute(Sid::ACCIDENTAL_NOTE_DISTANCE)),
                 table[ElementType::NOTE][ElementType::ACCIDENTAL] - ledger_length / 2);
    table[ElementType::LEDGER_LINE][ElementType::REST] = table[ElementType::LEDGER_LINE][ElementType::NOTE];
    table[ElementType::LEDGER_LINE][ElementType::CLEF] =
        std::max(table[ElementType::NOTE][ElementType::CLEF] - ledger_length / 2, ledger_pad);
    table[ElementType::LEDGER_LINE][ElementType::ARPEGGIO] = 0.5 * spatium;
    table[ElementType::LEDGER_LINE][ElementType::BAR_LINE] =
        std::max(table[ElementType::NOTE][ElementType::BAR_LINE] - ledger_length, ledger_pad);
    table[ElementType::LEDGER_LINE][ElementType::KEYSIG] =
        std::max(table[ElementType::NOTE][ElementType::KEYSIG] - ledger_length / 2, ledger_pad);
    table[ElementType::LEDGER_LINE][ElementType::TIMESIG] =
        std::max(table[ElementType::NOTE][ElementType::TIMESIG] - ledger_length / 2, ledger_pad);

    table[ElementType::HOOK][ElementType::NOTE] = 0.35 * spatium;
    table[ElementType::HOOK][ElementType::LEDGER_LINE] = std::max(table[ElementType::HOOK][ElementType::NOTE] - ledger_length, ledger_pad);
    table[ElementType::HOOK][ElementType::ACCIDENTAL] = 0.35 * spatium;
    table[ElementType::HOOK][ElementType::REST] = table[ElementType::HOOK][ElementType::NOTE];
    table[ElementType::HOOK][ElementType::CLEF] = 0.5 * spatium;
    table[ElementType::HOOK][ElementType::ARPEGGIO] = 0.35 * spatium;
    table[ElementType::HOOK][ElementType::BAR_LINE] = 1 * spatium;
    table[ElementType::HOOK][ElementType::KEYSIG] = 1.15 * spatium;
    table[ElementType::HOOK][ElementType::TIMESIG] = 1.15 * spatium;

    table[ElementType::NOTEDOT][ElementType::NOTE] =
        std::max(style.style_absolute(Sid::DOT_NOTE_DISTANCE), style.style_absolute(Sid::DOT_DOT_DISTANCE));
    table[ElementType::NOTEDOT][ElementType::LEDGER_LINE] =
        std::max(table[ElementType::NOTEDOT][ElementType::NOTE] - ledger_length, ledger_pad);
    table[ElementType::NOTEDOT][ElementType::ACCIDENTAL] = 0.35 * spatium;
    table[ElementType::NOTEDOT][ElementType::REST] = table[ElementType::NOTEDOT][ElementType::NOTE];
    table[ElementType::NOTEDOT][ElementType::CLEF] = 1.0 * spatium;
    table[ElementType::NOTEDOT][ElementType::ARPEGGIO] = 0.5 * spatium;
    table[ElementType::NOTEDOT][ElementType::BAR_LINE] = 0.8 * spatium;
    table[ElementType::NOTEDOT][ElementType::KEYSIG] = 1.35 * spatium;
    table[ElementType::NOTEDOT][ElementType::TIMESIG] = 1.35 * spatium;

    table[ElementType::REST][ElementType::NOTE] = table[ElementType::NOTE][ElementType::REST];
    table[ElementType::REST][ElementType::LEDGER_LINE] =
        std::max(table[ElementType::REST][ElementType::NOTE] - ledger_length / 2, ledger_pad);
    table[ElementType::REST][ElementType::ACCIDENTAL] = 0.45 * spatium;
    table[ElementType::REST][ElementType::REST] = table[ElementType::REST][ElementType::NOTE];
    table[ElementType::REST][ElementType::CLEF] = table[ElementType::NOTE][ElementType::CLEF];
    table[ElementType::REST][ElementType::BAR_LINE] = 1.65 * spatium;
    table[ElementType::REST][ElementType::KEYSIG] = 1.5 * spatium;
    table[ElementType::REST][ElementType::TIMESIG] = 1.5 * spatium;

    table[ElementType::CLEF][ElementType::NOTE] = style.style_absolute(Sid::CLEF_KEY_RIGHT_MARGIN);
    table[ElementType::CLEF][ElementType::LEDGER_LINE] =
        std::max(table[ElementType::CLEF][ElementType::NOTE] - ledger_length / 2, ledger_pad);
    table[ElementType::CLEF][ElementType::ACCIDENTAL] = 0.6 * spatium;
    table[ElementType::CLEF][ElementType::REST] = table[ElementType::CLEF][ElementType::NOTE];
    table[ElementType::CLEF][ElementType::CLEF] = 0.75 * spatium;
    table[ElementType::CLEF][ElementType::ARPEGGIO] = 0.65 * spatium;
    table[ElementType::CLEF][ElementType::BAR_LINE] = style.style_absolute(Sid::CLEF_BARLINE_DISTANCE);
    table[ElementType::CLEF][ElementType::KEYSIG] = style.style_absolute(Sid::CLEF_KEY_DISTANCE);
    table[ElementType::CLEF][ElementType::TIMESIG] = style.style_absolute(Sid::CLEF_TIMESIG_DISTANCE);

    table[ElementType::BAR_LINE][ElementType::NOTE] = style.style_absolute(Sid::BAR_NOTE_DISTANCE);
    table[ElementType::BAR_LINE][ElementType::LEDGER_LINE] =
        std::max(table[ElementType::BAR_LINE][ElementType::NOTE] - ledger_length, ledger_pad);
    table[ElementType::BAR_LINE][ElementType::ACCIDENTAL] = style.style_absolute(Sid::BAR_ACCIDENTAL_DISTANCE);
    table[ElementType::BAR_LINE][ElementType::REST] = style.style_absolute(Sid::BAR_NOTE_DISTANCE);
    table[ElementType::BAR_LINE][ElementType::CLEF] = style.style_absolute(Sid::CLEF_LEFT_MARGIN);
    table[ElementType::BAR_LINE][ElementType::ARPEGGIO] = 0.65 * spatium;
    table[ElementType::BAR_LINE][ElementType::BAR_LINE] = 1.35 * spatium;
    table[ElementType::BAR_LINE][ElementType::KEYSIG] = style.style_absolute(Sid::KEYSIG_LEFT_MARGIN);
    table[ElementType::BAR_LINE][ElementType::TIMESIG] = style.style_absolute(Sid::TIMESIG_LEFT_MARGIN);

    table[ElementType::KEYSIG][ElementType::NOTE] = 1.75 * spatium;
    table[ElementType::KEYSIG][ElementType::LEDGER_LINE] =
        std::max(table[ElementType::KEYSIG][ElementType::NOTE] - ledger_length, ledger_pad);
    table[ElementType::KEYSIG][ElementType::ACCIDENTAL] = 1.6 * spatium;
    table[ElementType::KEYSIG][ElementType::REST] = table[ElementType::KEYSIG][ElementType::NOTE];
    table[ElementType::KEYSIG][ElementType::CLEF] = 1.0 * spatium;
    table[ElementType::KEYSIG][ElementType::ARPEGGIO] = 1.35 * spatium;
    table[ElementType::KEYSIG][ElementType::BAR_LINE] = style.style_absolute(Sid::KEY_BARLINE_DISTANCE);
    table[ElementType::KEYSIG][ElementType::KEYSIG] = 1 * spatium;
    table[ElementType::KEYSIG][ElementType::TIMESIG] = style.style_absolute(Sid::KEY_TIMESIG_DISTANCE);

    table[ElementType::TIMESIG][ElementType::NOTE] = 1.35 * spatium;
    table[ElementType::TIMESIG][ElementType::LEDGER_LINE] =
        std::max(table[ElementType::TIMESIG][ElementType::NOTE] - ledger_length, ledger_pad);
    table[ElementType::TIMESIG][ElementType::ACCIDENTAL] = 0.8 * spatium;
    table[ElementType::TIMESIG][ElementType::REST] = table[ElementType::TIMESIG][ElementType::NOTE];
    table[ElementType::TIMESIG][ElementType::CLEF] = 1.0 * spatium;
    table[ElementType::TIMESIG][ElementType::ARPEGGIO] = 1.35 * spatium;
    table[ElementType::TIMESIG][ElementType::BAR_LINE] = style.style_absolute(Sid::TIMESIG_BARLINE_DISTANCE);
    table[ElementType::TIMESIG][ElementType::KEYSIG] = style.style_absolute(Sid::KEY_TIMESIG_DISTANCE);
    table[ElementType::TIMESIG][ElementType::TIMESIG] = 1.0 * spatium;

    // Ambitus
    table[ElementType::AMBITUS].fill(style.style_absolute(Sid::AMBITUS_MARGIN));
    for (auto &elem : table) {
        elem[ElementType::AMBITUS] = style.style_absolute(Sid::AMBITUS_MARGIN);
    }

    table[ElementType::ARPEGGIO][ElementType::NOTE] = style.style_absolute(Sid::ARPEGGIO_NOTE_DISTANCE);
    table[ElementType::ARPEGGIO][ElementType::LEDGER_LINE] = 0.3 * spatium;
    table[ElementType::ARPEGGIO][ElementType::ACCIDENTAL] = style.style_absolute(Sid::ARPEGGIO_ACCIDENTAL_DISTANCE);

    double chord_bracket_dist = style.style_absolute(Sid::CHORD_BRACKET_NOTE_DISTANCE);
    table[ElementType::CHORD_BRACKET].fill(chord_bracket_dist);
    for (auto &elem : table) {
        elem[ElementType::CHORD_BRACKET] = elem[ElementType::ARPEGGIO];
    }

    // Breath
    table[ElementType::BREATH].fill(1.0 * spatium);
    for (auto &elem : table) {
        elem[ElementType::BREATH] = 1.0 * spatium;
    }

    // Harmony
    table[ElementType::BAR_LINE][ElementType::HARMONY] = 0.5 * style.style_absolute(Sid::HARMONY_HARMONY_DISTANCE);
    table[ElementType::HARMONY][ElementType::HARMONY] = style.style_absolute(Sid::HARMONY_HARMONY_DISTANCE);
    table[ElementType::HARMONY][ElementType::FRET_DIAGRAM] = 0.3 * spatium;
    table[ElementType::FRET_DIAGRAM][ElementType::HARMONY] = 0.3 * spatium;
    table[ElementType::FRET_DIAGRAM][ElementType::FRET_DIAGRAM] = 0.25 * spatium;

    // Chordlines
    table[ElementType::CHORDLINE].fill(0.35 * spatium);
    for (auto &elem : table) {
        elem[ElementType::CHORDLINE] = 0.35 * spatium;
    }
    table[ElementType::BAR_LINE][ElementType::CHORDLINE] = 0.65 * spatium;
    table[ElementType::CHORDLINE][ElementType::BAR_LINE] = 0.65 * spatium;

    // For the x -> fingering padding use the same values as x -> accidental
    for (auto &elem : table) {
        elem[ElementType::FINGERING] = elem[ElementType::ACCIDENTAL];
    }

    // This is needed for beamlets, not beams themselves
    table[ElementType::BEAM].fill(0.35 * spatium);

    table[ElementType::TREMOLO_SINGLECHORD] = table[ElementType::BEAM];

    double lyrics_spacing = style.style_absolute(Sid::LYRICS_MIN_DISTANCE);
    table[ElementType::LYRICS].fill(lyrics_spacing);
    for (auto &elem : table) {
        elem[ElementType::LYRICS] = lyrics_spacing;
    }
    table[ElementType::NOTE][ElementType::LYRICS] = style.style_absolute(Sid::LYRICS_MELISMA_PAD);

    // Accidental -> padding (used by accidental placement algorithm)
    table[ElementType::ACCIDENTAL][ElementType::NOTE] = style.style_absolute(Sid::ACCIDENTAL_NOTE_DISTANCE);
    table[ElementType::ACCIDENTAL][ElementType::LEDGER_LINE] = 0.18 * spatium;

    table[ElementType::ARTICULATION][ElementType::NOTE] = 0.25 * spatium;
    table[ElementType::ARTICULATION][ElementType::REST] = 0.25 * spatium;
    table[ElementType::ARTICULATION][ElementType::ACCIDENTAL] = 0.25 * spatium;

    table[ElementType::LAISSEZ_VIB_SEGMENT][ElementType::NOTE] = 0.5 * spatium;
    table[ElementType::LAISSEZ_VIB_SEGMENT][ElementType::REST] = 0.5 * spatium;
    table[ElementType::LAISSEZ_VIB_SEGMENT][ElementType::ACCIDENTAL] = 0.35 * spatium;
    table[ElementType::LAISSEZ_VIB_SEGMENT][ElementType::BAR_LINE] = 0.35 * spatium;

    // Measure repeat set same values as note
    table[ElementType::MEASURE_REPEAT] = table[ElementType::NOTE];
    for (auto &elem : table) {
        elem[ElementType::MEASURE_REPEAT] = elem[ElementType::NOTE];
    }

    // Obtain the Stem -> * and * -> Stem values from the note equivalents
    table[ElementType::STEM] = table[ElementType::NOTE];
    for (auto &elem : table) {
        elem[ElementType::STEM] = elem[ElementType::NOTE];
    }

    table[ElementType::NOTE][ElementType::STEM] = style.style_absolute(Sid::MIN_NOTE_DISTANCE);
    table[ElementType::REST][ElementType::STEM] = table[ElementType::NOTE][ElementType::STEM];
    table[ElementType::CLEF][ElementType::STEM] = 0.75 * spatium;
    table[ElementType::ACCIDENTAL][ElementType::STEM] = table[ElementType::ACCIDENTAL][ElementType::NOTE];
    table[ElementType::LAISSEZ_VIB_SEGMENT][ElementType::STEM] = 0.35 * spatium;
    table[ElementType::STEM][ElementType::NOTE] = style.style_absolute(Sid::MIN_NOTE_DISTANCE);
    table[ElementType::STEM][ElementType::STEM] = 0.85 * spatium;
    table[ElementType::STEM][ElementType::ACCIDENTAL] = 0.35 * spatium;
    table[ElementType::STEM][ElementType::LEDGER_LINE] = 0.35 * spatium;
    table[ElementType::LEDGER_LINE][ElementType::STEM] = 0.35 * spatium;

    const double articulation_and_fermata_padding = 0.35 * spatium;
    table[ElementType::ARTICULATION].fill(articulation_and_fermata_padding);
    for (auto &elem : table) {
        elem[ElementType::ARTICULATION] = articulation_and_fermata_padding;
    }
    table[ElementType::FERMATA].fill(articulation_and_fermata_padding);
    for (auto &elem : table) {
        elem[ElementType::FERMATA] = articulation_and_fermata_padding;
    }

    table[ElementType::TAPPING] = table[ElementType::ARTICULATION];
    for (auto &elem : table) {
        elem[ElementType::TAPPING] = elem[ElementType::ARTICULATION];
    }
    table[ElementType::TAPPING_HALF_SLUR_SEGMENT] = table[ElementType::ARTICULATION];
    for (auto &elem : table) {
        elem[ElementType::TAPPING_HALF_SLUR_SEGMENT] = elem[ElementType::ARTICULATION];
    }
}

double ParenPaddingTable::padding(ElementType type1, ElementType type2) const {
    assert(type1 == ElementType::PARENTHESIS || type2 == ElementType::PARENTHESIS);
    const PaddingVector<double> &row = type1 == ElementType::PARENTHESIS ? paren_after_ : paren_before_;
    const ElementType &other_type = type1 == ElementType::PARENTHESIS ? type2 : type1;

    return row.at(other_type);
}

const ParenPaddingTable &ParenPaddingTables::get_table_for(const EngravingItem *parent) const {
    switch (parent->type()) {
        case ElementType::NOTE:
        case ElementType::CHORD: return note_paren_padding_table;
        case ElementType::KEYSIG: return key_sig_paren_padding_table;
        case ElementType::TIMESIG: return time_sig_paren_padding_table;
        case ElementType::CLEF: return clef_paren_padding_table;
        default: ASSERT_X("Not a valid parenthesised type") return note_paren_padding_table; // Just to return something
    }
}

void ParenPaddingTable::init(double min_pad_unit) {
    for (size_t i = 0; i < ::mu::engraving::tot_element_types; ++i) {
        paren_before_[i] = min_pad_unit;
        paren_after_[i] = min_pad_unit;
    }
}

void NoteParenPaddingTable::update(const MStyle &style) {
    const double spatium = style.spatium();
    const double min_pad_unit = 0.1 * spatium;
    init(min_pad_unit);

    paren_before_[ElementType::ARPEGGIO] = style.style_absolute(Sid::ARPEGGIO_ACCIDENTAL_DISTANCE);
    paren_before_[ElementType::BAR_LINE] = style.style_absolute(Sid::BAR_ACCIDENTAL_DISTANCE);
    paren_before_[ElementType::CLEF] = 0.6 * spatium;
    paren_before_[ElementType::HOOK] = 0.35 * spatium;
    paren_before_[ElementType::KEYSIG] = 1.6 * spatium;
    paren_before_[ElementType::LEDGER_LINE] = style.style_absolute(Sid::ACCIDENTAL_NOTE_DISTANCE);
    paren_before_[ElementType::NOTEDOT] = 0.35 * spatium;
    paren_before_[ElementType::NOTE] = 0.35 * spatium;
    paren_before_[ElementType::REST] = 0.45 * spatium;
    paren_before_[ElementType::STEM] = 0.35 * spatium;
    paren_before_[ElementType::TIMESIG] = 0.8 * spatium;
    paren_before_[ElementType::PARENTHESIS] = style.style_absolute(Sid::MIN_NOTE_DISTANCE) * 0.5;

    paren_after_[ElementType::ACCIDENTAL] = std::max(style.style_absolute(Sid::ACCIDENTAL_NOTE_DISTANCE), 0.35 * spatium);
    paren_after_[ElementType::ARPEGGIO] = 0.6 * spatium;
    paren_after_[ElementType::BAR_LINE] = style.style_absolute(Sid::NOTE_BAR_DISTANCE);
    paren_after_[ElementType::CLEF] = 0.8 * spatium;
    paren_after_[ElementType::KEYSIG] = 0.75 * spatium;
    paren_after_[ElementType::LEDGER_LINE] = 0.35 * spatium;
    paren_after_[ElementType::NOTE] = std::max(style.style_absolute(Sid::ACCIDENTAL_NOTE_DISTANCE), 0.35 * spatium);
    paren_after_[ElementType::REST] = style.style_absolute(Sid::MIN_NOTE_DISTANCE);
    paren_after_[ElementType::STEM] = style.style_absolute(Sid::MIN_NOTE_DISTANCE);
    paren_after_[ElementType::TIMESIG] = 0.75 * spatium;
    paren_after_[ElementType::PARENTHESIS] = style.style_absolute(Sid::MIN_NOTE_DISTANCE) * 0.5;
}

void KeySigParenPaddingTable::update(const MStyle &style) {
    const double spatium = style.spatium();
    const double min_pad_unit = 0.1 * spatium;
    init(min_pad_unit);

    paren_before_[ElementType::BAR_LINE] = 0.5 * spatium;
    paren_before_[ElementType::CLEF] = 0.25 * spatium;
    paren_before_[ElementType::HOOK] = 0.35 * spatium;
    paren_before_[ElementType::KEYSIG] = 0.25 * spatium;
    paren_before_[ElementType::NOTEDOT] = 0.35 * spatium;
    paren_before_[ElementType::NOTE] = style.style_absolute(Sid::NOTE_BAR_DISTANCE);
    paren_before_[ElementType::REST] = paren_before_[ElementType::NOTE];
    paren_before_[ElementType::STEM] = 0.35 * spatium;
    paren_before_[ElementType::TIMESIG] = 0.25 * spatium;
    paren_before_[ElementType::PARENTHESIS] = 1.0 * spatium;

    paren_after_[ElementType::BAR_LINE] = 0.5 * spatium;
    paren_after_[ElementType::CLEF] = 0.2 * spatium;
    paren_after_[ElementType::HOOK] = 0.35 * spatium;
    paren_after_[ElementType::KEYSIG] = 0.25 * spatium;
    paren_after_[ElementType::NOTEDOT] = 0.35 * spatium;
    paren_after_[ElementType::NOTE] = style.style_absolute(Sid::BAR_NOTE_DISTANCE);
    paren_after_[ElementType::REST] = paren_after_[ElementType::NOTE];
    paren_after_[ElementType::STEM] = 0.35 * spatium;
    paren_after_[ElementType::TIMESIG] = 0.25 * spatium;
    paren_after_[ElementType::PARENTHESIS] = 1.0 * spatium;
}

void TimeSigParenPaddingTable::update(const MStyle &style) {
    const double spatium = style.spatium();
    const double min_pad_unit = 0.1 * spatium;
    init(min_pad_unit);

    paren_before_[ElementType::BAR_LINE] = 0.5 * spatium;
    paren_before_[ElementType::CLEF] = 0.25 * spatium;
    paren_before_[ElementType::HOOK] = 0.35 * spatium;
    paren_before_[ElementType::KEYSIG] = 0.25 * spatium;
    paren_before_[ElementType::NOTEDOT] = 0.35 * spatium;
    paren_before_[ElementType::NOTE] = style.style_absolute(Sid::NOTE_BAR_DISTANCE);
    paren_before_[ElementType::REST] = paren_before_[ElementType::NOTE];
    paren_before_[ElementType::STEM] = 0.35 * spatium;
    paren_before_[ElementType::TIMESIG] = 0.25 * spatium;
    paren_before_[ElementType::PARENTHESIS] = 1.0 * spatium;

    paren_after_[ElementType::BAR_LINE] = 0.5 * spatium;
    paren_after_[ElementType::CLEF] = 0.2 * spatium;
    paren_after_[ElementType::HOOK] = 0.35 * spatium;
    paren_after_[ElementType::KEYSIG] = 0.35 * spatium;
    paren_after_[ElementType::NOTEDOT] = 0.2 * spatium;
    paren_after_[ElementType::NOTE] = style.style_absolute(Sid::BAR_NOTE_DISTANCE);
    paren_after_[ElementType::REST] = paren_after_[ElementType::NOTE];
    paren_after_[ElementType::STEM] = 0.35 * spatium;
    paren_after_[ElementType::TIMESIG] = 0.25 * spatium;
    paren_after_[ElementType::PARENTHESIS] = 1.0 * spatium;
}

void ClefParenPaddingTable::update(const MStyle &style) {
    const double spatium = style.spatium();
    const double min_pad_unit = 0.1 * spatium;
    init(min_pad_unit);

    paren_before_[ElementType::BAR_LINE] = 0.5 * spatium;
    paren_before_[ElementType::CLEF] = 0.25 * spatium;
    paren_before_[ElementType::HOOK] = 0.35 * spatium;
    paren_before_[ElementType::KEYSIG] = 0.25 * spatium;
    paren_before_[ElementType::NOTEDOT] = 0.35 * spatium;
    paren_before_[ElementType::NOTE] = style.style_absolute(Sid::NOTE_BAR_DISTANCE);
    paren_before_[ElementType::REST] = paren_before_[ElementType::NOTE];
    paren_before_[ElementType::STEM] = 0.35 * spatium;
    paren_before_[ElementType::TIMESIG] = 0.25 * spatium;
    paren_before_[ElementType::PARENTHESIS] = 1.0 * spatium;

    paren_after_[ElementType::BAR_LINE] = 0.5 * spatium;
    paren_after_[ElementType::CLEF] = 0.25 * spatium;
    paren_after_[ElementType::HOOK] = 0.35 * spatium;
    paren_after_[ElementType::KEYSIG] = 0.35 * spatium;
    paren_after_[ElementType::NOTEDOT] = 0.35 * spatium;
    paren_after_[ElementType::NOTE] = style.style_absolute(Sid::BAR_NOTE_DISTANCE);
    paren_after_[ElementType::REST] = paren_after_[ElementType::NOTE];
    paren_after_[ElementType::STEM] = 0.35 * spatium;
    paren_after_[ElementType::TIMESIG] = 0.2 * spatium;
    paren_after_[ElementType::PARENTHESIS] = 1.0 * spatium;
}
