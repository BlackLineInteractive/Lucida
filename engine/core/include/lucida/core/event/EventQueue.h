// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Event Queue (GPP ch.5).
//
// Sender and receiver are decoupled in time as well as in code: the producer
// pushes and returns, the consumer drains at a point the frame chooses. Fixed
// capacity ring buffer - no allocation while the game is running.

#include "lucida/core/diag/Assert.h"
#include "lucida/core/diag/Log.h"
#include "lucida/core/platform/Platform.h"

#include <array>

namespace lucida {

template <typename Event, usize Capacity = 256>
class EventQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool Push(const Event& e) {
        if (Size() == Capacity) {
            // Dropping is deliberate: growing here would allocate mid-frame and
            // hide the fact that a consumer stopped draining.
            LUCIDA_WARN(Core, "event queue full, dropping event");
            return false;
        }
        m_slots[m_tail & kMask] = e;
        ++m_tail;
        return true;
    }

    bool Pop(Event& out) {
        if (m_head == m_tail) return false;
        out = m_slots[m_head & kMask];
        ++m_head;
        return true;
    }

    // Drain in one pass; handler must not push into the same queue.
    template <typename Fn>
    void Drain(Fn&& handler) {
        while (m_head != m_tail) {
            handler(m_slots[m_head & kMask]);
            ++m_head;
        }
    }

    usize Size()  const { return m_tail - m_head; }
    bool  Empty() const { return m_head == m_tail; }
    void  Clear() { m_head = m_tail = 0; }

private:
    static constexpr usize kMask = Capacity - 1;

    std::array<Event, Capacity> m_slots{};
    usize m_head = 0;
    usize m_tail = 0;
};

} // namespace lucida
