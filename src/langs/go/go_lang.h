/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/section.h"
#include "core/common/error.h"
#include "core/common/expected.h"
#include "core/writer/bit_io.h"
#include "core/writer/config.h"

namespace brgen::go_lang {

    enum class WriteMode {
        unspec,
        encode,
        decode,
    };

    struct Context {
        bool last_should_be_return = false;
        WriteMode mode = WriteMode::unspec;
        bool def_done = false;
        writer::Config config;

       private:
        auto do_exchange(auto& m, auto v) {
            auto old = m;
            m = v;
            return utils::helper::defer([=, &m] {
                m = old;
            });
        }

       public:
        auto set_last_should_be_return(bool b) {
            return do_exchange(last_should_be_return, b);
        }

        auto set_write_mode(WriteMode m) {
            return do_exchange(mode, m);
        }
    };

    result<writer::SectionPtr> entry(Context& w, std::shared_ptr<ast::Program>& p);
}  // namespace brgen::go_lang
