#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <memory>
#include <unordered_map>
#include "core/ast/node/ast_enum.h"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "core/ast/node/translated.h"
#include "core/ast/node/type.h"
#include "reloc_ptr.hpp"
#include <wrap/cout.h>

namespace ebmgen {
    using GenerateType = ebm::GenerateType;
    struct VisitedKey {
        std::shared_ptr<ast::Node> node;
        GenerateType type;

        friend constexpr auto operator<=>(const VisitedKey& a, const VisitedKey& b) noexcept = default;
    };
}  // namespace ebmgen

namespace std {
    template <>
    struct hash<ebmgen::VisitedKey> {
        size_t operator()(const ebmgen::VisitedKey& key) const {
            return std::hash<std::shared_ptr<brgen::ast::Node>>{}(key.node) ^ std::hash<int>{}(static_cast<int>(key.type));
        }
    };
}  // namespace std

namespace ebmgen {

    enum class DebugIDInspect {
        issue,
        alias_creation_to,
        alias_creation_from,
        creation,
    };

    void debug_id_inspect(std::uint64_t id, DebugIDInspect issue);

    struct ReferenceSource {
       private:
        std::uint64_t next_id = 1;

       public:
        expected<ebm::Varint> new_id() {
            debug_id_inspect(next_id, DebugIDInspect::issue);
            auto v = varint(next_id);
            if (!v) {
                return unexpect_error(std::move(v.error()));
            }
            next_id++;
            return v;
        }

        expected<ebm::Varint> current_id() const {
            if (next_id == 1) {
                return null_varint;
            }
            return varint(next_id - 1);
        }

        void set_current_id(std::uint64_t id) {
            next_id = id + 1;
        }
    };

    template <typename T>
    expected<std::string> serialize(const T& body) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        Error err = body.encode(w);
        if (err) {
            return unexpect_error(std::move(err));
        }
        return buffer;
    }

    template <AnyRef ID, class Instance, class Body, ebm::AliasHint hint>
    struct ReferenceRepository {
        using RelocPtr = RelocPtr<ReferenceRepository, ID, Instance>;
        ReferenceRepository(std::vector<ebm::RefAlias>& aliases)
            : aliases(aliases) {}

        expected<ID> new_id(ReferenceSource& source) {
            return source.new_id().and_then([this](ebm::Varint id) -> expected<ID> {
                return ID{id};
            });
        }

       private:
        expected<ID> add_internal(ID id, Body&& body) {
            debug_id_inspect(get_id(id), DebugIDInspect::creation);
            id_index_map[get_id(id)] = instances.size();
            Instance instance;
            instance.id = id;
            instance.body = std::move(body);
            instances.push_back(std::move(instance));
            return id;
        }

       public:
        expected<ID> add(ID id, Body&& body) {
            auto serialized = serialize(body);
            if (!serialized) {
                return unexpect_error(std::move(serialized.error()));
            }
            if (auto it = cache.find(*serialized); it != cache.end()) {
                debug_id_inspect(get_id(it->second), DebugIDInspect::alias_creation_to);
                debug_id_inspect(get_id(id), DebugIDInspect::alias_creation_from);
                // add alias if the same body is already present
                aliases.push_back(ebm::RefAlias{
                    .hint = hint,
                    .from = to_any_ref(id),
                    .to = to_any_ref(it->second),
                });
                alias_id_map[get_id(id)] = get_id(it->second);
                return id;
            }
            cache[*serialized] = id;
            return add_internal(id, std::move(body));
        }

        expected<ID> add(ReferenceSource& source, Body&& body) {
            auto serialized = serialize(body);
            if (!serialized) {
                return unexpect_error(std::move(serialized.error()));
            }
            if (auto it = cache.find(*serialized); it != cache.end()) {
                return it->second;
            }
            auto id = new_id(source);
            if (!id) {
                return unexpect_error(std::move(id.error()));
            }
            cache[*serialized] = id.value();
            return add_internal(*id, std::move(body));
        }

        Instance* get(const ID& id) {
            auto ref = get_id(id);
            if (auto found = alias_id_map.find(ref); found != alias_id_map.end()) {
                ref = found->second;
            }
            auto it = id_index_map.find(ref);
            if (it == id_index_map.end()) {
                return nullptr;
            }
            if (it->second >= instances.size()) {
                return nullptr;  // for safety
            }
            return &instances[it->second];
        }

        expected<RelocPtr> get_reloc_ptr(const ID& id) {
            auto instance = get(id);
            if (!instance) {
                return unexpect_error("{}: ID not found: {}", Instance::visitor_name, get_id(id));
            }
            return RelocPtr{this, id};
        }

        std::vector<Instance>& get_all() {
            return instances;
        }

        void clear() {
            cache.clear();
            id_index_map.clear();
            instances.clear();
            alias_id_map.clear();
        }

        void recalculate_id_index_map() {
            id_index_map.clear();
            for (size_t i = 0; i < instances.size(); ++i) {
                id_index_map[get_id(instances[i].id)] = i;
            }
            alias_id_map.clear();
            for (auto& alias : aliases) {
                if (alias.hint == hint) {
                    alias_id_map[get_id(alias.from)] = get_id(alias.to);
                }
            }
        }

        void recalculate_cache() {
            cache.clear();
            for (const auto& instance : instances) {
                cache[serialize(instance.body).value()] = instance.id;
            }
        }

       private:
        std::unordered_map<std::string, ID> cache;
        std::unordered_map<uint64_t, size_t> id_index_map;
        std::vector<Instance> instances;
        std::vector<ebm::RefAlias>& aliases;  // for aliasing references
        std::unordered_map<uint64_t, uint64_t> alias_id_map;
    };
    bool is_alignment_vector(const std::shared_ptr<ast::Field>& t);

    struct StateVariable {
        ebm::StatementRef enc_var_def;
        ebm::ExpressionRef enc_var_expr;
        ebm::StatementRef dec_var_def;
        ebm::ExpressionRef dec_var_expr;
        ebm::StatementRef prop_get_var_def;
        ebm::ExpressionRef prop_get_var_expr;
        ebm::StatementRef prop_set_var_def;
        ebm::ExpressionRef prop_set_var_expr;
        std::shared_ptr<ast::Field> ast_field;
    };

    using StateVariables = std::vector<StateVariable>;

    struct FormatEncodeDecode {
        ebm::ExpressionRef encode;
        ebm::TypeRef encode_type;
        ebm::ExpressionRef encoder_input;
        ebm::StatementRef encoder_input_def;
        ebm::ExpressionRef decode;
        ebm::TypeRef decode_type;
        ebm::ExpressionRef decoder_input;
        ebm::StatementRef decoder_input_def;
        StateVariables state_variables;
    };

    struct StatementConverter;
    struct ExpressionConverter;
    struct EncoderConverter;
    struct DecoderConverter;
    struct TypeConverter;

    struct ConverterState {
       private:
        ebm::Endian global_endian = ebm::Endian::big;
        ebm::Endian local_endian = ebm::Endian::unspec;
        ebm::StatementRef current_dynamic_endian = ebm::StatementRef{};
        bool on_function = false;
        std::shared_ptr<ast::Node> current_node;
        GenerateType current_generate_type = GenerateType::Normal;
        std::unordered_map<VisitedKey, ebm::StatementRef> visited_nodes;
        std::unordered_map<std::shared_ptr<ast::Node>, FormatEncodeDecode> format_encode_decode;
        std::unordered_map<std::uint64_t, FormatEncodeDecode*> format_encode_decode_cache;
        ebm::Block* current_block = nullptr;
        ebm::StatementRef current_loop_id;
        ebm::StatementRef current_yield_statement;
        std::unordered_map<std::shared_ptr<ast::Node>, ebm::TypeRef> type_cache;
        std::optional<ebm::ExpressionRef> self_ref;
        std::unordered_map<std::uint64_t, ebm::ExpressionRef> self_ref_map;
        std::unordered_map<std::uint64_t, ebm::TypeRef> struct_variant_map;
        bool on_available_check = false;
        ebm::StatementRef current_function_id;

        void debug_visited(const char* action, const std::shared_ptr<ast::Node>& node, ebm::StatementRef ref, GenerateType typ) const;

       public:
        [[nodiscard]] auto set_current_block(ebm::Block* block) {
            auto old = current_block;
            current_block = block;
            return futils::helper::defer([this, old]() {
                current_block = old;
            });
        }

        GenerateType get_current_generate_type() const {
            return current_generate_type;
        }

        [[nodiscard]] auto set_current_generate_type(GenerateType type) {
            auto old = current_generate_type;
            current_generate_type = type;
            return futils::helper::defer([this, old]() {
                current_generate_type = old;
            });
        }

        expected<ebm::StatementRef> get_current_loop_id() const {
            if (is_nil(current_loop_id)) {
                return unexpect_error("No current loop");
            }
            return current_loop_id;
        }

        [[nodiscard]] auto set_current_loop_id(ebm::StatementRef id) {
            auto old = current_loop_id;
            current_loop_id = id;
            return futils::helper::defer([this, old]() {
                current_loop_id = old;
            });
        }

        ebm::StatementRef get_current_function_id() const {
            return current_function_id;
        }

        [[nodiscard]] auto set_current_function_id(ebm::StatementRef id) {
            auto old = current_function_id;
            current_function_id = id;
            return futils::helper::defer([this, old]() {
                current_function_id = old;
            });
        }

        expected<ebm::StatementRef> get_current_yield_statement() const {
            if (is_nil(current_yield_statement)) {
                return unexpect_error("No current yield statement");
            }
            return current_yield_statement;
        }

        [[nodiscard]] auto set_current_yield_statement(ebm::StatementRef id) {
            auto old = current_yield_statement;
            current_yield_statement = id;
            return futils::helper::defer([this, old]() {
                current_yield_statement = old;
            });
        }

        expected<ebm::IOAttribute> get_io_attribute(ebm::Endian base, bool sign);
        bool set_endian(ebm::Endian e, ebm::StatementRef id = ebm::StatementRef{});

        bool is_on_function() const {
            return on_function;
        }

        void set_on_function(bool value) {
            on_function = value;
        }

        void add_visited_node(const std::shared_ptr<ast::Node>& node, ebm::StatementRef ref) {
            visited_nodes[{node, current_generate_type}] = ref;
            debug_visited("Add", node, ref, current_generate_type);
        }

        expected<ebm::StatementRef> is_visited(const std::shared_ptr<ast::Node>& node, std::optional<GenerateType> t = std::nullopt) const {
            if (!t.has_value()) {
                t = current_generate_type;
            }
            auto it = visited_nodes.find({node, *t});
            if (it != visited_nodes.end()) {
                debug_visited("Found", node, it->second, *t);
                return it->second;
            }
            debug_visited("Not found", node, ebm::StatementRef{}, *t);
            auto ident = ast::as<ast::Member>(node);
            return unexpect_error("Node not visited: {} {}", !node ? "(null)" : node_type_to_string(node->node_type), ident && ident->ident ? ident->ident->ident : "(no ident)");
        }

        void add_format_encode_decode(const std::shared_ptr<ast::Node>& node,
                                      ebm::StatementRef format_id,
                                      ebm::ExpressionRef encode,
                                      ebm::TypeRef encode_type,
                                      ebm::ExpressionRef encoder_input,
                                      ebm::StatementRef encoder_input_def,
                                      ebm::ExpressionRef decode,
                                      ebm::TypeRef decode_type,
                                      ebm::ExpressionRef decoder_input,
                                      ebm::StatementRef decoder_input_def,
                                      StateVariables state_variables) {
            format_encode_decode[node] = FormatEncodeDecode{
                .encode = encode,
                .encode_type = encode_type,
                .encoder_input = encoder_input,
                .encoder_input_def = encoder_input_def,
                .decode = decode,
                .decode_type = decode_type,
                .decoder_input = decoder_input,
                .decoder_input_def = decoder_input_def,
                .state_variables = state_variables,
            };
            format_encode_decode_cache[get_id(format_id)] = &format_encode_decode[node];
        }

        auto set_current_node(const std::shared_ptr<ast::Node>& node) {
            auto old = std::move(current_node);
            current_node = node;
            return futils::helper::defer([this, old = std::move(old)]() mutable {
                current_node = std::move(old);
            });
        }

        std::shared_ptr<ast::Node> get_current_node() const {
            return current_node;
        }

        FormatEncodeDecode* get_format_encode_decode(const std::shared_ptr<ast::Node>& node) {
            auto it = format_encode_decode.find(node);
            if (it != format_encode_decode.end()) {
                return &it->second;
            }
            return nullptr;
        }

        FormatEncodeDecode* get_format_encode_decode(ebm::StatementRef id) {
            auto it = format_encode_decode_cache.find(get_id(id));
            if (it != format_encode_decode_cache.end()) {
                return it->second;
            }
            return nullptr;
        }

        void cache_type(const std::shared_ptr<ast::Node>& node, ebm::TypeRef type) {
            type_cache[node] = type;
        }

        ebm::TypeRef get_cached_type(const std::shared_ptr<ast::Node>& node) const {
            auto it = type_cache.find(node);
            if (it != type_cache.end()) {
                return it->second;
            }
            return ebm::TypeRef{};
        }

        [[nodiscard]] auto set_self_ref(std::optional<ebm::ExpressionRef> ref) {
            auto old = self_ref;
            self_ref = ref;
            return futils::helper::defer([this, old]() {
                self_ref = old;
            });
        }

        std::optional<ebm::ExpressionRef> get_self_ref() const {
            return self_ref;
        }

        void set_self_ref_for_id(ebm::StatementRef id, ebm::ExpressionRef ref) {
            self_ref_map[get_id(id)] = ref;
        }

        std::optional<ebm::ExpressionRef> get_self_ref_for_id(ebm::StatementRef id) const {
            auto it = self_ref_map.find(get_id(id));
            if (it != self_ref_map.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        void set_struct_variant_for_id(ebm::StatementRef id, ebm::TypeRef type) {
            struct_variant_map[get_id(id)] = type;
        }

        std::optional<ebm::TypeRef> get_struct_variant_for_id(ebm::StatementRef id) const {
            auto it = struct_variant_map.find(get_id(id));
            if (it != struct_variant_map.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        bool is_on_available_check() const {
            return on_available_check;
        }

        auto set_on_available_check(bool value) {
            auto old = on_available_check;
            on_available_check = value;
            return futils::helper::defer([this, old]() {
                on_available_check = old;
            });
        }
    };

    struct EBMRepository {
       private:
        ReferenceSource ident_source;
        std::vector<ebm::RefAlias> aliases;
        ReferenceRepository<ebm::IdentifierRef, ebm::Identifier, ebm::String, ebm::AliasHint::IDENTIFIER> identifier_repo{aliases};
        ReferenceRepository<ebm::StringRef, ebm::StringLiteral, ebm::String, ebm::AliasHint::STRING> string_repo{aliases};
        ReferenceRepository<ebm::TypeRef, ebm::Type, ebm::TypeBody, ebm::AliasHint::TYPE> type_repo{aliases};
        ReferenceRepository<ebm::ExpressionRef, ebm::Expression, ebm::ExpressionBody, ebm::AliasHint::EXPRESSION> expression_repo{aliases};
        ReferenceRepository<ebm::StatementRef, ebm::Statement, ebm::StatementBody, ebm::AliasHint::STATEMENT> statement_repo{aliases};
        std::vector<ebm::Loc> debug_locs;
        std::vector<ebm::StringRef> file_names;

        friend struct TransformContext;
        friend struct TestRepositoryAccessor;  // For test use only

       public:
        EBMRepository() = default;
        EBMRepository(const EBMRepository&) = delete;
        EBMRepository& operator=(const EBMRepository&) = delete;
        EBMRepository(EBMRepository&&) = delete;
        EBMRepository& operator=(EBMRepository&&) = delete;

        void clear() {
            ident_source = ReferenceSource{};
            identifier_repo.clear();
            string_repo.clear();
            type_repo.clear();
            expression_repo.clear();
            statement_repo.clear();
            debug_locs.clear();
        }

        // after this call, getter functions will returns nullptr
        // before reuse it, you should call clear()
        expected<void> finalize(ebm::ExtendedBinaryModule& mod, bool verify_uniqueness);

        expected<void> add_files(std::vector<std::string>&& names);

        template <AnyRef T>
        expected<void> add_debug_loc(brgen::lexer::Loc loc, T ref) {
            ebm::Loc debug_loc;
            MAYBE(file_id, varint(loc.file));
            MAYBE(line, varint(loc.line));
            MAYBE(column, varint(loc.col));
            MAYBE(start, varint(loc.pos.begin));
            MAYBE(end, varint(loc.pos.end));
            debug_loc.ident = to_any_ref(ref);
            debug_loc.file_id = file_id;
            debug_loc.line = line;
            debug_loc.column = column;
            debug_loc.start = start;
            debug_loc.end = end;
            debug_locs.push_back(std::move(debug_loc));
            return {};
        }

        expected<ebm::StatementRef> new_statement_id() {
            return statement_repo.new_id(ident_source);
        }

        expected<ebm::TypeRef> new_type_id() {
            return type_repo.new_id(ident_source);
        }

        expected<ebm::ExpressionRef> new_expression_id() {
            return expression_repo.new_id(ident_source);
        }

        ReferenceSource& get_identifier_source() {
            return ident_source;
        }

        expected<ebm::IdentifierRef> add_identifier(const std::string& name) {
            auto len = varint(name.size());
            if (!len) {
                return unexpect_error("Failed to create varint for identifier length: {}", len.error().error());
            }
            ebm::String string_literal;
            string_literal.data = name;
            string_literal.length = *len;
            return identifier_repo.add(ident_source, std::move(string_literal));
        }

        expected<ebm::TypeRef> add_type(ebm::TypeBody&& body) {
            return type_repo.add(ident_source, std::move(body));
        }

        expected<ebm::TypeRef> add_type(ebm::TypeRef id, ebm::TypeBody&& body) {
            return type_repo.add(id, std::move(body));
        }

        expected<ebm::StatementRef> add_statement(ebm::StatementRef id, ebm::StatementBody&& body) {
            return statement_repo.add(id, std::move(body));
        }

        expected<ebm::StatementRef> add_statement(ebm::StatementBody&& body) {
            return statement_repo.add(ident_source, std::move(body));
        }

        expected<ebm::StringRef> add_string(const std::string& str) {
            auto len = varint(str.size());
            if (!len) {
                return unexpect_error("Failed to create varint for string length: {}", len.error().error());
            }
            ebm::String string_literal;
            string_literal.data = str;
            string_literal.length = *len;
            return string_repo.add(ident_source, std::move(string_literal));
        }

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body) {
            if (is_nil(body.type)) {
                return unexpect_error("Expression type is not set: {}", to_string(body.kind));
            }
            return expression_repo.add(ident_source, std::move(body));
        }

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionRef id, ebm::ExpressionBody&& body) {
            if (is_nil(body.type)) {
                return unexpect_error("Expression type is not set: {}", to_string(body.kind));
            }
            return expression_repo.add(id, std::move(body));
        }

        ebm::Statement* get_statement(const ebm::StatementRef& ref) {
            return statement_repo.get(ref);
        }

        ebm::Expression* get_expression(const ebm::ExpressionRef& ref) {
            return expression_repo.get(ref);
        }

        ebm::Type* get_type(const ebm::TypeRef& ref) {
            return type_repo.get(ref);
        }
    };

    struct ConverterContext {
       private:
        std::shared_ptr<StatementConverter> statement_converter;
        std::shared_ptr<ExpressionConverter> expression_converter;
        std::shared_ptr<EncoderConverter> encoder_converter;
        std::shared_ptr<DecoderConverter> decoder_converter;
        std::shared_ptr<TypeConverter> type_converter;

        EBMRepository repo_;
        ConverterState state_;

       public:
        ConverterContext();

        EBMRepository& repository() {
            return repo_;
        }

        ConverterState& state() {
            return state_;
        }

        StatementConverter& get_statement_converter();

        ExpressionConverter& get_expression_converter();

        EncoderConverter& get_encoder_converter();

        DecoderConverter& get_decoder_converter();

        TypeConverter& get_type_converter();

        // shorthand for creating a type with a single kind
        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);
        expected<ebm::StatementRef> convert_statement(ebm::StatementRef, const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);
    };

    expected<ebm::TypeRef> get_single_type(ebm::TypeKind kind, ConverterContext& ctx);
    expected<ebm::TypeRef> get_counter_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_unsigned_n_int(ConverterContext& ctx, size_t n);
    expected<ebm::TypeRef> get_u8_n_array(ConverterContext& ctx, size_t n, ebm::ArrayAnnotation annot);
    expected<ebm::TypeRef> get_bool_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_void_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_encoder_return_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_decoder_return_type(ConverterContext& ctx);
    expected<ebm::ExpressionRef> get_int_literal(ConverterContext& ctx, std::uint64_t value);
    ebm::ExpressionBody get_int_literal_body(ebm::TypeRef type, std::uint64_t value);

    expected<std::string> decode_base64(const std::shared_ptr<ast::StrLiteral>& lit);

    struct StatementConverter {
        ConverterContext& ctx;
        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);

        expected<ebm::StructDecl> convert_struct_decl(ebm::IdentifierRef name, const std::shared_ptr<ast::StructType>& node, ebm::TypeRef related_variant = {});
        expected<ebm::StatementRef> convert_statement(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node);
        expected<ebm::FunctionDecl> convert_function_decl(ebm::StatementRef func_id, const std::shared_ptr<ast::Function>& node, GenerateType typ, ebm::StatementRef coder_input_ref);
        expected<void> derive_match_lowered_if(ebm::MatchStatement& match_stmt, bool trial_match);
        expected<ebm::StatementRef> convert_struct_decl(const std::shared_ptr<ast::StructType>& node, ebm::TypeRef related_variant = {});

       private:
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Assert>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Return>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Break>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Continue>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::If>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Loop>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Match>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::IndentBlock>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::MatchBranch>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Program>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Format>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Enum>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::EnumMember>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Function>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Metadata>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::State>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Field>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::ExplicitError>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Import>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::ImplicitYield>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Binary>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::ScopedStatement>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::SpecifyOrder>& node, ebm::StatementRef id, ebm::StatementBody& body);

        // Fallback for unhandled types
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Node>& node, ebm::StatementRef id, ebm::StatementBody& body);

        expected<ebm::StatementBody> convert_loop_body(const std::shared_ptr<ast::Loop>& node);

        expected<ebm::StatementBody> convert_property_decl(const std::shared_ptr<ast::Field>& node);
    };

    struct ExpressionConverter {
        ConverterContext& ctx;
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);

        // for match statement lowering
        expected<ebm::ExpressionRef> convert_equal(ebm::ExpressionRef a, ebm::ExpressionRef b);

       private:
        expected<void> convert_expr_impl(const std::shared_ptr<ast::IntLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::BoolLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::StrLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::TypeLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Ident>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Binary>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Unary>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Index>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::MemberAccess>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Cast>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Range>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::IOOperation>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Paren>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Cond>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Call>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Available>& node, ebm::ExpressionBody& body);

        expected<void> convert_expr_impl(const std::shared_ptr<ast::If>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Match>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::CharLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::OrCond>& node, ebm::ExpressionBody& body);

        // Fallback for unhandled types
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Expr>& node, ebm::ExpressionBody& body);
    };

    expected<ebm::ExpressionRef> get_alignment_requirement(ConverterContext& ctx, std::uint64_t alignment_bytes, ebm::StreamType type);

    struct EncoderConverter {
        ConverterContext& ctx;
        expected<ebm::StatementBody> encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field, ebm::StatementRef field_ref);
        // internally, this method casts base_ref to unsigned n int type if necessary
        // so no need to cast it before calling this method
        expected<ebm::StatementRef> encode_multi_byte_int_with_fixed_array(ebm::StatementRef io_ref, ebm::StatementRef field, size_t n, ebm::IOAttribute endian, ebm::ExpressionRef from, ebm::TypeRef cast_from);

       private:
        expected<void> encode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& typ, ebm::ExpressionRef base_ref);
        expected<void> encode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& typ, ebm::ExpressionRef base_ref);
        expected<void> encode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
        expected<void> encode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
        expected<void> encode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref);
        expected<void> encode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
    };

    struct DecoderConverter {
        ConverterContext& ctx;
        expected<ebm::StatementBody> decode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field, ebm::StatementRef field_ref);
        expected<ebm::StatementRef> decode_multi_byte_int_with_fixed_array(ebm::StatementRef io_ref, ebm::StatementRef field_ref, size_t n, ebm::IOAttribute endian, ebm::ExpressionRef to, ebm::TypeRef cast_to);

       private:
        expected<void> decode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& typ, ebm::ExpressionRef base_ref);
        expected<void> decode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& typ, ebm::ExpressionRef base_ref);
        expected<void> decode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
        expected<void> decode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
        expected<void> decode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref);
        expected<void> decode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
    };

    struct TypeConverter {
        ConverterContext& ctx;
        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);
        expected<ebm::TypeBody> convert_function_type(const std::shared_ptr<ast::FunctionType>& type);

        expected<ebm::CastType> get_cast_type(ebm::TypeRef dest, ebm::TypeRef src);
    };

    expected<ebm::StatementRef> add_endian_specific(ConverterContext& ctx, ebm::IOAttribute endian, std::function<expected<ebm::StatementRef>()> on_little_endian, std::function<expected<ebm::StatementRef>()> on_big_endian);
    expected<void> construct_string_array(ConverterContext& ctx, ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate);

    expected<ebm::StatementBody> assert_statement_body(ConverterContext& ctx, ebm::ExpressionRef condition);
    expected<ebm::StatementRef> assert_statement(ConverterContext& ctx, ebm::ExpressionRef condition);
    expected<ebm::ExpressionRef> get_max_value_expr(ConverterContext& ctx, ebm::TypeRef type);
    expected<ebm::BinaryOp> convert_assignment_binary_op(ast::BinaryOp op);
    expected<std::pair<ebm::ExpressionRef, ebm::ExpressionRef>> insert_binary_op_cast(ConverterContext& ctx, ebm::BinaryOp bop, ebm::TypeRef dst_type, ebm::ExpressionRef left, ebm::ExpressionRef right);
    expected<ebm::BinaryOp> convert_binary_op(ast::BinaryOp op);
    expected<std::optional<ebm::TypeRef>> get_common_type(ConverterContext& ctx, ebm::TypeRef a, ebm::TypeRef b);
    expected<ebm::ExpressionBody> make_conditional(ConverterContext& ctx, ebm::TypeRef type, ebm::ExpressionRef cond, ebm::ExpressionRef then, ebm::ExpressionRef els);
    expected<std::optional<std::pair<ebm::StatementRef, ebm::ExpressionRef>>> handle_variant_alternative(ConverterContext& ctx, ebm::TypeRef alt_type, ebm::InitCheckType typ, ebm::StatementRef related_function);
    expected<ebm::StatementRef> make_field_init_check(ConverterContext& ctx, ebm::ExpressionRef base_ref, bool encode, ebm::StatementRef function_ref);

    struct TransformContext {
       private:
        ConverterContext& ctx;

       public:
        TransformContext(ConverterContext& ctx)
            : ctx(ctx) {}

        auto& type_repository() {
            return ctx.repository().type_repo;
        }

        auto& statement_repository() {
            return ctx.repository().statement_repo;
        }

        auto& expression_repository() {
            return ctx.repository().expression_repo;
        }

        auto& identifier_repository() {
            return ctx.repository().identifier_repo;
        }

        auto& string_repository() {
            return ctx.repository().string_repo;
        }

        auto& alias_vector() {
            return ctx.repository().aliases;
        }

        auto& file_names() {
            return ctx.repository().file_names;
        }

        auto& debug_locations() {
            return ctx.repository().debug_locs;
        }

        auto max_id() const {
            return ctx.repository().ident_source.current_id();
        }

        void set_max_id(std::uint64_t id) {
            ctx.repository().ident_source.set_current_id(id);
        }

        expected<ebm::AnyRef> new_id() {
            return ctx.repository().ident_source.new_id().transform([](ebm::Varint id) {
                return ebm::AnyRef{id};
            });
        }

        auto& context() {
            return ctx;
        }

        void recalculate_id_index_map() {
            type_repository().recalculate_id_index_map();
            identifier_repository().recalculate_id_index_map();
            expression_repository().recalculate_id_index_map();
            statement_repository().recalculate_id_index_map();
            string_repository().recalculate_id_index_map();
        }
    };

    auto print_if_verbose(auto&&... args) {
        if (verbose_error) {
            (futils::wrap::cerr_wrap() << ... << args);
        }
    }
}  // namespace ebmgen
