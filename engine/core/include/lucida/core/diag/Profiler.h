// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Scoped CPU timing (GEA 3.5 / 15.6). Samples accumulate per named slot and are
// read once a frame; no strings are formatted in the hot path.

#include "lucida/core/platform/Platform.h"
#include "lucida/core/platform/Time.h"

namespace lucida {

inline constexpr usize kMaxProfileSlots = 64;

struct ProfileSlot {
    const char* name  = nullptr;
    f64  millis       = 0.0;   // this frame
    f64  millis_avg   = 0.0;   // smoothed, for UI that must stay readable
    u32  hits         = 0;
};

// Slots are addressed by index so the scope object stores an int, not a name.
u32 ProfileRegister(const char* name);
void ProfileAdd(u32 slot, f64 millis);
void ProfileEndFrame();
const ProfileSlot* ProfileSlots(usize& out_count);

class ProfileScope {
public:
    explicit ProfileScope(u32 slot) : m_slot(slot) {}
    ~ProfileScope() { ProfileAdd(m_slot, m_watch.ElapsedMillis()); }

    LUCIDA_NO_COPY(ProfileScope);

private:
    Stopwatch m_watch;
    u32 m_slot;
};

#define LUCIDA_PROFILE_CAT2(a, b) a##b
#define LUCIDA_PROFILE_CAT(a, b) LUCIDA_PROFILE_CAT2(a, b)
#define LUCIDA_PROFILE(name)                                                        \
    static const ::lucida::u32 LUCIDA_PROFILE_CAT(_lucida_slot_, __LINE__) =        \
        ::lucida::ProfileRegister(name);                                            \
    ::lucida::ProfileScope LUCIDA_PROFILE_CAT(_lucida_scope_, __LINE__)(            \
        LUCIDA_PROFILE_CAT(_lucida_slot_, __LINE__))

} // namespace lucida
