#include "converter.hpp"
#include "transform/transform.hpp"
#include "convert.hpp"

namespace ebmgen {

    expected<Output> convert_ast_to_ebm(std::shared_ptr<brgen::ast::Node>& ast_root, std::vector<std::string>&& file_names, ebm::ExtendedBinaryModule& ebm, Option opt) {
        ConverterContext converter;
        MAYBE(s, converter.convert_statement(ast_root));
        if (opt.timer_cb) {
            opt.timer_cb("convert");
        }
        MAYBE_VOID(file_names, converter.repository().add_files(std::move(file_names)));
        TransformContext transform_ctx(converter);
        MAYBE_VOID(t, transform(transform_ctx, opt.not_remove_unused, opt.timer_cb));
        MAYBE_VOID(f, converter.repository().finalize(ebm, opt.verify_uniqueness));
        if (opt.timer_cb) {
            opt.timer_cb("finalize");
        }
        return {};
    }

}  // namespace ebmgen
