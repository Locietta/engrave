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

#include "../editing/editclef.h"
#include "../editing/editkeysig.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"
#include "keysig.h"
#include "log.h"
#include "measure.h"
#include "mscore.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "translation.h"

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
