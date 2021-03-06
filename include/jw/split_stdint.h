/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2018 J.W. Jagersma, see COPYING.txt for details */
/* Copyright (C) 2017 J.W. Jagersma, see COPYING.txt for details */

#pragma once
#include <cstdint>
#pragma GCC diagnostic error "-Wpadded"

namespace jw
{
    namespace detail
    {
        template<typename T, std::size_t size, typename condition = bool>
        union split_int;

        template<typename T, std::size_t size>
        union [[gnu::packed]] split_int<T, size, std::enable_if_t<(size / 2) % 2 == 0, bool>>
        {
            struct [[gnu::packed]]
            {
                split_int<unsigned, (size / 2)> lo;
                split_int<T, (size / 2)> hi;
            };
            std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t> value : size;
            constexpr split_int() noexcept = default;
            constexpr split_int(auto v) noexcept : value(v) { };
            template<typename L, typename H> constexpr split_int(L&& l, H&& h) noexcept : lo(std::forward<L>(l)), hi(std::forward<H>(h)) { };
            constexpr operator auto() const noexcept { return value; }
        };

        template<typename T, std::size_t size>
        union [[gnu::packed]] split_int<T, size, std::enable_if_t<(size / 2) % 2 != 0, bool>>
        {
            struct [[gnu::packed]]
            {
                 unsigned lo : size / 2;
                 T hi : size / 2;
            };
            std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t> value : size;
            constexpr split_int() noexcept = default;
            constexpr split_int(auto v) noexcept : value(v) { };
            template<typename L, typename H> constexpr split_int(L&& l, H&& h) noexcept : lo(std::forward<L>(l)), hi(std::forward<H>(h)) { };
            constexpr operator auto() const noexcept { return value; }
        };

        template<typename T>
        union [[gnu::packed]] split_int<T, 8, bool>
        {
            std::conditional_t<std::is_signed<T>::value, std::int8_t, std::uint8_t> value;
            constexpr split_int() noexcept = default;
            constexpr split_int(auto v) noexcept : value(v) { }
            constexpr operator auto() const noexcept { return value; }
        };
    }

    template<typename T, std::size_t N> using split_int[[gnu::aligned([] { return std::min(N / 8, 4ul); }())]] = detail::split_int<T, N>;

    using split_uint16_t = split_int<unsigned, 16>;
    using split_uint32_t = split_int<unsigned, 32>;
    using split_uint64_t = split_int<unsigned, 64>;
    using split_int16_t = split_int<signed, 16>;
    using split_int32_t = split_int<signed, 32>;
    using split_int64_t = split_int<signed, 64>;

#ifndef __INTELLISENSE__
    static_assert(sizeof(split_uint64_t) == 8);
    static_assert(sizeof(split_uint32_t) == 4);
    static_assert(sizeof(split_uint16_t) == 2);
    static_assert(alignof(split_uint64_t) == 4);
    static_assert(alignof(split_uint32_t) == 4);
    static_assert(alignof(split_uint16_t) == 2);
#endif
}

#pragma GCC diagnostic pop
