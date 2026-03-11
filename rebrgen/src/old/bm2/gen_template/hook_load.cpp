/*license*/
#include "hook_load.hpp"

namespace rebgn {

    expected<bool> section_include_stack(Flags& flags, std::vector<std::filesystem::path> stack, std::string_view file_name, std::u8string& current_section);

    expected<bool> handle_section_common(Flags& flags, futils::view::rvec view, std::vector<std::filesystem::path> stack, const std::u8string& name, std::u8string& current_section) {
        auto lines = futils::strutil::lines<futils::view::rvec>(view);
        for (auto& line : lines) {
            if (futils::strutil::starts_with(line, "!@section ") ||
                futils::strutil::starts_with(line, "!@copy_section ")) {
                auto split = futils::strutil::split<futils::view::rvec>(line, " ", 2);
                if (split.size() == 2) {
                    auto hook_file = std::filesystem::path(flags.hook_file_dir) / std::string_view(split[1].as_char(), split[1].size());
                    auto new_section = hook_file.generic_u8string();
                    if (line[2] == 'c') {              // !@copy_section
                        if (current_section.size()) {  // copy current section
                            flags.hook_sections[new_section].lines = flags.hook_sections[current_section].lines;
                        }
                    }
                    current_section = std::move(new_section);
                }
                else {
                    return unexpect_error("invalid section: {}", std::string_view(line.as_char(), line.size()));
                }
            }
            else if (line == "!@end_section") {
                current_section.clear();
            }
            else if (current_section.size()) {
                flags.hook_sections[current_section].lines.push_back(std::string(line.as_char(), line.size()));
            }
            else if (futils::strutil::starts_with(line, "!@section_include ")) {
                auto split = futils::strutil::split<futils::view::rvec>(line, " ", 2);
                if (split.size() == 2) {
                    auto ok = section_include_stack(flags, stack, std::string_view(split[1].as_char(), split[1].size()), current_section);
                    if (!ok) {
                        return unexpect_error("failed to include section: {}: {}", std::string_view(split[1].as_char(), split[1].size()), ok.error().error());
                    }
                }
                else {
                    return unexpect_error("invalid section: {}", std::string_view(line.as_char(), line.size()));
                }
            }
            else {
                return unexpect_error("invalid section: {}", std::string_view(line.as_char(), line.size()));
            }
        }
        return true;
    }

    expected<bool> section_include_stack(Flags& flags, std::vector<std::filesystem::path> stack, std::string_view file_name, std::u8string& current_section) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / file_name;
        auto name = hook_file.generic_u8string();
        if (flags.debug || flags.print_hooks) {
            futils::wrap::cerr_wrap() << "try hook via include: " << name << '\n';
        }
        for (auto& parents : stack) {
            std::error_code ec;
            if (std::filesystem::equivalent(parents, hook_file, ec)) {
                futils::wrap::cerr_wrap() << "circular include: " << name << '\n';
                return false;
            }
        }
        stack.push_back(hook_file);
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(futils::view::rvec(view));
        futils::wrap::cerr_wrap() << "loaded hook via include: " << name << '\n';
        return handle_section_common(flags, futils::view::rvec(view), stack, name, current_section);
    }

    expected<bool> load_sections_txt(Flags& flags) {
        auto hook_file = std::filesystem::path(flags.hook_file_dir) / to_string(bm2::HookFile::sections);
        auto name = hook_file.generic_u8string() + u8".txt";
        if (flags.debug) {
            futils::wrap::cerr_wrap() << "try hook: " << name << '\n';
        }
        else if (flags.print_hooks) {
            futils::wrap::cerr_wrap() << "try hook: " << name << '\n';
        }
        futils::file::View view;
        if (auto res = view.open(name); !res) {
            return false;
        }
        if (!view.data()) {
            return false;
        }
        futils::wrap::cerr_wrap() << "loaded hook: " << name << '\n';
        std::vector<std::filesystem::path> stack;
        std::u8string current_section;
        stack.push_back(hook_file);
        auto res = handle_section_common(flags, futils::view::rvec(view), stack, name, current_section);
        if (!res) {
            return res;
        }
        if (flags.debug) {
            for (auto& section : flags.hook_sections) {
                futils::wrap::cerr_wrap() << "loaded section: " << section.first << '\n';
            }
        }
        return true;
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bm2::HookFileSub sub) {
        auto concat = std::format("{}{}", to_string(hook), to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, AbstractOp op, bm2::HookFileSub sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}", to_string(hook), op_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, rebgn::StorageType op, bm2::HookFileSub sub) {
        auto op_name = to_string(op);
        auto concat = std::format("{}_{}{}", to_string(hook), op_name, to_string(sub));
        // to lower
        for (auto& c : concat) {
            c = std::tolower(c);
        }
        return may_write_from_hook(flags, concat, [&](size_t i, futils::view::rvec line, bool is_last) {
            with_hook_comment(w, flags, concat, line, i, is_last);
        });
    }

    bool may_write_from_hook(bm2::TmpCodeWriter& w, Flags& flags, bm2::HookFile hook, bool as_code) {
        return may_write_from_hook(flags, hook, [&](size_t i, futils::view::rvec line, bool is_last) {
            if (i == 0) {
                w.writeln("// load hook: ", to_string(hook));
            }
            if (as_code) {
                if (!futils::strutil::contains(line, "\"") && !futils::strutil::contains(line, "\\")) {
                    w.writeln("w.writeln(\"", line, "\");");
                }
                else {
                    std::string escaped_line;
                    for (auto c : line) {
                        if (c == '"' || c == '\\') {
                            escaped_line.push_back('\\');
                        }
                        escaped_line.push_back(c);
                    }
                    w.writeln("w.writeln(\"", escaped_line, "\");");
                }
            }
            else {
                w.writeln(line);
            }
            if (is_last) {
                w.writeln("// end hook: ", to_string(hook));
            }
        });
    }
}  // namespace rebgn