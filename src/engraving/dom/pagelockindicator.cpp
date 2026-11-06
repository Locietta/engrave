/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "pagelockindicator.h"

#include "measure.h"
#include "measurebase.h"
#include "page.h"
#include "rangelock.h"
#include "rendering/iscorerenderer.h"
#include "system.h"

namespace mu::engraving {
PageLockIndicator::PageLockIndicator(DummyParentOr<System> parent, const RangeLock *lock)
    : IndicatorIcon(ElementType::PAGE_LOCK_INDICATOR, parent, ElementFlag::SYSTEM | ElementFlag::GENERATED), page_lock_(lock) {}

void PageLockIndicator::set_selected(bool v) {
    EngravingItem::set_selected(v);
    renderer()->layout_item(this);
    system()->page()->invalidate_bsp_tree();
}

const Page *PageLockIndicator::page() const { return system() ? system()->page() : nullptr; }

String PageLockIndicator::format_bars_and_beats() const {
    const MeasureBase *start_mb = page_lock_->start_mb();
    const MeasureBase *end_mb = page_lock_->end_mb();
    const Measure *start_measure = start_mb->is_measure() ? to_measure(start_mb) : to_measure(start_mb->prev_measure());
    const Measure *end_measure = end_mb->is_measure() ? to_measure(end_mb) : to_measure(end_mb->prev_measure());
    const int start_measure_num = start_measure ? start_measure->measure_number() : -1;
    const int end_measure_num = end_measure ? end_measure->measure_number() : -1;
    return muse::mtrc("engraving", "Start measure: %1; End measure: %2").arg(start_measure_num).arg(end_measure_num);
}
} // namespace mu::engraving
