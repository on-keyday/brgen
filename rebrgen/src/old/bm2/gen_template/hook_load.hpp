/*license*/
#pragma once
#include "flags.hpp"
#include <filesystem>
#include "../context.hpp"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <strutil/splits.h>
#include <bmgen/common.hpp>
namespace rebgn {

    bool include_stack(Flags& flags, size_t& line, bool via_include, std::vector<std::filesystem::path>& stack, auto&& file_name, auto&& per_line, bool is_last) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        auto name = hook_file.generic_u8string();
        if (flags.debug || flags.print_hooks) {
            if (via_include) {
                futils::wrap::cerr_wrap() << "try hook via include: " << name << '\n';
            }
            else {
                futils::wrap::cerr_wrap() << "try hook: " << name << '\n';
            }
        }
        for (auto& parents : stack) {
            std::error_code ec;
            if (std::filesystem::equivalent(parents, hook_file, ec)) {
                futils::wrap::cerr_wrap() << "circular include: " << name << '\n';
                return false;
            }
        }
        stack.push_back(hook_file);
        auto do_handle_lines = [&](auto&& lines, bool virtually) {
            if (via_include) {
                futils::wrap::cerr_wrap() << "loaded hook via include: " << name;
            }
            else {
                futils::wrap::cerr_wrap() << "loaded hook: " << name;
            }
            if (virtually) {
                futils::wrap::cerr_wrap() << " (from sections.txt)";
            }
            futils::wrap::cerr_wrap() << '\n';
            for (auto i = 0; i < lines.size(); i++) {
                if (futils::strutil::starts_with(lines[i], "!@include ")) {
                    auto split = futils::strutil::split(lines[i], " ", 2);
                    if (split.size() == 2) {
                        include_stack(flags, line, true, stack, split[1], per_line, is_last && i == lines.size() - 1);
                    }
                    else {
                        futils::wrap::cerr_wrap() << "invalid include: " << lines[i] << '\n';
                    }
                    continue;
                }
                per_line(line, lines[i], is_last && i == lines.size() - 1);
                line++;
            }
            stack.pop_back();
            return true;
        };
        if (auto found = flags.hook_sections.find(name); found != flags.hook_sections.end()) {
            auto& section = found->second;
            return do_handle_lines(section.lines, true);
        }
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        return do_handle_lines(lines, false);
    }

    expected<bool> load_sections_txt(Flags& flags);

    bool may_write_from_hook(Flags& flags, auto&& file_name, auto&& per_line) {
        std::vector<std::filesystem::path> stack;
        size_t line = 0;
        auto file_name_with_txt = std::format("{}.txt", file_name);
        return include_stack(flags, line, false, stack, file_name_with_txt, per_line, true);
    }

    bool may_write_from_hook(Flags& flags, bm2::HookFile hook, auto&& per_line) {
        return may_write_from_hook(flags, to_string(hook), per_line);
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bm2::HookFileSub sub);

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub = bm2::HookFileSub::main);

    void with_hook_comment(bm2::TmpCodeWriter& w, Flags& flags, auto&& concat, futils::view::rvec line, size_t i, bool is_last) {
        if (i == 0) {
            w.writeln("// load hook: ", concat);
        }
        w.writeln(line);
        if (is_last) {
            w.writeln("// end hook: ", concat);
        }
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub, auto sub_sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}_{}", to_string(hook), op_name, to_string(sub), to_string(sub_sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, rebgn::StorageType op, bm2::HookFileSub sub = bm2::HookFileSub::main);
    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bool as_code);

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, auto op, std::string_view variable_name, bm2::HookFileSub sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}_var_{}{}", to_string(hook), op_name, variable_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }
}  // namespace rebgn
