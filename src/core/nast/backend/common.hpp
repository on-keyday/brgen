/*license*/
#pragma once
#include "../node/nodes.h"
#include "invoke.hpp"
#include "lang.hpp"
#include "../node/error.h"

namespace brgen::nast::backend {
    template <class R>
    struct BaseContext;

    enum class UnhandledMode : std::uint8_t {
        error,
        dummy,
        ignore,
    };

    struct CommonConfig {
        UnhandledMode unhandled_mode = UnhandledMode::dummy;
    };

    template <class R, class Node>
    struct DefaultHandler;
    template <class R>
    struct DefaultBehavior;
    template <class R>
    struct Knobs;
    template <class R, class L>
    struct Context;
    template <class R>
    struct BaseContext : Invoker<BaseContext<R>> {
        using result_type = R;
        Arena& a;
        Knobs<R>& n;
        DefaultBehavior<R> d;
        CommonConfig c;
        LanguageConfig l;

        expected<R> visit(NodeAny n) {
            return Invoker<BaseContext<R>>::template invoke_impl<R>(n);
        }

        template <class L>
        Context<R, L> to_context(L& lc) {
            l.set_language(lc);
            return Context<R, L>{.b = *this, .l = lc};
        }

        Arena& arena() {
            return a;
        }

        CommonConfig& config() {
            return c;
        }
    };
    template <class R, class L>
    struct Context {
        BaseContext<R>& b;
        L& l;

        expected<R> visit(NodeAny n) {
            return b.visit(n);
        }
        Arena& arena() {
            return b.arena();
        }

        CommonConfig& config() {
            return b.config();
        }

        L& lang_config() {
            return l;
        }
    };

#define MAYBE(x, v)                                                      \
    auto x##_nast_maybe = (v);                                           \
    if (!x##_nast_maybe) {                                               \
        return brgen::nast::unexpect(std::move(x##_nast_maybe.error())); \
    }                                                                    \
    auto x = std::move(*x##_nast_maybe);
}  // namespace brgen::nast::backend
