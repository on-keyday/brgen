#pragma once

#include <memory>
#include <string_view>
#include <functional>
#include <core/ast/ast.h>
#include "common.hpp"
#include "converter.hpp"

namespace ebmgen {
    // Function to load brgen AST from JSON
    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json(std::string_view input, std::function<void(const char*)> timer_cb);
    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json_file(futils::view::rvec input, std::function<void(const char*)> timer_cb);
    expected<ebm::ExtendedBinaryModule> load_json_ebm(std::string_view input);
    expected<ebm::ExtendedBinaryModule> decode_json_ebm(futils::view::rvec input);

    // Test-only accessor for EBMRepository internals
    // This class is a friend of EBMRepository and provides access to internal methods
    // that should not be used in normal code paths
    struct TestRepositoryAccessor {
        EBMRepository& repo;

        explicit TestRepositoryAccessor(EBMRepository& r) : repo(r) {}

        expected<ebm::IdentifierRef> add_identifier_with_id(ebm::IdentifierRef id, ebm::String&& body) {
            return repo.identifier_repo.add(id, std::move(body));
        }

        expected<ebm::StringRef> add_string_with_id(ebm::StringRef id, ebm::String&& body) {
            return repo.string_repo.add(id, std::move(body));
        }

        expected<ebm::TypeRef> add_type_with_id(ebm::TypeRef id, ebm::TypeBody&& body) {
            return repo.type_repo.add(id, std::move(body));
        }

        expected<ebm::StatementRef> add_statement_with_id(ebm::StatementRef id, ebm::StatementBody&& body) {
            return repo.statement_repo.add(id, std::move(body));
        }

        expected<ebm::ExpressionRef> add_expression_with_id(ebm::ExpressionRef id, ebm::ExpressionBody&& body) {
            return repo.expression_repo.add(id, std::move(body));
        }

        void load_aliases(std::vector<ebm::RefAlias>&& aliases) {
            repo.aliases = std::move(aliases);
        }

        void load_debug_info(std::vector<ebm::StringRef>&& files, std::vector<ebm::Loc>&& locs) {
            repo.file_names = std::move(files);
            repo.debug_locs = std::move(locs);
        }

        void set_max_id(std::uint64_t id) {
            repo.ident_source.set_current_id(id);
        }

        void recalculate_caches() {
            repo.identifier_repo.recalculate_id_index_map();
            repo.string_repo.recalculate_id_index_map();
            repo.type_repo.recalculate_id_index_map();
            repo.expression_repo.recalculate_id_index_map();
            repo.statement_repo.recalculate_id_index_map();
        }
    };

    // Load ExtendedBinaryModule into EBMRepository for testing transform passes
    expected<void> load_ebm_to_repository(EBMRepository& repo, ebm::ExtendedBinaryModule&& ebm);

    // Helper class for testing transform passes
    // Usage:
    //   TestContextLoader loader;
    //   auto result = loader.load_json_file("test_data.json");
    //   if (!result) { handle error }
    //   auto& tctx = loader.transform_context();
    //   flatten_io_expression(tctx);
    class TestContextLoader {
       public:
        TestContextLoader() : tctx_(ctx_) {}

        expected<void> load_json_ebm_file(std::string_view file_path);
        expected<void> load_json_ebm_string(std::string_view json_str);
        expected<void> load_ebm(ebm::ExtendedBinaryModule&& ebm);

        ConverterContext& converter_context() { return ctx_; }
        TransformContext& transform_context() { return tctx_; }

        // Export result to ExtendedBinaryModule for verification
        expected<ebm::ExtendedBinaryModule> finalize(bool verify_uniqueness = false);

       private:
        ConverterContext ctx_;
        TransformContext tctx_;
    };
}  // namespace ebmgen
