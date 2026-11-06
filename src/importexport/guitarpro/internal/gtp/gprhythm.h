#ifndef MU_IMPORTEXPORT_GPRHYTHM_H
#define MU_IMPORTEXPORT_GPRHYTHM_H

#include <utility>

namespace mu::iex::guitarpro {
class GPRhythm {
public:
    enum class RhytmType { SIXTY_FOURTH, THIRTY_SECOND, SIXTEENTH, EIGHTH, QUARTER, HALF, WHOLE };

    struct Tuplet {
        int num{-1};
        int denom{-1};
    };

    void set_rhytm(RhytmType t) { rhythm = t; }

    std::pair<int, RhytmType> length() const { return {dot_count_value, rhythm}; }

    RhytmType type() const { return rhythm; }

    void set_tuplet(Tuplet t) { tuplet_value = t; }

    Tuplet tuplet() const { return tuplet_value; }

    void set_dot_count(int c) { dot_count_value = c; }

    int dot_count() const { return dot_count_value; }

private:
    RhytmType rhythm{RhytmType::WHOLE};
    Tuplet tuplet_value;
    int dot_count_value{0};
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_GPRHYTHM_H
