/*license*/
#pragma once
#include "config.h"
#include <helper/defer.h>

namespace brgen::writer {
    enum class WriteMode {
        unspec,
        encode,
        decode,
    };
    struct Context {
        WriteMode mode = WriteMode::unspec;
        bool def_done = false;
        Config config;

       private:
        auto do_exchange(auto& m, auto v) {
            auto old = m;
            m = v;
            return futils::helper::defer([=, &m] {
                m = old;
            });
        }

       public:
        auto set_write_mode(WriteMode m) {
            return do_exchange(mode, m);
        }
    };
}  // namespace brgen::writer
