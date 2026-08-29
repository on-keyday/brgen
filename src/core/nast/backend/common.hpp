/*license*/
#pragma once
#include "../node/nodes.h"
#include "lang.hpp"

namespace brgen::nast::backend {
    struct BaseContext;
    struct CommonConfig {
    };

    struct Knobs;
    struct BaseContext {
        Arena& a;
        Knobs& n;
        CommonConfig c;
        LanguageConfig l;
    };
    template <class L>
    struct Context {
        BaseContext& b;
        L& l;
    };

    template <class Node>
    struct DefaultHandler;

}  // namespace brgen::nast::backend