/*license*/
#pragma once
#include "../node/builtin.h"
#include "../node/type.h"
#include <json/json_export.h>
#include <core/common/expected.h>
#include <json/iterator.h>
#include <core/lexer/lexer.h>

namespace brgen::ast::builtin {
    struct BuiltinError {
        std::string msg;
    };

    auto builtin_error(auto&&... msg) {
        return futils::helper::either::unexpected{BuiltinError{brgen::concat(msg...)}};
    }

    expected<std::shared_ptr<Type>, BuiltinError> parse_type(std::string_view typ, bool allow_optional) {
        if (typ.starts_with("*")) {
            if (!allow_optional) {
                return builtin_error("cannot parse type; optional type is not allowed");
            }
            auto t = parse_type(typ.substr(1), false);
            if (!t) {
                return t;
            }
            return std::make_shared<OptionalType>(lexer::Loc{.file = lexer::builtin}, std::move(*t));
        }
        if (auto t = ast::is_int_type(typ)) {
            return std::make_shared<IntType>(lexer::Loc{.file = lexer::builtin}, t->bit_size, t->endian, t->is_signed);
        }
        if (typ == "bool") {
            return std::make_shared<BoolType>(lexer::Loc{.file = lexer::builtin});
        }
        if (typ == "void") {
            return std::make_shared<VoidType>(lexer::Loc{.file = lexer::builtin});
        }
        if (typ == "type") {
            return std::make_shared<MetaType>(lexer::Loc{.file = lexer::builtin});
        }
        if (typ.starts_with("[]")) {
            auto t = parse_type(typ.substr(2), false);
            if (!t) {
                return t;
            }
            return std::make_shared<ArrayType>(lexer::Loc{.file = lexer::builtin}, nullptr, lexer::Loc{.file = lexer::builtin}, std::move(*t));
        }
        auto seq = futils::make_ref_seq(typ);
        auto token = lexer::parse_one(seq, lexer::builtin);
        if (!seq.eos()) {
            return builtin_error("cannot parse type; characters that cannot be usable for identifiers are found");
        }
        if (!token) {
            return builtin_error("cannot parse type; maybe empty string?");
        }
        if (token->tag != lexer::Tag::ident) {
            return builtin_error("expect type name, got ", token->token);
        }
        auto name = token->token;
        auto ident = std::make_shared<Ident>(lexer::Loc{.file = lexer::builtin}, std::move(name));
        auto ident_type = std::make_shared<IdentType>(lexer::Loc{.file = lexer::builtin}, std::move(ident));
        return ident_type;
    }

    // TODO(on-keyday): arithmetic operation?
    expected<std::shared_ptr<Expr>, BuiltinError> parse_simple_expr(std::string_view p) {
        auto seq = futils::make_ref_seq(p);
        auto token = lexer::parse_one(seq, lexer::builtin);
        if (!seq.eos()) {
            return builtin_error("currently only support primitive value");
        }
        if (token->tag == lexer::Tag::bool_literal) {
            return std::make_shared<BoolLiteral>(lexer::Loc{.file = lexer::builtin}, token->token == "true");
        }
        if (token->tag == lexer::Tag::int_literal) {
            return std::make_shared<IntLiteral>(lexer::Loc{.file = lexer::builtin}, std::move(token->token));
        }
        if (token->tag == lexer::Tag::str_literal) {
            return std::make_shared<StringLiteral>(lexer::Loc{.file = lexer::builtin}, std::move(token->token));
        }
        if (token->token == "input" || token->token == "output" || token->token == "config") {
        }
        return builtin_error("currently only support primitive value");
    }

    expected<std::shared_ptr<BuiltinMember>, BuiltinError> parse(const futils::json::JSON& js) {
        if (!js.is_object()) {
            to_string(js.kind());
            return builtin_error("expected object, got ", to_string(js.kind()));
        }
        auto t = js.at("kind");
        if (!t) {
            return builtin_error("expect type field which is a string of either 'object', 'field', or 'function'");
        }
        if (!t->is_string()) {
            return builtin_error("expect type field which is a string of either 'object', 'field', or 'function'");
        }
        auto kind = t->force_as_string<std::string>();
        if (kind == "object") {
            auto m = js.at("members");
            if (!m) {
                return builtin_error("expect members field which is an array of either 'object', 'field', or 'function'");
            }
            if (!m->is_object()) {
                return builtin_error("expect members field which is an array of either 'object', 'field', or 'function'");
            }
            auto obj = std::make_shared<BuiltinObject>();
            obj->struct_type = std::make_shared<StructType>(lexer::Loc{.file = lexer::builtin});
            obj->struct_type->base = obj;
            for (auto& elem : as_object(*m)) {
                auto r = parse(elem.second);
                if (!r) {
                    return r;
                }
                (*r)->belong = obj;
                (*r)->belong_struct = obj->struct_type;
                auto ident = std::make_shared<Ident>(lexer::Loc{.file = lexer::builtin}, elem.first);
                (*r)->ident = ident;
                (*r)->ident->usage = IdentUsage::define_field;
                (*r)->ident->base = *r;
                obj->struct_type->fields.push_back(*r);
                obj->members.push_back(std::move(*r));
            }
            return obj;
        }
        else if (kind == "function") {
            auto ret = js.at("return");
            if (!ret) {
                return builtin_error("expect return field which is a string of type");
            }
            if (!ret->is_string()) {
                return builtin_error("expect return field which is a string of type");
            }
            auto func = std::make_shared<BuiltinFunction>();
            auto r = parse_type(ret->force_as_string<std::string>(), false);
            if (!r) {
                return unexpect(r.error());
            }
            func->func_type = std::make_shared<FunctionType>(lexer::Loc{.file = lexer::builtin});
            func->func_type->return_type = std::move(*r);
            auto args = js.at("args");
            if (!args) {
                return builtin_error("expect args field which is an array of type");
            }
            if (!args->is_array()) {
                return builtin_error("expect args field which is an array of type");
            }
            for (auto& elem : as_array(*args)) {
                if (!elem.is_string()) {
                    return builtin_error("expect args field which is an array of type (string)");
                }
                auto r = parse_type(elem.force_as_string<std::string>(), true);
                if (!r) {
                    return unexpect(r.error());
                }
                func->func_type->parameters.push_back(std::move(*r));
            }
            return func;
        }
        else if (kind == "field") {
            auto ret = js.at("type");
            if (!ret) {
                return builtin_error("expect type field which is a string of type");
            }
            if (!ret->is_string()) {
                return builtin_error("expect type field which is a string of type");
            }
            auto field = std::make_shared<BuiltinField>();
            auto r = parse_type(ret->force_as_string<std::string>(), false);
            if (!r) {
                return unexpect(r.error());
            }
            field->field_type = std::move(*r);
            auto def = js.at("default");
            if (def) {
                auto r = parse_simple_expr(def->force_as_string<std::string>());
                if (!r) {
                    return unexpect(r.error());
                }
                field->default_value = std::move(*r);
            }
            return field;
        }
        else {
            return builtin_error("expect type field which is a string of either 'object', 'field', or 'function'");
        }
    }

    constexpr auto builtin_json = R"(
{
    "kind": "object",
    "members": {
        "input": {
            "offset": {
                "kind": "field",
                "type": "u64"
            },
            "remain": {
                "kind": "field",
                "type": "u64"
            },
            "get": {
                "kind": "function",
                "return": "u8",
                "args": [
                    "*type",
                    "*u64"
                ]
            },
            "peek": {
                "kind": "function",
                "return": "type",
                "args": [
                    "*type",
                    "*u64"
                ]
            },
        },
        "output": {
            "put": {
                "kind": "function",
                "return": "void",
                "args": [
                    "*type",
                    "*u8"
                ]
            }
        },
        "config": {
            "endian": {
                "kind": "field",
                "type": "endian"
            },
        }
    }
}
    )";
}  // namespace brgen::ast::builtin
