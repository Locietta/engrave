// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef ENGRAVE_PROFILE
namespace muse::profiler {
class Scope {
public:
    explicit Scope(const char *);
    ~Scope();

private:
    const char *name_;
    long long start_;
};
} // namespace muse::profiler

#define TRACEFUNC muse::profiler::Scope __engrave_scope(__func__);
#define TRACEFUNC_C(info) muse::profiler::Scope __engrave_scope(info);
#else
#define TRACEFUNC
#define TRACEFUNC_C(info)
#endif
