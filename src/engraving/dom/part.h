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

#pragma once

#include <vector>

#include "../types/types.h"
#include "instrument.h"

namespace mu::engraving {
struct Staff;
struct Score;
struct SharedPart;
struct InstrumentTemplate;

struct TrackRange {
    track_idx_t start_track = muse::nidx;
    track_idx_t end_track = muse::nidx;
};

//---------------------------------------------------------
//   @@ Part
//   @P endTrack        int         (read only)
//   @P harmonyCount    int         (read only)
//   @P hasDrumStaff    bool        (read only)
//   @P hasPitchedStaff bool        (read only)
//   @P hasTabStaff     bool        (read only)
//   @P instrumentId    string      (read only)
//   @P longName        string
//   @P lyricCount      int         (read only)
//   @P midiChannel     int         (read only)
//   @P midiProgram     int         (read only)
//   @P mute            bool
//   @P partName        string      name of the part, used in the mixer
//   @P shortName       string
//   @P show            bool        check/set whether or not a part is shown
//   @P startTrack      int         (read only)
//   @P volume          int
//---------------------------------------------------------

struct Part : EngravingObject {
private:
    OBJECT_ALLOCATOR(engraving, Part)
    DECLARE_CLASSOF(ElementType::PART)

public:
    static const Fraction main_instrument_tick;

    Part(Score *score = nullptr, ElementType type = ElementType::PART);
    ~Part() override;
    void init_from_instr_template(const InstrumentTemplate *);

    const muse::ID &id() const;
    void set_id(const muse::ID &id);

    Part *clone() const;

    size_t nstaves() const;
    size_t visible_staves_count() const;
    const std::vector<Staff *> &staves() const;
    std::set<staff_idx_t> stave_idx_list() const;
    void append_staff(Staff *staff);
    void clear_staves();

    Staff *staff(staff_idx_t idx) const;
    String family_id() const;

    TrackRange track_range() const;

    InstrumentTrackIdList instrument_track_id_list() const;
    InstrumentTrackIdSet instrument_track_id_set() const;

    String long_name(const Fraction &tick = {-1, 1}) const;
    String short_name(const Fraction &tick = {-1, 1}) const;
    String instrument_name(const Fraction &tick = {-1, 1}) const;
    String instrument_id(const Fraction &tick = {-1, 1}) const;

    void set_long_name(const String &s, const Fraction &tick = {-1, 1});
    void set_short_name(const String &s, const Fraction &tick = {-1, 1});
    void set_long_name_all(const String &s);  // For all instruments in _instruments
    void set_short_name_all(const String &s); // For all instruments in _instruments

    int number(const Fraction &tick = {-1, 1}) const;
    void set_number(int v, const Fraction &tick = {-1, 1});

    String transposition(const Fraction &tick = {-1, 1}) const;
    void set_transposition(const String &s, const Fraction &tick = {-1, 1});

    void set_plain_long_name(const String &s);
    void set_plain_short_name(const String &s);
    void set_plain_long_name_all(const String &s);
    void set_plain_short_name_all(const String &s);

    void set_staves(int);

    int midi_program() const;
    void set_midi_program(int, int bank = 0);

    int midi_channel() const;
    int midi_port() const;
    void set_midi_channel(int ch, int port = -1, const Fraction &tick = {-1, 1}); // tick != -1 for InstrumentChange

    void insert_staff(Staff *, staff_idx_t idx);
    void remove_staff(Staff *);
    virtual bool show() const;

    void set_show(bool val) { show_ = val; }

    bool soloist() const { return soloist_; }

    void set_soloist(bool val) { soloist_ = val; }

    Instrument *instrument(Fraction = {-1, 1});
    const Instrument *instrument(Fraction = {-1, 1}) const;
    const Instrument *instrument_by_id(const String &id) const;
    void set_instrument(Instrument *, Fraction = {-1, 1}); // transfer ownership
    void set_instrument(Instrument *, int tick);
    void set_instrument(const Instrument &&, Fraction = {-1, 1});
    void set_instrument(const Instrument &, Fraction = {-1, 1});
    void set_instruments(const InstrumentList &instruments);
    void remove_instrument(Instrument *, const Fraction &);
    void remove_non_primary_instruments();
    const InstrumentList &instruments() const;

    const std::map<int, StringTunings *> &string_tunings() const { return string_tunings_; }

    const StringData *string_data(const Fraction &tick, staff_idx_t staff_idx) const;
    void add_string_tunings(StringTunings *string_tunings);
    void remove_string_tunings(StringTunings *string_tunings);
    StringTunings *next_string_tuning(const Fraction &tick) const;

    void insert_time(const Fraction &tick, const Fraction &len);

    const std::map<int, HarpPedalDiagram *> &harp_diagrams() const { return harp_diagrams_; }

    void add_harp_diagram(HarpPedalDiagram *);
    void remove_harp_diagram(HarpPedalDiagram *);
    void clear_harp_diagrams();
    HarpPedalDiagram *current_harp_diagram(const Fraction &) const;
    HarpPedalDiagram *next_harp_diagram(const Fraction &) const;
    HarpPedalDiagram *prev_harp_diagram(const Fraction &) const;
    Fraction current_harp_diagram_tick(const Fraction &) const;

    virtual String part_name() const;

    bool is_visible() const;

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;

    int lyric_count() const;
    int harmony_count() const;
    bool has_pitched_staff() const;
    bool has_tab_staff() const;
    bool has_drum_staff() const;

    void update_harmony_channels(bool is_do_on_instrument_changed, bool check_removal = false);
    const InstrChannel *harmony_channel() const;
    bool has_chord_symbol() const;

    const Part *master_part() const;
    Part *master_part();

    AutoOnOff hide_when_empty() const { return hide_when_empty_; }

    void set_hide_when_empty(AutoOnOff v) { hide_when_empty_ = v; }

    bool hide_staves_when_individually_empty() const { return hide_staves_when_individually_empty_; }

    void set_hide_staves_when_individually_empty(bool v) { hide_staves_when_individually_empty_ = v; }

    PreferSharpFlat prefer_sharp_flat() const { return prefer_sharp_flat_; }

    void set_prefer_sharp_flat(PreferSharpFlat v) { prefer_sharp_flat_ = v; }

    virtual const Part *origin_part(track_idx_t track, const Fraction &tick) const;
    Part *origin_part(track_idx_t track, const Fraction &tick);

    SharedPart *shared_part() const { return shared_part_; }

    void set_shared_part(SharedPart *p) { shared_part_ = p; }

private:
    friend struct SharedPart;

    static String plain_part_name(const Instrument *instrument);

    InstrumentList instruments_;
    std::vector<Staff *> staves_;
    muse::ID id_ = ::mu::engraving::invalid_id; ///< used for MusicXML import
    bool show_ = false;                         ///< show part in partitur if true
    bool soloist_ = false;                      ///< used in score ordering

    /// Hide staves in this part when empty
    AutoOnOff hide_when_empty_ = AutoOnOff::AUTO;

    /// Hide staves when they are individually empty, rather than only if all this part's staves are empty
    bool hide_staves_when_individually_empty_ = false;

    PreferSharpFlat prefer_sharp_flat_ = PreferSharpFlat::AUTO;

    std::map<int, HarpPedalDiagram *> harp_diagrams_;
    std::map<int, StringTunings *> string_tunings_;

    SharedPart *shared_part_ = nullptr;
};
} // namespace mu::engraving
