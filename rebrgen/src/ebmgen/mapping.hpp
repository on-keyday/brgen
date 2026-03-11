/*license*/
#pragma once
#include <ebm/extended_binary_module.hpp>
#include <unordered_map>
#include <cstdint>
#include <variant>
#include <vector>
#include "common.hpp"

namespace ebmgen {

    struct InverseRef {
        const char* name;
        std::optional<size_t> index;
        ebm::AnyRef ref;
        ebm::AliasHint hint;
    };

    using ObjectVariant = std::variant<std::monostate, const ebm::Identifier*, const ebm::StringLiteral*, const ebm::Type*, const ebm::Statement*, const ebm::Expression*>;

    struct EBMProxy {
        ebm::AnyRef max_id;
        const std::vector<ebm::Statement>& statements;
        const std::vector<ebm::Expression>& expressions;
        const std::vector<ebm::Type>& types;
        const std::vector<ebm::Identifier>& identifiers;
        const std::vector<ebm::StringLiteral>& strings;
        const std::vector<ebm::RefAlias>& aliases;
        const std::vector<ebm::Loc>& locs;
        const ebm::ExtendedBinaryModule* origin = nullptr;

        constexpr EBMProxy(const ebm::ExtendedBinaryModule& module)
            : max_id(module.max_id), statements(module.statements), expressions(module.expressions), types(module.types), identifiers(module.identifiers), strings(module.strings), aliases(module.aliases), locs(module.debug_info.locs), origin(&module) {
        }

        constexpr EBMProxy(const std::vector<ebm::Statement>& stmts,
                           const std::vector<ebm::Expression>& exprs,
                           const std::vector<ebm::Type>& tys,
                           const std::vector<ebm::Identifier>& idents,
                           const std::vector<ebm::StringLiteral>& strs,
                           const std::vector<ebm::RefAlias>& als,
                           const std::vector<ebm::Loc>& locs)
            : max_id{}, statements(stmts), expressions(exprs), types(tys), identifiers(idents), strings(strs), aliases(als), locs(locs), origin(nullptr) {
        }
    };

    struct lazy_init_tag {};

    constexpr lazy_init_tag lazy_init{};

    namespace mapping {
        enum class BuildMapOption {
            NONE = 0,
            BUILD_MAP_USE_DEBUG_LOC = 1 << 0,
            BUILD_MAP_USE_INVERSE_REF = 1 << 1,
            BUILD_MAP_SKIP_IF_UNCHANGED = 1 << 2,
        };
        constexpr BuildMapOption operator|(BuildMapOption a, BuildMapOption b) {
            return static_cast<BuildMapOption>(static_cast<int>(a) | static_cast<int>(b));
        }

        constexpr bool operator&(BuildMapOption a, BuildMapOption b) {
            return (static_cast<int>(a) & static_cast<int>(b)) != 0;
        }
    }  // namespace mapping

    struct MappingTable {
        explicit MappingTable(EBMProxy module)
            : module_(module) {
            build_maps();
        }

        explicit MappingTable(EBMProxy module, lazy_init_tag)
            : module_(module) {
        }

        bool valid() const;
        size_t original_id_count() const;
        size_t mapped_id_count() const;

        const ebm::Identifier* get_identifier(const ebm::IdentifierRef& ref) const;
        const ebm::StringLiteral* get_string_literal(const ebm::StringRef& ref) const;
        const ebm::Type* get_type(const ebm::TypeRef& ref) const;
        const ebm::Statement* get_statement(const ebm::StatementRef& ref) const;
        const ebm::Statement* get_statement(const ebm::WeakStatementRef& ref) const;
        const ebm::Expression* get_expression(const ebm::ExpressionRef& ref) const;

        ObjectVariant get_object(const ebm::AnyRef& ref) const;
        // useful for generic use
        ObjectVariant get_object(const ebm::StatementRef& ref) const;
        ObjectVariant get_object(const ebm::ExpressionRef& ref) const;
        ObjectVariant get_object(const ebm::TypeRef& ref) const;
        ObjectVariant get_object(const ebm::IdentifierRef& ref) const;
        ObjectVariant get_object(const ebm::StringRef& ref) const;

        std::optional<ebm::TypeKind> get_type_kind(const ebm::TypeRef& ref) const {
            if (const auto* type = get_type(ref)) {
                return type->body.kind;
            }
            return std::nullopt;
        }

        std::optional<ebm::StatementKind> get_statement_kind(const ebm::StatementRef& ref) const {
            if (const auto* stmt = get_statement(ref)) {
                return stmt->body.kind;
            }
            return std::nullopt;
        }

        std::optional<ebm::ExpressionKind> get_expression_kind(const ebm::ExpressionRef& ref) const {
            if (const auto* expr = get_expression(ref)) {
                return expr->body.kind;
            }
            return std::nullopt;
        }

        // Gets the identifier name associated with the given reference.
        // Code generator should use this, not direct access to IdentifierRef member access (via get_identifier).
        std::string get_associated_identifier(const ebm::StatementRef& ref, std::string_view prefix = "") const;
        std::string get_associated_identifier(const ebm::WeakStatementRef& ref, std::string_view prefix = "") const;
        expected<std::string> get_associated_identifier(const ebm::ExpressionRef& ref, std::string_view prefix = "") const;
        expected<std::string> get_associated_identifier(const ebm::TypeRef& ref, std::string_view prefix = "") const;

        const ebm::Identifier* get_identifier(const ebm::StatementRef& ref) const;
        const ebm::Identifier* get_identifier(const ebm::WeakStatementRef& ref) const;

        const ebm::Identifier* get_identifier(const ebm::ExpressionRef& ref) const;

        const ebm::Identifier* get_identifier(const ebm::TypeRef& ref) const;

        const ebm::Identifier* get_identifier(const ebm::AnyRef& ref) const;

        // same as get_statement(ebm::StatementRef{module().max_id.id})
        const ebm::Statement* get_entry_point() const;

        const EBMProxy& module() const {
            return module_;
        }

        const std::vector<InverseRef>* get_inverse_ref(const ebm::AnyRef& ref) const;

        void register_default_prefix(ebm::StatementKind kind, std::string_view prefix) {
            default_identifier_prefix_[kind] = prefix;
        }

        std::string_view get_default_prefix(ebm::StatementRef ref) const;

        std::string_view get_default_prefix(ebm::StatementKind kind) const {
            auto found = default_identifier_prefix_.find(kind);
            if (found != default_identifier_prefix_.end()) {
                return found->second;
            }
            return "tmp";
        }

        const ebm::Loc* get_debug_loc(const ebm::AnyRef& ref) const;

        void directly_map_statement_identifier(ebm::StatementRef ref, std::string&& name);
        void remove_directly_mapped_statement_identifier(ebm::StatementRef ref);
        void build_maps(mapping::BuildMapOption options = mapping::BuildMapOption::BUILD_MAP_USE_DEBUG_LOC | mapping::BuildMapOption::BUILD_MAP_USE_INVERSE_REF);

        void set_identifier_modifier(std::function<void(ebm::StatementRef, std::string&)>&& modifier) {
            identifier_modifier = std::move(modifier);
        }

       private:
        EBMProxy module_;
        // Caches for faster lookups
        std::unordered_map<std::uint64_t, const ebm::Identifier*> identifier_map_;
        std::unordered_map<std::uint64_t, const ebm::StringLiteral*> string_literal_map_;
        std::unordered_map<std::uint64_t, const ebm::Type*> type_map_;
        std::unordered_map<std::uint64_t, const ebm::Statement*> statement_map_;
        std::unordered_map<std::uint64_t, const ebm::Expression*> expression_map_;
        std::unordered_map<std::uint64_t, std::vector<InverseRef>> inverse_refs_;
        std::unordered_map<ebm::StatementKind, std::string> default_identifier_prefix_;
        std::unordered_map<std::uint64_t, std::string> statement_identifier_direct_map_;
        std::unordered_map<std::uint64_t, const ebm::Loc*> debug_loc_map_;
        std::function<void(ebm::StatementRef, std::string&)> identifier_modifier;
    };
}  // namespace ebmgen