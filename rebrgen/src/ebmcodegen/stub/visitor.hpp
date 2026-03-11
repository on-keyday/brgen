/*license*/
#pragma once

// for class based
#define DEFINE_VISITOR(dummy_name)                                                                                                                     \
    static_assert(std::string_view(__FILE__).contains(#dummy_name "_class.hpp") || std::string_view(__FILE__).contains(#dummy_name "_dsl_class.hpp")); \
    template <>                                                                                                                                        \
    struct CODEGEN_VISITOR(dummy_name) {                                                                                                               \
        ebmgen::expected<CODEGEN_NAMESPACE::Result> visit(CODEGEN_CONTEXT(dummy_name) & ctx);                                                          \
    };                                                                                                                                                 \
                                                                                                                                                       \
    inline ebmgen::expected<CODEGEN_NAMESPACE::Result> CODEGEN_VISITOR(dummy_name)::visit(CODEGEN_CONTEXT(dummy_name) & ctx)

#define DEFINE_VISITOR_CLASS(dummy_name)                                                                                                               \
    static_assert(std::string_view(__FILE__).contains(#dummy_name "_class.hpp") || std::string_view(__FILE__).contains(#dummy_name "_dsl_class.hpp")); \
    template <>                                                                                                                                        \
    struct CODEGEN_VISITOR(dummy_name)

#define DEFINE_VISITOR_FUNCTION(dummy_name) ebmgen::expected<CODEGEN_NAMESPACE::Result> visit(CODEGEN_CONTEXT(dummy_name) & ctx)

// template <class D, class V, class R = void>
// struct TraversalVisitorBase {
// because TraversalVisitorBase pattern requires to write using Base::visit for generic visit which is often forgotten
// we use macro to define TraversalVisitorBase to reduce boilerplate and tricky behaviour
#define TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(D, V) \
    V & visitor;                                  \
    D(V& v)                                       \
        : visitor(v) {}
#define TRAVERSAL_VISITOR_BASE(D, V, R)                             \
    TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(D, V)                       \
    template <class Ctx>                                            \
    ebmgen::expected<R> visit(Ctx&& ctx) {                          \
        if (ctx.is_before_or_after()) {                             \
            return pass;                                            \
        }                                                           \
        return traverse_children<R>(*this, std::forward<Ctx>(ctx)); \
    }
//};