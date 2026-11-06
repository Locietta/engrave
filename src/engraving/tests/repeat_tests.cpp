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

#include <gtest/gtest.h>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/repeatlist.h"

#include "utils/scorerw.h"

#include "log.h"

using namespace mu::engraving;

static const String repeat_data_dir(u"repeat_data/");

class EngravingRepeatTests : public ::testing::Test {
public:
    void repeat(const char *path, const String &ref);
};

void EngravingRepeatTests::repeat(const char *path, const String &ref) {
    MasterScore *score = ScoreRW::read_score(::repeat_data_dir + String::from_utf8(path));
    ASSERT_TRUE(score);

    score->set_expand_repeats(true);

    StringList sl;

    for (const RepeatSegment *rs : score->repeat_list()) {
        int start_tick = rs->tick;
        int end_tick = rs->end_tick();

        for (const Measure *m = score->tick2measure(Fraction::from_ticks(start_tick)); m; m = m->next_measure()) {
            sl.append(String::number(m->measure_number() + 1));

            if (m->end_tick().ticks() >= end_tick) {
                break;
            }
        }
    }

    String s = sl.join(u";");
    String ref1 = ref;
    ref1.replace(u" ", u"");

    LOGD("File <%s> sequence %s", path, muPrintable(s));

    EXPECT_EQ(s, ref1);

    delete score;
}

TEST_F(EngravingRepeatTests, repeat01) {
    // repeat barline 2 measures ||: | :||
    repeat("repeat01.mscx", u"1;2;3; 2;3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat02) {
    // repeat barline 1 measure ||: :||
    repeat("repeat02.mscx", u"1;2; 2;3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat03) {
    // repeat barline end to start :||
    repeat("repeat03.mscx", u"1;2; 1;2;3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat04) {
    // repeat barline ||: | :|| :||
    repeat("repeat04.mscx", u"1;2;3; 2;3;4; 2;3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat05) {
    // repeat barline ||: | x2 :|| :||
    repeat("repeat05.mscx", u"1;2;3; 2;3; 2;3;4; 2;3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat06) {
    // simple volta
    repeat("repeat06.mscx", u"1;2;3; 2; 4;5;6");
}

TEST_F(EngravingRepeatTests, repeat07) {
    // DC al fine
    repeat("repeat07.mscx", u"1;2;3;4;5;6; 1;2;3");
}

TEST_F(EngravingRepeatTests, repeat08) {
    // DS al coda
    repeat("repeat08.mscx", u"1;2;3;4;5;6; 2;3;4; 7;8;9;10;11");
}

TEST_F(EngravingRepeatTests, repeat09) {
    // 3 voltas, but twice a volta1
    repeat("repeat09.mscx", u"1;2;3; 2; 5;6");
}

TEST_F(EngravingRepeatTests, repeat10) {
    // 3 voltas
    repeat("repeat10.mscx", u"1;2;3;4; 1;2; 5;6;7;8; 1;2; 9;10; 1;2; 11;12");
}

TEST_F(EngravingRepeatTests, repeat11) {
    // volta after to coda
    repeat("repeat11.mscx", u"1;2;3;4; 2;3; 5;6;7;8; 2; 9;10");
}

TEST_F(EngravingRepeatTests, repeat12) {
    // volta between segno & DS
    repeat("repeat12.mscx", u"1;2;3;4; 3; 5;6; 2;3; 5;6;7");
}

TEST_F(EngravingRepeatTests, repeat13) {
    // incomplete jump -> carry on as good as you can
    repeat("repeat13.mscx", u"1;2;3;4;5; 1;2;3;4;5");
}

TEST_F(EngravingRepeatTests, repeat14) {
    // complex roadmap DS al coda, volta, repeat
    repeat("repeat14.mscx",
           u"1;2;3;4;5;6;7;8;9;10; 2;3;4;5;6;7;8; 11;12; 2;3;4;5;6;7;8; 13;14;15;16;17;18; 16;17;18; 19;20;21;22;23; 5;6;7; 24;25;26");
}

TEST_F(EngravingRepeatTests, repeat15) {
    // repeat barline ||: x8 :||
    repeat("repeat15.mscx", u"1;2; 2; 2; 2; 2; 2; 2; 2;3");
}

TEST_F(EngravingRepeatTests, repeat16) {
    // jump in simple repeat
    repeat("repeat16.mscx", u"1;2;3;4; 4; 1;2");
}

TEST_F(EngravingRepeatTests, repeat17) {
    // volta after coda
    repeat("repeat17.mscx", u"1;2; 1; 3;4;5; 4; 6;7;8; 7; 9");
}

TEST_F(EngravingRepeatTests, repeat18) {
    // twice volta
    repeat("repeat18.mscx", u"1;2; 1; 3;4;5;6; 5; 7;8");
}

TEST_F(EngravingRepeatTests, repeat19) {
    // DS al coda after the coda
    repeat("repeat19.mscx", u"1;2;3;4; 1;2; 4");
}

TEST_F(EngravingRepeatTests, repeat20) {
    // 1) DS al Coda, 2) DS1 al Fine
    repeat("repeat20.mscx", u"1;2;3; 1; 4;5;6;7;8; 5;6");
}

TEST_F(EngravingRepeatTests, repeat21) {
    // 1) DS, 2) DS1 al Coda
    repeat("repeat21.mscx", u"1;2;3; 1;2;3;4;5;6;7; 5; 8");
}

TEST_F(EngravingRepeatTests, repeat22) {
    // DS and ||: 3x :||
    repeat("repeat22.mscx", u"1;2;3; 2;3;4;5; 5; 5;6");
}

TEST_F(EngravingRepeatTests, repeat23) {
    // complex roadmap
    repeat("repeat23.mscx", u"1;2; 1;2;3; 2;3;4;5;6;7; 6;7;8;9;10;11; 9;10; 12; 12;13;14; 13;14;15;16; 13;14");
}

TEST_F(EngravingRepeatTests, repeat24) {
    // S ||: DS :||
    repeat("repeat24.mscx", u"1;2;3;4;5; 3;4; 2;3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat25) {
    // ||: S :||  ||: DS :||
    repeat("repeat25.mscx", u"1;2; 1;2;3;4;5; 4; 2;3;4;5");
}

TEST_F(EngravingRepeatTests, repeat26) {
    // empty and garbage jump
    repeat("repeat26.mscx", u"1; 1;2; 2;3");
}

TEST_F(EngravingRepeatTests, repeat27) {
    // #73486 single-measure repeat at end of section
    repeat("repeat27.mscx", u"1;2; 2; 1");
}

TEST_F(EngravingRepeatTests, repeat28) {
    // #73486 single-measure repeat at end of section w/DC
    repeat("repeat28.mscx", u"1;2; 2; 1;2; 1");
}

TEST_F(EngravingRepeatTests, repeat29) {
    // #73486 single-measure repeat at end of section w/DS
    repeat("repeat29.mscx", u"1;2;3; 3; 2;3; 1");
}

TEST_F(EngravingRepeatTests, repeat30) {
    // #73496 single measure section at beginning of score followed by a section with end repeat (without beginning repeat)
    repeat("repeat30.mscx", u"1; 1;2; 1;2");
}

TEST_F(EngravingRepeatTests, repeat31) {
    // #73531 ending measure has jump and repeat m1 |: m2 DC :|
    repeat("repeat31.mscx", u"1;2; 2; 1;2");
}

TEST_F(EngravingRepeatTests, repeat32) {
    // #73531 ending measure has jump and repeat m1 |S m2 |: m3 DS :|
    repeat("repeat32.mscx", u"1;2;3; 3; 2;3");
}

TEST_F(EngravingRepeatTests, repeat33) {
    // #73531 ending measure has jump and repeat m1 |: m2 | m3 DC :|
    repeat("repeat33.mscx", u"1;2;3; 2;3; 1;2;3");
}

TEST_F(EngravingRepeatTests, repeat34) {
    // #73531 ending measure has jump and repeat m1 |: m2 |1e m3 :| 2e m4 |: m5 | DC :|
    repeat("repeat34.mscx", u"1;2;3; 2; 4;5; 5; 1;2; 4;5");
}

TEST_F(EngravingRepeatTests, repeat35) {
    // #65161 multiple sections, each with possible DC, DS, al Fine
    repeat("repeat35.mscx", u"1;2; 1;2; 1;2;3;4; 2;3; 1;2;3; 1;2; 1;2;3;4; 2;3; 5");
}

TEST_F(EngravingRepeatTests, repeat36) {
    // incomplete jump, missing Coda from D.S. al Coda
    repeat("repeat36.mscx", u"1;2;3;4;5; 1;2;3");
}

TEST_F(EngravingRepeatTests, repeat37) {
    // #65161, with section breaks occurring on non-measure frames
    repeat("repeat37.mscx", u"1;2;1; 1;2;1; 1;2;1; 1;2;1; 1;2;1");
}

TEST_F(EngravingRepeatTests, repeat38) {
    // D.S. with repeats at start repeat
    repeat("repeat38.mscx", u"1;2; 2;3;4; 2; 2;3; 5");
}

TEST_F(EngravingRepeatTests, repeat39) {
    // volta 1.3.,2.4.
    repeat("repeat39.mscx", u"1;2;3; 2; 4;5; 2;3; 2; 4;5; 2; 6");
}

TEST_F(EngravingRepeatTests, repeat40) {
    // #148276 Open volta lines must extend over entire ending: next endRepeat
    repeat("repeat40.mscx", u"1;2; 1; 3;4; 1; 5");
}

TEST_F(EngravingRepeatTests, repeat41) {
    // #148276 Open volta lines must extend over entire ending: next endRepeat at volta (SLine) end
    repeat("repeat41.mscx", u"1;2; 1; 3; 1; 4;5");
}

TEST_F(EngravingRepeatTests, repeat42) {
    // #148276 Open volta lines must extend over entire ending: next volta
    repeat("repeat42.mscx", u"1;2; 1; 3; 5; 1; 4;5");
}

TEST_F(EngravingRepeatTests, repeat43) {
    // #8604 Complex alternate endings 1.2.4. / 3./ 5.
    repeat("repeat43.mscx", u"1;2; 1;2; 1; 3; 1;2; 1; 4; 1; 5");
}

TEST_F(EngravingRepeatTests, repeat44) {
    // Jump from within a volta
    repeat("repeat44.mscx", u"1;2;3;4;5; 3; 6; 3;4; 1; 7");
}

TEST_F(EngravingRepeatTests, repeat45) {
    // repeat12 but with 'play repeats' enabled
    repeat("repeat45.mscx", u"1;2;3;4; 3; 5;6; 2;3;4; 3; 5;6;7");
}

TEST_F(EngravingRepeatTests, repeat46) {
    // repeat24 but with 'play repeats' enabled
    repeat("repeat46.mscx", u"1;2;3;4;5; 3;4; 2;3;4;5; 3;4;5;6");
}

TEST_F(EngravingRepeatTests, repeat47) {
    // #269378 Double Coda AKA Doppia Coda messed up repeat rewind logic
    repeat("repeat47.mscx", u"1;2;3; 2; 4;5;6;7; 6; 8;9;10;11; 2; 4; 13;14;15; 9;10; 16;17;18");
}

TEST_F(EngravingRepeatTests, repeat48) {
    // jump into first volta, without playRepeats
    repeat("repeat48.mscx", u"1;2;3;4; 1; 5;6;7; 3;4; 1; 5;8");
}

TEST_F(EngravingRepeatTests, repeat49) {
    // D.S. with playRepeats
    repeat("repeat49.mscx", u"1;2;3; 1;2;3;4;5;6; 3; 1;2;3;4; 7");
}

TEST_F(EngravingRepeatTests, repeat50) {
    // D.S. with playRepeats with ToCoda inside the repeat
    repeat("repeat50.mscx", u"1;2;3;4; 1;2;3;4;5;6; 1;2;3;4; 1;2;3; 7");
}

TEST_F(EngravingRepeatTests, repeat51) {
    // #270332 twice D.S. with playRepeats to same target with different Coda
    repeat("repeat51.mscx", u"1;2;3;4;5;6; 3;4; 7;8;9; 3;4; 10;11");
}

TEST_F(EngravingRepeatTests, repeat52) {
    // Jump into volta "final" playthrough
    repeat("repeat52.mscx", u"1;2;3; 1; 4;5; 1;2;3; 1; 4;5; 1; 6; 3; 1; 4;5; 1; 6");
}

TEST_F(EngravingRepeatTests, repeat53) {
    // Jump into volta with repeats
    repeat("repeat53.mscx", u"1;2;3; 1; 4;5; 1;2;3; 1; 4;5; 1; 6; 5; 1;2;3; 1; 4;5; 1; 6");
}

TEST_F(EngravingRepeatTests, repeat54) {
    // repeat20 without label changes, pick correct segno
    repeat("repeat54.mscx", u"1;2;3; 1; 4;5;6;7;8; 5;6");
}

TEST_F(EngravingRepeatTests, repeat55) {
    // repeat21 without label changes, pick correct segno
    repeat("repeat55.mscx", u"1;2;3; 1;2;3;4;5;6;7; 5; 8");
}

TEST_F(EngravingRepeatTests, repeat56) {
    // start of volta and start repeat on same measure
    repeat("repeat56.mscx", u"1;2;3;4; 2; 5;6;7; 5;6;7;8");
}

TEST_F(EngravingRepeatTests, repeat57) {
    // no repeat, skip volta until section end, relates to #274690
    repeat("repeat57.mscx", u"1;2;3");
}

TEST_F(EngravingRepeatTests, repeat58) {
    // duplicate voltas #311986 - single instrument
    repeat("repeat58.mscx", u"1;2; 1; 3;4;5; 4; 6;7");
}

TEST_F(EngravingRepeatTests, repeat59) {
    // duplicate voltas #311986 - multiple instruments
    repeat("repeat59.mscx", u"1;2; 1; 3;4;5; 4; 6;7");
}

TEST_F(EngravingRepeatTests, repeat60) {
    // overlapping voltas
    repeat("repeat60.mscx", u"1;2;6;7; 1;2;3;6;7; 1;2;3;4;5;6;7; 1;2;3;6;7; 1;2;6;7; 1;7");
}

TEST_F(EngravingRepeatTests, repeat61) {
    // overlapping voltas - nested
    repeat("repeat61.mscx", u"1;2;3;6;7; 1;2;3;4;5;6;7; 1;2;3;6;7; 1;7");
}

TEST_F(EngravingRepeatTests, repeat62) {
    // overlapping voltas - same start
    repeat("repeat62.mscx", u"1;5;6;7; 1;2;3;4;5;6;7; 1;5;6;7; 1;7");
}

TEST_F(EngravingRepeatTests, repeat63) {
    // overlapping voltas - same end
    repeat("repeat63.mscx", u"1;2;3;7; 1;2;3;4;5;6;7; 1;2;3;7; 1;7");
}

TEST_F(EngravingRepeatTests, repeat64) {
    // overlapping voltas
    repeat("repeat64.mscx", u"1;2;3;7; 1;2;3;4;5;6;7; 1;2;3;7; 1;7");
}

TEST_F(EngravingRepeatTests, repeat65) {
    // Jump at volta end with end repeat
    repeat("repeat65.mscx", u"1;2;3;4; 2;3;4; 1;2; 5");
}

TEST_F(EngravingRepeatTests, repeat66) {
    // final repeat and new section
    repeat("repeat66.mscx", u"1;2;3;4; 2;3;4; 1;2; 5; 1;2;3;4; 2;3;4; 2; 5");
}

TEST_F(EngravingRepeatTests, repeat67) {
    // Jump at skipped open volta end with end repeat at end of score: #327681
    repeat("repeat67.mscx", u"1;2; 1");
}

TEST_F(EngravingRepeatTests, repeat68) {
    // Entire score skipped by volta: gh#14685
    repeat("repeat68.mscx", u"");
}

// There are 2 instruments (Piano) in this score, and each instrument has a D.S. at the same position (3rd measure)
// Make sure that we don't repeat the measures twice
// See: https://github.com/musescore/MuseScore/issues/27647
TEST_F(EngravingRepeatTests, repeat69) { repeat("repeat69.mscx", u"1; 2;3; 2;3; 4"); }

TEST_F(EngravingRepeatTests, repeat70) {
    // Open volta whose endings lie beyond the exhausted repeats: it is played after them
    repeat("repeat70.mscx", u"1;2; 1; 3;4;5");
}

TEST_F(EngravingRepeatTests, repeat71) {
    // Same as repeat70 with a Marker inside the open volta: it must not affect playback
    repeat("repeat71.mscx", u"1;2; 1; 3;4;5");
}

TEST_F(EngravingRepeatTests, repeat72) {
    // Same as repeat70 with a closed volta: it is played after the exhausted repeats as well
    repeat("repeat72.mscx", u"1;2; 1; 3;4;5");
}
