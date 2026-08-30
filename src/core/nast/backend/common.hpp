/*license*/
#pragma once
#include "../node/nodes.h"
#include "invoke.hpp"
#include "lang.hpp"

namespace brgen::nast::backend {
    template <class R>
    struct BaseContext;
    struct CommonConfig {
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

        result<R> visit(NodeAny n) {
            return Invoker<BaseContext<R>>::template invoke_impl<R>(n);
        }

        template <class L>
        Context<R, L> to_context(L& lc) {
            l.set_language(lc);
            return Context<R, L>{.b = *this, .l = lc};
        }
    };
    template <class R, class L>
    struct Context {
        BaseContext<R>& b;
        L& l;

        result<R> visit(NodeAny n) {
            return b.visit(n);
        }
        Arena& arena() {
            return b.a;
        }
    };

#define MAYBE(x, v)                                         \
    auto x##_nast_maybe = (v);                              \
    if (!x##_nast_maybe) {                                  \
        return unexpect(std::move(x##_nast_maybe.error())); \
    }                                                       \
    auto x = std::move(*x##_nast_maybe);
}  // namespace brgen::nast::backend
