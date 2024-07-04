/*license*/
#include <core/middle/resolve_import.h>
#include <core/middle/resolve_available.h>
#include <core/middle/replace_assert.h>
#include <core/middle/replace_order_spec.h>
#include <core/middle/replace_error.h>
#include <core/middle/resolve_io_operation.h>
#include <core/middle/replace_metadata.h>
#include <core/middle/resolve_state_dependency.h>
#include <core/middle/typing.h>
#include <core/middle/type_attribute.h>
#include <core/middle/analyze_block_trait.h>
#include <core/common/file.h>

namespace brgen::test {
    // simplified version of src2json
    // this only parse success case, otherwise throw exception
    std::shared_ptr<ast::Program> src2json(FileSet& fs) {
        auto input = fs.get_input(1);
        if (!input) {
            throw std::runtime_error("no input file");
        }
        brgen::ast::Context c;
        brgen::LocationError err_or_warn;
        auto program = c.enter_stream(input, [&](brgen::ast::Stream& s) {
            return brgen::ast::parse(s, &err_or_warn,
                                     {.collect_comments = true});
        });
        if (!program) {
            program.throw_error();
        }
        auto& p = *program;
        auto ok = brgen::middle::resolve_import(p, fs, err_or_warn, {.collect_comments = true});
        if (!ok) {
            ok.throw_error();
        }
        ok = brgen::middle::resolve_available(p);
        if (!ok) {
            ok.throw_error();
        }
        brgen::middle::replace_specify_order(p);
        ok = brgen::middle::replace_explicit_error(p);
        if (!ok) {
            ok.throw_error();
        }
        ok = brgen::middle::resolve_io_operation(p);
        if (!ok) {
            ok.throw_error();
        }
        brgen::middle::replace_metadata(p);
        ok = brgen::middle::analyze_type(p, &err_or_warn);
        if (!ok) {
            ok.throw_error();
        }
        brgen::middle::replace_assert(p, err_or_warn);
        brgen::middle::mark_recursive_reference(p);
        brgen::middle::detect_non_dynamic_type(p);
        brgen::middle::analyze_bit_size_and_alignment(p);
        brgen::middle::resolve_state_dependency(p);
        brgen::middle::analyze_block_trait(p);
        return p;
    }
}  // namespace brgen::test
