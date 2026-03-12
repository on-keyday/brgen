/*license*/
#include "dsl.h"
#include "code/code_writer.h"
#include "ebmcodegen/stub/structs.hpp"
#include "escape/escape.h"
#include "helper/defer_ex.h"
#include "strutil/strutil.h"
#include <set>
#include <vector>
#include "../stub/url.hpp"
namespace ebmcodegen::dsl {

    ebmgen::expected<std::string> generate_dsl_output(std::string_view file_name, std::string_view source) {
        DSLContext<syntax::OutputKind> ctx;
        bool is_class_based = file_name.contains("_class");
        auto seq = futils::make_ref_seq(source);
        auto res = syntax::dsl(seq, ctx, 0);
        if (res != futils::comb2::Status::match) {
            return ebmgen::unexpect_error("Failed to parse DSL source: {}", ctx.errbuf);
        }
        futils::code::CodeWriter<std::string> w;
        w.writeln("// Generated from ", file_name, " by ebmcodegen at ", repo_url, "; DO NOT EDIT");
        w.writeln("CodeWriter w;");
        bool expected_indent = false;
        size_t indent_level = 0;
        // first, insert indent nodes if only lines
        std::vector<DSLNode<syntax::OutputKind>> new_nodes;
        for (size_t i = 0; i < ctx.nodes.size(); i++) {
            if (i == 0 && ctx.nodes[i].kind != syntax::OutputKind::Indent) {
                // insert indent at beginning
                DSLNode<syntax::OutputKind> indent_node;
                indent_node.kind = syntax::OutputKind::Indent;
                indent_node.content = "";
                new_nodes.push_back(std::move(indent_node));
            }
            if (expected_indent) {
                if (ctx.nodes[i].kind != syntax::OutputKind::Indent) {
                    // insert indent after line
                    DSLNode<syntax::OutputKind> indent_node;
                    indent_node.kind = syntax::OutputKind::Indent;
                    indent_node.content = "";
                    new_nodes.push_back(std::move(indent_node));
                }
                expected_indent = false;
            }
            if (ctx.nodes[i].kind == syntax::OutputKind::Line) {
                expected_indent = true;
            }
            new_nodes.push_back(std::move(ctx.nodes[i]));
        }
        std::vector<size_t> indent_levels{0};
        std::set<size_t> indent_meaningful_levels;
        std::vector<DSLNode<syntax::OutputKind>> finalized_nodes;
        // next, insert dedent at change point
        for (size_t i = 0; i < new_nodes.size(); i++) {
            if (new_nodes[i].kind == syntax::OutputKind::Indent) {
                if (i == new_nodes.size() - 1) {
                    continue;  // skip final indent
                }
                if (new_nodes[i].content.size() > indent_levels.back()) {
                    indent_levels.push_back(new_nodes[i].content.size());  // increase indent
                    finalized_nodes.push_back(std::move(new_nodes[i]));
                }
                else if (new_nodes[i].content.size() < indent_levels.back()) {
                    // decrease indent
                    while (new_nodes[i].content.size() < indent_levels.back()) {
                        indent_levels.pop_back();
                        if (indent_levels.empty() || new_nodes[i].content.size() > indent_levels.back()) {
                            return ebmgen::unexpect_error("inconsistent indent");
                        }
                        finalized_nodes.push_back(DSLNode{.kind = syntax::OutputKind::Dedent, .content = ""});
                    }
                }
                else {
                    // same level, do nothing
                }
            }
            else {
                finalized_nodes.push_back(std::move(new_nodes[i]));
            }
        }
        while (indent_levels.size() > 1) {
            indent_levels.pop_back();
            finalized_nodes.push_back(DSLNode{.kind = syntax::OutputKind::Dedent, .content = ""});
        }
        std::vector<futils::helper::DynDefer> defers;
        bool prev_was_literal = false;
        size_t line = 1;
        auto make_dsl_line_comment = [&](size_t line) {
            return std::format("/* at {}:{} */", file_name, line);
        };
        auto trim = [](std::string_view str) {
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
                start++;
            }
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
                end--;
            }
            return str.substr(start, end - start);
        };
        std::vector<std::string> dsl_lines;
        std::vector<syntax::SpecialOutputKind> dsl_stack;
        auto most_recent = [&](syntax::SpecialOutputKind kind) -> bool {
            if (dsl_stack.empty()) {
                return false;
            }
            return dsl_stack.back() == kind;
        };
        size_t tmp_counter = 0;

        for (const auto& node : finalized_nodes) {
            switch (node.kind) {
                case syntax::OutputKind::CppLiteral: {
                    w.writeln(make_dsl_line_comment(line));
                    w.write_unformatted(node.content);
                    w.writeln();
                    prev_was_literal = true;
                    line += futils::strutil::count(node.content, "\n");
                    w.writeln(make_dsl_line_comment(line));
                    continue;
                }
                case syntax::OutputKind::CppSpecialMarker: {
                    auto content = trim(node.content);
                    DSLContext<syntax::SpecialOutputKind> inner_ctx;
                    auto seq = futils::make_ref_seq(content);
                    auto res = syntax::inner_special_marker(seq, inner_ctx, 0);
                    if (res != futils::comb2::Status::match) {
                        return ebmgen::unexpect_error("Failed to parse DSL special marker {}: {}", content, inner_ctx.errbuf);
                    }
                    if (inner_ctx.nodes.empty()) {
                        return ebmgen::unexpect_error("Empty DSL special marker: {}", content);
                    }
                    if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::TransferAndResetWriter) {
                        w.writeln("{");
                        auto scope = w.indent_scope();
                        w.writeln("MAYBE(got_writer, get_writer()); ", make_dsl_line_comment(line));
                        w.writeln("got_writer.write(std::move(w)); ", make_dsl_line_comment(line));
                        w.writeln("w.reset(); ", make_dsl_line_comment(line));
                        scope.execute();
                        w.writeln("}", make_dsl_line_comment(line));
                    }
                    else if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::ForIdent) {
                        auto ident = inner_ctx.nodes[0].content;
                        std::string_view init = "0";
                        std::string_view limit;
                        std::string_view step = "1";
                        std::string_view collection;
                        for (auto& n : inner_ctx.nodes) {
                            if (n.kind == syntax::SpecialOutputKind::ForItem) {
                                collection = n.content;
                            }
                            else if (n.kind == syntax::SpecialOutputKind::ForRangeBegin) {
                                limit = n.content;
                            }
                            else if (n.kind == syntax::SpecialOutputKind::ForRangeEnd) {
                                init = limit;
                                limit = n.content;
                            }
                            else if (n.kind == syntax::SpecialOutputKind::ForRangeStep) {
                                step = n.content;
                            }
                        }
                        if (limit.empty() || collection.size()) {
                            return ebmgen::unexpect_error("invalid for loop range: {}", content);
                        }
                        if (collection.size()) {
                            w.writeln("for (decltype(auto) ", ident, " : ", collection, ") {", make_dsl_line_comment(line));
                        }
                        else {
                            w.writeln("for (size_t ", ident, " = ", init, "; ", ident, " < ", limit, "; ", ident, " += ", step, ") {", make_dsl_line_comment(line));
                        }
                        auto scope = w.indent_scope_ex();
                        defers.emplace_back([&w, scope = std::move(scope)]() mutable {
                            scope.execute();
                            w.writeln("}");
                        });
                        dsl_stack.push_back(syntax::SpecialOutputKind::ForIdent);
                    }
                    else if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::IfCondition ||
                             inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::ElifCondition) {
                        auto condition = inner_ctx.nodes[0].content;
                        if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::ElifCondition) {
                            if (!most_recent(syntax::SpecialOutputKind::IfCondition)) {
                                return ebmgen::unexpect_error("inconsistent elif");
                            }
                            if (defers.empty()) {
                                return ebmgen::unexpect_error("inconsistent elif");
                            }
                            defers.pop_back();
                            w.write("else ");
                        }
                        w.writeln("if (", condition, ") {", make_dsl_line_comment(line));
                        auto scope = w.indent_scope_ex();
                        defers.emplace_back([&w, scope = std::move(scope)]() mutable {
                            scope.execute();
                            w.writeln("}");
                        });
                        if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::IfCondition) {
                            dsl_stack.push_back(syntax::SpecialOutputKind::IfCondition);
                        }
                    }
                    else if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::Else) {
                        if (!most_recent(syntax::SpecialOutputKind::IfCondition)) {
                            return ebmgen::unexpect_error("inconsistent else");
                        }
                        if (defers.empty()) {
                            return ebmgen::unexpect_error("inconsistent else");
                        }
                        defers.pop_back();
                        w.writeln("else {", make_dsl_line_comment(line));
                        auto scope = w.indent_scope_ex();
                        defers.emplace_back([&w, scope = std::move(scope)]() mutable {
                            scope.execute();
                            w.writeln("}");
                        });
                    }
                    else if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::EndFor) {
                        if (dsl_stack.empty() || dsl_stack.back() != syntax::SpecialOutputKind::ForIdent) {
                            return ebmgen::unexpect_error("inconsistent endfor");
                        }
                        if (defers.empty()) {
                            return ebmgen::unexpect_error("inconsistent endfor");
                        }
                        dsl_stack.pop_back();
                        defers.pop_back();
                    }
                    else if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::EndIf) {
                        if (dsl_stack.empty() || dsl_stack.back() != syntax::SpecialOutputKind::IfCondition) {
                            return ebmgen::unexpect_error("inconsistent endif");
                        }
                        if (defers.empty()) {
                            return ebmgen::unexpect_error("inconsistent endif");
                        }
                        dsl_stack.pop_back();
                        defers.pop_back();
                    }
                    else if (inner_ctx.nodes[0].kind == syntax::SpecialOutputKind::DefineVariableIdent) {
                        if (inner_ctx.nodes.size() < 2 || inner_ctx.nodes[1].kind != syntax::SpecialOutputKind::DefineVariableValue) {
                            return ebmgen::unexpect_error("invalid define variable syntax: {}", content);
                        }
                        auto ident = inner_ctx.nodes[0].content;
                        auto value = inner_ctx.nodes[1].content;
                        w.writeln("MAYBE(", ident, ", ebmcodegen::util::internal::force_wrap_expected(", value, ")); ", make_dsl_line_comment(line));
                    }
                    else {
                        return ebmgen::unexpect_error("unknown special marker: {}", content);
                    }
                    prev_was_literal = true;
                    line += futils::strutil::count(node.content, "\n");
                    continue;
                }
                case syntax::OutputKind::CppIdentifierGetter:
                case syntax::OutputKind::CppVisitedNode: {
                    auto handle_with_cached = [&](auto&& handle) {
                        w.writeln("{");
                        auto scope = w.indent_scope();
                        std::string temp_var = std::format("tmp");
                        handle(temp_var, false);
                        handle(temp_var, true);
                        scope.execute();
                        w.writeln("}", make_dsl_line_comment(line));
                    };
                    if (node.kind == syntax::OutputKind::CppIdentifierGetter) {
                        handle_with_cached([&](auto&& tmp, bool is_write) {
                            if (!is_write) {
                                if (is_class_based) {
                                    w.writeln("auto ", tmp, " = ctx.get(", node.content, "); ", make_dsl_line_comment(line));
                                }
                                else {
                                    w.writeln("auto ", tmp, " = get_associated_identifier(*this,", node.content, "); ", make_dsl_line_comment(line));
                                }
                            }
                            else {
                                w.writeln("w.write(", tmp, "); ", make_dsl_line_comment(line));
                            }
                        });
                    }
                    else {
                        handle_with_cached([&](auto&& tmp, bool is_write) {
                            if (!is_write) {
                                if (is_class_based) {
                                    w.writeln("MAYBE(", tmp, ", ctx.visit(", node.content, ")); ", make_dsl_line_comment(line));
                                }
                                else {
                                    w.writeln("MAYBE(", tmp, ", visit_Object(*this,", node.content, ")); ", make_dsl_line_comment(line));
                                }
                            }
                            else {
                                w.writeln("w.write(", tmp, ".to_writer()); ", make_dsl_line_comment(line));
                            }
                        });
                    }
                    line += futils::strutil::count(node.content, "\n");
                    break;
                }
                case syntax::OutputKind::CppWrite: {
                    w.writeln("w.write(", node.content, "); ", make_dsl_line_comment(line));
                    line += futils::strutil::count(node.content, "\n");
                    break;
                }
                case syntax::OutputKind::TargetLang: {
                    auto escaped = futils::escape::escape_str<std::string>(node.content);
                    w.writeln("w.write(\"", escaped, "\"); ", make_dsl_line_comment(line));
                    break;
                }
                case syntax::OutputKind::Indent: {
                    auto tmp_indent_counter = std::format("indent_{}", tmp_counter++);
                    w.writeln("{");
                    auto scope = w.indent_scope_ex();
                    w.writeln("auto ", tmp_indent_counter, " = w.indent_scope(); ", make_dsl_line_comment(line));
                    defers.emplace_back([&w, tmp_indent_counter, scope = std::move(scope)]() mutable {
                        scope.execute();
                        w.writeln("} // end indent ", tmp_indent_counter);
                    });
                    break;
                }
                case syntax::OutputKind::Dedent: {
                    if (defers.empty()) {
                        return ebmgen::unexpect_error("inconsistent indent");
                    }
                    defers.pop_back();
                    break;
                }
                case syntax::OutputKind::Line: {
                    if (!prev_was_literal) {
                        w.writeln("w.writeln();", make_dsl_line_comment(line));
                    }
                    line++;
                    break;
                }
            }
            prev_was_literal = false;
        }
        /*
        if (!dsl_stack.empty() || !defers.empty()) {
            return ebmgen::unexpect_error("unclosed DSL block");
        }
        */
        w.writeln("return w;");
        return w.out();
    }
}  // namespace ebmcodegen::dsl
