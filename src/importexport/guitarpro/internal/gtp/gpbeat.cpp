#include "gpbeat.h"

namespace mu::iex::guitarpro {
std::pair<int, GPRhythm::RhytmType> GPBeat::lenth() const { return rhythm->length(); }

GPRhythm::Tuplet GPBeat::tuplet() const { return rhythm->tuplet(); }

void GPBeat::add_harmonic_mark_type(GPBeat::HarmonicMarkType type) {
    switch (type) {
        case GPBeat::HarmonicMarkType::ARTIFICIAL: harmonic_mark_info.artificial = true; break;
        case GPBeat::HarmonicMarkType::PINCH: harmonic_mark_info.pinch = true; break;
        case GPBeat::HarmonicMarkType::TAP: harmonic_mark_info.tap = true; break;
        case GPBeat::HarmonicMarkType::SEMI: harmonic_mark_info.semi = true; break;
        case GPBeat::HarmonicMarkType::FEED_BACK: harmonic_mark_info.feedback = true; break;
        case GPBeat::HarmonicMarkType::NONE: break;
    }
}

void GPBeat::sort_gp_notes() { std::sort(notes_value.begin(), notes_value.end(), compare_pitch); }
} // namespace mu::iex::guitarpro
