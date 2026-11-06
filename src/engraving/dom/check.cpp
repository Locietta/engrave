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

#include "chordrest.h"
#include "durationtype.h"
#include "engravingerrors.h"
#include "factory.h"
#include "keysig.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "rest.h"
#include "segment.h"
#include "sig.h"
#include "staff.h"
#include "tuplet.h"
#include "utils.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   checkScore
//---------------------------------------------------------

void Score::check_score() {
    if (!first_measure()) {
        return;
    }

    for (Segment *s = first_measure()->first(); s;) {
        Segment *ns = s->next1();

        if (s->segment_type() & (SegmentType::CHORD_REST)) {
            bool empty = true;
            for (EngravingItem *e : s->elist()) {
                if (e) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                LOGD("checkScore: remove empty ChordRest segment");
            }
        }
        s = ns;
    }

    ChordRest *lcr = 0;
    for (size_t staff_idx = 0; staff_idx < staves_.size(); ++staff_idx) {
        size_t track = staff_idx * ::mu::engraving::voices;
        Fraction tick = Fraction(0, 1);
        Staff *st = staff(staff_idx);
        for (Segment *s = first_measure()->first(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
            ChordRest *cr = to_chord_rest(s->element(static_cast<int>(track)));
            if (!cr) {
                continue;
            }
            if (s->tick() != tick) {
                if (lcr) {
                    Fraction time_stretch = st->time_stretch(lcr->tick());
                    Fraction f = cr->global_ticks() * time_stretch;
                    LOGD() << "Chord/Rest gap at tick " << tick.ticks() << "(" << lcr->type_name() << "+" << f.ticks() << ")-"
                           << s->tick().ticks() << "(" << cr->type_name() << ") staffIdx " << staff_idx << " measure "
                           << cr->measure()->measure_number() << " (len = " << (cr->tick() - tick).ticks() << ")";
                } else {
                    LOGD() << "Chord/Rest gap at tick " << tick.ticks() << "-" << s->tick().ticks() << "(" << cr->type_name() << ") "
                           << "staffIdx " << staff_idx << " measure " << cr->measure()->measure_number()
                           << "  (len = " << (cr->tick() - tick).ticks() << ")";
                }
                tick = s->tick();
            }

            Fraction time_stretch = st->time_stretch(tick);
            Fraction f = cr->global_ticks() * time_stretch;
            tick += f;
            lcr = cr;
        }
    }
}

//---------------------------------------------------------
//   sanityCheck - Simple check for score
///    Check that voice 1 is complete
///    Check that voices > 1 contains less than measure duration
//---------------------------------------------------------

Ret MasterScore::sanity_check() {
    std::string accumulated_errors;

    for (Score *score : score_list()) {
        Ret ret = score->sanity_check_local();
        if (ret) {
            // everything is fine in this part, let's continue
            continue;
        }

        if (!accumulated_errors.empty()) {
            accumulated_errors += "\n";
        }

        accumulated_errors += ret.text();
    }

    if (accumulated_errors.empty()) {
        return muse::make_ok();
    }
    return Ret(static_cast<int>(Err::FILE_CORRUPTED), accumulated_errors);
}

Ret Score::sanity_check_local() {
    TRACEFUNC;

    StringList errors;
    int m_number = 1;

    auto excerpt_info = [this]() {
        if (is_master()) {
            return muse::mtrc("engraving", "Full score");
        }

        //: %1 is the name of a part score.
        return muse::mtrc("engraving", "Part score: %1").arg(name());
    };

    set_has_corrupted_measures(false);

    for (Measure *m = first_measure(); m; m = m->next_measure()) {
        Fraction m_len = m->ticks();
        size_t end_staff = staves().size();

        for (size_t staff_idx = 0; staff_idx < end_staff; ++staff_idx) {
            Rest *fmrest0 = nullptr; // full measure rest in voice 0
            Fraction voices[::mu::engraving::voices];

            m->set_corrupted(staff_idx, false);

            for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                for (voice_idx_t v = 0; v < ::mu::engraving::voices; ++v) {
                    EngravingItem *element = s->element(staff_idx * ::mu::engraving::voices + v);
                    if (!element) {
                        continue;
                    }

                    ChordRest *cr = to_chord_rest(element);
                    voices[v] += cr->actual_ticks();
                    if (v == 0 && cr->is_rest()) {
                        Rest *r = to_rest(cr);
                        if (r->duration_type().is_measure()) {
                            fmrest0 = r;
                        }
                    }
                }
            }

            bool check_repeats = m->is_measure_repeat_group(staff_idx);
            bool repeats_is_valid = true;

            if (check_repeats) {
                repeats_is_valid = m->measure_repeat_element(staff_idx) != nullptr;
            }

            if (!repeats_is_valid) {
                errors << muse::mtrc("engraving", "<b>Corrupted measure</b>: %1, measure %2, staff %3.")
                              .arg(excerpt_info())
                              .arg(m_number)
                              .arg(staff_idx + 1);
                m->set_corrupted(staff_idx, true);
                set_has_corrupted_measures(true);
            }

            if (voices[0] != m_len) {
                //: %1 describes in which score the corruption is (either `Full score` or `"[part name]" part score`)
                errors << muse::mtrc("engraving", "<b>Incomplete measure</b>: %1, measure %2, staff %3. Found: %4. Expected: %5.")
                              .arg(excerpt_info())
                              .arg(m_number)
                              .arg(staff_idx + 1)
                              .arg(voices[0].to_string(), m_len.to_string());
                m->set_corrupted(staff_idx, true);
                set_has_corrupted_measures(true);
                // try to fix a bad full measure rest
                if (fmrest0) {
                    // fmrest0->setDuration(mLen * fmrest0->staff()->timeStretch(fmrest0->tick()));
                    fmrest0->set_ticks(m_len);
                }
            }

            for (voice_idx_t v = 1; v < ::mu::engraving::voices; ++v) {
                if (voices[v] > m_len) {
                    //: %1 describes in which score the corruption is (either `Full score` or `"[part name]" part score`)
                    errors << muse::mtrc("engraving", "<b>Voice too long</b>: %1, measure %2, staff %3, voice %4. Found: %5. Expected: %6.")
                                  .arg(excerpt_info())
                                  .arg(m_number)
                                  .arg(staff_idx + 1)
                                  .arg(v + 1)
                                  .arg(voices[v].to_string(), m_len.to_string());
                    m->set_corrupted(staff_idx, true);
                    set_has_corrupted_measures(true);
                }
            }

            for (Segment *s = m->first(); s; s = s->next()) {
                const Fraction rtick = s->rtick();
                if ((rtick.negative() || rtick > m_len) && s->has_annotation_or_element(staff_idx)) {
                    //: %1 describes in which score the corruption is (either `Full score` or `"[part name]" part score`)
                    errors << muse::mtrc(
                                  "engraving",
                                  "<b>Element outside measure</b>: %1, measure %2, staff %3. Found at: %4. Expected between 0 and %5.")
                                  .arg(excerpt_info())
                                  .arg(m_number)
                                  .arg(staff_idx + 1)
                                  .arg(rtick.to_string(), m_len.to_string());
                    m->set_corrupted(staff_idx, true);
                    set_has_corrupted_measures(true);
                    break;
                }
            }
        }

        m_number++;
    }

    if (errors.empty()) {
        return muse::make_ok();
    }

    return Ret(static_cast<int>(Err::FILE_CORRUPTED), errors.join(u"\n").to_std_string());
}

//---------------------------------------------------------
//   checkKeys
///    check that key map is in sync with actual keys
//---------------------------------------------------------

bool Score::check_keys() {
    bool rc = true;
    for (size_t i = 0; i < nstaves(); ++i) {
        Key k = staff(i)->key(Fraction(0, 1));
        for (Measure *m = first_measure(); m; m = m->next_measure()) {
            Segment *s = m->find_segment(SegmentType::KEY_SIG, m->tick());
            if (s) {
                EngravingItem *element = s->element(static_cast<int>(i) * ::mu::engraving::voices);
                if (element) {
                    k = to_key_sig(element)->key();
                }
            }
            if (staff(i)->key(m->tick()) != k) {
                LOGD("measure %d (tick %d) : key %d, map %d", m->measure_number(), m->tick().ticks(), int(k),
                     int(staff(i)->key(m->tick())));
                rc = false;
            }
        }
    }
    return rc;
}

//---------------------------------------------------------
//   fillGap
//---------------------------------------------------------

void Measure::fill_gap(const Fraction &rtick_start, const Fraction &len, track_idx_t track, const Fraction &stretch, bool use_gap_rests) {
    LOGN("measure %6d pos %d, len %d/%d, stretch %d/%d track %zu", tick().ticks(), rtick_start.ticks(), len.numerator(), len.denominator(),
         stretch.numerator(), stretch.denominator(), track);

    // break the gap into shorter durations if necessary
    std::vector<TDuration> duration_list = to_rhythmic_duration_list(len, true, rtick_start, timesig(), this, 0, stretch);

    Fraction cur_tick = tick() + actual_ticks(rtick_start, nullptr, stretch);
    for (TDuration d : duration_list) {
        Rest *rest = Factory::create_rest(score()->dummy());
        rest->set_ticks(d.is_measure() ? ticks() * stretch : d.fraction());
        rest->set_duration_type(d);
        rest->set_track(track);
        rest->set_gap(use_gap_rests);
        score()->undo_add_cr(rest, this, cur_tick);
        cur_tick += rest->actual_ticks();
    }
}

//---------------------------------------------------------
//   checkMeasure
//    after opening / paste and every read operation
//    this method checks for gaps and fills them
//    with invisible rests
//---------------------------------------------------------

void Measure::check_measure(staff_idx_t staff_idx, bool use_gap_rests) {
    if (is_mm_rest()) {
        return;
    }

    track_idx_t strack = staff_idx * ::mu::engraving::voices;
    track_idx_t dtrack = strack + (has_voices(staff_idx) ? ::mu::engraving::voices : 1);
    Fraction stretch = score()->staff(staff_idx)->time_stretch(tick());
    Fraction f = ticks() * stretch;

    for (track_idx_t track = strack; track < dtrack; track++) {
        Fraction expected_pos = Fraction(0, 1);
        Fraction current_pos = Fraction(0, 1);

        for (Segment *seg = first(SegmentType::CHORD_REST); seg; seg = seg->next(SegmentType::CHORD_REST)) {
            EngravingItem *e = seg->element(track);
            if (!e) {
                continue;
            }

            ChordRest *cr = to_chord_rest(e);
            current_pos = seg->rtick() * stretch;

            if (current_pos < expected_pos) {
                LOGN("in measure overrun %6d at %d-%d track %zu", tick().ticks(), (current_pos / stretch).ticks(),
                     (expected_pos / stretch).ticks(), track);
                break;
            } else if (current_pos > expected_pos) {
                LOGN("in measure underrun %6d at %d-%d track %zu", tick().ticks(), (current_pos / stretch).ticks(),
                     (expected_pos / stretch).ticks(), track);
                fill_gap(expected_pos, current_pos - expected_pos, track, stretch, use_gap_rests);
                if (current_pos >= f) {
                    break;
                }
            }

            DurationElement *de = cr;
            Tuplet *tuplet = cr->top_tuplet();
            if (tuplet) {
                seg = skip_tuplet(tuplet);
                de = tuplet;
            }
            expected_pos = current_pos + de->ticks();
        }

        if (f > expected_pos) {
            // don't fill empty voices (except for the 1st)
            if (expected_pos.is_not_zero() || track2voice(track) == 0) {
                fill_gap(expected_pos, f - expected_pos, track, stretch);
            }
        } else if (f < expected_pos) {
            LOGD("measure overrun %6d, %d > %d, track %zu", tick().ticks(), expected_pos.ticks(), f.ticks(), track);
        }
    }
}
} // namespace mu::engraving
