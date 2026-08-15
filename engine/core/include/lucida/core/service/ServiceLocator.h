// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Service Locator (GPP ch.5).
//
// Subsystems find each other by interface, not by including each other's
// headers. Register concrete backends at startup in main(); the engine only
// ever sees the interface. A missing service returns the null implementation
// if one was provided, so callers do not branch on nullptr everywhere.

#include "lucida/core/diag/Assert.h"
#include "lucida/core/platform/Platform.h"

namespace lucida {

template <typename Service>
class Locator {
public:
    static void Provide(Service* service) { s_service = service; }
    static void ProvideNull(Service* null_service) { s_null = null_service; }

    static Service* Get() { return s_service ? s_service : s_null; }

    // For call sites that cannot proceed without it.
    static Service& Require() {
        Service* s = Get();
        LUCIDA_ASSERT(s != nullptr, "service is not registered");
        return *s;
    }

    static bool Has() { return s_service != nullptr; }
    static void Reset() { s_service = nullptr; }

private:
    inline static Service* s_service = nullptr;
    inline static Service* s_null    = nullptr;
};

} // namespace lucida
