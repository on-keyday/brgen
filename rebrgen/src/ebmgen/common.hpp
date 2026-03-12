/*license*/
#pragma once
#include <error/error.h>
#include <helper/expected.h>
#include <concepts>
#include <format>
#include <ebm/extended_binary_module.hpp>
#include <source_location>
#include <type_traits>
namespace brgen::ast {}

namespace ebmgen {
    namespace ast = brgen::ast;
    using Error = futils::error::Error<std::allocator<char>, std::string>;

    template <typename T>
    using expected = futils::helper::either::expected<T, Error>;

#define FORMAT_ARG std::format_string<Args...>
    template <typename... Args>
    Error error(FORMAT_ARG fmt, Args&&... args) {
        return futils::error::StrError{
            std::format(fmt, std::forward<Args>(args)...),
        };
    }

    extern bool verbose_error;

    struct LocationInfoError {
        std::source_location loc;
        const char* expr;
        const char* output;
        void error(auto&& pb) const {
            futils::strutil::append(pb, " at ");
            futils::strutil::append(pb, loc.file_name());
            futils::strutil::append(pb, ":");
            futils::number::to_string(pb, loc.line());
            futils::strutil::append(pb, ":");
            futils::number::to_string(pb, loc.column());
            if (verbose_error) {
                futils::strutil::append(pb, " in ");
                futils::strutil::append(pb, loc.function_name());
                if (expr) {
                    futils::strutil::append(pb, " Expression: ");
                    if (output) {
                        futils::strutil::append(pb, output);
                        futils::strutil::append(pb, " = ");
                    }
                    futils::strutil::append(pb, expr);
                }
            }
        }
    };

    template <typename... Args>
    futils::helper::either::unexpected<Error> unexpect_error_with_loc(std::source_location loc, FORMAT_ARG fmt, Args&&... args) {
        return futils::helper::either::unexpected(Error(futils::error::ErrList<Error>{
            .err = LocationInfoError{.loc = loc},
            .before = error(fmt, std::forward<Args>(args)...),
            .sep = '\n',
        }));
    }

    inline futils::helper::either::unexpected<Error> unexpect_error_with_loc(std::source_location loc, Error&& err, const char* output = nullptr, const char* expr_str = nullptr) {
        return futils::helper::either::unexpected(Error(futils::error::ErrList<Error>{
            .err = LocationInfoError{loc, expr_str, output},
            .before = std::move(err),
            .sep = '\n',
        }));
    }

#define unexpect_error(...) \
    unexpect_error_with_loc(std::source_location::current(), __VA_ARGS__)

    constexpr auto none = Error();

    constexpr ebm::Varint null_varint = ebm::Varint();

    constexpr expected<ebm::Varint> varint(std::uint64_t n) {
        ebm::Varint v;
        if (n < 0x40) {
            v.prefix(0);
            v.value(n);
        }
        else if (n < 0x4000) {
            v.prefix(1);
            v.value(n);
        }
        else if (n < 0x40000000) {
            v.prefix(2);
            v.value(n);
        }
        else if (n < 0x4000000000000000) {
            v.prefix(3);
            v.value(n);
        }
        else {
            return unexpect_error("Invalid varint value: {}", n);
        }
        return v;
    }

    template <class T>
    void append(T& container, const typename decltype(T::container)::value_type& value) {
        container.container.push_back(value);
        container.len = varint(container.container.size()).value();
    }

    template <typename T>
    concept AnyRef = requires(T t) {
        { t.id } -> std::convertible_to<ebm::Varint>;
        { std::integral_constant<bool, sizeof(T) == sizeof(ebm::AnyRef)>{} } -> std::same_as<std::true_type>;
    };

    template <typename T>
    concept WeakAnyRef = requires(T t) {
        { t.id } -> AnyRef;
    };

    template <class T, class U>
    concept has_visit = requires(T t, U u) {
        { t.visit(u) };
    };
    struct DummyFn {
        void operator()(auto&&, const char*, auto&&) const {}
    };

    template <class T>
    concept is_container = requires(T t) {
        { t.container };
    };

#define VISITOR_RECURSE(visitor, name, value)                              \
    if constexpr (ebmgen::has_visit<decltype(value), decltype(visitor)>) { \
        value.visit(visitor);                                              \
    }                                                                      \
    else if constexpr (std::is_pointer_v<std::decay_t<decltype(value)>>) { \
        if (value) {                                                       \
            visitor(visitor, name, *value);                                \
        }                                                                  \
    }
#define VISITOR_RECURSE_ARRAY(visitor, name, value)                                                      \
    if constexpr (futils::helper::is_template_instance_of<std::decay_t<decltype(value)>, std::vector>) { \
        for (auto& elem : value) {                                                                       \
            visitor(visitor, name, elem);                                                                \
        }                                                                                                \
    }

#define VISITOR_RECURSE_CONTAINER(visitor, name, value) \
    if constexpr (is_container<decltype(value)>) {      \
        for (auto& elem : value.container) {            \
            visitor(visitor, name, elem);               \
        }                                               \
    }

    inline Error unexpected_nullptr() {
        return futils::error::StrError<const char*>{"Unexpected nullptr"};
    }

    template <class E>
    concept error_convertible = requires(E e) {
        { e.error() } -> std::convertible_to<Error>;
    };

    auto handle_error(auto&& o, const char* out, const char* expr, std::source_location loc = std::source_location::current()) {
        if constexpr (std::is_pointer_v<std::decay_t<decltype(o)>>) {
            return unexpect_error_with_loc(loc, unexpected_nullptr(), out, expr);
        }
        else if constexpr (error_convertible<decltype(o)>) {
            return unexpect_error_with_loc(loc, std::move(o.error()), out, expr);
        }
        else {
            return unexpect_error_with_loc(loc, unexpected_nullptr(), out, expr);
        }
    }

    auto& dereference(auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (futils::helper::is_template_instance_of<T, expected>) {
            using P = typename futils::helper::template_of_t<T>::template param_at<0>;
            if constexpr (std::is_pointer_v<P>) {
                return **v;
            }
            else {
                return *v;
            }
        }
        else {
            return *v;
        }
    }

#define MAYBE_VOID(out, expr)                                  \
    auto out##____ = expr;                                     \
    if (!out##____) {                                          \
        return ::ebmgen::handle_error(out##____, #out, #expr); \
    }

#define MAYBE(out, expr)  \
    MAYBE_VOID(out, expr) \
    decltype(auto) out = ::ebmgen::dereference(out##____);

    template <class B>
    concept has_body_kind = requires(B b) {
        { b.body.kind };
    };
}  // namespace ebmgen

namespace ebm {
    template <ebmgen::AnyRef T>
    constexpr bool is_nil(T t) {
        return t.id.value() == 0;
    }

    template <ebmgen::WeakAnyRef T>
    constexpr bool is_nil(T t) {
        return is_nil(t.id);
    }

    template <ebmgen::AnyRef T>
    constexpr std::uint64_t get_id(T t) {
        return t.id.value();
    }

    template <ebmgen::WeakAnyRef T>
    constexpr std::uint64_t get_id(T t) {
        return get_id(t.id);
    }

    template <ebmgen::AnyRef T>
    constexpr ebm::AnyRef to_any_ref(T t) {
        return ebm::AnyRef{t.id};
    }

    template <ebmgen::AnyRef T>
    constexpr T from_any_ref(const ebm::AnyRef& ref) {
        T t;
        t.id = ref.id;
        return t;
    }

    constexpr ebm::WeakStatementRef to_weak(ebm::StatementRef ref) {
        return ebm::WeakStatementRef{ref};
    }

    constexpr ebm::StatementRef from_weak(ebm::WeakStatementRef ref) {
        return ref.id;
    }

    template <ebmgen::AnyRef T>
    constexpr bool operator==(const T& lhs, const T& rhs) {
        return get_id(lhs) == get_id(rhs);
    }

    template <ebmgen::AnyRef T>
    constexpr bool operator!=(const T& lhs, const T& rhs) {
        return !(lhs == rhs);
    }

    template <ebmgen::AnyRef T>
    constexpr auto operator<=>(const T& lhs, const T& rhs) {
        return get_id(lhs) <=> get_id(rhs);
    }

}  // namespace ebm

namespace std {
    template <ebmgen::AnyRef T>
    struct hash<T> {
        size_t operator()(const T& ref) const noexcept {
            return std::hash<std::uint64_t>{}(get_id(ref));
        }
    };
}  // namespace std