#pragma once

#include <cstdint>
#include <functional>
#include <cassert>
#include <cstddef>

namespace pdk {

// ── Fixed-width integer aliases (backward-compat names → stdint types) ─────────
using PDK8 = int8_t;
using PDK16 = int16_t;
using PDK32 = int32_t;
using PDK8U = uint8_t;
using PDK16U = uint16_t;
using PDK32U = uint32_t;
using PDKINT = int;
using PDKUINT = unsigned int;

// ── Utility macros ──────────────────────────────────────────────────────────────
#define _COUNTOF(array) (sizeof(array) / sizeof(array[0]))
#define NUMERIC(c) ((c) >= '0' && (c) <= '9')

template<class T>
constexpr T PDK_MIN(T a, T b) noexcept
{
    return a < b ? a : b;
}
template<class T>
constexpr T PDK_MAX(T a, T b) noexcept
{
    return a > b ? a : b;
}
template<class T>
constexpr T PDK_ABS(T x) noexcept
{
    return x >= 0 ? x : -x;
}

// Guard against sys/param.h MIN/MAX
#ifndef MIN
#define MIN(a, b) PDK_MIN(a, b)
#endif
#ifndef MAX
#define MAX(a, b) PDK_MAX(a, b)
#endif
#define ABS(x) PDK_ABS(x)

// ── Assertions ─────────────────────────────────────────────────────────────────
#define ASSERT(p) assert(p)
#define ASSERT_VALID(p) assert((p) != nullptr)

// ── Type-safe callbacks (replace raw function pointers) ────────────────────────
using PCallBack = std::function<int(void*)>;
using PCallBack2 = std::function<int(void*, void*)>;
using PCallBack3 = std::function<int(void*, void*, void*)>;

}  // namespace pdk
