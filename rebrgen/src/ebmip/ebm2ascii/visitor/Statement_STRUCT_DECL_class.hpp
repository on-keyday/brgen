/*DO NOT EDIT BELOW SECTION MANUALLY*/
/*license*/
/*
  Name: Statement_STRUCT_DECL_class
*/
/*DO NOT EDIT ABOVE SECTION MANUALLY*/

#include "../codegen.hpp"
#include "common.hpp"

DEFINE_VISITOR(Statement_STRUCT_DECL) {
    using namespace CODEGEN_NAMESPACE;
    if (is_nil(ctx.struct_decl.name)) {
        return {};  // anonymous struct — skip for MVP
    }

    auto name = ctx.identifier();

    // Extract pass: walk fields into flat list
    FieldExtractor extractor{ctx.visitor};
    for (auto& field_ref : ctx.struct_decl.fields.container) {
        MAYBE_VOID(_, ctx.visit<void>(extractor, field_ref));
    }

    Diagram d{
        .name = name,
        .fields = std::move(extractor.fields),
        .total_bits = extractor.bit_offset,
        .has_variable = extractor.has_variable,
    };

    // Render pass: dispatch on --format flag
    auto& root = ctx.visitor.wm.root;
    std::string_view fmt = ctx.flags().format;
    std::string rendered;
    if (fmt == "table") {
        rendered = render_table(d);
    }
    else {
        rendered = render_ascii(d, ctx.flags().width);
    }
    root.writeln(rendered);
    return {};
}
