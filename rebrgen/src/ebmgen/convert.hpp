#pragma once

#include <memory>
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include "common.hpp"
namespace ebmgen {
    // Function to convert brgen AST to ExtendedBinaryModule
    // This will be the main entry point for the conversion logic
    struct Option {
        bool not_remove_unused = false;  // for debug transformation
        bool verify_uniqueness = false;  // verify uniqueness of identifiers
        std::function<void(const char*)> timer_cb;
    };

    struct Output {
    };

    expected<Output> convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, std::vector<std::string>&& files, ebm::ExtendedBinaryModule& ebm, Option opt);

}  // namespace ebmgen
