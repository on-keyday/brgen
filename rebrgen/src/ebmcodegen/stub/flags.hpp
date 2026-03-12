/*license*/
#pragma once

#include "cmdline/option/optcontext.h"
#include "json/stringer.h"
namespace ebmcodegen {
    auto flag_description_json(futils::cmdline::option::Context& ctx, const char* lang_name, const char* ui_lang_name, const char* lsp_name, const char* webworker_name, std::vector<std::string_view> file_exts, auto&& web_filtered, auto&& web_type_map) {
        futils::json::Stringer<> str;
        str.set_indent("  ");
        auto root_obj = str.object();
        root_obj("lang_name", lang_name);
        root_obj("ui_lang_name", ui_lang_name);
        root_obj("lsp_name", lsp_name);
        root_obj("webworker_name", webworker_name);
        root_obj("file_extensions", file_exts);
        root_obj("flags", [&](auto& s) {
            auto fields = s.array();
            for (auto& opt : ctx.options()) {
                fields([&](auto& s) {
                    auto obj = s.object();
                    obj("name", opt->mainname);
                    obj("help", opt->help);
                    obj("argdesc", opt->argdesc);
                    obj("original_type", opt->type);
                    if (auto map_it = web_type_map.find(opt->mainname); map_it != web_type_map.end()) {
                        obj("type", map_it->second);
                    }
                    else {
                        obj("type", opt->type);
                    }
                    obj("web_filtered", web_filtered.contains(opt->mainname));
                });
            }
        });
        root_obj.close();
        return str.out();
    }
}  // namespace ebmcodegen
