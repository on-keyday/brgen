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

#define CODE(...) (code_write<CodeWriter>(__VA_ARGS__))
#define CODELINE(...) (code_writeln<CodeWriter>(__VA_ARGS__))

#define SEPARATED(...) (code_joint_write<CodeWriter>(__VA_ARGS__))

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
    // This is the shared logic extracted from the default STRUCT_DECL visitor.
    template <class CodeWriter>
    ebmgen::expected<void> emit_struct_methods(auto&& ctx, CodeWriter& w) {
        if (auto props = ctx.struct_decl.properties()) {
            for (const auto& prop_ref : props->container) {
                MAYBE(prop, ctx.visit(prop_ref));
                w.writeln(prop.to_writer());
            }
        }

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
        if (auto methods = ctx.struct_decl.methods()) {
            for (const auto& method_ref : methods->container) {
                MAYBE(method, ctx.visit(method_ref));
                w.writeln(method.to_writer());
            }
        }

        return {};
    }

}  // namespace ebmcodegen::util
