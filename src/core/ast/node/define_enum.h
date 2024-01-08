/*license*/
#pragma once
#include <array>
#include <type_traits>
#include <string_view>
#include <optional>

namespace brgen {
    namespace internal {
        template <class T, size_t... i>
        constexpr auto enum_string_array_impl(std::index_sequence<i...>) {
            return std::array<std::pair<T, std::string_view>, sizeof...(i)>{{
                {T(i), enum_string<i>(T{})}...,
            }};
        }
    }  // namespace internal

    template <class T, size_t i = 0>
    constexpr size_t enum_size() {
        if constexpr (enum_end<i>(T{})) {
            return i;
        }
        else {
            return enum_size<T, i + 1>();
        }
    }

    template <class T>
    constexpr std::array<std::pair<T, std::string_view>, enum_size<T>()> make_enum_string_array() {
        constexpr auto size = enum_size<T>();
        return internal::enum_string_array_impl<T>(std::make_index_sequence<size>{});
    }

    template <class T>
    constexpr auto enum_string_array = make_enum_string_array<T>();

#define define_enum(T)                            \
    namespace T {                                 \
        enum class T {};                          \
        using InternalT = T;                      \
        template <size_t i>                       \
        constexpr bool enum_end(T) {              \
            return true;                          \
        }                                         \
        constexpr auto begin_line = __LINE__ + 1; \
        template <size_t i>                       \
        constexpr const char* enum_string(T) {    \
            return nullptr;                       \
        }

#define define_enum_value_str(V, S)                       \
    constexpr auto V##_n = __LINE__ - begin_line;         \
    inline constexpr auto V = InternalT(V##_n);           \
    template <>                                           \
    constexpr bool enum_end<V##_n>(InternalT) {           \
        return false;                                     \
    }                                                     \
    constexpr auto V##_str = S;                           \
    template <>                                           \
    constexpr const char* enum_string<V##_n>(InternalT) { \
        return V##_str;                                   \
    }

#define define_enum_value(V) define_enum_value_str(V, #V)

#define end_define_enum()                                     \
    constexpr void as_json(InternalT t, auto&& d) {           \
        d.value(enum_string_array<InternalT>[int(t)].second); \
    }                                                         \
    }

    template <class T>
    constexpr std::optional<T> enum_from_string(std::string_view str) {
        for (auto&& [e, s] : enum_string_array<T>) {
            if (s == str) {
                return e;
            }
        }
        return std::nullopt;
    }

    template <class T>
    constexpr std::string_view enum_to_string(T e) {
        return enum_string_array<T>[int(e)].second;
    }

    namespace test {
        // comment
        define_enum(TestEnum);
        define_enum_value(A);
        define_enum_value(B);
        define_enum_value_str(C, "C_");
        end_define_enum();

        static_assert(enum_size<TestEnum::TestEnum>() == 3);

        constexpr auto test_enum_str = enum_string_array<TestEnum::TestEnum>;

        static_assert(test_enum_str[0].first == TestEnum::A);

        static_assert(test_enum_str[0].second == "A");

        static_assert(enum_to_string(TestEnum::A) == "A");

    }  // namespace test

}  // namespace brgen
