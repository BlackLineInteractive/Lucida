// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Handles instead of pointers (DOD ch.2: identifiers are the primary key).
// index addresses the slot, generation invalidates stale handles when a slot
// is reused, so a dangling handle is detected instead of read.

#include "lucida/core/platform/Platform.h"

namespace lucida {

template <typename Tag>
struct Handle {
    u32 index      = kInvalidIndex;
    u32 generation = 0;

    static constexpr u32 kInvalidIndex = 0xFFFFFFFFu;

    constexpr bool IsValid() const { return index != kInvalidIndex; }
    constexpr u64  Bits()    const { return (u64(generation) << 32) | index; }

    friend constexpr bool operator==(Handle a, Handle b) {
        return a.index == b.index && a.generation == b.generation;
    }
    friend constexpr bool operator!=(Handle a, Handle b) { return !(a == b); }
};

// Handle types are distinct even though the layout is identical: passing a
// MeshHandle where an InstanceHandle is expected must not compile.
#define LUCIDA_DECLARE_HANDLE(Name)      \
    struct Name##Tag {};                 \
    using Name = ::lucida::Handle<Name##Tag>

} // namespace lucida
