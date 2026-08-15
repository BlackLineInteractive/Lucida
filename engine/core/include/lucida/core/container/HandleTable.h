#pragma once
// Dense storage addressed by handle.
//
// Payloads stay packed with no holes, so iteration is a straight walk over
// contiguous memory (DOD ch.8). The sparse slot array maps handle -> dense
// index and carries the generation counter; removal swaps the last element
// into the hole and fixes one back-reference.

#include "lucida/core/container/Handle.h"
#include "lucida/core/diag/Assert.h"

#include <vector>

namespace lucida {

template <typename T, typename Tag>
class HandleTable {
public:
    using HandleType = Handle<Tag>;

    void Reserve(usize n) {
        m_dense.reserve(n);
        m_dense_to_slot.reserve(n);
        m_slots.reserve(n);
    }

    HandleType Add(const T& value) {
        u32 slot;
        if (m_free_head != HandleType::kInvalidIndex) {
            slot = m_free_head;
            m_free_head = m_slots[slot].dense;   // free slots chain through dense
        } else {
            slot = static_cast<u32>(m_slots.size());
            m_slots.push_back(Slot{});
        }

        m_slots[slot].dense = static_cast<u32>(m_dense.size());
        m_slots[slot].alive = true;

        m_dense.push_back(value);
        m_dense_to_slot.push_back(slot);

        return HandleType{ slot, m_slots[slot].generation };
    }

    bool IsValid(HandleType h) const {
        return h.IsValid() && h.index < m_slots.size()
            && m_slots[h.index].alive && m_slots[h.index].generation == h.generation;
    }

    T* Get(HandleType h) {
        if (!IsValid(h)) return nullptr;
        return &m_dense[m_slots[h.index].dense];
    }
    const T* Get(HandleType h) const {
        if (!IsValid(h)) return nullptr;
        return &m_dense[m_slots[h.index].dense];
    }

    void Remove(HandleType h) {
        if (!IsValid(h)) return;

        const u32 dense = m_slots[h.index].dense;
        const u32 last  = static_cast<u32>(m_dense.size() - 1);

        if (dense != last) {
            m_dense[dense] = m_dense[last];
            const u32 moved_slot = m_dense_to_slot[last];
            m_dense_to_slot[dense] = moved_slot;
            m_slots[moved_slot].dense = dense;
        }
        m_dense.pop_back();
        m_dense_to_slot.pop_back();

        m_slots[h.index].alive = false;
        ++m_slots[h.index].generation;          // stale handles now fail IsValid
        m_slots[h.index].dense = m_free_head;
        m_free_head = h.index;
    }

    void Clear() {
        m_dense.clear();
        m_dense_to_slot.clear();
        m_slots.clear();
        m_free_head = HandleType::kInvalidIndex;
    }

    // Packed payloads: iterate these, not the handles.
    std::vector<T>&       Items()       { return m_dense; }
    const std::vector<T>& Items() const { return m_dense; }
    usize Size() const { return m_dense.size(); }
    bool  Empty() const { return m_dense.empty(); }

    HandleType HandleAt(usize dense_index) const {
        const u32 slot = m_dense_to_slot[dense_index];
        return HandleType{ slot, m_slots[slot].generation };
    }

private:
    struct Slot {
        u32  dense      = 0;
        u32  generation = 1;
        bool alive      = false;
    };

    std::vector<T>    m_dense;
    std::vector<u32>  m_dense_to_slot;
    std::vector<Slot> m_slots;
    u32 m_free_head = HandleType::kInvalidIndex;
};

} // namespace lucida
