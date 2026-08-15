#pragma once
// Platform Independence Layer (GEA 1.6.4).
// Every platform/compiler #ifdef in the engine lives here. Porting means
// editing this file, not hunting macros across the tree.

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#   define LUCIDA_PLATFORM_WINDOWS 1
#   define LUCIDA_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
#   define LUCIDA_PLATFORM_MACOS 1
#   define LUCIDA_PLATFORM_NAME "macOS"
#elif defined(__linux__)
#   define LUCIDA_PLATFORM_LINUX 1
#   define LUCIDA_PLATFORM_NAME "Linux"
#else
#   error "Lucida: unsupported platform"
#endif

#ifndef LUCIDA_PLATFORM_WINDOWS
#   define LUCIDA_PLATFORM_WINDOWS 0
#endif
#ifndef LUCIDA_PLATFORM_MACOS
#   define LUCIDA_PLATFORM_MACOS 0
#endif
#ifndef LUCIDA_PLATFORM_LINUX
#   define LUCIDA_PLATFORM_LINUX 0
#endif

#if defined(__clang__)
#   define LUCIDA_COMPILER_CLANG 1
#elif defined(_MSC_VER)
#   define LUCIDA_COMPILER_MSVC 1
#elif defined(__GNUC__)
#   define LUCIDA_COMPILER_GCC 1
#endif

#if defined(_MSC_VER)
#   define LUCIDA_FORCEINLINE __forceinline
#   define LUCIDA_NOINLINE    __declspec(noinline)
#   define LUCIDA_DEBUGBREAK() __debugbreak()
#   define LUCIDA_RESTRICT    __restrict
#else
#   define LUCIDA_FORCEINLINE inline __attribute__((always_inline))
#   define LUCIDA_NOINLINE    __attribute__((noinline))
#   define LUCIDA_DEBUGBREAK() __builtin_trap()
#   define LUCIDA_RESTRICT    __restrict__
#endif

#define LUCIDA_CACHE_LINE 64

// Keeps cross-thread data off shared cache lines (false sharing).
#define LUCIDA_CACHE_ALIGNED alignas(LUCIDA_CACHE_LINE)

namespace lucida {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;
using iptr  = std::intptr_t;
using uptr  = std::uintptr_t;

static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);

#define LUCIDA_NO_COPY(T)              \
    T(const T&)            = delete;   \
    T& operator=(const T&) = delete

} // namespace lucida
