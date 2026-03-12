#include "converter.hpp"
#include <core/ast/traverse.h>
#include <functional>
#include <unordered_set>
#include "binary/log2i.h"
#include "common.hpp"
#include "convert/helper.hpp"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
namespace ebmgen {

    void debug_id_inspect(std::uint64_t id, DebugIDInspect inspect) {
        if (id == 3481) {
            ;
        }
    }

    void ConverterState::debug_visited(const char* action, const std::shared_ptr<ast::Node>& node, ebm::StatementRef ref, GenerateType typ) const {
        if (!verbose_error) {
            return;
        }
        auto member = ast::as<ast::Member>(node);
        const char* ident = member && member->ident ? member->ident->ident.c_str() : "(no ident)";
        futils::wrap::cerr_wrap() << action << ": (" << (node ? node_type_to_string(node->node_type) : "(null)") << " " << ident << "(" << node.get() << "), " << to_string(typ) << ")";
        if (!is_nil(ref)) {
            futils::wrap::cerr_wrap() << " -> " << get_id(ref);
        }
        futils::wrap::cerr_wrap() << '\n';
    }

    expected<ebm::IOAttribute> ConverterState::get_io_attribute(ebm::Endian base, bool sign) {
        ebm::IOAttribute e;
        e.sign(sign);
        e.endian(base);
        if (base != ebm::Endian::unspec) {
            return e;
        }
        e.endian(global_endian);
        if (on_function) {
            e.endian(local_endian);
        }
        if (e.endian() == ebm::Endian::dynamic) {
            e.dynamic_ref(current_dynamic_endian);
        }
        return e;
    }

    bool ConverterState::set_endian(ebm::Endian e, ebm::StatementRef id) {
        if (on_function) {
            local_endian = e;
            current_dynamic_endian = id;
            return true;
        }
        if (e == ebm::Endian::dynamic) {
            return false;
        }
        global_endian = e;
        return true;
    }

    expected<void> EBMRepository::add_files(std::vector<std::string>&& names) {
        for (auto& name : names) {
            MAYBE(name_ref, add_string(name));
            file_names.push_back(name_ref);
        }
        return {};
    }

    expected<void> EBMRepository::finalize(ebm::ExtendedBinaryModule& ebm, bool verify_uniqueness) {
        MAYBE(max_id, ident_source.current_id());
        ebm.max_id = ebm::AnyRef{max_id};
        MAYBE(identifiers_len, varint(identifier_repo.get_all().size()));
        ebm.identifiers_len = identifiers_len;
        ebm.identifiers = std::move(identifier_repo.get_all());
        std::unordered_set<std::uint64_t> seen;

        auto check_duplicate = [&](const auto& items, const char* item_name) -> expected<void> {
            if (!verify_uniqueness) {
                return {};
            }
            for (const auto& item : items) {
                auto id = get_id(item.id);
                if (!seen.insert(id).second) {
                    if constexpr (has_body_kind<decltype(item)>) {
                        return unexpect_error("Duplicate {}({}) found during finalize: {}", item_name, to_string(item.body.kind), id);
                    }
                    else {
                        return unexpect_error("Duplicate {} found during finalize: {}", item_name, id);
                    }
                }
            }
            return {};
        };
        MAYBE_VOID(ok1, check_duplicate(ebm.identifiers, "identifier"));

        MAYBE(strings_len, varint(string_repo.get_all().size()));
        ebm.strings_len = strings_len;
        ebm.strings = std::move(string_repo.get_all());

        MAYBE_VOID(ok2, check_duplicate(ebm.strings, "string literal"));

        MAYBE(types_len, varint(type_repo.get_all().size()));
        ebm.types_len = types_len;
        ebm.types = std::move(type_repo.get_all());

        MAYBE_VOID(ok3, check_duplicate(ebm.types, "type"));

        MAYBE(statements_len, varint(statement_repo.get_all().size()));
        ebm.statements_len = statements_len;
        ebm.statements = std::move(statement_repo.get_all());

        MAYBE_VOID(ok4, check_duplicate(ebm.statements, "statement"));

        MAYBE(expressions_len, varint(expression_repo.get_all().size()));
        ebm.expressions_len = expressions_len;
        ebm.expressions = std::move(expression_repo.get_all());
        MAYBE_VOID(ok5, check_duplicate(ebm.expressions, "expression"));

        MAYBE(aliases_len, varint(aliases.size()));
        ebm.aliases_len = aliases_len;
        ebm.aliases = std::move(aliases);
        for (auto& alias : ebm.aliases) {
            if (alias.hint == ebm::AliasHint::ALIAS) {
                return unexpect_error("Alias hint should not contains ALIAS: {} -> {}", get_id(alias.from), get_id(alias.to));
            }
            if (verify_uniqueness) {
                auto id = get_id(alias.from);
                if (!seen.insert(id).second) {
                    return unexpect_error("Duplicate alias({}) from id found during finalize: {}", to_string(alias.hint), id);
                }
            }
        }

        MAYBE(files_len, varint(file_names.size()));
        ebm.debug_info.len_files = files_len;
        ebm.debug_info.files = std::move(file_names);

        MAYBE(loc_len, varint(debug_locs.size()));

        ebm.debug_info.locs = std::move(debug_locs);
        ebm.debug_info.len_locs = loc_len;

        return {};
    }

    ConverterContext::ConverterContext() {
        statement_converter = std::make_shared<StatementConverter>(*this);
        expression_converter = std::make_shared<ExpressionConverter>(*this);
        encoder_converter = std::make_shared<EncoderConverter>(*this);
        decoder_converter = std::make_shared<DecoderConverter>(*this);
        type_converter = std::make_shared<TypeConverter>(*this);
    }

    StatementConverter& ConverterContext::get_statement_converter() {
        return *statement_converter;
    }

    ExpressionConverter& ConverterContext::get_expression_converter() {
        return *expression_converter;
    }

    EncoderConverter& ConverterContext::get_encoder_converter() {
        return *encoder_converter;
    }

    DecoderConverter& ConverterContext::get_decoder_converter() {
        return *decoder_converter;
    }

    TypeConverter& ConverterContext::get_type_converter() {
        return *type_converter;
    }

    expected<ebm::StatementRef> ConverterContext::convert_statement(const std::shared_ptr<ast::Node>& node) {
        return statement_converter->convert_statement(node);
    }

    expected<ebm::StatementRef> ConverterContext::convert_statement(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node) {
        return statement_converter->convert_statement(ref, node);
    }
    expected<ebm::ExpressionRef> ConverterContext::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        return expression_converter->convert_expr(node);
    }
    expected<ebm::TypeRef> ConverterContext::convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field) {
        return type_converter->convert_type(type, field);
    }

    expected<ebm::StatementRef> add_endian_specific(ConverterContext& ctx, ebm::IOAttribute endian, std::function<expected<ebm::StatementRef>()> on_little_endian, std::function<expected<ebm::StatementRef>()> on_big_endian) {
        ebm::StatementRef ref;
        const auto is_native_or_dynamic = endian.endian() == ebm::Endian::native || endian.endian() == ebm::Endian::dynamic;
        if (is_native_or_dynamic) {
            ebm::ExpressionBody is_little;
            is_little.kind = ebm::ExpressionKind::IS_LITTLE_ENDIAN;
            auto endian_expr = endian.dynamic_ref();
            is_little.endian_expr(endian_expr ? *endian_expr : ebm::StatementRef{});  // if native, this will be empty
            EBMA_ADD_EXPR(is_little_ref, std::move(is_little));
            MAYBE(then_block, on_little_endian());
            MAYBE(else_block, on_big_endian());
            EBM_IF_STATEMENT(res, is_little_ref, then_block, else_block);
            ref = res;
        }
        else if (endian.endian() == ebm::Endian::little) {
            MAYBE(res, on_little_endian());
            ref = res;
        }
        else if (endian.endian() == ebm::Endian::big) {
            MAYBE(res, on_big_endian());
            ref = res;
        }
        else {
            return unexpect_error("Unsupported endian type: {}", to_string(endian.endian()));
        }
        return ref;
    }

    expected<ebm::TypeRef> get_unsigned_n_int(ConverterContext& ctx, size_t n) {
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::UINT;
        MAYBE(bit_size, varint(n));
        typ.size(bit_size);
        EBMA_ADD_TYPE(type_ref, std::move(typ));
        return type_ref;
    }

    expected<ebm::TypeRef> get_counter_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::USIZE, ctx);
    }

    ebm::ExpressionBody get_int_literal_body(ebm::TypeRef type, std::uint64_t value) {
        ebm::ExpressionBody body;
        body.type = type;
        body.kind = value >= (std::uint64_t(1) << 62) ? ebm::ExpressionKind::LITERAL_INT64 : ebm::ExpressionKind::LITERAL_INT;
        if (body.kind == ebm::ExpressionKind::LITERAL_INT64) {
            body.int64_value(value);
        }
        else {
            body.int_value(varint(value).value());  // failure at here is fatal, so we can use .value() directly
        }
        return body;
    }

    expected<ebm::ExpressionRef> get_int_literal(ConverterContext& ctx, std::uint64_t value) {
        ebm::ExpressionBody body;
        MAYBE(t, get_unsigned_n_int(ctx, value == 0 ? 1 : futils::binary::log2i(value)));
        body = get_int_literal_body(t, value);
        EBMA_ADD_EXPR(int_literal, std::move(body));
        return int_literal;
    }

    expected<ebm::TypeRef> get_single_type(ebm::TypeKind kind, ConverterContext& ctx) {
        ebm::TypeBody typ;
        typ.kind = kind;
        EBMA_ADD_TYPE(type_ref, std::move(typ));
        return type_ref;
    }

    expected<ebm::TypeRef> get_bool_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::BOOL, ctx);
    }

    expected<ebm::TypeRef> get_void_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::VOID, ctx);
    }

    expected<ebm::TypeRef> get_encoder_return_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::ENCODER_RETURN, ctx);
    }
    expected<ebm::TypeRef> get_decoder_return_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::DECODER_RETURN, ctx);
    }

    expected<ebm::TypeRef> get_u8_n_array(ConverterContext& ctx, size_t n, ebm::ArrayAnnotation annot) {
        EBMU_U8(u8typ);
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::ARRAY;
        typ.element_type(u8typ);
        typ.length(*varint(n));
        typ.array_annotation(annot);
        EBMA_ADD_TYPE(type_ref, std::move(typ));
        return type_ref;
    }

    expected<ebm::StatementRef> make_field_init_check(ConverterContext& ctx, ebm::ExpressionRef base_ref, bool encode, ebm::StatementRef related_function) {
        ebm::InitCheck init_check;
        init_check.init_check_type = encode ? ebm::InitCheckType::field_init_encode : ebm::InitCheckType::field_init_decode;
        init_check.target_field = base_ref;
        MAYBE(base_ref_type, ctx.repository().get_expression(base_ref));
        EBM_DEFAULT_VALUE(default_, base_ref_type.body.type);
        init_check.expect_value = default_;
        EBM_INIT_CHECK_STATEMENT(init_check_ref, std::move(init_check));
        return init_check_ref;
    }

}  // namespace ebmgen
