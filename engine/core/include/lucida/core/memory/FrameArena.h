#pragma once
// Per-frame scratch memory. Double buffered so data produced this frame can be
// read next frame while the other half fills (GPP: Double Buffer).

#include "lucida/core/memory/LinearAllocator.h"

namespace lucida {

class FrameArena {
public:
    void Init(usize bytes_per_frame) {
        m_arena[0].Init(bytes_per_frame, "frame-a");
        m_arena[1].Init(bytes_per_frame, "frame-b");
    }
    void Shutdown() { m_arena[0].Shutdown(); m_arena[1].Shutdown(); }

    // Called once per frame, before anything allocates.
    void Flip() {
        m_current ^= 1;
        m_arena[m_current].Reset();
    }

    LinearAllocator& Current()  { return m_arena[m_current]; }
    LinearAllocator& Previous() { return m_arena[m_current ^ 1]; }

    template <typename T>
    T* AllocArray(usize count) {
        return static_cast<T*>(Current().Allocate(sizeof(T) * count, alignof(T)));
    }

private:
    LinearAllocator m_arena[2];
    u32 m_current = 0;
};

} // namespace lucida
