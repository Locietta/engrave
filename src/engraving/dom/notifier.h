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

#ifndef MU_ENGRAVING_NOTIFIER_H
#define MU_ENGRAVING_NOTIFIER_H

#include "global/allocator.h"

namespace mu::engraving {
template <typename Data>
class Notifier;

//---------------------------------------------------------
//   Listener
//---------------------------------------------------------

template <typename Data>
class Listener {
    OBJECT_ALLOCATOR(engraving, Listener)

public:
    Listener() = default;

    Listener(Notifier<Data> *n) : notifier_(n) {}

    // do not copy notifier attachment
    Listener(const Listener<Data> &) {}

    Listener(Listener<Data> &&);

    Listener &operator=(const Listener<Data> &) { return *this; }

    Listener &operator=(Listener &&);
    virtual ~Listener();

    void set_notifier(Notifier<Data> *n);

    void detach_notifier(Notifier<Data> *n) {
        if (notifier_ == n) {
            set_notifier(nullptr);
        }
    }

    Notifier<Data> *notifier() { return notifier_; }

    const Notifier<Data> *notifier() const { return notifier_; }

    virtual void receive(Data d) = 0;

    template <typename T>
    friend void swap(Listener<T> &l1, Listener<T> &l2);

private:
    Notifier<Data> *notifier_ = nullptr;
};

//---------------------------------------------------------
//   Notifier
//---------------------------------------------------------

template <typename Data>
class Notifier {
public:
    Notifier() = default;

    // do not copy listeners list
    Notifier(const Notifier<Data> &) {}

    Notifier &operator=(const Notifier<Data> &) { return *this; }

    ~Notifier() {
        at_change_ = true; // we don't need to update listeners list anymore
        for (Listener<Data> *l : listeners_) {
            l->detach_notifier(this);
        }
    }

    void add_listener(Listener<Data> *l) {
        if (at_change_ || !l) {
            return;
        }
        at_change_ = true;
        listeners_.push_back(l);
        l->set_notifier(this);
        at_change_ = false;
    }

    void remove_listener(Listener<Data> *l) {
        if (at_change_ || !l) {
            return;
        }
        at_change_ = true;
        listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l), listeners_.end());
        l->detach_notifier(this);
        at_change_ = false;
    }

    void notify(Data d) const {
        for (Listener<Data> *l : listeners_) {
            l->receive(d);
        }
    }

private:

    std::vector<Listener<Data> *> listeners_;
    bool at_change_ = false;
};

template <typename Data>
Listener<Data>::Listener(Listener<Data> &&other) {
    if (Notifier<Data> *n = other.notifier()) {
        n->remove_listener(other);
        set_notifier(n);
    }
}

template <typename Data>
Listener<Data> &Listener<Data>::operator=(Listener<Data> &&other) {
    if (Notifier<Data> *n = other.notifier()) {
        n->remove_listener(other);
        set_notifier(n);
    } else {
        set_notifier(nullptr);
    }

    return *this;
}

template <typename Data>
Listener<Data>::~Listener() {
    if (notifier_) {
        notifier_->remove_listener(this);
    }
}

template <typename Data>
void Listener<Data>::set_notifier(Notifier<Data> *n) {
    if (n == notifier_) {
        return;
    }
    Notifier<Data> *old_notifier = notifier_;
    notifier_ = n;
    if (old_notifier) {
        old_notifier->remove_listener(this);
    }
    if (notifier_) {
        notifier_->add_listener(this);
    }
}

template <typename Data>
void swap(Listener<Data> &l1, Listener<Data> &l2) {
    Notifier<Data> *n1 = l1.notifier();
    Notifier<Data> *n2 = l2.notifier();
    l1.set_notifier(n2);
    l2.set_notifier(n1);
}
} // namespace mu::engraving
#endif
