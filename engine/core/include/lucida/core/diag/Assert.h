// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Assertions (GEA 3.3.3).
//   LUCIDA_ASSERT - compiled out in shipping builds.
//   LUCIDA_VERIFY - always evaluates the expression; safe for calls with side effects.

#include "lucida/core/platform/Platform.h"

namespace lucida::diag {

void ReportAssertFailure(const char* expr, const char* file, int line, const char* fmt, ...);

} // namespace lucida::diag

#define LUCIDA_FATAL(...)                                                       \
    do {                                                                        \
        ::lucida::diag::ReportAssertFailure("FATAL", __FILE__, __LINE__, __VA_ARGS__); \
        LUCIDA_DEBUGBREAK();                                                    \
    } while (0)

#define LUCIDA_VERIFY(expr, ...)                                                \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ::lucida::diag::ReportAssertFailure(#expr, __FILE__, __LINE__, __VA_ARGS__); \
            LUCIDA_DEBUGBREAK();                                                \
        }                                                                       \
    } while (0)

#if defined(LUCIDA_SHIPPING)
#   define LUCIDA_ASSERT(expr, ...) ((void)0)
#else
#   define LUCIDA_ASSERT(expr, ...) LUCIDA_VERIFY(expr, __VA_ARGS__)
#endif
