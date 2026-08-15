// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Memory management (GEA 6.2). General malloc in the frame loop means
// fragmentation and unpredictable latency spikes; these have known cost.

#include "lucida/core/platform/Platform.h"

#include <new>

namespace lucida {

LUCIDA_FORCEINLINE uptr AlignUp(uptr value, usize alignment) {
    const uptr mask = static_cast<uptr>(alignment) - 1;
    return (value + mask) & ~mask;
}

LUCIDA_FORCEINLINE bool IsPowerOfTwo(usize v) { return v != 0 && (v & (v - 1)) == 0; }

// Virtual calls are fine here: allocators are hit at subsystem boundaries,
// not per element.
class IAllocator {
public:
    virtual ~IAllocator() = default;

    virtual void* Allocate(usize size, usize alignment = alignof(std::max_align_t)) = 0;
    virtual void  Deallocate(void* ptr) = 0;

    virtual usize BytesUsed() const = 0;
    virtual usize Capacity()  const = 0;

    template <typename T, typename... Args>
    T* New(Args&&... args) {
        void* raw = Allocate(sizeof(T), alignof(T));
        return raw ? new (raw) T(static_cast<Args&&>(args)...) : nullptr;
    }

    template <typename T>
    void Delete(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        Deallocate(ptr);
    }
};

} // namespace lucida
