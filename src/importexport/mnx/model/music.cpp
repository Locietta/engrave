// SPDX-License-Identifier: GPL-3.0-only
#include "music.h"

#include <music_theory/music_theory.hpp>

namespace mu::iex::mnxio::model {

namespace {

music_theory::NoteName to_theory(Step step) {
    switch (step) {
        case Step::C: return music_theory::NoteName::C;
        case Step::D: return music_theory::NoteName::D;
        case Step::E: return music_theory::NoteName::E;
        case Step::F: return music_theory::NoteName::F;
        case Step::G: return music_theory::NoteName::G;
        case Step::A: return music_theory::NoteName::A;
        case Step::B: return music_theory::NoteName::B;
    }
    return music_theory::NoteName::C;
}

Step from_theory(music_theory::NoteName name) {
    switch (name) {
        case music_theory::NoteName::C: return Step::C;
        case music_theory::NoteName::D: return Step::D;
        case music_theory::NoteName::E: return Step::E;
        case music_theory::NoteName::F: return Step::F;
        case music_theory::NoteName::G: return Step::G;
        case music_theory::NoteName::A: return Step::A;
        case music_theory::NoteName::B: return Step::B;
    }
    return Step::C;
}

music_theory::Pitch to_theory(const Pitch &pitch) { return {to_theory(pitch.step), pitch.octave, pitch.alter_or_default()}; }

} // namespace

Rational duration_of(const NoteValue &value) {
    std::int64_t num = 1;
    std::int64_t den = 1;
    switch (value.base) {
        case NoteValueBase::N4096TH: den = 4096; break;
        case NoteValueBase::N2048TH: den = 2048; break;
        case NoteValueBase::N1024TH: den = 1024; break;
        case NoteValueBase::N512TH: den = 512; break;
        case NoteValueBase::N256TH: den = 256; break;
        case NoteValueBase::N128TH: den = 128; break;
        case NoteValueBase::N64TH: den = 64; break;
        case NoteValueBase::N32ND: den = 32; break;
        case NoteValueBase::N16TH: den = 16; break;
        case NoteValueBase::EIGHTH: den = 8; break;
        case NoteValueBase::QUARTER: den = 4; break;
        case NoteValueBase::HALF: den = 2; break;
        case NoteValueBase::WHOLE: break;
        case NoteValueBase::BREVE: num = 2; break;
        case NoteValueBase::LONGA: num = 4; break;
        case NoteValueBase::MAXIMA: num = 8; break;
        case NoteValueBase::DUPLEX_MAXIMA: num = 16; break;
    }
    // Dots multiply by 1 + 1/2 + ... + 1/2^dots = (2^(dots+1) - 1) / 2^dots. build_index caps
    // dots, so the shift cannot overflow.
    if (const int dots = value.dots_or_default(); dots > 0) {
        num *= (std::int64_t{1} << (dots + 1)) - 1;
        den *= std::int64_t{1} << dots;
    }
    return {num, den};
}

Rational duration_of(const NoteValueQuantity &quantity) { return duration_of(quantity.duration) * Rational(quantity.multiple); }

Rational duration_of(const Time &time) { return {time.count, static_cast<int>(time.unit)}; }

Rational tuplet_ratio(const Tuplet &tuplet) { return duration_of(tuplet.outer) / duration_of(tuplet.inner); }

int flag_count(NoteValueBase base) {
    switch (base) {
        case NoteValueBase::EIGHTH: return 1;
        case NoteValueBase::N16TH: return 2;
        case NoteValueBase::N32ND: return 3;
        case NoteValueBase::N64TH: return 4;
        case NoteValueBase::N128TH: return 5;
        case NoteValueBase::N256TH: return 6;
        case NoteValueBase::N512TH: return 7;
        case NoteValueBase::N1024TH: return 8;
        case NoteValueBase::N2048TH: return 9;
        case NoteValueBase::N4096TH: return 10;
        default: return 0;
    }
}

SpelledPitch spelled(const Pitch &pitch) { return {pitch.step, pitch.octave, pitch.alter_or_default()}; }

bool same_pitch(const Pitch &lhs, const Pitch &rhs) {
    if (spelled(lhs) == spelled(rhs)) return true;
    const music_theory::Transposer transposer(to_theory(rhs));
    return transposer.isEnharmonicEquivalent(music_theory::calcDisplacement(to_theory(lhs)), lhs.alter_or_default());
}

int semitones(const Pitch &pitch) {
    return music_theory::calc12EdoHalfstepsInInterval(music_theory::calcDisplacement(to_theory(pitch)), pitch.alter_or_default());
}

int transposed_key(const PartTransposition &transposition, int concert_fifths) {
    const auto &interval = transposition.interval;
    const int alteration = music_theory::calcAlterationFrom12EdoHalfsteps(interval.staff_distance, interval.half_steps);
    int result = concert_fifths + music_theory::calcKeySigChangeFromInterval(interval.staff_distance, alteration);
    if (const auto flip_at = transposition.key_fifths_flip_at) {
        constexpr int fifths_wrap = 12; // enharmonic wrap in fifths
        if (*flip_at >= 0) {
            if (result >= *flip_at) result -= fifths_wrap;
        } else if (result <= *flip_at) {
            result += fifths_wrap;
        }
    }
    return result;
}

SpelledPitch written_pitch(const Note &note, const std::optional<PartTransposition> &transposition, int concert_fifths) {
    if (!transposition) return spelled(note.pitch);
    music_theory::Transposer transposer(to_theory(note.pitch));
    const auto &interval = transposition->interval;
    const int displacement = interval.staff_distance;
    const int alteration = music_theory::calcAlterationFrom12EdoHalfsteps(displacement, interval.half_steps);
    transposer.chromaticTranspose(displacement, alteration);
    const int expected_fifths = music_theory::calcKeySigChangeFromInterval(displacement, alteration) + concert_fifths;
    const int wraps = (expected_fifths - transposed_key(*transposition, concert_fifths)) / 12; // truncates toward zero
    if (wraps != 0) transposer.enharmonicTranspose(wraps);
    if (note.written) transposer.enharmonicTranspose(note.written->diatonic_delta_or_default());
    int octaves = 0;
    const int name = music_theory::positiveModulus(transposer.displacement(), music_theory::STANDARD_DIATONIC_STEPS, &octaves);
    return {from_theory(music_theory::noteNames[name]), octaves + 4, transposer.alteration()};
}

} // namespace mu::iex::mnxio::model
