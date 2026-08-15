// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/core/memory/LinearAllocator.h"
#include "lucida/core/diag/Assert.h"
#include "lucida/core/diag/Log.h"

#include <cstdlib>

namespace lucida {

LinearAllocator::LinearAllocator(usize capacity_bytes, const char* name) {
    Init(capacity_bytes, name);
}

LinearAllocator::~LinearAllocator() { Shutdown(); }

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : m_base(other.m_base), m_capacity(other.m_capacity), m_offset(other.m_offset),
      m_peak(other.m_peak), m_name(other.m_name) {
    other.m_base = nullptr;
    other.m_capacity = other.m_offset = other.m_peak = 0;
}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept {
    if (this != &other) {
        Shutdown();
        m_base = other.m_base; m_capacity = other.m_capacity;
        m_offset = other.m_offset; m_peak = other.m_peak; m_name = other.m_name;
        other.m_base = nullptr;
        other.m_capacity = other.m_offset = other.m_peak = 0;
    }
    return *this;
}

void LinearAllocator::Init(usize capacity_bytes, const char* name) {
    LUCIDA_ASSERT(m_base == nullptr, "'%s' already initialised", name);
    const usize bytes = AlignUp(capacity_bytes, LUCIDA_CACHE_LINE);
    m_base = static_cast<u8*>(std::aligned_alloc(LUCIDA_CACHE_LINE, bytes));
    LUCIDA_VERIFY(m_base != nullptr, "'%s': cannot reserve %zu bytes", name, bytes);
    m_capacity = bytes;
    m_offset   = 0;
    m_peak     = 0;
    m_name     = name;
    LUCIDA_DEBUG(Memory, "linear '%s': %.2f MiB", name, double(bytes) / (1024.0 * 1024.0));
}

void LinearAllocator::Shutdown() {
    if (!m_base) return;
    LUCIDA_DEBUG(Memory, "linear '%s': peak %.2f of %.2f MiB", m_name,
                 double(m_peak) / (1024.0 * 1024.0), double(m_capacity) / (1024.0 * 1024.0));
    std::free(m_base);
    m_base = nullptr;
    m_capacity = m_offset = 0;
}

void* LinearAllocator::Allocate(usize size, usize alignment) {
    LUCIDA_ASSERT(IsPowerOfTwo(alignment), "alignment %zu is not a power of two", alignment);
    LUCIDA_ASSERT(m_base != nullptr, "'%s' not initialised", m_name);

    const uptr base    = reinterpret_cast<uptr>(m_base);
    const uptr aligned = AlignUp(base + m_offset, alignment);
    const usize next   = static_cast<usize>(aligned - base) + size;

    if (next > m_capacity) {
        LUCIDA_ERROR(Memory, "linear '%s' out of space: want %zu, free %zu",
                     m_name, size, m_capacity - m_offset);
        return nullptr;
    }

    m_offset = next;
    if (m_offset > m_peak) m_peak = m_offset;
    return reinterpret_cast<void*>(aligned);
}

void LinearAllocator::Deallocate(void* ptr) {
    if (!ptr) return;
    LUCIDA_ASSERT(false, "linear '%s': use ReleaseTo(Mark()) or ScopedMark", m_name);
}

void LinearAllocator::ReleaseTo(Marker marker) {
    LUCIDA_ASSERT(marker <= m_offset, "marker %zu is past the top %zu", marker, m_offset);
    m_offset = marker;
}

} // namespace lucida
