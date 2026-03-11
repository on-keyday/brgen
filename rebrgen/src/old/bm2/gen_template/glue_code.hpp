/*license*/
#pragma once
#include <string>
#include "flags.hpp"
#include <vector>
#include "../context.hpp"

namespace rebgn {
    struct Flag {
        std::string option;
        std::string bind_target;
        std::string type;
        std::string default_value;
        std::string place_holder;
        std::string description;
    };

    std::vector<Flag> get_flags(Flags& flags);
    void code_main(bm2::TmpCodeWriter& w, Flags& flags);
    void write_code_header(bm2::TmpCodeWriter& w, Flags& flags);
    void write_code_cmake(bm2::TmpCodeWriter& w, Flags& flags);
    void write_code_js_glue_worker(bm2::TmpCodeWriter& w, Flags& flags);
    void write_code_js_glue_ui_and_generator_call(bm2::TmpCodeWriter& w, Flags& flags);
    void write_code_config(bm2::TmpCodeWriter& w, Flags& flags);
    void write_template_document(Flags& flags, bm2::TmpCodeWriter& w);
    void write_cmptest_config(Flags& flags, bm2::TmpCodeWriter& w);
}  // namespace rebgn
