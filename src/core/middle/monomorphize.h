/*license*/
#pragma once
#include "../common/error.h"
#include <core/ast/ast.h>
#include <memory>

namespace brgen::middle {

    // Expand every GenericType instance under `program` into a concrete Format
    // clone (appended to `program->elements`) and rewrite each GenericType in
    // the tree into a plain IdentType referring to the clone. Runs after typing
    // and before size analysis. Nested generic instantiation (generics in type
    // arguments) is not supported in this MVP.
    //
    // Unexpected states (unresolved target, arity mismatch, missing body) are
    // reported through `warnings` rather than silently skipped.
    void monomorphize(const std::shared_ptr<ast::Program>& program, LocationError* warnings);

}  // namespace brgen::middle
