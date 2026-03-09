/*license*/
#include <wrap/cin.h>
#include <wrap/cout.h>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include "binary/writer.h"
#include "code/src_location.h"
#include "comb2/composite/number.h"
#include "comb2/composite/range.h"
#include "comb2/composite/string.h"
#include "comb2/internal/test.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmcodegen/stub/structs.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/debug_printer.hpp"
#include "ebmgen/mapping.hpp"
#include "error/error.h"
#include "escape/escape.h"
#include "number/hex/bin2hex.h"
#include "number/prefix.h"
#include <strutil/splits.h>
#include <comb2/composite/cmdline.h>
#include <console/ansiesc.h>
#include <comb2/tree/simple_node.h>
#include "debugger.hpp"

namespace ebmgen {
    namespace query {
        enum class NodeType {
            Root,
            Operator,
            Ident,
            Number,
            StringLit,
            BinaryOp,
            UnaryOp,
            ObjectType,
            Object,
        };
        using namespace futils::comb2::ops;
        constexpr auto space = futils::comb2::composite::space | futils::comb2::composite::tab | futils::comb2::composite::eol;
        constexpr auto spaces = *(space);
        constexpr auto compare_op = str(NodeType::Operator, lit(">=") | lit("<=") | lit("==") | lit("!=") | lit(">") | lit("<"));
        constexpr auto and_op = str(NodeType::Operator, lit("and") | lit("&&"));
        constexpr auto or_op = str(NodeType::Operator, lit("or") | lit("||"));
        constexpr auto not_op = str(NodeType::Operator, lit("not") | lit("!"));
        constexpr auto contains_op = str(NodeType::Operator, lit("contains"));
        constexpr auto in_op = str(NodeType::Operator, lit("in"));
        constexpr auto number = str(NodeType::Number, futils::comb2::composite::hex_integer | futils::comb2::composite::dec_integer);
        constexpr auto ident = str(NodeType::Ident, futils::comb2::composite::c_ident&*(((lit(".") | lit("->")) & +futils::comb2::composite::c_ident) | lit("[") & +futils::comb2::composite::dec_integer & +lit("]")));
        constexpr auto string_lit = str(NodeType::StringLit, futils::comb2::composite::c_str | futils::comb2::composite::char_str);
        constexpr auto expr_recurse = method_proxy(expr);
        constexpr auto compare_recurse = method_proxy(compare);
        constexpr auto and_recurse = method_proxy(and_expr);
        constexpr auto or_recurse = method_proxy(or_expr);
        constexpr auto object_recurse = method_proxy(object);
        constexpr auto primary = spaces & +(string_lit | number | object_recurse | ident | ('('_l & expr_recurse & +')'_l)) & spaces;
        constexpr auto unary_op = group(NodeType::UnaryOp, ((not_op | contains_op) & +primary) | +primary);
        constexpr auto in_object = group(NodeType::BinaryOp, (unary_op & -(in_op & spaces & +unary_op)));
        constexpr auto compare = group(NodeType::BinaryOp, in_object & -(compare_op & spaces & +compare_recurse));
        constexpr auto and_expr = group(NodeType::BinaryOp, compare & -(and_op & spaces & +and_recurse));
        constexpr auto or_expr = group(NodeType::BinaryOp, and_expr & -(or_op & spaces & +or_recurse));
        constexpr auto expr = or_expr;
        constexpr auto object_type = str(NodeType::ObjectType, lit("Identifier") | lit("String") | lit("Type") | lit("Statement") | lit("Expression") | lit("Alias") | lit("Any"));
        constexpr auto object = group(NodeType::Object, object_type& spaces & +lit("{") & spaces & +expr_recurse & spaces & +lit("}"));
        constexpr auto full_expr = spaces & ~(object_recurse & spaces) & spaces & +eos;
        struct Recurse {
            decltype(expr) expr;
            decltype(compare) compare;
            decltype(and_expr) and_expr;
            decltype(or_expr) or_expr;
            decltype(object) object;
        };
        constexpr Recurse recurse{expr, compare, and_expr, or_expr, object};
        constexpr auto check = []() {
            auto seq = futils::make_ref_seq("Identifier { id == 123 } Expression { id >= 0 and id < 100 } Statement { bop == \"add\"} Any { body.condition->body.kind == \"Identifier\" or body.condition in Any { id == 456 } } Any{contains 11 and not (id == 11) and body.block[0] == 1}");
            return full_expr(seq, futils::comb2::test::TestContext<>{}, recurse) == futils::comb2::Status::match;
        };

        static_assert(check(), "Expression parser static assert");

        using Node = futils::comb2::tree::node::GenericNode<NodeType>;

        futils::helper::either::expected<std::shared_ptr<Node>, std::pair<futils::code::SrcLoc, std::string>> parse_line(std::string_view line) {
            auto input = futils::make_ref_seq(line);
            futils::comb2::tree::BranchTable table;
            auto res = query::full_expr(input, table, query::recurse);
            if (res != futils::comb2::Status::match || !input.eos()) {
                std::string buffer;
                auto src_loc = futils::code::write_src_loc(buffer, input);
                return futils::helper::either::unexpected(std::make_pair(src_loc, std::move(buffer)));
            }
            return futils::comb2::tree::node::collect<NodeType>(table.root_branch);
        }
    }  // namespace query

    using ObjectSet = std::unordered_set<ebm::AnyRef>;

    using EvalValue = std::variant<bool, std::uint64_t, std::string, ebm::AnyRef, ObjectSet>;

    using ObjectVariantExtended = std::variant<std::monostate, const ebm::Identifier*, const ebm::StringLiteral*, const ebm::Type*, const ebm::Statement*, const ebm::Expression*, const ebm::RefAlias*>;
    struct EvalContext {
        MappingTable& table;
        Failures& failures;
        std::vector<EvalValue> stack;
        std::map<std::string, EvalValue> variables;
        ObjectVariantExtended current_target;

        template <class T>
        void unwrap_any_ref(EvalValue& value) {
            if (std::holds_alternative<ebm::AnyRef>(value)) {
                auto ref = std::get<ebm::AnyRef>(value);
                if constexpr (std::is_same_v<T, std::string>) {
                    auto ident = table.get_identifier(ref);
                    if (ident) {
                        value = ident->body.data;
                    }
                    else {
                        auto str_lit = table.get_string_literal(ebm::StringRef{ref.id});
                        if (str_lit) {
                            value = str_lit->body.data;
                        }
                    }
                }
                else if (std::is_same_v<T, std::uint64_t>) {
                    value = get_id(ref);
                }
                else if (std::is_same_v<T, bool>) {
                    value = !is_nil(ref);
                }
            }
        }
    };
    using EvalFunc = std::function<ExecutionResult(EvalContext&)>;
    static void collect_variables(std::map<std::string, EvalValue>& variables, const std::unordered_set<std::string>& related_identifiers, has_visit<DummyFn> auto& t) {
        std::vector<std::string> idents;
        auto qualified_name = [&](const char* name) -> std::string {
            if (idents.size()) {
                return idents.back() + "." + name;
            }
            return name;
        };
        t.visit([&](auto&& visitor, const char* name, auto&& value) -> void {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_pointer_v<std::decay_t<decltype(value)>>) {
                if (value) {
                    visitor(visitor, name, *value);
                }
            }
            else if constexpr (is_container<decltype(value)>) {
                for (size_t i = 0; i < value.container.size(); i++) {
                    std::string elem_name = std::format("{}[{}]", name, i);
                    visitor(visitor, elem_name.c_str(), value.container[i]);
                }
            }
            else {
                auto qname = qualified_name(name);
                if constexpr (std::is_same_v<T, ebm::Varint>) {
                    if (related_identifiers.contains(qname)) {
                        variables[qname] = value.value();
                    }
                }
                else if constexpr (AnyRef<T>) {
                    if (related_identifiers.contains(qname)) {
                        variables[qname] = to_any_ref(value);
                    }
                }
                else if constexpr (std::is_enum_v<T>) {
                    if (related_identifiers.contains(qname)) {
                        variables[qname] = to_string(value, true);
                    }
                }
                else if constexpr (std::is_same_v<T, bool>) {
                    if (related_identifiers.contains(qname)) {
                        variables[qname] = value;
                    }
                }
                else if constexpr (std::is_integral_v<T>) {
                    if (related_identifiers.contains(qname)) {
                        variables[qname] = std::uint64_t(value);
                    }
                }
                else if constexpr (std::is_same_v<T, std::string>) {
                    if (related_identifiers.contains(qname)) {
                        variables[qname] = value;
                    }
                }
                else if constexpr (ebmgen::has_visit<decltype(value), decltype(visitor)>) {
                    idents.push_back(std::move(qname));
                    value.visit(visitor);
                    idents.pop_back();
                }
                else {
                    static_assert(std::is_same_v<T, void>, "unsupported type in query");
                }
            }
        });
    }
    struct Object {
        std::string type;
        std::unordered_set<std::string> related_identifiers;
        EvalFunc evaluator;

        ExecutionResult query(MappingTable& table, ObjectSet& matched, Failures& failures, ObjectResult* result = nullptr) const {
            bool some_has_valid_result = false;
            bool has_identifier_not_found = false;
            auto for_each = [&](auto& objects) {
                for (auto& t : objects) {
                    EvalContext ctx{.table = table, .failures = failures, .current_target = &t};
                    collect_variables(ctx.variables, related_identifiers, t);
                    auto exec_result = evaluator(ctx);
                    if (exec_result == ExecutionResult::Success && ctx.stack.size() == 1) {
                        ctx.unwrap_any_ref<bool>(ctx.stack[0]);
                        some_has_valid_result = true;
                        if (std::holds_alternative<bool>(ctx.stack[0]) && std::get<bool>(ctx.stack[0])) {
                            if constexpr (std::is_same_v<decltype(t), const ebm::RefAlias&>) {
                                if (matched.insert(t.from).second && result) {
                                    result->push_back(to_any_ref(t.from));
                                }
                            }
                            else {
                                if (matched.insert(to_any_ref(t.id)).second && result) {
                                    result->push_back(to_any_ref(t.id));
                                }
                            }
                        }
                    }
                    else if (exec_result == ExecutionResult::IdentifierNotFound) {
                        has_identifier_not_found = true;
                    }
                    else if (exec_result != ExecutionResult::Success) {
                        failures.insert(exec_result);
                    }
                    else {
                        failures.insert(ExecutionResult::InconsistentStack);
                    }
                }
            };
            if (type == "Identifier") {
                for_each(table.module().identifiers);
            }
            else if (type == "String") {
                for_each(table.module().strings);
            }
            else if (type == "Type") {
                for_each(table.module().types);
            }
            else if (type == "Statement") {
                for_each(table.module().statements);
            }
            else if (type == "Expression") {
                for_each(table.module().expressions);
            }
            else if (type == "Alias") {
                for_each(table.module().aliases);
            }
            else if (type == "Any") {
                for_each(table.module().identifiers);
                for_each(table.module().strings);
                for_each(table.module().types);
                for_each(table.module().statements);
                for_each(table.module().expressions);
            }
            else {
                failures.insert(ExecutionResult::InvalidOperator);
                return ExecutionResult::InvalidOperator;
            }
            if (!some_has_valid_result && has_identifier_not_found) {
                failures.insert(ExecutionResult::IdentifierNotFound);
                return ExecutionResult::IdentifierNotFound;
            }
            return ExecutionResult::Success;
        }
    };

    struct Query {
        std::vector<Object> objects;

        void query(MappingTable& table, ObjectSet& matched, Failures& failures, ObjectResult* result = nullptr) const {
            for (const auto& obj : objects) {
                obj.query(table, matched, failures, result);
            }
        }
    };
#define CHECK_AND_TAKE(var)                 \
    if (ctx.stack.empty()) {                \
        return ExecutionResult::StackEmpty; \
    }                                       \
    auto var = std::move(ctx.stack.back()); \
    ctx.stack.pop_back()

#define CHECK_AND_TAKE_AS(var, T)              \
    CHECK_AND_TAKE(var##__);                   \
    ctx.unwrap_any_ref<T>(var##__);            \
    if (!std::holds_alternative<T>(var##__)) { \
        return ExecutionResult::TypeMismatch;  \
    }                                          \
    auto var = std::get<T>(var##__)

    using NodeType = query::NodeType;
    using Node = query::Node;

#define RETURN_EXEC_FAILURE(expr)                                 \
    do {                                                          \
        if (auto res = (expr); res != ExecutionResult::Success) { \
            return res;                                           \
        }                                                         \
    } while (0)

    struct QueryCompiler {
        MappingTable& table;
        ExecutionResult query_short_circuit(EvalContext& ctx, const EvalFunc& left, const EvalFunc& right, bool short_circuit) {
            RETURN_EXEC_FAILURE(left(ctx));
            CHECK_AND_TAKE_AS(left_value, bool);
            if (left_value == short_circuit) {
                ctx.stack.push_back(EvalValue{left_value});
                return ExecutionResult::Success;
            }
            return right(ctx);
        }

        static void adjust_operands(EvalContext& ctx, EvalValue& left_value, EvalValue& right_value) {
            auto adjust_any_ref = [&](EvalValue& any_ref, EvalValue& adjust_to) {
                if (std::holds_alternative<std::string>(adjust_to)) {
                    ctx.unwrap_any_ref<std::string>(any_ref);
                }
                else if (std::holds_alternative<std::uint64_t>(adjust_to)) {
                    ctx.unwrap_any_ref<std::uint64_t>(any_ref);
                }
                else if (std::holds_alternative<bool>(adjust_to)) {
                    ctx.unwrap_any_ref<bool>(any_ref);
                }
            };
            if (std::holds_alternative<ebm::AnyRef>(left_value) && !std::holds_alternative<ebm::AnyRef>(right_value)) {
                adjust_any_ref(left_value, right_value);
            }
            else if (std::holds_alternative<ebm::AnyRef>(right_value) && !std::holds_alternative<ebm::AnyRef>(left_value)) {
                adjust_any_ref(right_value, left_value);
            }
        }

        expected<EvalFunc> compile_expr(Object& object, const std::shared_ptr<Node>& node) {
            using futils::comb2::tree::node::as_group, futils::comb2::tree::node::as_tok;
            if (auto g = as_group<NodeType>(node)) {
                if (g->tag == NodeType::BinaryOp) {
                    if (g->children.size() == 1) {
                        return compile_expr(object, g->children[0]);
                    }
                    else if (g->children.size() == 3) {
                        MAYBE(left, compile_expr(object, g->children[0]));
                        auto op = as_tok<NodeType>(g->children[1]);
                        MAYBE(right, compile_expr(object, g->children[2]));
                        if (op && op->tag == NodeType::Operator) {
                            if (op->token == "in") {
                                return unexpect_error("Operator 'in' not implemented");
                            }
                            else if (op->token == "and" || op->token == "&&") {
                                return [this, left = std::move(left), right = std::move(right)](EvalContext& ctx) {
                                    return query_short_circuit(ctx, left, right, false);
                                };
                            }
                            else if (op->token == "or" || op->token == "||") {
                                return [this, left = std::move(left), right = std::move(right)](EvalContext& ctx) {
                                    return query_short_circuit(ctx, left, right, true);
                                };
                            }
                            else if (op->token == "==" || op->token == "!=" || op->token == ">" || op->token == ">=" || op->token == "<" || op->token == "<=") {
                                return [this, left = std::move(left), right = std::move(right), op = op->token](EvalContext& ctx) {
                                    RETURN_EXEC_FAILURE(left(ctx));
                                    CHECK_AND_TAKE(left_value);
                                    RETURN_EXEC_FAILURE(right(ctx));
                                    CHECK_AND_TAKE(right_value);
                                    adjust_operands(ctx, left_value, right_value);
                                    bool result = false;
                                    if (std::holds_alternative<bool>(left_value) && std::holds_alternative<bool>(right_value)) {
                                        auto l = std::get<bool>(left_value);
                                        auto r = std::get<bool>(right_value);
                                        if (op == "==") {
                                            result = (l == r);
                                        }
                                        else if (op == "!=") {
                                            result = (l != r);
                                        }
                                        else {
                                            return ExecutionResult::InvalidOperator;
                                        }
                                    }
                                    else if (std::holds_alternative<std::string>(left_value) && std::holds_alternative<std::string>(right_value)) {
                                        auto l = std::get<std::string>(left_value);
                                        auto r = std::get<std::string>(right_value);
                                        if (op == "==") {
                                            result = (l == r);
                                        }
                                        else if (op == "!=") {
                                            result = (l != r);
                                        }
                                        else {
                                            return ExecutionResult::InvalidOperator;
                                        }
                                    }
                                    else if (std::holds_alternative<std::uint64_t>(left_value) && std::holds_alternative<std::uint64_t>(right_value)) {
                                        auto l = std::get<std::uint64_t>(left_value);
                                        auto r = std::get<std::uint64_t>(right_value);
                                        if (op == "==") {
                                            result = (l == r);
                                        }
                                        else if (op == "!=") {
                                            result = (l != r);
                                        }
                                        else if (op == ">") {
                                            result = (l > r);
                                        }
                                        else if (op == ">=") {
                                            result = (l >= r);
                                        }
                                        else if (op == "<") {
                                            result = (l < r);
                                        }
                                        else if (op == "<=") {
                                            result = (l <= r);
                                        }
                                        else {
                                            return ExecutionResult::InvalidOperator;
                                        }
                                    }
                                    else if (std::holds_alternative<ebm::AnyRef>(left_value) && std::holds_alternative<ebm::AnyRef>(right_value)) {
                                        auto l = std::get<ebm::AnyRef>(left_value);
                                        auto r = std::get<ebm::AnyRef>(right_value);
                                        if (op == "==") {
                                            result = (get_id(l) == get_id(r));
                                        }
                                        else if (op == "!=") {
                                            result = (get_id(l) != get_id(r));
                                        }
                                        else {
                                            return ExecutionResult::InvalidOperator;
                                        }
                                    }
                                    else {
                                        return ExecutionResult::InvalidOperator;
                                    }
                                    ctx.stack.push_back(EvalValue{result});
                                    return ExecutionResult::Success;
                                };
                            }
                            else {
                                return unexpect_error("Unsupported binary operator: {}", op->token);
                            }
                        }
                        return unexpect_error("Unsupported binary operation: {}", op ? op->token : "(null)");
                    }
                    else {
                        return unexpect_error("Invalid binary operation node with {} children", g->children.size());
                    }
                }
                else if (g->tag == NodeType::UnaryOp) {
                    if (g->children.size() == 1) {
                        return compile_expr(object, g->children[0]);
                    }
                    else if (g->children.size() == 2) {
                        auto op = as_tok<NodeType>(g->children[0]);
                        MAYBE(operand, compile_expr(object, g->children[1]));
                        if (op && op->tag == NodeType::Operator) {
                            if (op->token == "not" || op->token == "!") {
                                return [operand = std::move(operand)](EvalContext& ctx) {
                                    RETURN_EXEC_FAILURE(operand(ctx));
                                    CHECK_AND_TAKE_AS(value, bool);
                                    ctx.stack.push_back(EvalValue{!value});
                                    return ExecutionResult::Success;
                                };
                            }
                            else if (op->token == "contains") {
                                return [operand = std::move(operand)](EvalContext& ctx) {
                                    RETURN_EXEC_FAILURE(operand(ctx));
                                    CHECK_AND_TAKE(operand_value);
                                    bool found = std::visit([&](auto& target) {
                                        if constexpr (std::is_pointer_v<std::decay_t<decltype(target)>>) {
                                            bool found = false;
                                            target->visit([&](auto&& v, auto name, auto&& value) {
                                                if (found) {
                                                    return;
                                                }
                                                if constexpr (AnyRef<decltype(value)>) {
                                                    auto ref = to_any_ref(value);
                                                    EvalValue val{ref};
                                                    adjust_operands(ctx, operand_value, val);
                                                    if (std::holds_alternative<std::string>(operand_value) && std::holds_alternative<std::string>(val)) {
                                                        found = (std::get<std::string>(operand_value) == std::get<std::string>(val));
                                                    }
                                                    else if (std::holds_alternative<std::uint64_t>(operand_value) && std::holds_alternative<std::uint64_t>(val)) {
                                                        found = (std::get<std::uint64_t>(operand_value) == std::get<std::uint64_t>(val));
                                                    }
                                                    else if (std::holds_alternative<bool>(operand_value) && std::holds_alternative<bool>(val)) {
                                                        found = (std::get<bool>(operand_value) == std::get<bool>(val));
                                                    }
                                                    else if (std::holds_alternative<ebm::AnyRef>(operand_value) && std::holds_alternative<ebm::AnyRef>(val)) {
                                                        found = (get_id(std::get<ebm::AnyRef>(operand_value)) == get_id(std::get<ebm::AnyRef>(val)));
                                                    }
                                                    else if (std::holds_alternative<ObjectSet>(operand_value) && std::holds_alternative<ebm::AnyRef>(val)) {
                                                        found = (std::get<ObjectSet>(operand_value).contains(std::get<ebm::AnyRef>(val)));
                                                    }
                                                }
                                                else
                                                    VISITOR_RECURSE_CONTAINER(v, name, value)
                                                else VISITOR_RECURSE(v, name, value)
                                            });
                                            return found;
                                        }
                                        else {
                                            return false;
                                        }
                                    },
                                                            ctx.current_target);
                                    ctx.stack.push_back(EvalValue{found});
                                    return ExecutionResult::Success;
                                };
                            }
                            else {
                                return unexpect_error("Unsupported unary operator: {}", op->token);
                            }
                        }
                        return unexpect_error("Unsupported unary operation: {}", op ? op->token : "(null)");
                    }
                    else {
                        return unexpect_error("Invalid unary operation node with {} children", g->children.size());
                    }
                }
                else if (g->tag == NodeType::Object) {
                    MAYBE(obj, compile_object(node));
                    return [this, obj = std::move(obj)](EvalContext& ctx) {
                        std::unordered_set<ebm::AnyRef> matched;
                        RETURN_EXEC_FAILURE(obj.query(this->table, matched, ctx.failures));
                        ctx.stack.push_back(EvalValue{std::move(matched)});
                        return ExecutionResult::Success;
                    };
                }
                else {
                    return unexpect_error("Unsupported expression group node");
                }
            }
            else if (auto t = as_tok<NodeType>(node)) {
                if (t->tag == NodeType::Number) {
                    std::uint64_t value;
                    if (!futils::number::prefix_integer(t->token, value)) {
                        return unexpect_error("Invalid number: {}", t->token);
                    }
                    return [value](EvalContext& ctx) {
                        ctx.stack.push_back(EvalValue{value});
                        return ExecutionResult::Success;
                    };
                }
                else if (t->tag == NodeType::Ident) {
                    if (t->token == "nil" || t->token == "null") {
                        return [](EvalContext& ctx) {
                            ctx.stack.push_back(EvalValue{ebm::AnyRef{0}});
                            return ExecutionResult::Success;
                        };
                    }
                    if (t->token == "true") {
                        return [](EvalContext& ctx) {
                            ctx.stack.push_back(EvalValue{true});
                            return ExecutionResult::Success;
                        };
                    }
                    if (t->token == "false") {
                        return [](EvalContext& ctx) {
                            ctx.stack.push_back(EvalValue{false});
                            return ExecutionResult::Success;
                        };
                    }
                    auto per_pointer = futils::strutil::split<std::string>(t->token, "->");
                    object.related_identifiers.insert(per_pointer[0]);
                    return [this, per_pointer](EvalContext& ctx) {
                        if (auto found = ctx.variables.find(per_pointer[0]); found != ctx.variables.end()) {
                            auto result = found->second;
                            for (size_t i = 1; i < per_pointer.size(); i++) {
                                if (!std::holds_alternative<ebm::AnyRef>(result)) {
                                    return ExecutionResult::TypeMismatch;
                                }
                                const auto& part = per_pointer[i];
                                std::unordered_set<std::string> member_idents{part};
                                auto v = table.get_object(std::get<ebm::AnyRef>(result));
                                std::map<std::string, EvalValue> members;
                                std::visit(
                                    [&](auto&& obj) {
                                        if constexpr (std::is_pointer_v<std::decay_t<decltype(obj)>>) {
                                            collect_variables(members, member_idents, *obj);
                                        }
                                    },
                                    v);
                                if (members.empty()) {
                                    return ExecutionResult::IdentifierNotFound;
                                }
                                result = std::move(members[part]);
                            }
                            ctx.stack.push_back(std::move(result));
                            return ExecutionResult::Success;
                        }
                        return ExecutionResult::IdentifierNotFound;
                    };
                }
                else if (t->tag == NodeType::StringLit) {
                    std::string value;
                    auto removed_quotes = t->token;
                    removed_quotes.erase(0, 1);
                    removed_quotes.pop_back();
                    if (!futils::escape::unescape_str(removed_quotes, value)) {
                        return unexpect_error("Invalid string literal: {}", t->token);
                    }
                    return [value = std::move(value)](EvalContext& ctx) {
                        ctx.stack.push_back(EvalValue{value});
                        return ExecutionResult::Success;
                    };
                }
            }
            return unexpect_error("Unsupported expression node");
        }

        expected<Object> compile_object(const std::shared_ptr<Node>& node) {
            using futils::comb2::tree::node::as_group, futils::comb2::tree::node::as_tok;
            if (auto g = as_group<NodeType>(node)) {
                if (g->tag == NodeType::Object) {
                    if (g->children.size() == 2) {
                        auto type_tok = as_tok<NodeType>(g->children[0]);
                        if (!type_tok || type_tok->tag != NodeType::ObjectType) {
                            return unexpect_error("Invalid object type token");
                        }
                        auto expr_node = g->children[1];
                        Object obj;
                        MAYBE(fn, compile_expr(obj, expr_node));
                        obj.evaluator = std::move(fn);
                        obj.type = type_tok->token;
                        return obj;
                    }
                    else {
                        return unexpect_error("Invalid object node with {} children", g->children.size());
                    }
                }
            }
            return unexpect_error("Unsupported object node");
        }

        expected<void> compile(Query& eval, const std::shared_ptr<Node>& node) {
            using futils::comb2::tree::node::as_group;
            if (auto g = as_group<NodeType>(node)) {
                if (g->tag == NodeType::Root) {
                    for (auto& child : g->children) {
                        MAYBE(obj, compile_object(child));
                        eval.objects.push_back(std::move(obj));
                    }
                    return {};
                }
            }
            return unexpect_error("Unsupported root node");
        }
    };

    struct Debugger {
        futils::wrap::UtfOut& cout;
        futils::wrap::UtfIn& cin;
        MappingTable& table;

        std::string line;
        std::vector<std::string_view> args;

        void print_hex() {
            if (args.size() < 2) {
                cout << "Usage: hex <identifier>\n";
                return;
            }
            auto ident = args[1];
            std::uint64_t id;
            if (!futils::number::prefix_integer(ident, id)) {
                cout << "Invalid identifier: " << ident << "\n";
                return;
            }
            auto obj = table.get_object(ebm::AnyRef{id});
            std::string buffer;
            std::visit(
                [&](auto&& obj) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(obj)>, std::monostate>) {
                        cout << "Identifier not found: " << ident << "\n";
                    }
                    else {
                        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
                        auto err = obj->encode(w);
                        if (err) {
                            cout << "Error encoding object: " << err.template error<std::string>() << "\n";
                        }
                        else {
                            cout << futils::number::hex::to_hex<std::string>(buffer) << "\n";
                            cout << futils::number::hex::hexdump<std::string>(buffer) << "\n";
                        }
                    }
                },
                obj);
        }

        void print() {
            std::stringstream print_buf;
            DebugPrinter printer{table, print_buf};
            if (args.size() < 2) {
                cout << "Usage: print <identifier>\n";
                return;
            }
            auto ident = args[1];
            std::uint64_t id;
            if (!futils::number::prefix_integer(ident, id)) {
                cout << "Invalid identifier: " << ident << "\n";
                return;
            }
            auto obj = table.get_object(ebm::AnyRef{id});
            std::visit(
                [&](auto&& obj) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(obj)>, std::monostate>) {
                        cout << "Identifier not found: " << ident << "\n";
                    }
                    else {
                        printer.print_object(*obj);
                    }
                },
                obj);
            cout << print_buf.str();
        }

        void header() {
            auto& raw_module = *table.module().origin;
            cout << "Header Information:\n";
            cout << " Version: " << raw_module.version << "\n";
            cout << " Max ID: " << get_id(raw_module.max_id) << "\n";
            cout << " Identifiers: " << raw_module.identifiers.size() << "\n";
            cout << " String Literals: " << raw_module.strings.size() << "\n";
            cout << " Types: " << raw_module.types.size() << "\n";
            cout << " Statements: " << raw_module.statements.size() << "\n";
            cout << " Expressions: " << raw_module.expressions.size() << "\n";
            cout << " Aliases: " << raw_module.aliases.size() << "\n";
            cout << " Total Objects: " << (raw_module.identifiers.size() + raw_module.strings.size() + raw_module.types.size() + raw_module.statements.size() + raw_module.expressions.size() + raw_module.aliases.size()) << "\n";
            cout << " Source File:\n";
            for (auto& s : raw_module.debug_info.files) {
                auto file_path = table.get_string_literal(s);
                cout << "  ID " << get_id(s) << ": " << (file_path ? file_path->body.data : "(invalid)") << "\n";
            }
            cout << " Debug Locations: " << raw_module.debug_info.locs.size() << "\n";
            cout << "----\n";
        }

        bool setup_evaluator(QueryCompiler& compiler, Query& eval) {
            auto offset = args[0].data() + args[0].size() - line.data();
            auto input = std::string_view(line).substr(offset);
            auto parsed = query::parse_line(input);
            if (!parsed) {
                auto src_loc = parsed.error().first;
                cout << "Error parsing query expression at " << src_loc.line + 1 << ":" << src_loc.pos + 1 << ":\n";
                cout << parsed.error().second << "\n";
                return false;
            }
            auto result = compiler.compile(eval, *parsed);
            if (!result) {
                cout << "Error in query expression: " << result.error().error() << "\n";
                return false;
            }
            return true;
        }

        void query() {
            if (args.size() < 2) {
                cout << "Usage: query <conditions...>\n";
                return;
            }
            QueryCompiler compiler{table};
            Query query;
            if (!setup_evaluator(compiler, query)) {
                return;
            }
            size_t count = 0;
            ObjectSet matched;
            Failures failures;
            query.query(table, matched, failures);
            for (auto& ref : matched) {
                auto obj = table.get_object(ref);
                std::visit(
                    [&](auto&& obj) {
                        if constexpr (std::is_same_v<std::decay_t<decltype(obj)>, std::monostate>) {
                            cout << "Identifier not found: " << get_id(ref) << "\n";
                        }
                        else {
                            std::stringstream print_buf;
                            DebugPrinter printer{table, print_buf};
                            printer.print_object(*obj);
                            cout << print_buf.str();
                            cout << "----\n";
                            count++;
                        }
                    },
                    obj);
            }
            cout << "Total matched objects: " << count << "\n";
            if (matched.empty() && !failures.empty()) {
                cout << "Query syntax/semantic error:\n";
                for (auto f : failures) {
                    cout << "  " << to_string(f) << "\n";
                }
            }
        }

        void print_struct() {
            auto [struct_, enum_] = ebmcodegen::make_struct_map();
            if (args[0] == "list") {
                cout << "Available structs and enums:\n";
                for (auto& [name, _] : struct_) {
                    cout << "  struct " << name << "\n";
                }
                for (auto& [name, _] : enum_) {
                    cout << "  enum " << name << "\n";
                }
                return;
            }
            if (args.size() < 2) {
                cout << "Usage: show <struct_or_enum_name>\n";
                return;
            }
            auto it = struct_.find(args[1]);
            if (it == struct_.end()) {
                if (auto eit = enum_.find(args[1]); eit != enum_.end()) {
                    auto& e = eit->second;
                    cout << "enum " << e.name << " {\n";
                    for (auto& member : e.members) {
                        cout << "  " << member.name << " = " << member.value << ";\n";
                    }
                    cout << "}\n";
                    return;
                }
                cout << "Struct or enum not found: " << args[1] << "\n";
                return;
            }
            auto& s = it->second;
            cout << "struct " << s.name << " {\n";
            for (auto& field : s.fields) {
                cout << "  " << field.type;
                if (field.attr & ebmcodegen::ARRAY) {
                    cout << "[]";
                }
                if (field.attr & ebmcodegen::PTR) {
                    cout << "*";
                }
                cout << " " << field.name << ";\n";
            }
            cout << "}\n";
        }

        void print_help() {
            cout << "Commands:\n";
            cout << "  help             Show this help message\n";
            cout << "  exit, quit       Exit the debugger\n";
            cout << "  print <id>       Print the object with the given identifier\n";
            cout << "  hex <id>         Print the raw hex data of the object with the given identifier\n";
            cout << "  p <id>           Alias for print\n";
            cout << "  pr <id>          Alias for print\n";
            cout << "  header           Print module header information\n";
            cout << "  query <expr>     Query objects matching the expression\n";
            cout << "  q <expr>         Alias for query\n";
            cout << "  clear            Clear the console\n";
            cout << "  show <name>      Show the structure of the given struct or enum\n";
            cout << "  list             List all available structs and enums\n";
            cout << "  (empty line)     Do nothing\n";
        }

        ExecutionResult run_line() {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            std::set<std::string> temporary_buffer;
            args = futils::comb2::cmdline::command_line<std::vector<std::string_view>>(std::string_view(line), [&](auto& buf) {
                buf = buf.substr(1, buf.size() - 2);
                if (buf.contains("\\")) {
                    auto tmp_buf = futils::escape::unescape_str<std::string>(buf);
                    buf = *temporary_buffer.insert(std::move(tmp_buf)).first;
                }
            });
            if (args.empty()) {
                return ExecutionResult::Success;
            }
            auto command = args[0];
            if (command == "exit" || command == "quit") {
                return ExecutionResult::Exit;
            }
            else if (command == "help") {
                print_help();
            }
            else if (command == "print" || command == "p" || command == "pr") {
                print();
            }
            else if (command == "hex") {
                print_hex();
            }
            else if (command == "header") {
                header();
            }
            else if (command == "clear") {
                cout << futils::console::escape::cursor(futils::console::escape::CursorMove::clear, 2).c_str();
                cout << futils::console::escape::cursor(futils::console::escape::CursorMove::home, 1).c_str();
            }
            else if (command == "query" || command == "q") {
                query();
            }
            else if (command == "show") {
                print_struct();
            }
            else if (command == "list") {
                print_struct();
            }
            else {
                cout << "Unknown command: " << command << ", type 'help' for commands\n";
            }
            return ExecutionResult::Success;
        }

        void start() {
            cout << "Interactive Debugger\n";
            cout << "Type 'exit' to quit\n";
            cout << "Type 'help' for commands\n";
            while (true) {
                cout << "ebmgen> ";
                line.clear();
                cin >> line;
                auto res = run_line();
                if (res == ExecutionResult::Exit) {
                    break;
                }
            }
        }
    };

    void interactive_debugger(MappingTable& table) {
        Debugger debugger{futils::wrap::cout_wrap(), futils::wrap::cin_wrap(), table};
        debugger.start();
    }

    expected<std::pair<ObjectResult, Failures>> run_query(MappingTable& table, std::string_view input) {
        std::uint64_t id = 0;
        if (futils::number::prefix_integer(input, id)) {
            if (table.get_object(ebm::AnyRef{id}).index() == 0) {
                return unexpect_error("Identifier not found: {}", id);
            }
            return std::make_pair(ObjectResult{ebm::AnyRef{id}}, Failures{});
        }
        QueryCompiler compiler{table};
        Query query;
        auto parsed = query::parse_line(input);
        if (!parsed) {
            auto src_loc = parsed.error().first;
            return unexpect_error("Error parsing query expression at {}:{}:\n{}", src_loc.line + 1, src_loc.pos + 1, parsed.error().second);
        }
        auto result = compiler.compile(query, *parsed);
        if (!result) {
            return unexpect_error("Error in query expression: {}", result.error().error());
        }
        ObjectSet matched;
        ObjectResult matched_result;
        Failures failures;
        query.query(table, matched, failures, &matched_result);
        return std::make_pair(std::move(matched_result), std::move(failures));
    }
}  // namespace ebmgen
