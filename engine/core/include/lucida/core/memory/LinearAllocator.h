#pragma once
// Stack allocator (GEA 6.2.1). Bump pointer; individual blocks are never freed,
// the stack rewinds to a marker instead. No fragmentation, no free-list search.

#include "lucida/core/memory/Allocator.h"

namespace lucida {

class LinearAllocator final : public IAllocator {
public:
    using Marker = usize;

    LinearAllocator() = default;
    explicit LinearAllocator(usize capacity_bytes, const char* name = "linear");
    ~LinearAllocator() override;

    LUCIDA_NO_COPY(LinearAllocator);
    LinearAllocator(LinearAllocator&& other) noexcept;
    LinearAllocator& operator=(LinearAllocator&& other) noexcept;

    void Init(usize capacity_bytes, const char* name = "linear");
    void Shutdown();

    void* Allocate(usize size, usize alignment = alignof(std::max_align_t)) override;

    // Asserts. Calling it is a bug at the call site, not a no-op worth hiding.
    void  Deallocate(void* ptr) override;

    Marker Mark() const { return m_offset; }
    void   ReleaseTo(Marker marker);
    void   Reset() { m_offset = 0; }

    usize BytesUsed() const override { return m_offset; }
    usize Capacity()  const override { return m_capacity; }
    usize PeakUsed()  const { return m_peak; }
    const char* Name() const { return m_name; }

private:
    u8*   m_base     = nullptr;
    usize m_capacity = 0;
    usize m_offset   = 0;
    usize m_peak     = 0;
    const char* m_name = "linear";
};

// Rewinds the allocator on scope exit.
class ScopedMark {
public:
    explicit ScopedMark(LinearAllocator& allocator)
        : m_allocator(allocator), m_marker(allocator.Mark()) {}
    ~ScopedMark() { m_allocator.ReleaseTo(m_marker); }

    LUCIDA_NO_COPY(ScopedMark);

private:
    LinearAllocator& m_allocator;
    LinearAllocator::Marker m_marker;
};

} // namespace lucida
