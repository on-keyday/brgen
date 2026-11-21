/*license*/
#pragma once
#include "../common.hpp"
#include "handler.hpp"
#ifndef DEFINE_VISITOR
#define DEFINE_VISITOR(T)                               \
    namespace brgen::nast::backend {                    \
        template <class R>                              \
        expected<R> dummy_fn(BaseContext<R>&, Node<T>); \
    }                                                   \
    template <class R>                                  \
    brgen::nast::expected<R> brgen::nast::backend::dummy_fn(BaseContext<R>& ctx, Node<T> node)
#endif
