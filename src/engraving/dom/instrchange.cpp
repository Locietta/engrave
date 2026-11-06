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

#include "instrchange.h"

#include "translation.h"

#include "../editing/editclef.h"
#include "../editing/editinstrumentchange.h"
#include "../editing/editkeysig.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"

#include "keysig.h"
#include "measure.h"
#include "mscore.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   instrumentChangeStyle
//---------------------------------------------------------

static const ElementStyle instrument_change_style{
    {Sid::INSTRUMENT_CHANGE_PLACEMENT, Pid::PLACEMENT},
    {Sid::INSTRUMENT_CHANGE_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   InstrumentChange
//---------------------------------------------------------

InstrumentChange::InstrumentChange(EngravingObject *parent)
    : TextBase(ElementType::INSTRUMENT_CHANGE, parent, TextStyleType::INSTRUMENT_CHANGE, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::instrument_change_style);
    instrument_ = new Instrument();
}

InstrumentChange::InstrumentChange(const Instrument &i, EngravingObject *parent)
    : TextBase(ElementType::INSTRUMENT_CHANGE, parent, TextStyleType::INSTRUMENT_CHANGE, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::instrument_change_style);
    instrument_ = new Instrument(i);
}

InstrumentChange::InstrumentChange(const InstrumentChange &is) : TextBase(is) {
    instrument_ = new Instrument(*is.instrument_);
    init_ = is.init_;
}

InstrumentChange::~InstrumentChange() { delete instrument_; }

void InstrumentChange::set_instrument(const Instrument &i) {
    *instrument_ = i;
    //delete _instrument;
    //_instrument = new Instrument(i);
}

void InstrumentChange::setup_instrument(const Instrument *instrument, bool update_text) {
    if (!init_) {
        return;
    }

    Transaction &tx = score()->transaction_manager()->current_or_dummy_transaction();

    Fraction tick_start = segment()->tick();
    Part *part = staff()->part();
    Interval old_v = part->instrument(tick_start)->transpose();
    Interval old_kv = staff()->transpose(tick_start);
    Interval v = instrument->transpose();
    bool conc_pitch = style().style_b(Sid::CONCERT_PITCH);

    // change the clef for each staff
    for (size_t i = 0; i < part->nstaves(); i++) {
        ClefType old_clef_type = conc_pitch ? part->instrument(tick_start)->clef_type(i).concert_clef :
                                              part->instrument(tick_start)->clef_type(i).transposing_clef;
        ClefType new_clef_type = conc_pitch ? instrument->clef_type(i).concert_clef : instrument->clef_type(i).transposing_clef;
        // Introduce cleff change only if the new clef *symbol* is different from the old one
        if (ClefInfo::sym_id(old_clef_type) != ClefInfo::sym_id(new_clef_type)) {
            // If instrument change is at the start of a measure, use the measure as the element, as this will place the instrument change before the barline.
            EngravingItem *element = rtick().is_zero() ? to_engraving_item(find_measure()) : to_engraving_item(this);
            EditClef::undo_change_clef(tx, score(), part->staff(i), element, new_clef_type, true);
        }
    }

    // Change key signature if necessary. CAUTION: not necessary in case of octave-transposing!
    if ((v.chromatic - old_v.chromatic) % 12) {
        for (size_t i = 0; i < part->nstaves(); i++) {
            if (!part->staff(i)->key_sig_event(tick_start).is_atonal()) {
                KeySigEvent ks;
                // Check, if some key signature is already there, if no, mark new one "for instrument change"
                Segment *seg = segment()->prev1(SegmentType::KEY_SIG);
                voice_idx_t voice = part->staff(i)->idx() * ::mu::engraving::voices;
                KeySig *ksig = seg ? to_key_sig(seg->element(voice)) : nullptr;
                bool for_inst_change = !(ksig && ksig->tick() == tick_start && !ksig->generated());
                ks.set_for_instrument_change(for_inst_change);
                Key c_key = part->staff(i)->concert_key(tick_start);
                ks.set_concert_key(c_key);
                EditKeySig::undo_change_key_sig(tx, score(), part->staff(i), tick_start, ks);
            }
        }
    }

    // change instrument in all linked scores
    for (EngravingObject *se : link_list()) {
        InstrumentChange *lic = static_cast<InstrumentChange *>(se);
        Instrument *new_instrument = new Instrument(*instrument);
        tx.push(new ChangeInstrument(lic, new_instrument));
    }

    // transpose for current score only
    // this automatically propagates to linked scores
    if (part->instrument(tick_start)->transpose() != old_v) {
        auto i = part->instruments().upper_bound(tick_start.ticks()); // find(), ++i
        Fraction tick_end;
        if (i == part->instruments().end()) {
            tick_end = Fraction(-1, 1);
        } else {
            tick_end = Fraction::from_ticks(i->first);
        }
        Transpose::transposition_changed(tx, score(), part, old_kv, tick_start, tick_end);
    }

    if (!update_text) {
        return;
    }

    //: The text of an "instrument change" marking. It is an instruction to the player to switch to another instrument.
    const String new_instr_change_text = muse::mtrc("engraving", "To %1").arg(instrument->track_name());
    undo_change_property(Pid::TEXT, TextBase::plain_to_xml_text(new_instr_change_text));
}

//---------------------------------------------------------
//   keySigs
//---------------------------------------------------------

std::vector<KeySig *> InstrumentChange::key_sigs(bool all) const {
    std::vector<KeySig *> keysigs;
    Segment *seg = segment()->prev1(SegmentType::KEY_SIG);
    if (seg) {
        voice_idx_t start_voice = part()->staff(0)->idx() * ::mu::engraving::voices;
        voice_idx_t end_voice = part()->staff(part()->nstaves() - 1)->idx() * ::mu::engraving::voices;
        Fraction t = tick();
        for (voice_idx_t i = start_voice; i <= end_voice; i += ::mu::engraving::voices) {
            KeySig *ks = to_key_sig(seg->element(i));
            if (ks && (all || ks->for_instrument_change()) && ks->tick() == t) {
                keysigs.push_back(ks);
            }
        }
    }
    return keysigs;
}

//---------------------------------------------------------
//   clefs
//---------------------------------------------------------

std::vector<Clef *> InstrumentChange::clefs() const {
    std::vector<Clef *> clefs;
    Segment *seg = segment()->prev1(SegmentType::CLEF);
    if (seg) {
        voice_idx_t start_voice = part()->staff(0)->idx() * ::mu::engraving::voices;
        voice_idx_t end_voice = part()->staff(part()->nstaves() - 1)->idx() * ::mu::engraving::voices;
        Fraction t = tick();
        for (voice_idx_t i = start_voice; i <= end_voice; i += ::mu::engraving::voices) {
            Clef *clef = to_clef(seg->element(i));
            if (clef && clef->for_instrument_change() && clef->tick() == t) {
                clefs.push_back(clef);
            }
        }
    }
    return clefs;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue InstrumentChange::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::TEXT_STYLE: return TextStyleType::INSTRUMENT_CHANGE;
        default: return TextBase::property_default(property_id);
    }
}
} // namespace mu::engraving
