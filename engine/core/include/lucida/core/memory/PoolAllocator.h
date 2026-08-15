#pragma once
// Pool allocator (GEA 6.2.1.2). Fixed-size blocks, O(1) alloc and free.
// Free blocks hold the free list inline, so the pool costs no side table.

#include "lucida/core/memory/Allocator.h"

namespace lucida {

class PoolAllocator final : public IAllocator {
public:
    PoolAllocator() = default;
    PoolAllocator(usize block_size, usize block_count, usize alignment = alignof(std::max_align_t),
                  const char* name = "pool");
    ~PoolAllocator() override;

    LUCIDA_NO_COPY(PoolAllocator);

    void Init(usize block_size, usize block_count, usize alignment = alignof(std::max_align_t),
              const char* name = "pool");
    void Shutdown();

    // size must fit the block; alignment must not exceed the pool's.
    void* Allocate(usize size, usize alignment = alignof(std::max_align_t)) override;
    void  Deallocate(void* ptr) override;

    usize BytesUsed() const override { return m_live * m_block_size; }
    usize Capacity()  const override { return m_block_count * m_block_size; }
    usize LiveBlocks() const { return m_live; }
    usize FreeBlocks() const { return m_block_count - m_live; }

private:
    struct FreeNode { FreeNode* next; };

    u8*       m_base        = nullptr;
    FreeNode* m_free        = nullptr;
    usize     m_block_size  = 0;
    usize     m_block_count = 0;
    usize     m_live        = 0;
    const char* m_name      = "pool";
};

// Typed wrapper. Construction and destruction still go through New/Delete.
template <typename T>
class TypedPool {
public:
    void Init(usize count, const char* name = "pool") {
        m_pool.Init(sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T), count, alignof(T), name);
    }
    void Shutdown() { m_pool.Shutdown(); }

    template <typename... Args>
    T* Create(Args&&... args) { return m_pool.New<T>(static_cast<Args&&>(args)...); }
    void Destroy(T* ptr)      { m_pool.Delete(ptr); }

    usize Live() const { return m_pool.LiveBlocks(); }

private:
    PoolAllocator m_pool;
};

} // namespace lucida
