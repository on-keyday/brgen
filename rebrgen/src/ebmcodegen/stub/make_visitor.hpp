/*license*/
#pragma once
#include <type_traits>
#include <utility>
#include <string_view>
#include "ebmgen/common.hpp"
#include "context.hpp"
#include <helper/func_arg.hpp>

namespace ebmcodegen::util {
    template <class R>
    struct dummy_next {
        template <class Self, class Input>
        ebmgen::expected<R> visit(Self&, Input&) {
            return ebmgen::unexpect_error("No matching visitor function found");
        }

        template <class ActualNext>
        auto build(ActualNext&& next) {
            return next;
        }
    };

    template <class R, class F, class Ctx, class Next>
    struct FuncHolder {
        F func;
        Next next;

        template <class Self, class Input>
        ebmgen::expected<R> visit(Self& self, Input& input) {
            if constexpr (std::is_same_v<Input, Ctx>) {
                return func(self, input);
            }
            else {
                return next.visit(self, input);
            }
        }

        template <class ActualNext>
        auto build(ActualNext&& anext) {
            return next.build(FuncHolder<R, F, Ctx, std::decay_t<ActualNext>>{.func = std::move(func), .next = std::forward<ActualNext>(anext)});
        }
    };

    template <class R, class F, class Next>
    struct TerminateFunc {
        F func;
        Next next;

        template <class Self, class Input>
        ebmgen::expected<R> visit(Self& self, Input& input) {
            return func(self, input);
        }

        template <class ActualNext>
        auto build(ActualNext&& anext) {
            static_assert(std::is_same_v<ActualNext, dummy_next<R>>, "on_default must be the last in the chain");
            return next.build(TerminateFunc<R, F, std::decay_t<ActualNext>>{.func = std::move(func), .next = std::forward<ActualNext>(anext)});
        }
    };

    template <class R, class Next>

    struct DefaultTraverseChildren {
        Next next;

        template <class Self, class Input>
        ebmgen::expected<R> visit(Self& self, Input& input) {
            return traverse_children_adl<R>(self, input, self.visitor);
        }

        template <class ActualNext>
        auto build(ActualNext&& anext) {
            static_assert(std::is_same_v<ActualNext, dummy_next<R>>, "on_default_traverse_children must be the last in the chain");
            return next.build(DefaultTraverseChildren<R, std::decay_t<ActualNext>>{.next = std::forward<ActualNext>(anext)});
        };
    };

    template <class R, class Next>
    struct NotContextFilter {
        std::string_view contains;
        Next next;

        template <class Self, class Input>
        ebmgen::expected<R> visit(Self& self, Input& input) {
            if (input.context_name.contains(contains)) {
                if (input.is_before_or_after()) {
                    return pass;
                }
                else {
                    return {};
                }
            }
            return next.visit(self, input);
        }

        template <class ActualNext>
        auto build(ActualNext&& anext) {
            return next.build(NotContextFilter<R, std::decay_t<ActualNext>>{.contains = contains, .next = std::forward<ActualNext>(anext)});
        }
    };

    template <class R, class Next>
    struct NotBeforeOrAfterFilter {
        Next next;

        template <class Self, class Input>
        ebmgen::expected<R> visit(Self& self, Input& input) {
            if (input.is_before_or_after()) {
                return pass;
            }
            return next.visit(self, input);
        }

        template <class ActualNext>
        auto build(ActualNext&& anext) {
            return next.build(NotBeforeOrAfterFilter<R, std::decay_t<ActualNext>>{.next = std::forward<ActualNext>(anext)});
        }
    };

    template <class R, class V, class Next>
    struct FinalizedVisitor {
        V& visitor;
        std::string_view name_;
        Next next;

        std::string_view name() const {
            return name_;
        }

        template <class Input>
        auto visit(Input& input) {
            return next.visit(*this, input);
        }
    };

    template <class F, class V>
    struct func_template_instance {
        using type = decltype(&F::template operator()<FinalizedVisitor<void, V, dummy_next<void>>>);
    };

    template <class F, class V>
    using visitor_func_arg_t = std::remove_reference_t<futils::helper::func_arg_t<typename func_template_instance<F, V>::type, 1>>;

    template <class R, class V, class Next>
    struct MakeVisitor {
        V& visitor;
        std::string_view name_;
        Next next;

        // only for documentation purpose, not actually used
        auto name(std::string_view name) {
            return MakeVisitor<R, V, Next>{.visitor = visitor, .name_ = name, .next = next};
        }

        template <class F>
        MakeVisitor<R, V, FuncHolder<R, F, visitor_func_arg_t<F, V>, Next>> on(F&& f) {
            return MakeVisitor<R, V, FuncHolder<R, F, visitor_func_arg_t<F, V>, Next>>{
                .visitor = visitor,
                .name_ = name_,
                .next = FuncHolder<R, F, visitor_func_arg_t<F, V>, Next>{.func = std::forward<F>(f), .next = next},
            };
        }

        template <class F>
        MakeVisitor<R, V, TerminateFunc<R, F, Next>> on_default(F&& f) {
            return MakeVisitor<R, V, TerminateFunc<R, F, Next>>{
                .visitor = visitor,
                .name_ = name_,
                .next = TerminateFunc<R, F, Next>{.func = std::forward<F>(f), .next = next},
            };
        }

        MakeVisitor<R, V, DefaultTraverseChildren<R, Next>> on_default_traverse_children() {
            return MakeVisitor<R, V, DefaultTraverseChildren<R, Next>>{
                .visitor = visitor,
                .name_ = name_,
                .next = DefaultTraverseChildren<R, Next>{.next = next},
            };
        }

        MakeVisitor<R, V, NotContextFilter<R, Next>> not_context(std::string_view str) {
            return MakeVisitor<R, V, NotContextFilter<R, Next>>{
                .visitor = visitor,
                .name_ = name_,
                .next = NotContextFilter<R, Next>{.contains = str, .next = next},
            };
        }

        MakeVisitor<R, V, NotBeforeOrAfterFilter<R, Next>> not_before_or_after() {
            return MakeVisitor<R, V, NotBeforeOrAfterFilter<R, Next>>{
                .visitor = visitor,
                .name_ = name_,
                .next = NotBeforeOrAfterFilter<R, Next>{.next = next},
            };
        }

        auto build() {
            return FinalizedVisitor<R, V, decltype(next.build(dummy_next<R>{}))>{.visitor = visitor, .name_ = name_, .next = next.build(dummy_next<R>{})};
        }
    };

    template <class R, class V>
    auto make_visitor(V& visitor) {
        return MakeVisitor<R, V, dummy_next<R>>{visitor};
    }

    namespace test {
        struct dummy_context {
            std::string_view context_name;
            bool is_before_or_after() const {
                return context_name.ends_with("_before") || context_name.ends_with("_after");
            }
        };
        struct dummy2_context {
            std::string_view context_name;
            bool is_before_or_after() const {
                return context_name.ends_with("_before") || context_name.ends_with("_after");
            }
        };
        inline void test_make_visitor() {
            int x = 0;
            auto visitor = make_visitor<int>(x)
                               .not_context("test")
                               .not_before_or_after()
                               .on([&](auto&& self, dummy_context& input) { return input.context_name == "test_before" ? 1 : 2; })
                               .on_default([&](auto&& self, auto&) { return 42; })

                               .build();
            dummy2_context ctx;
            visitor.visit(ctx);
        }
    }  // namespace test
}  // namespace ebmcodegen::util