#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <type_traits>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using TableTag = std::array<char, 4>;

// Taken from https://stackoverflow.com/a/4410728
#if defined(__linux__)
#    include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#    include <sys/endian.h>
#elif defined(__OpenBSD__)
#    include <sys/types.h>
#    define be16toh(x) betoh16(x)
#    define be32toh(x) betoh32(x)
#    define be64toh(x) betoh64(x)
#endif

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
