/*license*/
#pragma once
#include "util.h"
#include <escape/escape.h>
#include <concepts>
#include <ranges>
#include <functional>

namespace brgen {
    template <class T, class V>
    concept has_debug = requires(T t) {
        { t.debug(std::declval<V>()) };
    };

    template <class T>
    concept has_deref = requires(T t) {
        { !t } -> std::convertible_to<bool>;
        { *t };
    };

    template <class T>
    concept has_get2 = requires(T t) {
        { get<2>(t.begin()) };
    };

    template <class T>
    concept is_object_like = std::ranges::range<T> && requires(T t) {
        { get<0>(t.begin()) } -> std::convertible_to<std::string_view>;
        { get<1>(t.begin()) };
    } && !has_get2<T>;

    template <class Debug>
    struct DebugFieldBase {
       protected:
        Debug& buffer;
        bool comma = false;

        constexpr DebugFieldBase(Debug& buf)
            : buffer(buf) {}
        constexpr void with_comma(auto&& fn) {
            if (comma) {
                buffer.write(",");
            }
            fn();
            comma = true;
        }
    };

    template <class Debug>
    struct DebugObjectField : DebugFieldBase<Debug> {
        constexpr DebugObjectField(Debug& buf)
            : DebugFieldBase<Debug>(buf) {}

        constexpr void operator()(std::string_view name, auto&& value) {
            this->with_comma([&] {
                this->buffer.string(name);
                this->buffer.write(": ");
                this->buffer.value(value);
            });
        }

        constexpr ~DebugObjectField() {
            this->buffer.write("}");
        }
    };

    template <class Debug>
    struct DebugArrayField : DebugFieldBase<Debug> {
        constexpr DebugArrayField(Debug& buf)
            : DebugFieldBase<Debug>(buf) {}

        void operator()(auto&& value) {
            this->with_comma([&] {
                this->buffer.value(value);
            });
        }

        constexpr ~DebugArrayField() {
            this->buffer.write("]");
        }
    };

#define sdebugf(name) #name, name

    struct Debug {
       private:
        template <class Debug>
        friend struct DebugObjectField;

        template <class Debug>
        friend struct DebugArrayField;

        template <class Debug>
        friend struct DebugFieldBase;
        std::string buf;

        void apply_value(auto&& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Debug>) {
                write(value.buf);
            }
            else if constexpr (std::invocable<T, Debug&>) {
                std::invoke(value, *this);
            }
            else if constexpr (std::invocable<T>) {
                std::invoke(value);
            }
            else if constexpr (has_debug<T, Debug&>) {
                value.debug(*this);
            }
            else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                number(value);
            }
            else if constexpr (std::is_convertible_v<T, std::string_view>) {
                string(value);
            }
            else if constexpr (has_deref<T>) {
                !value ? null() : apply_value(*value);
            }
            else if constexpr (std::is_convertible_v<T, void*>) {
                !value ? null() : number(std::uintptr_t(value));
            }
            else if constexpr (std::is_convertible_v<T, bool>) {
                boolean(bool(value));
            }
            else if constexpr (is_object_like<T>) {
                auto field = object();
                for (auto& v : value) {
                    field(get<0>(v), get<1>(v));
                }
            }
            else if constexpr (std::ranges::range<T>) {
                auto field = array();
                for (auto& v : value) {
                    field(v);
                }
            }
            else {
                static_assert(has_debug<T, Debug&>);
            }
        }

        void write(auto&&... a) {
            appends(buf, a...);
        }

       public:
        [[nodiscard]] constexpr DebugObjectField<Debug> object() {
            write("{");
            return DebugObjectField<Debug>{*this};
        }

        [[nodiscard]] constexpr DebugArrayField<Debug> array() {
            write("[");
            return DebugArrayField<Debug>{*this};
        }

        constexpr void number(std::int64_t value) {
            write(nums(value));
        }

        constexpr void string(std::string_view s) {
            write("\"", utils::escape::escape_str<std::string>(s, utils::escape::EscapeFlag::all, utils::escape::json_set()), "\"");
        }

        constexpr void null() {
            write("null");
        }

        constexpr void boolean(bool v) {
            write(v ? "true" : "false");
        }

        constexpr void value(auto&& v) {
            apply_value(v);
        }

        constexpr std::string& out() {
            return buf;
        }
    };

}  // namespace brgen
