#include "lucida/core/diag/Profiler.h"
#include "lucida/core/diag/Log.h"

#include <atomic>

namespace lucida {
namespace {

ProfileSlot      g_slots[kMaxProfileSlots];
std::atomic<u32> g_slot_count{0};

} // namespace

u32 ProfileRegister(const char* name) {
    const u32 slot = g_slot_count.fetch_add(1, std::memory_order_relaxed);
    if (slot >= kMaxProfileSlots) {
        LUCIDA_WARN(Core, "profiler out of slots, '%s' ignored", name);
        return kMaxProfileSlots - 1;
    }
    g_slots[slot].name = name;
    return slot;
}

void ProfileAdd(u32 slot, f64 millis) {
    if (slot >= kMaxProfileSlots) return;
    g_slots[slot].millis += millis;
    ++g_slots[slot].hits;
}

void ProfileEndFrame() {
    const u32 count = g_slot_count.load(std::memory_order_relaxed);
    for (u32 i = 0; i < count && i < kMaxProfileSlots; ++i) {
        ProfileSlot& s = g_slots[i];
        s.millis_avg = s.millis_avg * 0.9 + s.millis * 0.1;
        s.millis = 0.0;
        s.hits   = 0;
    }
}

const ProfileSlot* ProfileSlots(usize& out_count) {
    const usize count = g_slot_count.load(std::memory_order_relaxed);
    out_count = count < kMaxProfileSlots ? count : kMaxProfileSlots;
    return g_slots;
}

} // namespace lucida
