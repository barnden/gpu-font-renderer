#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <format>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using TableTag = std::array<char, 4>;

#if __cpp_lib_ranges_enumerate == 202302L
#    define enumerate(v) std::views::enumerate(v)
#else
#    define enumerate(v) std::views::zip(std::views::iota(0), v)
#endif

// Based off of https://stackoverflow.com/a/4410728
#if defined(__linux__)
#    include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#    include <sys/endian.h>
#elif defined(__OpenBSD__)
#    include <sys/types.h>
#    define be16toh(x) betoh16(x)
#    define be32toh(x) betoh32(x)
#    define be64toh(x) betoh64(x)
#elif defined(__APPLE__)
#    include <libkern/_OSByteOrder.h>
#    define be64toh(x) __DARWIN_OSSwapInt64(x)
#endif

constexpr u32 be24toh(u32 x) {
    return (x & 0xFF) << 16 | (x & 0x00FF00) | ((x >> 16) & 0xFF);
}

#if _MSC_VER && !__INTEL_COMPILER
#    define ASSERT_NOT_REACHED __assume(false);
#else
#    define ASSERT_NOT_REACHED __builtin_unreachable();
#endif

template <typename T>
concept Stringable = requires(T x) {
    { x.to_string() } -> std::same_as<std::string>;
};

namespace std {

template <Stringable T>
struct formatter<T> : formatter<string> {
    auto format(T const& instance, auto& context) const
    {
        return formatter<string>::format(instance.to_string(), context);
    }
};
}

struct Fixed {
    u32 data {};

    [[nodiscard]] constexpr auto value() const noexcept -> float
    {
        return static_cast<float>(data >> 16) + (data & 0xFFFF) / static_cast<float>(0xFFFF);
    }
};

struct F2DOT14 {
    static constexpr float MAX = 1. + 16383. / 16384.;
    static constexpr float MIN = -2 - 16383. / 16384.;

    u16 data {};

    F2DOT14(float value)
    {
        float integral = 0.;
        float fractional = 0.;

        fractional = std::modf(value, &integral);

        data = (static_cast<i8>(integral) & 0x3) << 14;
        data |= static_cast<u16>(std::abs(fractional) * 0x10000) & 0xFFFF;
    }

    [[nodiscard]] constexpr auto value() const noexcept -> float
    {
        auto val = static_cast<float>(static_cast<i16>(data) >> 14);
        auto sign = (val < 0.f) ? -1.f : 1.f;
        auto frac = (data & 0x3FFF) / 16384.f;

        return val + sign * frac;
    }

    [[nodiscard]] auto to_string() const noexcept -> std::string
    {
        return std::to_string(value());
    }
};
