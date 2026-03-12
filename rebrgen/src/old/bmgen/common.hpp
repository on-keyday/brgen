/*license*/
#pragma once
#include <error/error.h>
#include <helper/expected.h>
#include <format>
namespace brgen::ast {}

namespace rebgn {
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

    template <typename... Args>
    futils::helper::either::unexpected<Error> unexpect_error(FORMAT_ARG fmt, Args&&... args) {
        return futils::helper::either::unexpected(error(fmt, std::forward<Args>(args)...));
    }

    inline futils::helper::either::unexpected<Error> unexpect_error(Error&& err) {
        return futils::helper::either::unexpected(std::forward<decltype(err)>(err));
    }

    constexpr auto none = Error();

}  // namespace rebgn
