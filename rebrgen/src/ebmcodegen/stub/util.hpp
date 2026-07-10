/*license*/
#pragma once
#include <optional>
#include <string>
#include <ebm/extended_binary_module.hpp>
#include <ebmgen/common.hpp>
#include <string_view>
#include <type_traits>
#include "ebmgen/access.hpp"
#include "helper/template_instance.h"
#include "ebmgen/mapping.hpp"
#include <unordered_set>
#include <vector>
#include "core/ast/line_map.h"
#include "code_writer.hpp"
#include "context.hpp"
#include "url.hpp"
#include "helper/scoped.h"  // temporary added here

namespace ebmcodegen::util {

    template <class T>
    concept has_to_writer = requires(T t) {
        { t.to_writer() };
    };

    template <class CodeWriter>
    auto code_write(auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_write. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_write. Please convert them to string");
        static_assert(!(... || has_to_writer<decltype(args)>), "Result type is not supported in code_write. Please convert using to_writer()");
        CodeWriter w;
        w.write(std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_write(ebm::AnyRef loc, auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_write. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_write. Please convert them to string");
        CodeWriter w;
        w.write_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_writeln(auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_writeln. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_writeln. Please convert them to string");
        CodeWriter w;
        w.writeln(std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_writeln(ebm::AnyRef loc, auto&&... args) {
        static_assert(!(... || futils::helper::is_template_instance_of<std::decay_t<decltype(args)>, futils::helper::either::expected>), "Unexpected expected<> in code_writeln. Please unwrap it first.");
        static_assert(!(... || std::is_integral_v<std::decay_t<decltype(args)>>), "Integral types are not supported in code_writeln. Please convert them to string");
        CodeWriter w;
        w.writeln_with_loc(loc, std::forward<decltype(args)>(args)...);
        return w;
    }

    template <class CodeWriter>
    auto code_joint_write(auto&& joint, auto&& vec) {
        CodeWriter w;
        bool first = true;
        using T = std::decay_t<decltype(vec)>;
        for (auto&& v : vec) {
            if (!first) {
                w.write(joint);
            }
            w.write(v);
            first = false;
        }
        return w;
    }

    template <class CodeWriter>
    auto code_joint_write(auto&& joint, size_t count, auto&& func) {
        CodeWriter w;
        bool first = true;
        for (size_t i = 0; i < count; ++i) {
            if (!first) {
                w.write(joint);
            }
            if constexpr (std::is_invocable_v<decltype(func), size_t>) {
                w.write(func(i));
            }
            else {
                w.write(func);
            }
            first = false;
        }
        return w;
    }

    template <class CodeWriter>
    auto code_joint_write(ebm::AnyRef loc, auto&&... vec) {
        CodeWriter w;
        auto loc_scope = w.with_loc_scope(loc);
        w.write(code_joint_write<CodeWriter>(std::forward<decltype(vec)>(vec)...));
        return w;
    }

    // Join fallible per-element writers with a separator: fn(elem) returns an
    // expected<> (Result or CodeWriter); the first failing element aborts the
    // whole join. Use this instead of a hand-rolled `bool first` loop when the
    // element rendering can fail (e.g. ctx.visit); the infallible variant is
    // code_joint_write/SEPARATED above.
    template <class CodeWriter>
    ebmgen::expected<CodeWriter> try_joint_write(auto&& joint, auto&& container, auto&& fn) {
        CodeWriter w;
        bool first = true;
        for (auto&& elem : container) {
            MAYBE(part, fn(elem));
            if (!first) {
                w.write(joint);
            }
            first = false;
            if constexpr (has_to_writer<decltype(part)>) {
                w.write(part.to_writer());
            }
            else {
                w.write(std::move(part));
            }
        }
        return w;
    }

#define CODE(...) (code_write<CodeWriter>(__VA_ARGS__))
#define CODELINE(...) (code_writeln<CodeWriter>(__VA_ARGS__))

#define SEPARATED(...) (code_joint_write<CodeWriter>(__VA_ARGS__))
#define TRY_SEPARATED(...) (try_joint_write<CodeWriter>(__VA_ARGS__))

    // remove top level brace
    inline std::string tidy_condition_brace(std::string&& brace) {
        if (brace.starts_with("(") && brace.ends_with(")")) {
            // check it is actually a brace
            // for example, we may encounter (1 + 1) + (2 + 2)
            // so ensure that brace is actually (<expr>)
            size_t level = 0;
            for (size_t i = 0; i < brace.size(); ++i) {
                if (brace[i] == '(') {
                    ++level;
                }
                else if (brace[i] == ')') {
                    --level;
                }
                if (level == 0) {
                    if (i == brace.size() - 1) {
                        // the last char is the matching ')'
                        brace = brace.substr(1, brace.size() - 2);
                        return std::move(brace);
                    }
                    break;  // not a top-level brace
                }
            }
        }
        return std::move(brace);
    }

    inline std::string tidy_condition_brace(const std::string& brace) {
        return tidy_condition_brace(std::string(brace));
    }

    ebmgen::expected<CodeWriter> get_size_str(auto&& ctx, const ebm::Size& s) {
        if (auto size = s.size()) {
            return CODE(std::format("{}", size->value()));
        }
        else if (auto ref = s.ref()) {
            MAYBE(expr, visit_Expression(ctx, *ref));
            return expr.to_writer();
        }
        return ebmgen::unexpect_error("unsupported size: {}", to_string(s.unit));
    }

    ebmgen::expected<size_t> get_type_size_bit(auto&& ctx, ebm::TypeRef type_ref) {
        ebmgen::MappingTable& m = get_visitor(ctx).module_;
        MAYBE(type, m.get_type(type_ref));
        if (auto size = type.body.size()) {
            return size->value();
        }
        if (auto element_type_ref = type.body.element_type()) {
            if (auto length = type.body.length()) {
                MAYBE(element_size, get_type_size_bit(ctx, *element_type_ref));
                return element_size * length->value();
            }
        }
        if (auto base_type_ref = type.body.base_type()) {
            MAYBE(base_size, get_type_size_bit(ctx, *base_type_ref));
            return base_size;
        }
        if (auto id = type.body.id()) {
            MAYBE(stmt, m.get_statement(*id));
            if (auto struct_decl = stmt.body.struct_decl()) {
                if (auto size = struct_decl->size()) {
                    if (size->unit == ebm::SizeUnit::BIT_FIXED) {
                        MAYBE(val, size->size());
                        return val.value();
                    }
                    if (size->unit == ebm::SizeUnit::BYTE_FIXED) {
                        MAYBE(val, size->size());
                        return val.value() * 8;
                    }
                }
            }
        }
        return ebmgen::unexpect_error("cannot determine size of type");
    }

    // do_div(x,x_scale,y) returns (x*x_scale)/y
    ebmgen::expected<CodeWriter> get_element_count(auto&& ctx, ebm::TypeRef array_type, const ebm::Size& s, auto&& do_div) {
        if (s.unit == ebm::SizeUnit::DYNAMIC || s.unit == ebm::SizeUnit::UNKNOWN) {
            return ebmgen::unexpect_error("Dynamic or unknown size is not supported for getting element size");
        }
        if (s.unit == ebm::SizeUnit::ELEMENT_FIXED || s.unit == ebm::SizeUnit::ELEMENT_DYNAMIC) {
            return get_size_str(ctx, s);
        }
        MAYBE(size_str, get_size_str(ctx, s));
        ebmgen::MappingTable& m = get_visitor(ctx).module_;
        auto element_type = ebmgen::access_field<"element_type">(m, array_type);
        if (!element_type) {
            return ebmgen::unexpect_error("array type does not have element type");
        }
        MAYBE(element_size_bit, get_type_size_bit(ctx, *element_type));
        if (element_size_bit == 0) {
            return ebmgen::unexpect_error("element size cannot be determined or is zero, maybe bug");
        }
        if (s.unit == ebm::SizeUnit::BIT_FIXED || s.unit == ebm::SizeUnit::BIT_DYNAMIC) {
            return do_div(size_str, 1, CODE(std::format("{}", element_size_bit)));
        }
        if (s.unit == ebm::SizeUnit::BYTE_FIXED || s.unit == ebm::SizeUnit::BYTE_DYNAMIC) {
            if (element_size_bit % 8 != 0) {  // for case [4]u4 /* 2byte*/
                return do_div(size_str, 8, CODE(std::format("{}", element_size_bit)));
            }
            if (element_size_bit == 8) {
                return size_str;
            }
            return do_div(size_str, 1, CODE(std::format("{}", element_size_bit / 8)));
        }
        return ebmgen::unexpect_error("unsupported size unit for getting element size: {}", to_string(s.unit));
    }

    ebmgen::expected<CodeWriter> get_element_count_default(auto&& ctx, ebm::TypeRef array_type, const ebm::Size& s, const char* mul = "*", const char* div = "/") {
        return get_element_count(ctx, array_type, s, [&](auto&& x, size_t scale, auto&& y) {
            if (scale != 1) {
                return CODE("((", x, mul, std::to_string(scale), ")", div, y, ")");
            }
            return CODE("(", x, div, y, ")");
        });
    }

    auto as_IDENTIFIER(auto&& visitor, ebm::StatementRef stmt) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::IDENTIFIER;
        expr.body.id(to_weak(stmt));
        return visit_Expression(visitor, expr);
    }

    auto as_DEFAULT_VALUE(auto&& visitor, ebm::TypeRef typ) {
        ebm::Expression expr;
        expr.body.kind = ebm::ExpressionKind::DEFAULT_VALUE;
        expr.body.type = typ;
        return visit_Expression(visitor, expr);
    }

    // 0: most outer last index: most inner
    ebmgen::expected<std::vector<const ebm::Type*>> get_type_tree(auto&& ctx, ebm::TypeRef type_ref) {
        ebmgen::MappingTable& m = get_visitor(ctx).module_;
        MAYBE(type, m.get_type(type_ref));
        std::vector<const ebm::Type*> result;
        result.push_back(&type);
        const ebm::Type* current_type = &type;
        while (true) {
            switch (current_type->body.kind) {
                case ebm::TypeKind::PTR: {
                    MAYBE(pointee_type_ref, current_type->body.pointee_type());
                    MAYBE(pointee_type, m.get_type(pointee_type_ref));
                    result.push_back(&pointee_type);
                    current_type = &pointee_type;
                    break;
                }
                case ebm::TypeKind::OPTIONAL: {
                    MAYBE(inner_type_ref, current_type->body.inner_type());
                    MAYBE(inner_type, m.get_type(inner_type_ref));
                    result.push_back(&inner_type);
                    current_type = &inner_type;
                    break;
                }
                case ebm::TypeKind::ARRAY:
                case ebm::TypeKind::VECTOR: {
                    MAYBE(element_type_ref, current_type->body.element_type());
                    MAYBE(element_type, m.get_type(element_type_ref));
                    result.push_back(&element_type);
                    current_type = &element_type;
                    break;
                }
                case ebm::TypeKind::RANGE: {
                    MAYBE(value_type_ref, current_type->body.base_type());
                    MAYBE(value_type, m.get_type(value_type_ref));
                    result.push_back(&value_type);
                    current_type = &value_type;
                    break;
                }
                case ebm::TypeKind::ENUM: {
                    auto base_type_ref = *current_type->body.base_type();
                    if (is_nil(base_type_ref)) {
                        return result;
                    }
                    else {
                        MAYBE(base_type, m.get_type(base_type_ref));
                        result.push_back(&base_type);
                        current_type = &base_type;
                        break;
                    }
                }
                default: {
                    return result;
                }
            }
        }
        return result;
    }

    struct DefaultValueOption {
        std::string_view object_init = "{}";
        std::string_view vector_init = "[]";
        std::string_view bytes_init;
        std::string_view pointer_init = "nullptr";
        std::string_view optional_init = "std::nullopt";
        std::string_view encoder_return_init = "{}";
        std::string_view decoder_return_init = "{}";
    };

    enum class BytesType {
        both,
        array,
        vector,
    };

    // ADR 0039: path of the has_absolute_offset flag on a coder input type.
    constexpr auto has_absolute_offset_type = "io_input_desc.has_absolute_offset";

    // ADR 0039: true when the io stream's input type is flagged has_absolute_offset,
    // i.e. lower_runtime_state threaded a RuntimeState companion through the
    // enclosing function (parameter/local name is always `runtime_state`).
    // io_ref may be the coder input parameter or a subrange io variable.
    // The offset *increment* emission stays per-backend (ADR 0008/0039).
    constexpr bool has_absolute_offset(auto&& ctx, ebm::StatementRef io_ref) {
        auto typ = ctx.template get_field<"param_decl.param_type">(io_ref);
        if (!typ) {
            typ = ctx.template get_field<"var_decl.var_type">(io_ref);
        }
        return ctx.template get_field<has_absolute_offset_type>(typ) == true;
    }

    std::optional<BytesType> is_bytes_type(auto&& visitor, ebm::TypeRef type_ref, BytesType candidate = BytesType::both) {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        auto type = module_.get_type(type_ref);
        if (!type) {
            return std::nullopt;
        }
        std::optional<BytesType> is_candidate;
        if (candidate == BytesType::both || candidate == BytesType::vector) {
            if (type->body.kind == ebm::TypeKind::VECTOR) {
                is_candidate = BytesType::vector;
            }
        }
        if (candidate == BytesType::both || candidate == BytesType::array) {  // for backward compatibility
            if (type->body.kind == ebm::TypeKind::ARRAY) {
                is_candidate = BytesType::array;
            }
        }
        if (is_candidate) {
            auto elem_type_ref = type->body.element_type();
            if (!elem_type_ref) {
                return std::nullopt;
            }
            auto elem_type = module_.get_type(*elem_type_ref);
            if (elem_type && elem_type->body.kind == ebm::TypeKind::UINT && elem_type->body.size() && elem_type->body.size()->value() == 8) {
                return is_candidate;
            }
        }
        return std::nullopt;
    }

    ebmgen::expected<std::string> get_default_value(auto&& visitor, ebm::TypeRef ref, const DefaultValueOption& option = {}) {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(ref));
        switch (type.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::USIZE:  // e.g. RuntimeState.offset (ADR 0039)
            case ebm::TypeKind::FLOAT: {
                ebm::Expression int_literal;
                int_literal.body.kind = ebm::ExpressionKind::LITERAL_INT;
                int_literal.body.int_value(*ebmgen::varint(0));
                MAYBE(val, visit_Expression(visitor, int_literal));
                return val.to_string();
            }
            case ebm::TypeKind::BOOL: {
                ebm::Expression bool_literal;
                bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
                bool_literal.body.bool_value(0);
                MAYBE(val, visit_Expression(visitor, bool_literal));
                return val.to_string();
            }
            case ebm::TypeKind::ENUM:
            case ebm::TypeKind::STRUCT:
            case ebm::TypeKind::RECURSIVE_STRUCT: {
                MAYBE(ident, visit_Type(visitor, ref));
                return std::format("{}{}", ident.to_string(), option.object_init);
            }
            case ebm::TypeKind::ARRAY:
            case ebm::TypeKind::VECTOR: {
                MAYBE(elem_type_ref, type.body.element_type());
                MAYBE(elem_type, module_.get_type(elem_type_ref));
                if (elem_type.body.kind == ebm::TypeKind::UINT && elem_type.body.size() && elem_type.body.size()->value() == 8) {
                    if (option.bytes_init.size()) {
                        return std::format("{}", option.bytes_init);
                    }
                    // use default vector init
                }
                return std::format("{}", option.vector_init);
            }
            case ebm::TypeKind::PTR: {
                return std::string(option.pointer_init);
            }
            case ebm::TypeKind::OPTIONAL: {
                return std::string(option.optional_init);
            }
            case ebm::TypeKind::ENCODER_RETURN: {
                return std::string(option.encoder_return_init);
            }
            case ebm::TypeKind::DECODER_RETURN: {
                return std::string(option.decoder_return_init);
            }
            default: {
                return ebmgen::unexpect_error("unsupported default: {}", to_string(type.body.kind));
            }
        }
    }

    ebmgen::expected<std::string> get_bool_literal(auto&& visitor, bool v) {
        ebm::Expression bool_literal;
        bool_literal.body.kind = ebm::ExpressionKind::LITERAL_BOOL;
        bool_literal.body.bool_value(v ? 1 : 0);
        MAYBE(expr, visit_Expression(visitor, bool_literal));
        return expr.to_string();
    }

    std::string join(auto&& joint, auto&& vec) {
        std::string result;
        bool first = true;
        for (auto&& v : vec) {
            if (!first) {
                result += joint;
            }
            result += v;
            first = false;
        }
        return result;
    }

    enum class LayerState {
        root,
        as_type,
        as_expr,
    };

    ebmgen::expected<std::vector<std::pair<ebm::StatementKind, std::string>>> get_identifier_layer(auto&& ctx, ebm::StatementRef stmt, LayerState state = LayerState::root) {
        auto& visitor = get_visitor(ctx);
        MAYBE(statement, visitor.module_.get_statement(stmt));
        if (state == LayerState::root) {
            if (statement.body.kind == ebm::StatementKind::STRUCT_DECL ||
                statement.body.kind == ebm::StatementKind::ENUM_DECL) {
                state = LayerState::as_type;
            }
            else {
                state = LayerState::as_expr;
            }
        }
        auto ident = visitor.module_.get_associated_identifier(stmt);
        std::vector<std::pair<ebm::StatementKind, std::string>> layers;
        if (const ebm::StructDecl* decl = statement.body.struct_decl()) {
            if (auto related_varint = decl->related_variant()) {
                MAYBE(type, visitor.module_.get_type(*related_varint));
                MAYBE(desc, type.body.struct_union_desc());
                MAYBE(upper_layers, get_identifier_layer(visitor, from_weak(desc.related_field), state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
                if (state == LayerState::as_expr) {
                    return layers;
                }
            }
        }
        if (const ebm::FieldDecl* field = statement.body.field_decl()) {
            if (!is_nil(field->parent_struct)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, from_weak(field->parent_struct), state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        if (const ebm::PropertyDecl* prop = statement.body.property_decl()) {
            if (!is_nil(prop->parent_format)) {
                MAYBE(upper_layers, get_identifier_layer(visitor, from_weak(prop->parent_format), state));
                layers.insert(layers.end(), upper_layers.begin(), upper_layers.end());
            }
            if (state == LayerState::as_type) {
                return layers;
            }
        }
        layers.emplace_back(statement.body.kind, ident);
        return layers;
    }

    ebmgen::expected<std::string> get_identifier_layer_str(auto&& ctx, ebm::StatementRef stmt, std::string_view joint = "::", LayerState state = LayerState::root) {
        MAYBE(layers, get_identifier_layer(ctx, stmt, state));
        std::vector<std::string> layer_strs;
        for (auto& [kind, name] : layers) {
            layer_strs.push_back(name);
        }
        return join(joint, layer_strs);
    }

    auto get_struct_union_members(auto&& visitor, ebm::TypeRef variant) -> ebmgen::expected<std::vector<ebm::StatementRef>> {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(variant));
        std::vector<ebm::StatementRef> result;
        if (auto struct_union_desc = type.body.struct_union_desc()) {
            auto& members = struct_union_desc->variant_desc.members;
            for (auto& mem : members.container) {
                MAYBE(member_type, module_.get_type(mem));
                MAYBE(stmt_id, member_type.body.id());
                result.push_back(stmt_id.id);
            }
        }
        return result;
    }

    auto struct_union_members(auto&& visitor, ebm::TypeRef variant) -> ebmgen::expected<std::vector<std::pair<ebm::StatementRef, std::decay_t<decltype(*visit_Statement(visitor, ebm::StatementRef{}))>>>> {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(variant));
        std::vector<std::pair<ebm::StatementRef, std::decay_t<decltype(*visit_Statement(visitor, ebm::StatementRef{}))>>> result;
        if (auto struct_union_desc = type.body.struct_union_desc()) {
            auto& members = struct_union_desc->variant_desc.members;
            for (auto& mem : members.container) {
                MAYBE(member_type, module_.get_type(mem));
                MAYBE(stmt_id, member_type.body.id());
                MAYBE(stmt_str, visit_Statement(visitor, from_weak(stmt_id)));
                result.push_back({stmt_id.id, std::move(stmt_str)});
            }
        }
        return result;
    }

    void convert_location_info(auto&& ctx, auto&& loc_writer) {
        auto& visitor = get_visitor(ctx);
        auto& m = visitor.module_;
        for (auto& loc : loc_writer.locs_data()) {
            const ebm::Loc* l = m.get_debug_loc(loc.loc);
            if (!l) {
                continue;
            }
            visitor.output.line_maps.push_back(brgen::ast::LineMap{

                .loc = brgen::lexer::Loc{
                    .pos = {
                        .begin = static_cast<size_t>(l->start.value()),
                        .end = static_cast<size_t>(l->end.value()),
                    },
                    .file = static_cast<size_t>(l->file_id.value()),
                    .line = static_cast<size_t>(l->line.value()),
                    .col = static_cast<size_t>(l->column.value()),
                },
                .line = loc.start.line,
            });
        }
    }

    bool is_u8(auto&& visitor, ebm::TypeRef type_ref) {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        if (auto type = module_.get_type(type_ref)) {
            if (type->body.kind == ebm::TypeKind::UINT) {
                if (auto size = type->body.size()) {
                    if (size->value() == 8) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    ebmgen::expected<void> handle_fields(auto&& ctx, const ebm::Block& fields, bool recurse_composite, auto&& callback) {
        auto& visitor = get_visitor(ctx);
        for (auto& field_ref : fields.container) {
            MAYBE(field, visitor.module_.get_statement(field_ref));
            if (auto comp = field.body.composite_field_decl()) {
                if (recurse_composite) {
                    MAYBE_VOID(res, handle_fields(ctx, comp->fields, recurse_composite, callback));
                    continue;
                }
            }
            auto result = callback(field_ref, field);
            if (!result) {
                return ebmgen::unexpect_error(std::move(result.error()));
            }
        }
        return {};
    }

    ebmgen::expected<std::pair<std::string, std::string>> first_enum_name(auto&& visitor, ebm::TypeRef enum_type) {
        const ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(enum_type));
        if (auto enum_id = type.body.id()) {
            MAYBE(enum_decl, module_.get_statement(*enum_id));
            if (auto enum_ = enum_decl.body.enum_decl()) {
                return std::make_pair(module_.get_associated_identifier(*enum_id), module_.get_associated_identifier(enum_->members.container[0]));
            }
        }
        return ebmgen::unexpect_error("enum type has no members");
    }

    void modify_keyword_identifier(ebm::ExtendedBinaryModule& m, const std::unordered_set<std::string_view>& keyword_list, auto&& change_rule) {
        for (auto& ident : m.identifiers) {
            if (keyword_list.contains(ident.body.data)) {
                ident.body.data = change_rule(ident.body.data);
            }
        }
    }

    ebmgen::expected<size_t> get_struct_union_index(auto&& visitor, ebm::TypeRef variant_type, ebm::TypeRef candidate_type) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(type, module_.get_type(variant_type));
        if (type.body.kind != ebm::TypeKind::STRUCT_UNION) {
            return ebmgen::unexpect_error("not STRUCT_UNION variant type");
        }
        MAYBE(desc, type.body.struct_union_desc());
        for (size_t i = 0; i < desc.variant_desc.members.container.size(); ++i) {
            if (get_id(desc.variant_desc.members.container[i]) == get_id(candidate_type)) {
                return i;
            }
        }
        return ebmgen::unexpect_error("type not found in variant members");
    }

    ebmgen::expected<ebm::TypeRef> get_struct_union_member_from_field(auto&& visitor, ebm::StatementRef field_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(field_stmt, module_.get_statement(field_ref));
        ebm::WeakStatementRef parent_struct_ref;
        if (auto field_decl = field_stmt.body.field_decl()) {
            parent_struct_ref = field_decl->parent_struct;
        }
        else if (auto prop_decl = field_stmt.body.property_decl()) {
            parent_struct_ref = prop_decl->parent_format;
        }
        else {
            return ebmgen::unexpect_error("not field or property");
        }
        MAYBE(parent_struct, module_.get_statement(parent_struct_ref));
        MAYBE(struct_decl, parent_struct.body.struct_decl());
        MAYBE(rel_var, struct_decl.related_variant());
        MAYBE(type, module_.get_type(rel_var));
        if (type.body.kind != ebm::TypeKind::STRUCT_UNION) {
            return ebmgen::unexpect_error("not STRUCT_UNION variant type");
        }
        MAYBE(desc, type.body.struct_union_desc());
        for (auto& member_type_ref : desc.variant_desc.members.container) {
            MAYBE(member_type, module_.get_type(member_type_ref));
            MAYBE(member_stmt_id, member_type.body.id());
            if (get_id(member_stmt_id) == get_id(parent_struct_ref)) {
                return member_type_ref;
            }
        }
        return ebmgen::unexpect_error("type not found in variant members");
    }

    ebmgen::expected<ebm::StatementRef> parent_struct_to_parent_format(auto&& visitor, ebm::StatementRef struct_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(struct_stmt, module_.get_statement(struct_ref));
        auto p = struct_stmt.body.struct_decl();
        ebm::StatementRef stmt_ref = struct_ref;
        while (p) {
            auto rel = p->related_variant();
            if (!rel) {
                return stmt_ref;
            }
            MAYBE(variant_type, module_.get_type(*rel));
            MAYBE(variant_desc, variant_type.body.struct_union_desc());
            MAYBE(parent_field_stmt, module_.get_statement(variant_desc.related_field));
            MAYBE(field_decl, parent_field_stmt.body.field_decl());
            stmt_ref = from_weak(field_decl.parent_struct);
            MAYBE(parent_struct_stmt, module_.get_statement(stmt_ref));
            p = parent_struct_stmt.body.struct_decl();
        }
        return ebmgen::unexpect_error("cannot find parent format");
    }

    // Line-1 banner for generated output files:
    // `<prefix> Code generated by <tool> at <repo_url>, DO NOT EDIT.<suffix>`
    // The tool name comes from the visitor's program_name. Comment syntax is
    // passed explicitly (not metadata_comment_prefix) because several backends
    // use a different comment style for the banner than for metadata comments.
    template <class CodeWriter>
    void write_generated_banner(auto&& ctx, CodeWriter& w, std::string_view comment_prefix = "//", std::string_view comment_suffix = "") {
        w.writeln(comment_prefix, " Code generated by ", get_visitor(ctx).program_name, " at ", repo_url, ", DO NOT EDIT.", comment_suffix);
    }

    struct FuncParamNames {
        std::vector<std::string> params;        // all parameter identifiers, in declaration order
        std::vector<std::string> state_params;  // subset of params with is_state_variable()
        std::string args;                       // params joined with ", "
        std::string state_args;                 // state_params joined with ", "
    };

    ebmgen::expected<FuncParamNames> collect_func_param_names(auto&& visitor, const ebm::FunctionDecl& func) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        FuncParamNames names;
        for (auto& param : func.params.container) {
            auto param_name = module_.get_associated_identifier(param);
            if (!names.args.empty()) {
                names.args += ", ";
            }
            names.args += param_name;
            names.params.push_back(param_name);
            MAYBE(param_stmt, module_.get_statement(param));
            if (auto pd = param_stmt.body.param_decl(); pd && pd->is_state_variable()) {
                if (!names.state_args.empty()) {
                    names.state_args += ", ";
                }
                names.state_args += param_name;
                names.state_params.push_back(param_name);
            }
        }
        return names;
    }

    // parent_struct != parent_format: the property lives on an anon inner
    // variant-arm struct while its accessor is emitted onto the outer format
    // (see collect_anon_inner_descendants / emit_anon_inner_properties).
    inline bool is_inner_anon_property(const ebm::PropertyDecl& prop) {
        return get_id(prop.parent_struct) != get_id(prop.parent_format);
    }

    struct PropertyAccessInfo {
        ebm::StatementRef member_stmt;              // the PROPERTY_DECL statement the member expression refers to
        const ebm::PropertyDecl* prop = nullptr;
        ebm::StatementRef getter_ref;               // nil when the property has no getter function
        const ebm::FunctionDecl* getter = nullptr;  // null when getter_ref is nil
        ebm::MergeMode merge_mode{};
        bool inner_anon = false;          // is_inner_anon_property: the accessor is hoisted onto
                                          // parent_format with a `<parent_struct_ident><sep><member_ident>` name
        std::string member_ident;         // identifier of the member expression
        std::string parent_struct_ident;  // identifier of prop->parent_struct
        std::string parent_format_ident;  // identifier of prop->parent_format
        FuncParamNames getter_params;     // empty when getter is null

        // Accessor method name after hoisting onto parent_format:
        // <parent_struct_ident><sep><member_ident>. Definition-side
        // counterpart: hoisted_accessor_inner_ident. Pass sep="" for
        // backends whose identifiers join without separator.
        std::string hoisted_method_name(std::string_view sep = "_") const {
            return parent_struct_ident + std::string(sep) + member_ident;
        }
    };

    // Resolve the member of a MEMBER_ACCESS expression to a property-getter call shape.
    // Returns std::nullopt when the member statement is not a PROPERTY_DECL, so callers
    // can fall through to their other branches or the default `base.member` emission.
    // With require_getter=true (default) a PROPERTY_DECL without getter function is an
    // error; pass false for generators that fall back to default emission in that case.
    ebmgen::expected<std::optional<PropertyAccessInfo>> analyze_property_member_access(auto&& visitor, ebm::ExpressionRef member, bool require_getter = true) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(member_expr, module_.get_expression(member));
        MAYBE(id, member_expr.body.id());
        MAYBE(member_stmt, module_.get_statement(from_weak(id)));
        if (member_stmt.body.kind != ebm::StatementKind::PROPERTY_DECL) {
            return std::nullopt;
        }
        MAYBE(prop, member_stmt.body.property_decl());
        PropertyAccessInfo info;
        info.member_stmt = from_weak(id);
        info.prop = &prop;
        info.merge_mode = prop.merge_mode;
        info.inner_anon = is_inner_anon_property(prop);
        MAYBE(member_ident, module_.get_associated_identifier(member));
        info.member_ident = member_ident;
        info.parent_struct_ident = module_.get_associated_identifier(prop.parent_struct);
        info.parent_format_ident = module_.get_associated_identifier(prop.parent_format);
        if (is_nil(prop.getter_function.id)) {
            if (require_getter) {
                return ebmgen::unexpect_error("PROPERTY_DECL member access without getter function");
            }
            return std::optional(std::move(info));
        }
        info.getter_ref = prop.getter_function.id;
        MAYBE(getter_stmt, module_.get_statement(info.getter_ref));
        MAYBE(getter_decl, getter_stmt.body.func_decl());
        info.getter = &getter_decl;
        MAYBE(getter_params, collect_func_param_names(visitor, getter_decl));
        info.getter_params = std::move(getter_params);
        return std::optional(std::move(info));
    }

    // Definition-side counterpart of PropertyAccessInfo::hoisted_method_name:
    // when fctx's FUNCTION_DECL is a property accessor hoisted out of an anon
    // inner variant-arm struct (its PropertyDecl's parent_struct differs from
    // the function's parent_format), return the inner struct identifier the
    // emitted method name must be prefixed with; nullopt otherwise.
    // Composition with other prefixes (set_, separators) stays with each backend.
    std::optional<std::string> hoisted_accessor_inner_ident(auto&& fctx) {
        if (auto prop_ref = fctx.func_decl.property()) {
            if (auto prop_decl = fctx.template get_field<"property_decl">(prop_ref->id)) {
                if (get_id(prop_decl->parent_struct) != get_id(fctx.func_decl.parent_format)) {
                    return std::string(fctx.identifier(prop_decl->parent_struct));
                }
            }
        }
        return std::nullopt;
    }

    bool variant_candidate_equal(auto&& visitor, ebm::TypeRef candidate, ebm::TypeRef target) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        if (get_id(candidate) == get_id(target)) {
            return true;
        }
        // if candidate is VARIANT, check its members
        auto maybe_candidate_type = module_.get_type(candidate);
        if (!maybe_candidate_type) {
            return false;
        }
        auto& candidate_type = *maybe_candidate_type;
        if (candidate_type.body.kind != ebm::TypeKind::VARIANT) {
            return false;
        }
        auto candidate_desc = candidate_type.body.variant_desc();
        if (!candidate_desc) {
            return false;
        }
        for (auto& member_ref : candidate_desc->members.container) {
            if (get_id(member_ref) == get_id(target)) {
                return true;
            }
        }
        return false;
    }

    struct UnwrapIndexResult {
        std::vector<ebm::ExpressionRef> index_layers;  // first element is the most outer, last element is the most inner
        std::vector<ebm::ExpressionRef> index;         // first element is the most outer, last element is the most inner
        ebm::ExpressionRef base_expression;
    };

    // first element is the most outer, last element is the most inner
    ebmgen::expected<UnwrapIndexResult> unwrap_index(auto&& visitor, ebm::ExpressionRef expr_ref) {
        UnwrapIndexResult result;
        result.base_expression = expr_ref;
        auto do_unwrap = [&](auto&& self, ebm::ExpressionRef ref) -> ebmgen::expected<void> {
            result.index_layers.push_back(ref);
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            MAYBE(expr, module_.get_expression(ref));
            if (expr.body.kind == ebm::ExpressionKind::INDEX_ACCESS) {
                MAYBE(base_expr_ref, expr.body.base());
                MAYBE(index, expr.body.index());
                result.index.push_back(index);
                return self(self, base_expr_ref);
            }
            else {
                return {};
            }
        };
        MAYBE_VOID(_, do_unwrap(do_unwrap, expr_ref));
        return result;
    }

    struct Metadata {
        const std::string_view name;
        const ebm::Metadata* const data;

        ebmgen::expected<std::string_view> get_string(auto&& visitor, size_t index) const {
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            if (index >= data->values.container.size()) {
                return ebmgen::unexpect_error("metadata {} has no value at index {}", name, index);
            }
            auto expr = module_.get_expression(data->values.container[index]);
            if (!expr) {
                return ebmgen::unexpect_error("metadata {} value expression not found", name);
            }
            auto str_value = expr->body.string_value();
            if (!str_value) {
                return ebmgen::unexpect_error("metadata {} value is not string literal", name);
            }
            MAYBE(string_lit, module_.get_string_literal(*str_value));
            return string_lit.body.data;
        }
    };

    enum class MetadataPriority {
        CommandFlag,  // get from visitor flags
        Metadata,     // get from metadata
    };

    struct MetadataSet {
        std::unordered_map<std::string_view, std::vector<Metadata>> items;

        const std::vector<Metadata>* get(std::string_view name) const {
            auto it = items.find(name);
            if (it != items.end()) {
                return &it->second;
            }
            return nullptr;
        }

        const Metadata* get_first(std::string_view name) const {
            auto it = items.find(name);
            if (it != items.end() && !it->second.empty()) {
                return &it->second[0];
            }
            return nullptr;
        }

        std::string_view get_first_string(auto&& visitor, std::string_view name, size_t index = 0) const {
            auto meta = get_first(name);
            if (!meta) {
                return {};
            }
            auto str = meta->get_string(visitor, index);
            if (!str) {
                return {};
            }
            return *str;
        }

        // ensure visitor is
        std::string_view get_flag_or_first_string(auto&& visitor, std::string_view name, MetadataPriority priority) const {
            if (priority == MetadataPriority::CommandFlag) {
                auto got = get_visitor(visitor).flags.get_config(name);
                if (!got.empty()) {
                    return got;
                }
                auto meta = get_first(name);
                if (!meta) {
                    return {};
                }
                auto str = meta->get_string(visitor, 0);
                if (str) {
                    return *str;
                }
                return {};
            }
            else {
                auto meta = get_first(name);
                if (!meta) {
                    return get_visitor(visitor).flags.get_config(name);
                }
                auto str = meta->get_string(visitor, 0);
                if (str) {
                    return *str;
                }
                return get_visitor(visitor).flags.get_config(name);
            }
        }

        ebmgen::expected<void> try_add_metadata(auto&& visitor, const ebm::Metadata* meta_decl) {
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            MAYBE(key_str, module_.get_identifier(meta_decl->name));
            items[key_str.body.data].push_back(Metadata{key_str.body.data, meta_decl});
            return {};
        }

        ebmgen::expected<void> try_add_metadata(auto&& visitor, ebm::StatementRef meta_ref) {
            ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
            MAYBE(meta_stmt, module_.get_statement(meta_ref));
            auto meta_decl = meta_stmt.body.metadata();
            if (!meta_decl) {
                return {};
            }
            MAYBE(key_str, module_.get_identifier(meta_decl->name));
            items[key_str.body.data].push_back(Metadata{key_str.body.data, meta_decl});
            return {};
        }
    };

    ebmgen::expected<MetadataSet> get_metadata(auto&& visitor, ebm::StatementRef stmt_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(statement, module_.get_statement(stmt_ref));
        MAYBE(block, statement.body.block());
        MetadataSet result;
        for (auto& meta_ref : block.container) {
            MAYBE_VOID(_, result.try_add_metadata(visitor, meta_ref));
        }
        return result;
    }

    ebmgen::expected<MetadataSet> get_global_metadata(auto&& visitor) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        MAYBE(entry_point, module_.get_entry_point());
        return get_metadata(visitor, entry_point.id);
    }

    constexpr bool is_composite_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::COMPOSITE_GETTER:
            case ebm::FunctionKind::COMPOSITE_SETTER:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_property_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::PROPERTY_GETTER:
            case ebm::FunctionKind::PROPERTY_SETTER:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_setter_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::PROPERTY_SETTER:
            case ebm::FunctionKind::COMPOSITE_SETTER:
            case ebm::FunctionKind::VECTOR_SETTER:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_getter_func(ebm::FunctionKind kind) {
        switch (kind) {
            case ebm::FunctionKind::PROPERTY_GETTER:
            case ebm::FunctionKind::COMPOSITE_GETTER:
                return true;
            default:
                return false;
        }
    }

    struct IfThen {
        std::optional<ebm::ExpressionRef> condition;
        ebm::StatementRef then_branch;
    };

    ebmgen::expected<std::vector<IfThen>> flatten_if_then(auto&& visitor, ebm::StatementRef stmt_ref) {
        ebmgen::MappingTable& module_ = get_visitor(visitor).module_;
        std::vector<IfThen> result;
        MAYBE(current_stmt, module_.get_statement(stmt_ref));
        const ebm::IfStatement* if_stmt = current_stmt.body.if_statement();
        if (!if_stmt) {
            return ebmgen::unexpect_error("not an if statement");
        }
        result.push_back(IfThen{if_stmt->condition.cond, if_stmt->then_block});
        while (true) {
            if (is_nil(if_stmt->else_block)) {
                break;
            }
            MAYBE(next_stmt, module_.get_statement(if_stmt->else_block));
            auto else_id = if_stmt->else_block;
            if_stmt = next_stmt.body.if_statement();
            if (!if_stmt) {
                result.push_back(IfThen{std::nullopt, else_id});
                break;
            }
            result.push_back(IfThen{if_stmt->condition.cond, if_stmt->then_block});
        }
        return result;
    }

    constexpr bool is_int_literal(auto&& kind) {  // argument maybe TypeKind or optional<TypeKind>
        return kind == ebm::ExpressionKind::LITERAL_INT || kind == ebm::ExpressionKind::LITERAL_INT64;
    }

    ebmgen::expected<std::optional<std::pair<ebm::TypeKind, ebm::TypeRef>>> may_cause_allocation_type(auto&& ctx, ebm::TypeRef type) {
        ebmgen::MappingTable& m = get_visitor(ctx).module_;
        MAYBE(t, m.get_type(type));
        switch (t.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::FLOAT:
            case ebm::TypeKind::BOOL: {
                return std::nullopt;
            }
            case ebm::TypeKind::VECTOR: {
                MAYBE(element_type_ref, t.body.element_type());
                MAYBE(child, may_cause_allocation_type(ctx, element_type_ref));
                if (child) {  // if element cause it, return it directly, no need to wrap with vector
                    return child;
                }
                return std::make_pair(t.body.kind, type);
            }
            case ebm::TypeKind::ARRAY: {
                MAYBE(element_type_ref, t.body.element_type());
                return may_cause_allocation_type(ctx, element_type_ref);
            }
            case ebm::TypeKind::STRUCT: {
                MAYBE(decl, ebmgen::access_field<"struct_decl">(m, t.body.id()));
                if (decl.is_fixed_size()) {
                    return std::nullopt;
                }
                return std::make_pair(t.body.kind, type);
            }
            case ebm::TypeKind::RECURSIVE_STRUCT: {
                return std::make_pair(t.body.kind, type);
            }
            case ebm::TypeKind::ENUM: {
                MAYBE(decl, ebmgen::access_field<"enum_decl">(m, t.body.id()));
                if (is_nil(decl.base_type)) {
                    return std::nullopt;
                }
                return may_cause_allocation_type(ctx, decl.base_type);
            }
            case ebm::TypeKind::PTR:
            case ebm::TypeKind::OPTIONAL: {
                MAYBE(inner_type_ref, t.body.inner_type());
                return may_cause_allocation_type(ctx, inner_type_ref);
            }
            default: {
                return std::make_pair(t.body.kind, type);
            }
        }
    }

    const ebm::VariantDesc* is_variant_like(auto&& visitor, ebm::TypeRef type_ref) {
        ebmgen::MappingTable& m = get_visitor(visitor).module_;
        auto type = m.get_type(type_ref);
        if (!type) {
            return nullptr;
        }
        if (auto desc = type->body.variant_desc()) {
            return desc;
        }
        if (auto desc = type->body.struct_union_desc()) {
            return &desc->variant_desc;
        }
        return nullptr;
    }
    // Emit method-like statements (properties, encode/decode, methods) for a struct.
    // ctx must be Context_Statement_STRUCT_DECL (or compatible).
    // w is the CodeWriter to write to.
    // Granular helpers extracted from emit_struct_methods so backends that
    // need to relocate or filter parts (e.g. ebm2rust pulling anon-inner
    // property accessors into the outer impl) can call only the pieces
    // they want, in whatever order.
    template <class CodeWriter>
    ebmgen::expected<void> emit_struct_properties(auto&& ctx, CodeWriter& w) {
        if (auto props = ctx.struct_decl.properties()) {
            for (const auto& prop_ref : props->container) {
                MAYBE(prop, ctx.visit(prop_ref));
                w.writeln(prop.to_writer());
            }
        }
        return {};
    }

    template <class CodeWriter>
    ebmgen::expected<void> emit_struct_codec(auto&& ctx, CodeWriter& w) {
        if (auto enc = ctx.struct_decl.encode_fn()) {
            if (ctx.config().struct_encode_start_wrapper) {
                MAYBE(encode_fn, ctx.config().struct_encode_start_wrapper(ctx, *enc));
                w.writeln(std::move(encode_fn.to_writer()));
            }
            else {
                MAYBE(encode_fn, ctx.visit(*enc));
                w.writeln(std::move(encode_fn.to_writer()));
            }
        }
        if (auto dec = ctx.struct_decl.decode_fn()) {
            if (ctx.config().struct_decode_start_wrapper) {
                MAYBE(decode_fn, ctx.config().struct_decode_start_wrapper(ctx, *dec));
                w.writeln(std::move(decode_fn.to_writer()));
            }
            else {
                MAYBE(decode_fn, ctx.visit(*dec));
                w.writeln(std::move(decode_fn.to_writer()));
            }
        }
        return {};
    }

    template <class CodeWriter>
    ebmgen::expected<void> emit_struct_user_methods(auto&& ctx, CodeWriter& w) {
        if (auto methods = ctx.struct_decl.methods()) {
            for (const auto& method_ref : methods->container) {
                MAYBE(method, ctx.visit(method_ref));
                w.writeln(method.to_writer());
            }
        }
        return {};
    }

    // Convenience wrapper preserving the original call-all behavior.
    template <class CodeWriter>
    ebmgen::expected<void> emit_struct_methods(auto&& ctx, CodeWriter& w) {
        MAYBE_VOID(_p, emit_struct_properties(ctx, w));
        MAYBE_VOID(_c, emit_struct_codec(ctx, w));
        MAYBE_VOID(_m, emit_struct_user_methods(ctx, w));
        return {};
    }

    // Walk the variant containment chain from a struct's fields outward
    // (depth-first) and collect every anon inner STRUCT_DECL reachable
    // through STRUCT_UNION-typed fields. Used by backends that need to
    // hoist inner-anon property accessors into the parent_format's scope
    // (e.g. ebm2rust outer impl, ebm2cpp outer class) so their bodies'
    // self/this references land on the right receiver. Language-agnostic
    // because it only looks at EBM structure.
    inline ebmgen::expected<void> collect_anon_inner_descendants(auto&& ctx, ebm::WeakStatementRef struct_ref, std::vector<ebm::WeakStatementRef>& out, std::unordered_set<std::uint64_t>& seen) {
        MAYBE(stmt, ctx.get(from_weak(struct_ref)));
        auto struct_decl_p = stmt.body.struct_decl();
        if (!struct_decl_p) {
            return {};
        }
        for (auto& field_ref : struct_decl_p->fields.container) {
            MAYBE(field_stmt, ctx.get(field_ref));
            auto field_decl_p = field_stmt.body.field_decl();
            if (!field_decl_p) {
                continue;
            }
            MAYBE(field_type, ctx.get(field_decl_p->field_type));
            if (field_type.body.kind != ebm::TypeKind::STRUCT_UNION) {
                continue;
            }
            auto desc_p = field_type.body.struct_union_desc();
            if (!desc_p) {
                continue;
            }
            for (auto& member_type_ref : desc_p->variant_desc.members.container) {
                MAYBE(member_type, ctx.get(member_type_ref));
                if (member_type.body.kind != ebm::TypeKind::STRUCT) {
                    continue;
                }
                auto member_struct_weak_p = member_type.body.id();
                if (!member_struct_weak_p) {
                    continue;
                }
                auto member_struct_weak = *member_struct_weak_p;
                if (seen.contains(get_id(member_struct_weak))) {
                    continue;
                }
                seen.insert(get_id(member_struct_weak));
                out.push_back(member_struct_weak);
                MAYBE_VOID(_, collect_anon_inner_descendants(ctx, member_struct_weak, out, seen));
            }
        }
        return {};
    }

    // Whether any anon inner descendant (see collect_anon_inner_descendants)
    // of ctx's struct declares properties. Lets backends that wrap the
    // hoisted accessors in a dedicated scope (e.g. ebm2rust's outer impl
    // block) decide whether to open that scope before emitting.
    inline ebmgen::expected<bool> has_anon_inner_properties(auto&& ctx) {
        std::vector<ebm::WeakStatementRef> descendants;
        std::unordered_set<std::uint64_t> seen;
        ebm::WeakStatementRef self_weak{};
        self_weak.id = ctx.item_id;
        MAYBE_VOID(_collect, collect_anon_inner_descendants(ctx, self_weak, descendants, seen));
        for (auto& inner_ref : descendants) {
            MAYBE(inner_stmt, ctx.get(from_weak(inner_ref)));
            auto inner_decl_p = inner_stmt.body.struct_decl();
            if (inner_decl_p && inner_decl_p->has_properties()) {
                return true;
            }
        }
        return false;
    }

    // Emit the property accessors of every anon inner descendant of ctx's
    // struct into w, so they land on the outer (parent_format) scope and
    // their self/this references resolve against the outer instance.
    // Name-prefixing of the accessors is each backend's responsibility
    // (function_definition_start_wrapper / MEMBER_ACCESS hooks).
    // use_writeln=false packs the accessors with write (python-style blocks
    // that already end with their own newline).
    template <class CodeWriter>
    ebmgen::expected<void> emit_anon_inner_properties(auto&& ctx, CodeWriter& w, bool use_writeln = true) {
        std::vector<ebm::WeakStatementRef> descendants;
        std::unordered_set<std::uint64_t> seen;
        ebm::WeakStatementRef self_weak{};
        self_weak.id = ctx.item_id;
        MAYBE_VOID(_collect, collect_anon_inner_descendants(ctx, self_weak, descendants, seen));
        for (auto& inner_ref : descendants) {
            MAYBE(inner_stmt, ctx.get(from_weak(inner_ref)));
            auto inner_decl_p = inner_stmt.body.struct_decl();
            if (!inner_decl_p || !inner_decl_p->has_properties()) {
                continue;
            }
            auto inner_props = inner_decl_p->properties();
            if (!inner_props) {
                continue;
            }
            for (auto& prop_ref : inner_props->container) {
                MAYBE(prop_w, ctx.visit(prop_ref));
                if (use_writeln) {
                    w.writeln(prop_w.to_writer());
                }
                else {
                    w.write(prop_w.to_writer());
                }
            }
        }
        return {};
    }

}  // namespace ebmcodegen::util
