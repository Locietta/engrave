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

#ifndef MU_ENGRAVING_TEMPOTEXT_H
#define MU_ENGRAVING_TEMPOTEXT_H

#include "durationtype.h"
#include "textbase.h"

namespace mu::engraving {
enum class TempoTextType : signed char {
    NORMAL,
    A_TEMPO,
    TEMPO_PRIMO,
};

//-------------------------------------------------------------------
//   @@ TempoText
///    Tempo marker which determines the midi tempo.
//
//   @P tempo       float     tempo in quarter notes (crochets) per second
//   @P followText  bool      determine tempo from text
//-------------------------------------------------------------------

struct TempoText final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, TempoText)
    DECLARE_CLASSOF(ElementType::TEMPO_TEXT)

public:
    TempoText(DummyParentOr<Segment> parent);

    TempoText *clone() const override { return new TempoText(*this); }

    Segment *segment() const { return to_segment(ownership_parent()); }

    Measure *measure() const { return to_measure(ownership_parent()->ownership_parent()); }

    TempoTextType tempo_text_type() const { return tempo_text_type_; }

    void set_tempo_text_type(TempoTextType);

    BeatsPerSecond tempo() const;
    double tempo_bpm() const;
    void set_tempo(BeatsPerSecond v);

    bool is_relative() const { return is_relative_; }

    void set_relative(double v) {
        is_relative_ = true;
        relative_ = v;
    }

    double relative_value() const { return relative_; }

    bool is_normal() const { return tempo_text_type_ == TempoTextType::NORMAL; }

    void set_normal() { set_tempo_text_type(TempoTextType::NORMAL); }

    bool is_a_tempo() const { return tempo_text_type_ == TempoTextType::A_TEMPO; }

    void set_a_tempo() { set_tempo_text_type(TempoTextType::A_TEMPO); }

    bool is_tempo_primo() const { return tempo_text_type_ == TempoTextType::TEMPO_PRIMO; }

    void set_tempo_primo() { set_tempo_text_type(TempoTextType::TEMPO_PRIMO); }

    bool play_tempo_text() const { return play_tempo_text_; }

    void set_play_tempo_text(bool v) { play_tempo_text_ = v; }

    bool follow_text() const { return follow_text_; }

    void set_follow_text(bool v) { follow_text_ = v; }

    void update_tempo();

    TDuration duration() const;

    static int find_tempo_duration(const String &s, int &len, TDuration &dur);
    static String duration2tempo_text_string(const TDuration dur);
    static String duration2user_name(const TDuration t);

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    bool position_relative_to_notehead_rest() const override { return true; }

    String tempo_info() const;

    static constexpr double default_sym_size_ratio = 5.0 / 3.0;

protected:
    void commit_text() override;

    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;

    TempoTextType tempo_text_type_;
    BeatsPerSecond tempo_;     // beats per second
    bool follow_text_ = false; // parse text to determine tempo
    bool play_tempo_text_ = true;
    double relative_ = 0.0;
    bool is_relative_ = false;
    bool align_right_of_rehearsal_mark_ = true;
};
} // namespace mu::engraving
#endif
