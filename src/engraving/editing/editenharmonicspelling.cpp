/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "log.h"
#include "editenharmonicspelling.h"

#include <algorithm>
#include <vector>

#include "transpose.h"

#include "../dom/chord.h"
#include "../dom/key.h"
#include "../dom/note.h"
#include "../dom/part.h"
#include "../dom/pitchspelling.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/staff.h"

using namespace mu::engraving;

// table of alternative spellings for one octave
// each entry is the TPC of the note
//    tab1 does not contain double sharps
//    tab2 does not contain double flats

static constexpr int tab1[24] = {
    14, 2,  // 60  C   Dbb
    21, 9,  // 61  C#  Db
    16, 4,  // 62  D   Ebb
    23, 11, // 63  D#  Eb
    18, 6,  // 64  E   Fb
    13, 1,  // 65  F   Gbb
    20, 8,  // 66  F#  Gb
    15, 3,  // 67  G   Abb
    22, 10, // 68  G#  Ab
    17, 5,  // 69  A   Bbb
    24, 12, // 70  A#  Bb
    19, 7,  // 71  B   Cb
};

static constexpr int tab2[24] = {
    26, 14, // 60  B#  C
    21, 9,  // 61  C#  Db
    28, 16, // 62  C## D
    23, 11, // 63  D#  Eb
    30, 18, // 64  D## E
    25, 13, // 65  E#  F
    20, 8,  // 66  F#  Gb
    27, 15, // 67  F## G
    22, 10, // 68  G#  Ab
    29, 17, // 69  G## A
    24, 12, // 70  A#  Bb
    31, 19, // 71  A## B
};

static constexpr int interval_penalty[13] = {0, 0, 0, 0, 0, 0, 1, 3, 1, 1, 1, 3, 3};

static constexpr int enharmonic_spelling[15][34] = {{
                                                        //Ces f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        0, 0, 0, 0, 0, 0, 0,                      // b
                                                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //Ges f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 0, 0, 0, 0, 0, 0,                      // b
                                                        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //Des f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 0, 0, 0, 0, 0,                      // b
                                                        0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //As  f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 0, 0, 0, 0, 0,                      // b
                                                        0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //Es  f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 0, 0, 0, 0, 0,                      // b
                                                        0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //Bb  f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 0, 0, 0, 0, 0,                      // b
                                                        0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, // #     // (ws) penalty for f#
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //F   f  c  g  d  a  e  b           // extra penalty for a# b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 0, 0, 0, 0, 0,                      // b
                                                        0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //C   f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 0, 0, 0, 0, 0,                      // b
                                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //G   f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 1, 0, 0, 0, 0,                      // b
                                                        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //D   f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 1, 1, 0, 0, 0,                      // b
                                                        1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //A   f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 1, 1, 1, 0, 0,                      // b
                                                        1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //E   f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 1, 1, 1, 1, 0,                      // b
                                                        1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // #
                                                        0, 0, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //H   f  c  g  d  a  e  b
                                                        1, 1, 1, 1, 1, 1,                         // bb
                                                        1, 1, 1, 1, 1, 1, 1,                      // b
                                                        1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, // #
                                                        1, 1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //Fis f  c  g  d  a  e  b
                                                        1,   1, 1, 1, 1, 1,                         // bb
                                                        1,   1, 1, 1, 1, 1, 1,                      // b
                                                        100, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // #
                                                        0,   1, 1, 1, 1, 1, 1                       // ##
                                                    },
                                                    {
                                                        //Cis f  c  g  d  a  e  b
                                                        1,   1, 1, 1, 1, 1,                         // bb
                                                        1,   1, 0, 0, 0, 0, 0,                      // b  //Fis
                                                        100, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, // #
                                                        0,   0, 1, 1, 1, 1, 1                       // ##
                                                    }};

static constexpr int penalty(int lof1, int lof2, int k) {
    IF_ASSERT_FAILED((k >= 0 && k < 15)) { return 0; }
    assert(lof1 >= 0 && lof1 < 34);
    assert(lof2 >= 0 && lof2 < 34);
    int penalty = ::enharmonic_spelling[k][lof1] * 4 + ::enharmonic_spelling[k][lof2] * 4;
    int distance = lof2 > lof1 ? lof2 - lof1 : lof1 - lof2;
    if (distance > 12) {
        penalty += 3;
    } else {
        penalty += ::interval_penalty[distance];
    }
    return penalty;
}

static int compute_window(const std::vector<Note *> &notes, int start, int end) {
    int p = 10000;
    int idx = -1;
    int pitch[10];
    int key[10];

    int i = start;
    int k = 0;
    while (i < end) {
        pitch[k] = notes[i]->pitch() % 12;
        Fraction tick = notes[i]->chord()->tick();
        key[k] = int(notes[i]->staff()->key(tick)) + 7;
        if (key[k] < 0 || key[k] > 14) {
            LOGD("illegal key at tick %d: %d, window %d-%d", tick.ticks(), key[k] - 7, start, end);
            return 0;
            // abort();
        }
        ++k;
        ++i;
    }

    for (; k < 10; ++k) {
        pitch[k] = pitch[k - 1];
        key[k] = key[k - 1];
    }

    for (i = 0; i < 512; ++i) {
        int pa = 0;
        int pb = 0;
        int l = pitch[0] * 2 + (i & 1);
        assert(l >= 0 && l <= static_cast<int>(sizeof(tab1) / sizeof(*tab1)));
        int lof1a = tab1[l];
        int lof1b = tab2[l];

        for (k = 1; k < 10; ++k) {
            int l1 = pitch[k] * 2 + ((i & (1 << k)) >> k);
            assert(l1 >= 0 && l1 <= static_cast<int>(sizeof(tab1) / sizeof(*tab1)));
            int lof2a = tab1[l1];
            int lof2b = tab2[l1];
            pa += penalty(lof1a, lof2a, key[k]);
            pb += penalty(lof1b, lof2b, key[k]);
            lof1a = lof2a;
            lof1b = lof2b;
        }
        if (pa < pb) {
            if (pa < p) {
                p = pa;
                idx = i;
            }
        } else {
            if (pb < p) {
                p = pb;
                idx = i * -1;
            }
        }
    }

    return idx;
}

static void change_all_tpcs(Note *n, int tpc1) {
    if (!n) {
        return;
    }
    Interval v;
    Fraction tick = n->chord() ? n->chord()->tick() : Fraction(-1, 1);
    if (n->part() && n->part()->instrument(tick)) {
        v = n->staff()->transpose(tick);
        v.flip();
    }
    int tpc2 = Transpose::transpose_tpc(tpc1, v, true);
    n->undo_change_property(Pid::TPC1, tpc1);
    n->undo_change_property(Pid::TPC2, tpc2);
    for (Note *tied : n->tied_notes()) {
        tied->undo_change_property(Pid::TPC1, tpc1);
        tied->undo_change_property(Pid::TPC2, tpc2);
    }
}

static void spell_notelist(std::vector<Note *> &notes) {
    static constexpr int window = 9;

    int n = int(notes.size());

    int start = 0;
    while (start < n) {
        int end = start + window;
        if (end > n) {
            end = n;
        }
        int opt = compute_window(notes, start, end);
        const int *tab;
        if (opt < 0) {
            tab = tab2;
            opt *= -1;
        } else {
            tab = tab1;
        }

        if (start == 0) {
            change_all_tpcs(notes[0], tab[(notes[0]->pitch() % 12) * 2 + (opt & 1)]);
            if (n > 1) {
                change_all_tpcs(notes[1], tab[(notes[1]->pitch() % 12) * 2 + ((opt & 2) >> 1)]);
            }
            if (n > 2) {
                change_all_tpcs(notes[2], tab[(notes[2]->pitch() % 12) * 2 + ((opt & 4) >> 2)]);
            }
        }
        if ((end - start) >= 6) {
            change_all_tpcs(notes[start + 3], tab[(notes[start + 3]->pitch() % 12) * 2 + ((opt & 8) >> 3)]);
            change_all_tpcs(notes[start + 4], tab[(notes[start + 4]->pitch() % 12) * 2 + ((opt & 16) >> 4)]);
            change_all_tpcs(notes[start + 5], tab[(notes[start + 5]->pitch() % 12) * 2 + ((opt & 32) >> 5)]);
        }
        if (end == n) {
            int n1 = end - start;
            int k;
            switch (n1 - 6) {
                case 3:
                    k = end - start - 3;
                    change_all_tpcs(notes[end - 3], tab[(notes[end - 3]->pitch() % 12) * 2 + ((opt & (1 << k)) >> k)]);
                    [[fallthrough]];
                case 2:
                    k = end - start - 2;
                    change_all_tpcs(notes[end - 2], tab[(notes[end - 2]->pitch() % 12) * 2 + ((opt & (1 << k)) >> k)]);
                    [[fallthrough]];
                case 1:
                    k = end - start - 1;
                    change_all_tpcs(notes[end - 1], tab[(notes[end - 1]->pitch() % 12) * 2 + ((opt & (1 << k)) >> k)]);
            }
            break;
        }
        // advance to next window
        start += 3;
    }
}

void EditEnharmonicSpelling::spell(Score *score) {
    IF_ASSERT_FAILED(score) { return; }

    for (staff_idx_t i = 0; i < score->nstaves(); ++i) {
        std::vector<Note *> notes;
        for (Segment *s = score->first_segment(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
            track_idx_t strack = i * ::mu::engraving::voices;
            track_idx_t etrack = strack + ::mu::engraving::voices;
            for (track_idx_t track = strack; track < etrack; ++track) {
                EngravingItem *e = s->element(track);
                if (!e || !e->is_chord()) {
                    continue;
                }

                Chord *chord = to_chord(e);
                std::copy_if(chord->notes().begin(), chord->notes().end(), std::back_inserter(notes),
                             [score](EngravingItem *chord_element) { return score->selection().is_none() || chord_element->selected(); });

                for (Chord *grace_chord : chord->grace_notes()) {
                    std::copy_if(
                        grace_chord->notes().begin(), grace_chord->notes().end(), std::back_inserter(notes),
                        [score](EngravingItem *chord_element) { return score->selection().is_none() || chord_element->selected(); });
                }
            }
        }

        spell_notelist(notes);
    }
}

void EditEnharmonicSpelling::spell_with_sharps_or_flats(Score *score, Prefer prefer) {
    IF_ASSERT_FAILED(score) { return; }

    std::vector<Note *> notes = score->selection().note_list();
    for (Note *note : notes) {
        Interval interval = note->part()->instrument(note->chord()->tick())->transpose();
        int tpc1 = TPC_INVALID;
        int tpc2 = TPC_INVALID;
        if (note->staff()->concert_pitch() || interval.is_zero()) {
            // Note: using Key::C always and ignoring actual key signature. Otherwise, "respell with flats" would still use sharps
            // sometimes if they're in the key, and vice versa, which seems contrary to the intent of the command.
            tpc1 = pitch2tpc(note->pitch(), Key::C, prefer);
            interval.flip();
            tpc2 = Transpose::transpose_tpc(tpc1, interval, true);
        } else {
            // Spell the transposed pitch first, then convert to concert pitch
            int written_pitch = note->pitch() - interval.chromatic;
            tpc2 = pitch2tpc(written_pitch, Key::C, prefer);
            tpc1 = Transpose::transpose_tpc(tpc2, interval, true);
        }

        note->undo_change_property(Pid::TPC1, tpc1);
        note->undo_change_property(Pid::TPC2, tpc2);
        for (Note *tied_note : note->tied_notes()) {
            tied_note->undo_change_property(Pid::TPC1, tpc1);
            tied_note->undo_change_property(Pid::TPC2, tpc2);
        }
    }
}

void EditEnharmonicSpelling::change_enharmonic_spelling(Score *score, bool both) {
    IF_ASSERT_FAILED(score) { return; }

    static constexpr int tpc_table[36] = {
        26, 14, 2,  // 60  B#   C   Dbb
        21, 21, 9,  // 61  C#   C#  Db
        28, 16, 4,  // 62  C##  D   Ebb
        23, 23, 11, // 63  D#   D#  Eb
        30, 18, 6,  // 64  D##  E   Fb
        25, 13, 1,  // 65  E#   F   Gbb
        20, 20, 8,  // 66  F#   F#  Gb
        27, 15, 3,  // 67  F##  G   Abb
        22, 22, 10, // 68  G#   G#  Ab
        29, 17, 5,  // 69  G##  A   Bbb
        24, 24, 12, // 70  A#   A#  Bb
        31, 19, 7,  // 71  A##  B   Cb
    };

    std::vector<Note *> notes = score->selection().unique_notes();
    for (Note *note : notes) {
        Staff *staff = note->staff();
        if (note->origin_part()->instrument(note->tick())->use_drumset()) {
            continue;
        }

        if (staff->is_tab_staff(note->tick())) {
            int string = note->line() + (both ? 1 : -1);
            int fret = note->origin_part()->string_data(note->tick(), staff->idx())->fret(note->pitch(), string, staff);
            if (fret != -1) {
                note->undo_change_property(Pid::FRET, fret);
                note->undo_change_property(Pid::STRING, string);
            }
            continue;
        }

        int tpc = note->tpc();
        for (int i = 0; i < 36; ++i) {
            if (tpc_table[i] != tpc) {
                continue;
            }

            if ((i % 3) < 2) {
                tpc = tpc_table[i] == tpc_table[i + 1] ? tpc_table[i + 2] : tpc_table[i + 1];
            } else {
                tpc = tpc_table[i - 2];
            }
            break;
        }

        note->undo_set_tpc(tpc);
        if (both || staff->part()->instrument(note->chord()->tick())->transpose().is_zero()) {
            int transposed_tpc = note->transpose_tpc(tpc);
            if (note->concert_pitch()) {
                note->undo_change_property(Pid::TPC2, transposed_tpc);
            } else {
                note->undo_change_property(Pid::TPC1, transposed_tpc);
            }
        }
    }
}

int EditEnharmonicSpelling::best_enharmonic_fit(const std::vector<int> tpcs, Key key) {
    int key_index = int(key) - int(Key::MIN);
    if (key_index < 0 || key_index >= int(Key::NUM_OF)) {
        return tpcs.front();
    }

    // Highest penalty in enharmonicSpelling available (100) + 1
    int best_penalty = 101;
    int closest_tpc = Tpc::TPC_INVALID;

    for (int tpc : tpcs) {
        if (tpc == Tpc::TPC_INVALID) {
            continue;
        }

        int lof = tpc - Tpc::TPC_MIN;
        if (lof < 0 || lof >= 34) {
            continue;
        }

        int penalty = ::enharmonic_spelling[key_index][lof];

        if (penalty < best_penalty) {
            best_penalty = penalty;
            closest_tpc = tpc;
        }
    }

    return closest_tpc;
}
