/*license*/
#pragma once
#include "../common.hpp"
#ifndef DEFINE_VISITOR
#define DEFINE_VISITOR(T)                             \
    namespace brgen::nast::backend {                  \
        template <class R>                            \
        result<R> dummy_fn(BaseContext<R>&, Node<T>); \
    }                                                 \
    template <class R>                                \
    brgen::result<R> brgen::nast::backend::dummy_fn(BaseContext<R>& ctx, Node<T> node)
#endif
