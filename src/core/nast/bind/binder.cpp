/*license*/
#pragma once
#include "../nodes.h"
#include "../access.h"

namespace brgen::nast::bind {
    struct Binder {
        Arena& a;

        void bind(Node<Module> mod) {
            for (auto& bound : mod.ref(a)->statements) {
            }
        }
    };
}  // namespace brgen::nast::bind