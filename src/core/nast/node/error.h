/*license*/
#pragma once
#include <format>
#include <source_location>
#include "core/common/error.h"
#include "nodes.h"
#include "strutil/append.h"
#include <helper/expected.h>
namespace brgen::nast {
    struct LocError {
        NodeAny node;
        std::source_location src;
        std::string message;

        void error(auto&& buf) {
            futils::strutil::appends(buf, message);
        }

        brgen::LocationError to_location_error(Arena& a) {
            return LocationError{.src = src}.error(node.ref(a).loc(), message);
        }
    };

    template <class... Args>
    futils::helper::either::unexpected<LocError> unexpect_loc_error_impl(NodeAny n, std::source_location src, std::format_string<Args...> fmt, Args&&... args) {
        return futils::helper::either::unexpected<LocError>{LocError{.node = n, .src = src, .message = std::format(fmt, std::forward<Args>(args)...)}};
    }
#define unexpect_loc_error(loc, fmt, ...) unexpect_loc_error_impl(loc, std::source_location::current(), fmt __VA_OPT__(, ) __VA_ARGS__)

    template <class T>
    using expected = futils::helper::either::expected<T, LocError>;

    inline futils::helper::either::unexpected<LocError> unexpect(LocError&& err) {
        return futils::helper::either::unexpected<LocError>{std::move(err)};
    }
}  // namespace brgen::nast
