// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/core/memory/PoolAllocator.h"
#include "lucida/core/diag/Assert.h"
#include "lucida/core/diag/Log.h"

#include <cstdlib>

namespace lucida {

PoolAllocator::PoolAllocator(usize block_size, usize block_count, usize alignment,
                             const char* name) {
    Init(block_size, block_count, alignment, name);
}

PoolAllocator::~PoolAllocator() { Shutdown(); }

void PoolAllocator::Init(usize block_size, usize block_count, usize alignment, const char* name) {
    LUCIDA_ASSERT(m_base == nullptr, "'%s' already initialised", name);
    LUCIDA_ASSERT(IsPowerOfTwo(alignment), "alignment %zu is not a power of two", alignment);
    LUCIDA_ASSERT(block_count > 0, "'%s' needs at least one block", name);

    // A free block must hold a pointer, and every block must stay aligned.
    if (block_size < sizeof(FreeNode)) block_size = sizeof(FreeNode);
    block_size = static_cast<usize>(AlignUp(block_size, alignment));

    m_base = static_cast<u8*>(std::aligned_alloc(alignment, block_size * block_count));
    LUCIDA_VERIFY(m_base != nullptr, "'%s': cannot reserve %zu bytes", name,
                  block_size * block_count);

    m_block_size  = block_size;
    m_block_count = block_count;
    m_live        = 0;
    m_name        = name;

    // Thread the free list through the blocks, front to back, so the first
    // allocations walk forward in memory instead of backwards.
    m_free = nullptr;
    for (usize i = block_count; i-- > 0;) {
        auto* node = reinterpret_cast<FreeNode*>(m_base + i * block_size);
        node->next = m_free;
        m_free = node;
    }

    LUCIDA_DEBUG(Memory, "pool '%s': %zu x %zu B = %.2f MiB", name, block_count, block_size,
                 double(block_count * block_size) / (1024.0 * 1024.0));
}

void PoolAllocator::Shutdown() {
    if (!m_base) return;
    LUCIDA_ASSERT(m_live == 0, "pool '%s' leaked %zu blocks", m_name, m_live);
    std::free(m_base);
    m_base = nullptr;
    m_free = nullptr;
    m_block_count = m_live = 0;
}

void* PoolAllocator::Allocate(usize size, usize alignment) {
    LUCIDA_ASSERT(size <= m_block_size, "pool '%s': %zu B does not fit a %zu B block",
                  m_name, size, m_block_size);
    (void)alignment;

    if (!m_free) {
        LUCIDA_ERROR(Memory, "pool '%s' exhausted (%zu blocks)", m_name, m_block_count);
        return nullptr;
    }
    FreeNode* node = m_free;
    m_free = node->next;
    ++m_live;
    return node;
}

void PoolAllocator::Deallocate(void* ptr) {
    if (!ptr) return;
    const uptr addr = reinterpret_cast<uptr>(ptr);
    const uptr base = reinterpret_cast<uptr>(m_base);
    LUCIDA_ASSERT(addr >= base && addr < base + m_block_size * m_block_count,
                  "pool '%s': pointer is not ours", m_name);
    LUCIDA_ASSERT((addr - base) % m_block_size == 0,
                  "pool '%s': pointer is not on a block boundary", m_name);

    auto* node = static_cast<FreeNode*>(ptr);
    node->next = m_free;
    m_free = node;
    --m_live;
}

} // namespace lucida
