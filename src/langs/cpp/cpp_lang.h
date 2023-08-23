/*license*/
#pragma once
#include "core/ast/ast.h"
#include "core/writer/section.h"
#include "core/common/error.h"
#include "core/common/expected.h"
#include "core/writer/bit_manager.h"

namespace brgen::cpp_lang {

    enum class WriteMode {
        unspec,
        encode,
        decode,
    };

    struct Config {
        bool test_main = false;
        bool insert_bit_pos_debug_code = false;
    };

    struct Context {
        bool last_should_be_return = false;
        WriteMode mode = WriteMode::unspec;
        bool def_done = false;
        Config config;

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
}  // namespace brgen::cpp_lang
