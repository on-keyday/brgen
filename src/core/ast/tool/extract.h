/*license*/
#pragma once

#include "extract_config.h"
#include "eval.h"

namespace brgen::ast::tool {
    enum class ValueStyle {
        assign,
        call,
        any,
    };

    template <EResultType t, bool unescape_str = true>
    result<std::decay_t<decltype(EvalResult{}.get<t>())>> get_config_value(
        Evaluator& eval, const ConfigDesc& conf, ValueStyle style = ValueStyle::assign, size_t index = 0, size_t size_require = 1) {
        if (style == ValueStyle::assign) {
            if (!conf.assign_style || conf.arguments.size() != 1) {
                return brgen::unexpect(brgen::error(conf.loc, conf.name, " must be assigned"));
            }
        }
        else if (style == ValueStyle::call) {
            if (conf.assign_style) {
                return brgen::unexpect(brgen::error(conf.loc, conf.name, " must be called"));
            }
        }
        if (conf.arguments.size() != size_require) {
            return brgen::unexpect(brgen::error(conf.loc, conf.name, " must have ", nums(size_require), " arguments"));
        }
        if (conf.arguments.size() <= index) {
            return brgen::unexpect(brgen::error(conf.loc, conf.name, " must have at least ", nums(index + 1), " arguments"));
        }
        auto r = eval.template eval_as<t, unescape_str>(conf.arguments[index]);
        if (!r) {
            return r.transform([](auto&&) { return std::decay_t<decltype(EvalResult{}.get<t>())>{}; }).transform_error(ast::tool::to_loc_error());
        }
        return r->template get<t>();
    }
}  // namespace brgen::ast::tool
