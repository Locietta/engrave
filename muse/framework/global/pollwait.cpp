// SPDX-License-Identifier: GPL-3.0-only
#include "pollwait.h"

#include <chrono>

#ifdef _WIN32
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

using namespace muse;

PollWait::PollWait() {
#ifdef _WIN32
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!event_ || !timer_) {
        if (event_) CloseHandle(event_);
        if (timer_) CloseHandle(timer_);
        event_ = timer_ = nullptr;
    }
#endif
}

PollWait::~PollWait() {
#ifdef _WIN32
    if (event_) CloseHandle(event_);
    if (timer_) CloseHandle(timer_);
#endif
}

void PollWait::signal() {
#ifdef _WIN32
    if (event_) {
        SetEvent(event_);
        return;
    }
#endif
    cv_.notify_all();
}

void PollWait::wait(std::unique_lock<std::mutex> &lock) {
#ifdef _WIN32
    if (event_) {
        LARGE_INTEGER due;
        // Relative, in 100 ns units. The timer fires up to ~0.5 ms late, so ask for half the interval.
        due.QuadPart = -5'000;
        lock.unlock();
        if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)) {
            const HANDLE handles[] = {event_, timer_};
            WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        } else {
            WaitForSingleObject(event_, 1);
        }
        lock.lock();
        return;
    }
#endif
    cv_.wait_for(lock, std::chrono::milliseconds(1));
}
