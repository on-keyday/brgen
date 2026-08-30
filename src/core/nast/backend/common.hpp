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

    template <class R, class Node>
    struct DefaultHandler;
    template <class R>
    struct DefaultBehavior;
    template <class R>
    struct Knobs;
    template <class R, class L>
    struct Context;

    struct CommonConfig {
        UnhandledMode unhandled_mode = UnhandledMode::dummy;

       private:
        template <class R>
        friend struct BaseContext;
        template <class C>
        friend struct Invoker;
        bool inner_default = false;
    };

    template <class R>
    struct BaseContext : Invoker<BaseContext<R>> {
        using result_type = R;
        Arena& a;
        // 解析の結果はここにある。木だけ見ても分からないもの (Reference の
        // 解決先、畳み込んだ定数、union の重ね合わせ、入出力の要求) は
        // 全部 side table 側なので、バックエンドからも読めないと困る。
        SideTables& t;
        Knobs<R>& n;
        DefaultBehavior<R> d;
        CommonConfig c;
        LanguageConfig l;

        expected<R> visit(NodeAny n) {
            return Invoker<BaseContext<R>>::template invoke_custom<R>(n);
        }

        expected<R> visit_default(NodeAny n) {
            if (c.inner_default) {
                return unexpect_loc_error(n, "DO NOT invoke visit_default from default hook");
            }
            return Invoker<BaseContext<R>>::template invoke_default<R>(n);
        }

        template <class L>
        Context<R, L> to_context(L& lc) {
            l.set_language(lc);
            return Context<R, L>{.b = *this, .l = lc};
        }

        Arena& arena() {
            return a;
        }

        SideTables& tables() {
            return t;
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

        // must be called from hook, not from default itself
        expected<R> visit_default(NodeAny n) {
            return b.visit_default(n);
        }

        Arena& arena() {
            return b.arena();
        }

        SideTables& tables() {
            return b.tables();
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
