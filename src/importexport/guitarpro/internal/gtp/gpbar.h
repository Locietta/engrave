#ifndef MU_IMPORTEXPORT_GPBAR_H
#define MU_IMPORTEXPORT_GPBAR_H

#include "gpvoice.h"

namespace mu::iex::guitarpro {
class GPBar {
public:
    enum class ClefType { G2, F4, C3, C4, NEUTRAL };
    enum class OttaviaType { MA15, VA8, REGULAR, VB8, MB15 };
    enum class SimileMark { NONE, SIMPLE, FIRST_OF_DOUBLE, SECOND_OF_DOUBLE };

    struct Clef {
        ClefType type{ClefType::G2};
        OttaviaType ottavia{OttaviaType::REGULAR};

        bool operator==(const GPBar::Clef &c) const { return type == c.type && ottavia == c.ottavia; }
    };

    ~GPBar() = default;

    void add_gp_voice(std::unique_ptr<GPVoice> &&v) { voices_value.push_back(std::move(v)); }

    void set_clef_type(GPBar::ClefType clef) { clef_value.type = clef; }

    void set_ottavia_type(GPBar::OttaviaType ottavia) { clef_value.ottavia = ottavia; }

    GPBar::Clef clef() const { return clef_value; }

    void set_simile_mark(SimileMark s) { simile_mark_value = s; }

    SimileMark simile_mark() const { return simile_mark_value; }

    void set_id(int id) { id_value = id; }

    const std::vector<std::unique_ptr<GPVoice>> &voices() const { return voices_value; }

private:

    int id_value{-1};
    std::vector<std::unique_ptr<GPVoice>> voices_value;
    Clef clef_value;
    SimileMark simile_mark_value;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPBAR_H
