/*license*/
#pragma once
#include <comb2/composite/range.h>
#include <comb2/basic/logic.h>
#include <comb2/basic/peek.h>
#include <comb2/basic/literal.h>
#include <comb2/basic/group.h>
#include "comb2/internal/test.h"
#include "comb2/lexctx.h"
#include "comb2/seqrange.h"
#include "ebmgen/common.hpp"
#include <vector>
#include <string_view>

namespace ebmcodegen::dsl {
    namespace syntax {
        using namespace futils::comb2::ops;
        namespace cps = futils::comb2::composite;
        enum class OutputKind {
            CppLiteral,
            CppWrite,
            CppVisitedNode,
            CppIdentifierGetter,
            CppSpecialMarker,
            TargetLang,
            Indent,
            Dedent,
            Line,
        };
        constexpr auto begin_cpp = lit("{%");
        constexpr auto end_cpp = lit("%}");
        constexpr auto begin_cpp_var_writer = lit("{{");
        constexpr auto end_cpp_var_writer = lit("}}");
        constexpr auto begin_cpp_visited_node = lit("{*");
        constexpr auto end_cpp_visited_node = lit("*}");
        constexpr auto begin_cpp_identifier_getter = lit("{&");
        constexpr auto end_cpp_identifier_getter = lit("&}");
        constexpr auto begin_cpp_special_marker = lit("{!");
        constexpr auto end_cpp_special_marker = lit("!}");
        constexpr auto cpp_code = [](OutputKind tag, auto end) { return str(tag, *(not_(end) & +uany)); };
        constexpr auto cpp_target = begin_cpp & cpp_code(OutputKind::CppLiteral, end_cpp) & end_cpp;
        constexpr auto cpp_var_writer = begin_cpp_var_writer & cpp_code(OutputKind::CppWrite, end_cpp_var_writer) & end_cpp_var_writer;
        constexpr auto cpp_visited_node = begin_cpp_visited_node & cpp_code(OutputKind::CppVisitedNode, end_cpp_visited_node) & end_cpp_visited_node;
        constexpr auto cpp_identifier_getter = begin_cpp_identifier_getter & cpp_code(OutputKind::CppIdentifierGetter, end_cpp_identifier_getter) & end_cpp_identifier_getter;
        constexpr auto cpp_special_marker = begin_cpp_special_marker & cpp_code(OutputKind::CppSpecialMarker, end_cpp_special_marker) & end_cpp_special_marker;
        constexpr auto indent = str(OutputKind::Indent, bol & ~cps::space);
        constexpr auto line = str(OutputKind::Line, cps::eol);
        constexpr auto target_lang = str(OutputKind::TargetLang, ~(not_(cps::eol | begin_cpp | begin_cpp_var_writer | begin_cpp_visited_node | begin_cpp_identifier_getter | begin_cpp_special_marker) & uany));

        constexpr auto dsl = *(cpp_target | cpp_var_writer | cpp_visited_node | cpp_identifier_getter | cpp_special_marker | indent | line | target_lang) & eos;

        enum class SpecialOutputKind {
            TransferAndResetWriter,
            ForIdent,
            ForRangeBegin,
            ForRangeEnd,
            ForRangeStep,
            ForItem,
            EndFor,
            EndIf,
            IfCondition,
            ElifCondition,
            Else,
            DefineVariableIdent,
            DefineVariableValue,
        };

        constexpr auto transfer_and_reset = str(SpecialOutputKind::TransferAndResetWriter, lit("transfer_and_reset_writer"));
        constexpr auto for_ident = str(SpecialOutputKind::ForIdent, cps::c_ident);
        constexpr auto for_range_begin = str(SpecialOutputKind::ForRangeBegin, ~(not_(lit(",") | lit(")")) & uany));
        constexpr auto for_range_end = str(SpecialOutputKind::ForRangeEnd, ~(not_(lit(",") | lit(")")) & uany));
        constexpr auto for_range_step = str(SpecialOutputKind::ForRangeStep, ~(not_(lit(")")) & uany));
        constexpr auto spaces = *(cps::space);
        constexpr auto least_one_space = ~(cps::space);
        constexpr auto ranges = lit("range") &
                                least_one_space &
                                lit("(") &
                                for_range_begin &
                                -(
                                    lit(",") &
                                    for_range_end) &
                                -(
                                    lit(",") &
                                    for_range_step) &
                                lit(")");
        constexpr auto for_loop = lit("for") &
                                  least_one_space &
                                  for_ident &
                                  least_one_space &
                                  lit("in") &
                                  least_one_space & (ranges | str(SpecialOutputKind::ForItem, *uany));

        constexpr auto end_for = str(SpecialOutputKind::EndFor, lit("endfor"));
        constexpr auto end_if = str(SpecialOutputKind::EndIf, lit("endif"));

        constexpr auto if_ = lit("if") &
                             least_one_space &
                             str(SpecialOutputKind::IfCondition, *uany);

        constexpr auto elif_ = lit("elif") &
                               least_one_space &
                               str(SpecialOutputKind::ElifCondition, *uany);

        constexpr auto else_ =
            str(SpecialOutputKind::Else, lit("else"));

        constexpr auto define_variable =
            str(SpecialOutputKind::DefineVariableIdent, cps::c_ident) &
            spaces &
            lit(":=") &
            spaces &
            str(SpecialOutputKind::DefineVariableValue, *uany);

        constexpr auto inner_special_marker =
            spaces & (transfer_and_reset | for_loop | end_for | end_if | if_ | elif_ | else_ | define_variable) &
            eos;

        constexpr auto test_syntax() {
            auto seq = futils::make_ref_seq(R"(
{% int a = 0; %}
if ({{ a }} > 0) {
    {{ a }} += 1;
    {& item_id &}
    {* expr *}
    {! special !}
})");
            return dsl(seq, futils::comb2::test::TestContext<OutputKind>{}, 0) == futils::comb2::Status::match;
        }

        static_assert(test_syntax(), "DSL syntax static assert");

        constexpr auto test_inner_syntax() {
            auto test_case = [](const char* str) {
                auto seq = futils::make_ref_seq(str);
                return inner_special_marker(seq, futils::comb2::test::TestContext<SpecialOutputKind>{}, 0) == futils::comb2::Status::match;
            };
            return test_case("transfer_and_reset_writer") &&
                   test_case("for item in range(0, 10)") &&
                   test_case("for i in range(1, 20, 2)") &&
                   test_case("for element in some_collection") &&
                   test_case("endfor") &&
                   test_case("endif") &&
                   test_case("if (a > b)") &&
                   test_case("my_var := 42") &&
                   test_case("elif (a == b)") &&
                   test_case("else");
        }

        static_assert(test_inner_syntax(), "DSL inner special marker syntax static assert");
    }  // namespace syntax

    template <class Kind>
    struct DSLNode {
        Kind kind{};
        std::string_view content;
    };

    template <class Kind>
    struct DSLContext : futils::comb2::LexContext<Kind, std::string> {
        std::vector<DSLNode<Kind>> nodes;

        constexpr void end_string(futils::comb2::Status res, Kind kind, auto&& seq, futils::comb2::Pos pos) {
            if (res == futils::comb2::Status::match) {
                DSLNode<Kind> node;
                node.kind = kind;
                futils::comb2::seq_range_to_string(node.content, seq, pos);
                nodes.push_back(std::move(node));
            }
        }
    };

    ebmgen::expected<std::string> generate_dsl_output(std::string_view file_name, std::string_view source);

}  // namespace ebmcodegen::dsl
